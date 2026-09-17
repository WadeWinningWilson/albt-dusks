// ============================================
// NEW CODE - ALBW Port (supplemental Tear-of-Light scene particles)
//
// WHY THE TEAR IS INVISIBLE (audio works)
// ------------------------------------------------------------------
// The death recovery orb's visible body is SCENE particles (Pscene###.jpc, res
// ids 0x838B..0x842B). Those FX only exist in some overworld archives; every
// dungeon (and many stages) loads a scene archive WITHOUT them, so
// createBodyEffect() makes empty emitters -> an invisible-but-working orb, while
// the appear SFX still plays. This is the fork's documented trap
// (d_albw_death_rupee.cpp / d_particle.cpp).
//
// The fork fixes it with a supplemental archive: Pscene011.jpc (Faron Woods,
// which has the full tear FX set) is loaded once onto its own heap, registered
// in JPAEmitterManager slot 2, and dPa_control_c::set() remaps tear ids to that
// slot when the current stage's own scene archive lacks them.
//
// This module reproduces that verbatim. dPa_control_c::set() is stock here (no
// fork edit), but it derives the resource-manager slot from the EXPORTED static
// dPa_control_c::getRM_ID(resID) and uses it directly — so hooking getRM_ID to
// apply the same fallback lands at the identical seam.
// ============================================

#include "global.h"
#include <os.h>

// Open d_particle.h FIRST under private->public so dPa_control_c's mSceneResMng
// is reachable; later headers that pull it hit the include guard.
#define private public
#include "d/d_particle.h"
#undef private

#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_dvd_thread.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_graphic.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JParticle/JPAEmitterManager.h"
#include "JSystem/JParticle/JPAResourceManager.h"
#include "JSystem/JParticle/JPAResource.h"
#include "JSystem/JParticle/JPATexture.h"  // JPATexture::getName/mpData (drawP diag)
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JParticle/JPABaseShape.h"

#include "albw_common.h"
#include "modules.h"
#include "mods/svc/hook.hpp"

#include <cstdio>

#if TARGET_PC

namespace {

// Faron Woods scene archive: the one Pscene with the full tear FX set
// (confirmed in the fork's orb debug log: sceneJpc=Pscene011 hasTearRes=1).
constexpr const char* kTearScenePath = "/res/Particle/Pscene011.jpc";
constexpr u8          kTearRmSlot    = 2;   // JPAEmitterManager supplemental slot

JKRExpHeap*             sTearHeap       = NULL;
mDoDvdThd_toMainRam_c*  sTearResCommand = NULL;
JPAResourceManager*     sTearResMng     = NULL;
bool                    sTearResFailed  = false;

DEFINE_HOOK(&dPa_control_c::getRM_ID, TearGetRmId);
// fork creates sTearHeap in the dPa_control_c ctor (d_particle.cpp:1294) - a stable,
// once-per-boot allocation from the archive heap. We can't hook the ctor, so create it
// here in createCommon (the very next init step, same heap phase). This is load-bearing:
// creating the 0xA0000 heap LAZILY at death instead corrupts the death->respawn stage
// transition (the archive heap is being rebuilt then), which crashes in JPATexture::load.
DEFINE_HOOK(&dPa_control_c::createCommon, TearCreateCommon);
DEFINE_HOOK(&JPAResource::drawP, TearDrawP);

// TEMP DIAGNOSTIC: JPALoadTex (the crash site) is too short to hook, so instrument
// its hookable caller drawP and replicate JPALoadTex's index math:
// mpResMgr->load(mpRes->getTexIdx(pBsp->getTexIdx())). Log the fault case (texIdx
// out of the RM's registered range, or in range but the texture slot is NULL).
HookAction on_draw_p_pre(ModContext*, void* args, void*, void*) {
    JPAResource* res = mods::arg<JPAResource*>(args, 0);
    JPAEmitterWorkData* work = mods::arg<JPAEmitterWorkData*>(args, 1);
    if (res == NULL || work == NULL || work->mpResMgr == NULL) {
        return HOOK_CONTINUE;
    }
    JPABaseShape* bsp = res->getBsp();
    if (bsp == NULL) {
        return HOOK_CONTINUE;
    }
    const u8 animIdx = bsp->getTexIdx();
    const u16 texIdx = res->getTexIdx(animIdx);
    const u16 texReg = work->mpResMgr->texRegNum;
    const bool inRange = texIdx < texReg;
    const bool nullTex = inRange && work->mpResMgr->pTexAry != NULL &&
                         work->mpResMgr->pTexAry[texIdx] == NULL;

    // DECISIVE PROBE + GUARD: the crash is JUTTexture::load -> `if (mPalette) mPalette->load()`
    // with a garbage mPalette (deterministic fault 0x41900148). Detect any drawP whose
    // JPATexture has a non-canonical palette pointer, NAME it (identifies the culprit texture
    // and which manager), and SKIP that single draw to prevent the crash - a mod-side guard at
    // the JParticle consumption seam (no fork equivalent: the fork never reaches a bad palette).
    if (inRange && work->mpResMgr->pTexAry != NULL) {
        JPATexture* tex = work->mpResMgr->pTexAry[texIdx];
        if (tex != NULL) {
            const uintptr_t pal = reinterpret_cast<uintptr_t>(tex->getJUTTexture()->getPalette());
            if (pal != 0 && (pal < 0x10000ULL || pal >= 0x0000800000000000ULL)) {
                if (svc_log != nullptr) {
                    static int s_bad = 0;
                    if (s_bad < 16) {
                        s_bad++;
                        const JPATextureData* data = tex->mpData;
                        const char* name = (data != NULL) ? tex->getName() : "(nodata)";
                        char buf[192];
                        std::snprintf(
                            buf, sizeof(buf),
                            "[tear] BADTEX name='%.20s' pal=%p mng=%p tear=%d texIdx=%u/%u",
                            name, (void*)pal, (void*)work->mpResMgr,
                            work->mpResMgr == sTearResMng ? 1 : 0, (unsigned)texIdx,
                            (unsigned)texReg);
                        svc_log->info(mod_ctx, buf);
                    }
                }
                return HOOK_SKIP_ORIGINAL;
            }
        }
    }

    // TEMP DIAGNOSTIC: log EVERY drawP that resolves through the supplemental tear
    // manager (slot 2). The crash is JPATexture::load (mTexture.load) on a valid,
    // non-null JPATexture, so the fault is the texture's BACKING data - most likely
    // the swapped "dummy" (framebuffer timg). The LAST line before the log ends is the
    // texture that JUTTexture::load choked on; its name tells us if it is "dummy".
    if (svc_log != nullptr && work->mpResMgr == sTearResMng) {
        static int s_t = 0;
        if (s_t < 24) {
            s_t++;
            JPATexture* tex = (inRange && work->mpResMgr->pTexAry != NULL)
                                  ? work->mpResMgr->pTexAry[texIdx]
                                  : NULL;
            const JPATextureData* data = (tex != NULL) ? tex->mpData : NULL;
            const char* name = (data != NULL) ? tex->getName() : "(nodata)";
            char buf[176];
            std::snprintf(buf, sizeof(buf),
                          "[tear] drawP SLOT2 animIdx=%u texIdx=%u/%u tex=%p data=%p name='%.20s'",
                          (unsigned)animIdx, (unsigned)texIdx, (unsigned)texReg, (void*)tex,
                          (const void*)data, name);
            svc_log->info(mod_ctx, buf);
        }
    }

    if ((!inRange || nullTex) && svc_log != nullptr) {
        static int s_n = 0;
        if (s_n < 20) {
            s_n++;
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "[tear] drawP FAULT animIdx=%u texIdx=%u texReg=%u nullTex=%d",
                          (unsigned)animIdx, (unsigned)texIdx, (unsigned)texReg, nullTex ? 1 : 0);
            svc_log->info(mod_ctx, buf);
        }
    }
    return HOOK_CONTINUE;
}

