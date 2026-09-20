// ============================================
// NEW CODE — ALBW Port (daAlink_c::changeLink whole-function replacement)
//
// Fixes the Zora + Sumo quick-swap crash. See changelink.h for the full write-up:
// stock changeLink builds the sumo body's face from getObjectRes(mArcName,
// "al_face.bmd"); over the Zora base ("Zmdl") that arc ships only zl_face, so the
// lookup is NULL, initModel(NULL) yields a NULL face model, and the eye-LOD block
// then derefs mpLinkFaceModel->getModelData()->getTexture() -> access violation.
//
// The fork hardened changeLink's face sourcing into a cascade (private Kmdl al_face
// via dAlbwSumoTest_sumoFaceData, else the Kmdl base arc over a Zora base, never a
// NULL face). We port the WHOLE fork changeLink body verbatim onto a layout-
// compatible subclass (AlbwChangeLink_c) and dispatch every call to it from a
// changeLink pre-hook + HOOK_SKIP_ORIGINAL — the same technique as outfit_swim.cpp /
// mq_heart_meter.cpp, because changeLink is an exported daAlink_c method and cannot
// be redefined (LNK2005). Sibling method calls inside the body resolve to the
// exported stock methods through inheritance.
//
// User-approved DN-10 escalation (replacing an exported engine function incl. its
// file-static deps). This is the escalation alink_compat.cpp:32-36 flagged as
// "needs the user's go" and clothes_pipeline.cpp:418-427 documented as blocked.
//
// COEXISTENCE with clothes_pipeline.cpp's existing changeLink hooks: the hook
// framework keys its trampoline by target VALUE (HookImpl<TargetTag<Target>>), so
// our DEFINE_HOOK on the same &daAlink_c::changeLink shares clothes_pipeline's single
// trampoline. All pre-hooks run (its magic-ready flag setter AND our replacement),
// our SKIP_ORIGINAL suppresses only the real original, and all post-hooks (its token
// stamp) still run. albw_changelink_init is wired AFTER albw_clothes_pipeline_init so
// the magic-ready pre-hook registers first (dispatch order preserves the pre-existing
// FLG2_UNK_200000 set-then-clear behaviour exactly).
//
// BOUNDARY TRANSLATIONS (DN-10 step 2 — the DUSK's own porting convention; the fork
// body stays byte-verbatim, the environment is translated):
//   * dusk::getSettings().game.armorRupeeDrain / dusk::MagicArmorMode and
//     dusk::custom_assets::try_load — added to albw_dusk_compat.h (armorRupeeDrain
//     fixed at NORMAL, try_load returns null): the Magic branch + WW-boots block
//     then compile verbatim and fold to the stock behaviour the DUSK runs today.
//   * midna->resetDemoBck() — the fork-only daMidna_c method is retargeted in the
//     .inc to albw_midna_reset_demo_bck (clothes_pipeline's verbatim free-function
//     port of the same body).
//   * setMagicArmorBrk — the fork returns BOOL, stock's is void; AlbwChangeLink_c
//     overrides it to forward to the hooked stock method and report resolution.
//   * The fork actor-internal build/epoch/foot statics (s_albw*) get inert local
//     copies below: the DUSK maintains the REAL equivalents via separate hooks
//     (clothes_pipeline token stamp + draw guard, alink_compat epoch), so these
//     writes are dead — they exist only so the verbatim body links.
// ============================================

#include "global.h"
#include <os.h>

#include "JSystem/J3DGraphAnimator/J3DAnimation.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_resorce.h"
#include "m_Do/m_Do_ext.h"

#define private public
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_midna.h"
#undef private

#include "changelink.h"
#include "clothes_pipeline.h"   // albw_midna_reset_demo_bck
#include "sumo_test.h"          // dAlbwSumoTest_*
#include "outfit.h"             // dAlbwOutfit_isSumoWorn
#include "albw_dusk_compat.h"   // dusk::getSettings / CapWearMode / MagicArmorMode / custom_assets
#include "albw_fork_compat.h"   // dMeter2_isALBWArmorDepleted
#include "albw_common.h"
#include "config_vars.h"  // outfit-feature gate for the changeLink dispatch
#include "albw_dusk_log.h"      // DuskLog
#include "modules.h"
#include "mods/svc/hook.hpp"

#if TARGET_PC

// The fork ships its build/anim lifecycle probes behind this define; force it 0 so
// those diagnostic blocks compile out (they reference debug-only helpers/locals).
#ifndef D_ALBW_ARC_LIFECYCLE_DEBUG
#define D_ALBW_ARC_LIFECYCLE_DEBUG 0
#endif

