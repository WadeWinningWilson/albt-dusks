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

#include "albw_common.h"
#include "modules.h"
#include "mods/svc/hook.hpp"

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

// fork dPa_control_c::hasSceneParticleRes — does the CURRENT stage's scene
// archive already contain this id? (Then no fallback needed.)
bool sceneHasRes(const dPa_control_c* pa, u16 resID) {
    return pa != NULL && pa->mSceneResMng != NULL &&
           pa->mSceneResMng->checkUserIndexDuplication(resID);
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
        // fork allocates this in the dPa_control_c ctor (0xA0000, archive heap);
        // do it lazily on first need so we don't hook the ctor.
        JKRHeap* archive = mDoExt_getArchiveHeap();
        if (archive == NULL) {
            return false;
        }
        sTearHeap = JKRCreateExpHeap(0xA0000, archive, false);
        if (sTearHeap == NULL) {
            sTearResFailed = true;
            return false;
        }
    }
    if (sTearResCommand == NULL) {
        sTearResCommand = mDoDvdThd_toMainRam_c::create(kTearScenePath, 0, sTearHeap);
        if (sTearResCommand == NULL) {
            sTearResFailed = true;
            OS_REPORT("ALBW tear res: failed to queue Pscene011.jpc read\n");
        }
        return false;
    }
    if (!sTearResCommand->sync()) {
        return false;  // read still in flight
    }
    void* data = sTearResCommand->getMemAddress();
    sTearResCommand->destroy();
    sTearResCommand = NULL;
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
    sTearResMng->swapTexture(mDoGph_gInf_c::getFrameBufferTimg(), "dummy");
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
    if (retval == NULL || sTearResMng == NULL) {
        return;
    }
    u8* rmID = static_cast<u8*>(retval);
    if (*rmID != 1) {
        return;
    }
    const u16 resID = mods::arg<u16>(args, 0);
    dPa_control_c* pa = g_dComIfG_gameInfo.play.getParticle();
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
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