// fork dPa_control_c::hasSceneParticleRes — does the CURRENT stage's scene
// archive already contain this id? (Then no fallback needed.)
bool sceneHasRes(const dPa_control_c* pa, u16 resID) {
    return pa != NULL && pa->mSceneResMng != NULL &&
           pa->mSceneResMng->checkUserIndexDuplication(resID);
}

// Create the supplemental tear heap ONCE, from the archive heap - matching the fork's
// dPa_control_c ctor (d_particle.cpp:1294). Must run at a stable init point, NOT lazily
// at death: a mid-transition 0xA0000 alloc corrupts the rebuilding archive heap.
bool ensure_tear_heap() {
    if (sTearHeap != NULL) {
        return true;
    }
    if (sTearResFailed) {
        return false;
    }
    JKRHeap* archive = mDoExt_getArchiveHeap();
    if (archive == NULL) {
        return false;
    }
    sTearHeap = JKRCreateExpHeap(0xA0000, archive, false);
    if (sTearHeap == NULL) {
        sTearResFailed = true;
        return false;
    }
    if (svc_log != nullptr) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "[tear] HEAP created heap=%p parent=%p", (void*)sTearHeap,
                      (void*)archive);
        svc_log->info(mod_ctx, buf);
    }
    return true;
}

// fork dPa_control_c ctor:1294 - allocate the tear heap at particle init, once.
void on_create_common_post(ModContext*, void*, void*, void*) {
    if (svc_log != nullptr) {
        svc_log->info(mod_ctx, "[tear] createCommon hook fired - pre-creating heap");
    }
    ensure_tear_heap();
}

}  // namespace