// ============================================
// Fork actor-internal file-statics the ported changeLink writes. In the fork these
// live in d_a_alink.cpp and feed guards inlined into draw()/modelDraw(). The DUSK
// maintains the real equivalents from SEPARATE hooks (clothes_pipeline token stamp +
// AlinkDrawGuard, alink_compat epoch), which still run around this replacement, so
// these copies are inert — present only so the verbatim body links.
// ============================================
static u32         s_albwArcEpoch          = 0;
static u32         s_albwClothesModelEpoch  = 0;
static u32         s_albwBuiltModelState    = 0xFFFFFFFF;
static bool        s_albwBuildIntegrityOk   = true;
static u32         s_albwBuildCounter       = 0;
static const char* s_albwPrevArcName        = NULL;
static int         s_albwFootReseedFrames   = 0;
static bool        s_albwWwBootsSkinned     = false;
static bool        s_albwNativeCapResolved  = true;
// ============================================
// NEW CODE - outfit-transition crash family P3 (single magic-ready flag)
// s_albwMagicModelReady is NO LONGER a local inert copy: unlike the statics
// above (whose real equivalents are maintained by separate hooks), this one had
// TWO live definitions - the ported body wrote THIS copy (changelink_port.inc:93)
// while the draw consumer albw_setWaterDropColor read clothes_pipeline.cpp's -
// so body and draw could diverge. It is now the ONE extern defined in
// clothes_pipeline.cpp and declared in clothes_pipeline.h (included above), and
// the ported body's write lands on the real flag.
// ============================================

// fork d_a_alink.cpp:199 (verbatim).
static inline u32 albwModelStateToken(bool wolf, bool sumoBody, bool casual, bool zora,
                                      bool magic) {
    return (wolf ? 1u : 0u) | (sumoBody ? 2u : 0u) | (casual ? 4u : 0u) | (zora ? 8u : 0u) |
           (magic ? 16u : 0u);
}

// fork d_a_alink.cpp:221 (verbatim) — build-time corruption detector the ported body
// runs at the end of a rebuild. getPEBlock() is a member read (no deref), so it never
// faults; the result feeds s_albwBuildIntegrityOk (inert here — see note above).
static int albwFirstCorruptMat(J3DModel* m) {
    if (m == NULL) return -1;            // absent model is not corrupt
    J3DModelData* md = m->getModelData();
    if (md == NULL) return -2;
    u16 n = md->getMaterialNum();
    if (n > 256) return -3;              // wild material count = corrupt
    const intptr_t base  = (intptr_t)md;
    const intptr_t kNear = (intptr_t)0x10000000;  // 256MB; BMD < 1MB, corruption ~GBs
    for (u16 k = 0; k < n; k++) {
        J3DMaterial* mat = md->getMaterialNodePointer(k);
        if (mat == NULL) return (int)k;
        intptr_t d = (intptr_t)mat->getPEBlock() - base;
        if (d < -kNear || d > kNear) return (int)k;
    }
    return -1;
}

// ============================================
// FILE-STATIC WALL — no-external-linkage statics the fork changeLink references,
// copied VERBATIM from the fork so the ported body binds them (they have no exported
// symbol). l_autoUpHeight/l_autoDownHeight are MUTABLE in the fork and read back by
// other d_a_alink.cpp code through the ENGINE's own copies; these local copies mean
// changeLink's writes to them are local-only (see residual-concern note in the port
// report). l_jntColData + l_crawl* are const init data (fork d_a_alink.cpp).
// ============================================
static Vec l_jntColPos0[] = {
    {0.0f, 0.0f, 0.0f},
    {44.0f, 0.0f, 0.0f}
};

static Vec l_jntColPos1 = {
    0.0f, -10.0f, 0.0f
};

static Vec l_jntColPos2[] = {
    {-5.0f, 0.0f, 0.0f},
    {28.0f, 0.0f, 0.0f}
};

static Vec l_jntColPos4[] = {
    {-4.0f, 0.0f, 0.0f},
    {28.0f, 0.0f, 0.0f}
};

static Vec l_jntColPos6 = { 8.0f, 0.0f, 0.0f };

static Vec l_jntColPos8[] = {
    {0.0f, 0.0f, 0.0f},
    {38.0f, 0.0f, 0.0f}
};

static Vec l_jntColPos9[] = {
    {0.0f, -5.0f, 0.0f},
    {35.0f, 0.0f, 0.0f}
};

static Vec l_jntColPos10[] = {
    {0.0f, 5.0f, 0.0f},
    {35.0f, 0.0f, 0.0f}
};