// fork dPa_control_c::ensureTearSceneRes — load Pscene011.jpc once onto sTearHeap
// and register it in emitter slot 2. Async (DVD->main), so it is polled each
// frame the orb executes until sync() completes. Returns true once resident.
bool albw_tear_ensure_scene_res() {
    if (sTearResMng != NULL) {
        return true;
    }
    if (sTearResFailed) {
        return false;
    }
    if (sTearHeap == NULL) {
        // Normally the heap is already created by the createCommon hook (fork ctor
        // timing). This lazy path is a fallback only - if it runs during the death
        // stage transition it can corrupt the rebuilding archive heap, so the early
        // createCommon creation is the real fix.
        if (!ensure_tear_heap()) {
            return false;
        }
    }
    if (sTearResCommand == NULL) {
        sTearResCommand = mDoDvdThd_toMainRam_c::create(kTearScenePath, 0, sTearHeap);
        if (svc_log != nullptr) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "[tear] QUEUE read cmd=%p", (void*)sTearResCommand);
            svc_log->info(mod_ctx, buf);
        }
        if (sTearResCommand == NULL) {
            sTearResFailed = true;
            OS_REPORT("ALBW tear res: failed to queue Pscene011.jpc read\n");
        }
        return false;
    }
    if (!sTearResCommand->sync()) {
        if (svc_log != nullptr) {
            static int s_sync = 0;
            if (s_sync < 6) {
                s_sync++;
                svc_log->info(mod_ctx, "[tear] SYNC pending (read in flight)");
            }
        }
        return false;  // read still in flight
    }
    void* data = sTearResCommand->getMemAddress();
    sTearResCommand->destroy();
    sTearResCommand = NULL;
    if (svc_log != nullptr) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "[tear] SYNC done data=%p", data);
        svc_log->info(mod_ctx, buf);
    }
    if (data == NULL) {
        sTearResFailed = true;
        OS_REPORT("ALBW tear res: Pscene011.jpc read returned no data\n");
        return false;
    }
    sTearResMng = JKR_NEW_ARGS(sTearHeap, 0) JPAResourceManager(data, sTearHeap);
    if (sTearResMng == NULL) {
        sTearResFailed = true;
        return false;
    }
    ResTIMG* fbTimg = mDoGph_gInf_c::getFrameBufferTimg();
    const ResTIMG* oldDummy = sTearResMng->swapTexture(fbTimg, "dummy");
    if (svc_log != nullptr) {
        char buf[112];
        std::snprintf(buf, sizeof(buf), "[tear] swap fbTimg=%p oldDummy=%p (null fbTimg = crash)",
                      (void*)fbTimg, (const void*)oldDummy);
        svc_log->info(mod_ctx, buf);
    }
    JPAEmitterManager* mng = dPa_control_c::getEmitterManager();
    if (mng != NULL) {
        mng->entryResourceManager(sTearResMng, kTearRmSlot);
    }
    OS_REPORT("ALBW tear res: supplemental Pscene011 registered (slot 2)\n");
    return true;
}

namespace {

// fork dPa_tearResFallbackRM — when the natural slot is the scene bank (1) and
// the current stage lacks this id but the supplemental tear archive has it,
// redirect to slot 2. Applied at getRM_ID (the seam stock set() reads from).
void on_get_rm_id_post(ModContext*, void* args, void* retval, void*) {
    if (retval == NULL) {
        return;
    }
    u8* rmID = static_cast<u8*>(retval);
    const u16 resID = mods::arg<u16>(args, 0);
    dPa_control_c* pa = g_dComIfG_gameInfo.play.getParticle();

    // TEMP DIAGNOSTIC: fires for tear-range ids BEFORE any early-return, so we see
    // what the tear resolves to in Ordon even when the supplemental isn't resident.
    if (svc_log != nullptr && resID >= 0x838B && resID <= 0x842B) {
        static int s_n = 0;
        if (s_n < 40) {
            s_n++;
            const bool sh = sceneHasRes(pa, resID);
            const bool su = (sTearResMng != NULL) && sTearResMng->checkUserIndexDuplication(resID);
            const int texReg = (sTearResMng != NULL) ? (int)sTearResMng->texRegNum : -1;
            const int resReg = (sTearResMng != NULL) ? (int)sTearResMng->resRegNum : -1;
            char buf[144];
            std::snprintf(
                buf, sizeof(buf),
                "[tear] getRM_ID res=0x%04X inSlot=%d sceneHas=%d supp=%d mng=%d slot2:res=%d tex=%d",
                resID, (int)*rmID, sh ? 1 : 0, su ? 1 : 0, sTearResMng != NULL ? 1 : 0, resReg,
                texReg);
            svc_log->info(mod_ctx, buf);
        }
    }

    if (sTearResMng == NULL) {
        return;
    }
    if (*rmID != 1) {
        return;
    }
    if (sceneHasRes(pa, resID)) {
        return;  // current stage already has it — leave on slot 1
    }
    if (sTearResMng->checkUserIndexDuplication(resID)) {
        *rmID = kTearRmSlot;
    }
}

}  // namespace

ModResult albw_tear_particles_init(ModError*) {
    if (mods::hook::add_post<TearGetRmId>(on_get_rm_id_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook dPa_control_c::getRM_ID (tear particles)");
        return MOD_ERROR;
    }
    // Pre-create the tear heap at particle init (fork ctor timing) so the death-time
    // load kick never allocates during the stage transition.
    if (mods::hook::add_post<TearCreateCommon>(on_create_common_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook dPa_control_c::createCommon (tear heap)");
        return MOD_ERROR;
    }
    if (mods::hook::add_pre<TearDrawP>(on_draw_p_pre) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook JPAResource::drawP (tear diag)");
    }
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