static Vec l_jntColPos11[] = {
    {0.0f, 0.0f, 0.0f},
    {48.0f, 2.0f, 0.0f}
};

static Vec l_jntColPos12[] = {
    {0.0f, 0.0f, 0.0f},
    {48.0f, -2.0f, 0.0f}
};

static Vec l_jntColPos13[] = {
    {0.0f, 5.0f, 0.0f},
    {22.0f, 0.0f, 0.0f}
};

static Vec l_jntColPos14[] = {
    {0.0f, -5.0f, 0.0f},
    {22.0f, 0.0f, 0.0f}
};

static Vec l_jntColPos15[] = {
    {-15.0f, 0.0f, 17.0f},
    {0.0f, 0.0f, 1.0f}
};

static Vec l_jntColPos16[] = {
    {13.0f, 13.0f, -8.0f},
    {0.0f, 1.0f, 0.0f}
};

static Vec l_jntColPos17 = {
    -15.0f, -10.0f, -30.0f
};

static dJntColData_c l_jntColData[] = {
    {
        1,
        0,
        1,
        18.0f,
        l_jntColPos0,
    },
    {
        0,
        0,
        4,
        16.0f,
        &l_jntColPos1,
    },
    {
        1,
        0,
        7,
        7.0f,
        l_jntColPos2,
    },
    {
        1,
        0,
        12,
        7.0f,
        l_jntColPos2,
    },
    {
        1,
        0,
        8,
        6.0f,
        l_jntColPos4,
    },
    {
        1,
        0,
        13,
        6.0f,
        l_jntColPos4,
    },
    {
        0,
        0,
        9,
        6.0f,
        &l_jntColPos6,
    },
    {
        0,
        0,
        14,
        6.0f,
        &l_jntColPos6,
    },
    {
        1,
        0,
        16,
        18.0f,
        l_jntColPos8,
    },
    {
        1,
        0,
        18,
        8.0f,
        l_jntColPos9,
    },
    {
        1,
        0,
        23,
        8.0f,
        l_jntColPos10,
    },
    {
        1,
        0,
        19,
        7.0f,
        l_jntColPos11,
    },
    {
        1,
        0,
        24,
        7.0f,
        l_jntColPos12,
    },
    {
        1,
        0,
        20,
        6.0f,
        l_jntColPos13,
    },
    {
        1,
        0,
        25,
        6.0f,
        l_jntColPos14,
    },
    {
        2,
        3,
        15,
        40.0f,
        l_jntColPos15,
    },
    {
        2,
        3,
        5,
        40.0f,
        l_jntColPos16,
    },
    {
        0,
        3,
        15,
        45.0f,
        &l_jntColPos17,
    },
};

static Vec const l_crawlTopUpOffset = {0.0f, 80.0f, 0.0f};

static Vec const l_crawlSideOffset = {55.0f, 80.0f, 0.0f};

static f32 l_autoUpHeight = 30.010000228881836f;

static f32 l_autoDownHeight = -30.010000228881836f;

// Arc-name statics (verbatim, fork d_a_alink.cpp:104/106/339/341). The resource
// manager keys on the STRING content, so getObjectRes(l_kArcName, ...) is the same
// lookup as the fork. (The one POINTER compare — bootsHIdx, mArcName == l_bArcName —
// only feeds custom_assets::try_load, which returns null here, so a mismatch there is
// harmless: the vanilla al_bootsH fallback runs regardless.)
static const char l_bArcName[] = "Bmdl";
static const char l_kArcName[] = "Kmdl";
static const char l_zArcName[] = "Zmdl";
static const char l_mArcName[] = "Mmdl";

// fork d_a_alink_kandelaar.inc:141 (verbatim) — joint callback the ported body binds
// to the kantera model. Free static function; no external linkage in the fork either.
static int daAlink_kandelaarModelCallBack(J3DJoint* i_joint, int param_1) {
    UNUSED(i_joint);

    daAlink_c* player_p = (daAlink_c*)j3dSys.getModel()->getUserArea();

    if (param_1 == 0) {
        player_p->kandelaarModelCallBack();
    }

    return 1;
}

// ============================================
// Layout-compatible subclass carrying the ported fork changeLink body.
// ============================================
class AlbwChangeLink_c : public daAlink_c {
public:
    void changeLink(int param_0);
    BOOL setMagicArmorBrk(int i_status);
};

// The fork's setMagicArmorBrk returns BOOL ("did the Brks resolve"); stock's is void.
// The DUSK hooks the stock method (clothes_pipeline albw_setMagicArmorBrk), which NULLs
// the Brks on a miss. Forward to it and report resolution, so the ported body only
// touches the Brks when they actually resolved (this also closes stock's latent
// unconditional NULL-Brk deref in the Magic branch).
BOOL AlbwChangeLink_c::setMagicArmorBrk(int i_status) {
    daAlink_c::setMagicArmorBrk(i_status);  // funchook intercepts -> albw_setMagicArmorBrk
    return (mMagicArmorBodyBrk != NULL && mMagicArmorHeadBrk != NULL) ? TRUE : FALSE;
}

#include "changelink_port.inc"  // AlbwChangeLink_c::changeLink (verbatim fork body)

// ============================================
// NEW CODE - outfit-transition crash family P1 (dispatch-gate latch)
// Sampled once per accepted clothes change (see changelink.h write-up); read by
// on_change_link_pre while a transition is in flight. File-static at file scope
// (not in the anonymous namespace) only so the extern accessors below can also
// touch it; linkage stays internal.
// ============================================
static bool s_changelinkGateLatch = false;

namespace {

// ============================================
// COMPAT GATE (fix: "loading the mod switches Link's outfit"): the ported fork
// body re-derives Link's clothes from OUR ALBW outfit state, so it must NOT own
// the model rebuild unless our outfit machinery is actually in play.
// Unconditional dispatch re-dressed Link at mod load and trampled every other
// legitimate rebuild (vanilla state, other mods such as cosmetics). Stock
// changeLink runs untouched whenever the gate is off; the hardened face cascade
// (the Zora+Sumo quick-swap crash fix) only matters when these features are on.
// ============================================
bool changelink_dispatch_active() {
    if (albw_cfg_bool(g_dpad_quick_swap, false)) return true;   // outfit cycling
    if (albw_cfg_bool(g_outfit_stats, false)) return true;      // outfit stats worn-kits
    if (albw_cfg_int(g_cap_wear, 0) != 0) return true;          // cap-wear variants
    return dAlbwOutfit_isSumoWorn();                            // sumo body active
}

DEFINE_HOOK(&daAlink_c::changeLink, ChangeLink);
HookAction on_change_link_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    // ============================================
    // NEW CODE - outfit-transition crash family P1 (gate latch, split-brain fix)
    // While a clothes transition is settling (wait timer running, alt-heap swap
    // active, or this call was made from inside the ported loadModelDVD's
    // completion branches), the dispatch decision is the value SAMPLED when the
    // change was accepted - a mid-transition flip of a gate input (sumo worn
    // bit cleared during decompose, outfit.cpp:274) must not hand the settling
    // rebuild to stock changeLink (stock's Magic branch derefs Brks our
    // SetMagicArmorBrk hook may have NULLed, stock d_a_alink_wolf.inc:342-363,
    // and has no Kmdl fallback). Idle calls evaluate live, exactly as before.
    // All-toggles-off: the latch is false when the gate was false at accept
    // time, so every dispatch - latched or live - stays stock.
    // ============================================
    const bool inFlight =
        link->mClothesChangeWaitTimer != 0 || albw_clothes_transition_in_flight();
    const bool active = inFlight ? s_changelinkGateLatch : changelink_dispatch_active();
    if (!active) {
        return HOOK_CONTINUE;
    }
    static_cast<AlbwChangeLink_c*>(link)->AlbwChangeLink_c::changeLink(mods::arg<int>(args, 1));
    return HOOK_SKIP_ORIGINAL;
}

}  // namespace

// ============================================
// NEW CODE - outfit-transition crash family P1 (latch accessors)
// External linkage: clothes_pipeline.cpp samples the latch at the accepted
// setClothesChange / metamorphose pickup and clears it in the P0 destructor
// teardown. Declared in changelink.h.
// ============================================
void albw_changelink_latch_dispatch_gate() {
    s_changelinkGateLatch = changelink_dispatch_active();
}

void albw_changelink_clear_dispatch_latch() {
    s_changelinkGateLatch = false;
}

ModResult albw_changelink_init(ModError* error) {
    if (mods::hook::add_pre<ChangeLink>(on_change_link_pre) != MOD_OK) {
        if (svc_log != nullptr) svc_log->error(mod_ctx, "failed to hook daAlink_c::changeLink");
        mods::set_error(error, MOD_ERROR, "changelink replacement");
        return MOD_ERROR;
    }
    if (svc_log != nullptr) {
        svc_log->info(mod_ctx, "albw changeLink replacement (hardened face cascade) ready");
    }
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
