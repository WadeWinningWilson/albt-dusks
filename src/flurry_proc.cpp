// ============================================
// NEW CODE - ALBW Port (Flurry Rush attack proc) - part of dev.albt.albw.
//
// Port of the fork's Flurry Rush melee proc, src/d/actor/d_a_alink_flurry.inc
// (407 lines, twelve daAlink_c methods). Bodies are auto-extracted, BYTE-
// VERBATIM, by tools/port/port_tool.py (tools/port/flurry.json ->
// src/flurry_port.inc). Not one character of a fork body is retyped here.
//
// THE OVERLAY
// -----------
// The fork gave the rush its own proc, PROC_FLURRY_RUSH (fork id 0x162,
// fork d_a_alink.h:1372; proc-table row fork d_a_alink.cpp:2116). Stock has
// no such enumerator and no table slot - adding an id would index past
// daAlink_c::m_procInitTable - so the proc is OVERLAID onto an existing valid
// proc, the technique Hurricane Spin (src/hurricane_spin.cpp) already ships:
//
//   host proc  PROC_CUT_NORMAL          stock d_a_alink.h:1053
//   per-frame  procCutNormal            stock :1900   PRE + SKIP_ORIGINAL
//   re-entry   procCutNormalInit        stock :1899   PRE + SKIP_ORIGINAL
//   lifetime   commonProcInit           stock :1744   PRE (donor chokepoint)
//
// The fork's own comment calls the rush "a cut-normal swing with a custom
// chain gate" - flurryBeginSwing sets the cut-normal HIO params, the
// cut-normal animations and the cut-normal AT profile - so PROC_CUT_NORMAL is
// the host the donor's own body already assumes.
//
// THE SUBCLASS (why the bodies can stay verbatim)
// -----------------------------------------------
// A method body full of unqualified member access (`current.pos`, `mpHIO`,
// `setCutType(...)`) only compiles inside a daAlink_c. We cannot redefine the
// stock exported methods, so - exactly as outfit_swim.cpp does for the swim
// procs - the ported bodies live on a layout-compatible subclass,
// AlbwFlurry_c. Sibling calls inside the bodies resolve to the exported stock
// methods through inheritance, and a qualified call from the hooks is static
// dispatch on the same object. The ONLY transform port_tool applies is the
// rename `daAlink_c::` -> `AlbwFlurry_c::` on the definition headers.
//
// THE THREE GAPS (see MANIFEST for the full ledger)
//  1. daAlink_CutNmParamType   fork d_a_alink_cut.inc:22-28, reproduced
//                              verbatim below (file-scope enum, not a header
//                              symbol, absent from stock's includes)
//  2. l_halfAtnWaitBaseAnime   stock d_a_alink.cpp:133 - same file-static
//                              datum in BOTH trees, reproduced verbatim
//  3. manualShieldBlocksSwordInput()  fork-only daAlink_c method; the mod
//                              already has albw_manual_shield_blocks_sword()
//                              (shield_adapt.h:56). Rather than substituting
//                              inside a fork body, the subclass DECLARES the
//                              donor's method name and forwards - so the two
//                              bodies that call it stay byte-verbatim.
// ============================================

#include "flurry_proc.h"

#include "global.h"
#include "albw_common.h"
#include "albw_dusk_log.h"
#include "flurry_probe.h"
#include "flurry_rush.h"

#define private public
#include "d/actor/d_a_alink.h"
#undef private

#include "d/actor/d_a_player.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_xyz.h"
#include "Z2AudioLib/Z2SeMgr.h"

#include "shield_adapt.h"  // albw_manual_shield_blocks_sword
#include "mods/svc/hook.hpp"

#if TARGET_PC

namespace {

// ============================================
// GAP 1 - PORTED VERBATIM, fork src/d/actor/d_a_alink_cut.inc:22-28.
// A file-scope enum in the fork's cut TU, so it reaches no header and is
// absent from the stock include tree. Reproduced whole (all five members,
// original order) rather than just the three the flurry body needs, because a
// partial enum is a different type.
// ============================================
enum daAlink_CutNmParamType {
    CUT_NM_PARAM_VERTICAL,
    CUT_NM_PARAM_LEFT,
    CUT_NM_PARAM_RIGHT,
    CUT_NM_PARAM_COMBO_STAB,
    CUT_NM_PARAM_STAB,
};

// ============================================
// GAP 2 - PORTED VERBATIM, stock src/d/actor/d_a_alink.cpp:133 (the fork's
// copy, d_a_alink.cpp:865, is the identical line - this is stock data that
// simply never reached a header). flurryBeginSwing assigns it to
// field_0x3588, the half-attention wait base offset.
// DUSK_CONSTEXPR is host-internal, so the storage class is spelled out; the
// initialiser is unchanged.
// ============================================
const cXyz l_halfAtnWaitBaseAnime(3.5f, 97.0f, -7.0f);

}  // namespace

// ============================================
// The port surface. Every method below is defined ONLY in src/flurry_port.inc,
// verbatim from the fork, except the two marked ADAPTER - which exist so that
// the fork bodies need no substitution at all.
// ============================================
class AlbwFlurry_c : public daAlink_c {
public:
    // fork d_a_alink.h:1930-1941 (the twelve declarations), in fork order.
    int  procFlurryRushInit();                                  // fork .inc:243
    int  procFlurryRush();                                      // fork .inc:280
    void flurryReserveChainInput();                             // fork .inc:35
    bool flurryConsumeChainInput();                             // fork .inc:41
    void flurryBeginSwing(int i_swingIndex);                    // fork .inc:54
    bool flurryCheckSwordHit();                                 // fork .inc:133
    int  flurryExitToWait(int i_reason);                        // fork .inc:156
    bool flurryTryChainSwing(int i_nextSwingIndex);             // fork .inc:164
    f32  flurryEngageDistance() const;                          // fork .inc:178
    bool flurryIsWithinEngageRange(fopAc_ac_c* i_target) const; // fork .inc:182
    void flurryEnterWaitFirstSwing();                           // fork .inc:191
    bool flurryUpdateSnapToTarget();                            // fork .inc:200

    // ============================================
    // ADAPTER - the one substitution port_tool applies
    // (tools/port/flurry.json "substitutions"):
    //     commonProcInit(PROC_FLURRY_RUSH);  ->  flurryOverlayProcInit();
    // at fork d_a_alink_flurry.inc:250, the single line of the donor that
    // cannot survive the overlay because PROC_FLURRY_RUSH has no stock id.
    //
    // commonProcInit does exactly two things with the proc id
    // (stock d_a_alink.cpp:15159-15161):
    //     mpProcFunc = m_procInitTable[id].m_procFunc;
    //     mProcID    = id;
    //     mModeFlg   = m_procInitTable[id].m_mode;
    // The procFunc is irrelevant - we intercept procCutNormal and never let
    // the stock one run. The mode word is NOT irrelevant: the fork's table row
    // gives PROC_FLURRY_RUSH 0x101 (fork d_a_alink.cpp:2116) while
    // PROC_CUT_NORMAL carries 0x20000300 (stock d_a_alink.cpp:1062). So the
    // fork's own row value is written straight back over it. That is the
    // donor's data, reproduced - NOT a patch of the game's table, which would
    // change vanilla cut-normal behaviour with the feature off.
    // ============================================
    void flurryOverlayProcInit();

    // ADAPTER - gap 3. Donor name, mod implementation, so the two fork bodies
    // that call it (flurryReserveChainInput / flurryConsumeChainInput) are
    // byte-verbatim.
    bool manualShieldBlocksSwordInput() { return albw_manual_shield_blocks_sword(this); }
};

namespace {

// ============================================
// Overlay ownership. This bool is the overlay's stand-in for the one thing the
// donor gets for free - `mProcID == PROC_FLURRY_RUSH`. It is NOT redundant
// with `mProcID == PROC_CUT_NORMAL`: PROC_CUT_NORMAL is a proc Link enters
// constantly in ordinary play, so the proc id alone cannot say whether the
// overlay or a plain sword swing owns him. Set only by flurryOverlayProcInit,
// cleared only by the chokepoint / watch below.
// ============================================
bool s_overlayActive = false;

// Set only when ALL THREE hooks installed. An overlay must never be started
// when the commonProcInit lifetime chokepoint is missing - that is the exact
// shape of the Hurricane Spin stranding bug.
bool s_procInstalled = false;

#if ALBW_FLURRY_PROBE
const char* phase_name(int phase) {
    switch (phase) {
    case 0:  return "SnapToTarget";
    case 1:  return "WaitFirstSwing";
    case 2:  return "Swinging";
    case 3:  return "RecoveryGate";
    default: return "?";
    }
}

int s_probePhase = -1;
int s_probeSwing = -1;
int s_probeHitFlag = -1;
bool s_probeCpsLiveBefore = false;

// Every probe line fires on a CHANGE of the state it reports, never per frame.
// Nothing here touches a ported body: the probe samples the donor's own proc
// variables around the call, so instrumentation and fidelity do not trade off.
void probe_reset() {
    s_probePhase = -1;
    s_probeSwing = -1;
    s_probeHitFlag = -1;
    s_probeCpsLiveBefore = false;
}

void probe_sample(daAlink_c* link, bool before) {
    const int phase = link->mProcVar1.field_0x300a;
    const int swing = link->mProcVar5.field_0x3012;
    const int hit   = link->mProcVar3.field_0x300e;

    if (before) {
        s_probeCpsLiveBefore = false;
        for (int i = 0; i < 3; i++) {
            if (link->mAtCps[i].ChkAtHit()) {
                s_probeCpsLiveBefore = true;
            }
        }
        return;
    }

    if (phase != s_probePhase || swing != s_probeSwing) {
        DuskLog.info("[flurry] phase {} -> {} swing {} -> {} frame={} window=[{},{}) "
                     "cancel={} gateClose={}",
                     phase_name(s_probePhase), phase_name(phase), s_probeSwing, swing,
                     link->mUnderFrameCtrl[0].getFrame(), link->field_0x3478, link->field_0x347c,
                     link->field_0x3484, link->field_0x3480);
        s_probePhase = phase;
        s_probeSwing = swing;
        s_probeHitFlag = hit;  // a new swing resets the flag; do not report that as a hit
        return;
    }

    if (hit != s_probeHitFlag) {
        if (hit != 0) {
            // The discriminator the slow-mo made necessary. flurryCheckSwordHit
            // (fork .inc:133-154) first asks the three sword AT primitives, then
            // falls back to a Z-lock range test against the pinned target,
            // because at 0.1x the enemy's hurt spheres and Link's 1.0x sword
            // desync. If no primitive was live on entry, the fallback is what
            // scored - which is exactly the case worth seeing in a log.
            DuskLog.info("[flurry] HIT registered swing={} via={} frame={} window=[{},{})",
                         swing, s_probeCpsLiveBefore ? "atcps" : "slowmo-fallback",
                         link->mUnderFrameCtrl[0].getFrame(), link->field_0x3478,
                         link->field_0x347c);
        }
        s_probeHitFlag = hit;
    }
}
#endif  // ALBW_FLURRY_PROBE

}  // namespace

// ============================================
// The ONE adapted body. Everything else in flurry_port.inc is verbatim.
// ============================================
void AlbwFlurry_c::flurryOverlayProcInit() {
#if ALBW_FLURRY_PROBE
    const int equipBefore = mEquipItem;
#endif

    commonProcInit(PROC_CUT_NORMAL);
    mModeFlg = 0x101;  // fork m_procInitTable[PROC_FLURRY_RUSH] (d_a_alink.cpp:2116)

    // ============================================
    // DRAW THE SWORD - the one thing the donor never had to do.
    //
    // THE DEFECT. Every stock route into a sword swing is gated on
    // mEquipItem == 0x103, "sword in Link's hand":
    //     checkForceSwordSwing   stock d_a_alink_cut.inc:512-514
    //     checkCutJumpInFly      stock d_a_alink.cpp:11022
    //     procSideStep follow-up stock d_a_alink.cpp:15922
    // The overlay is the only entry that is not, because the fork's trigger is
    // a perfect dodge mid-combat - the sword is already out, so its own gate
    // asks for OWNERSHIP, not equip (checkSwordGet, fork
    // d_albw_flurry_rush.cpp:542). Our trigger is a backflip on bash charges,
    // which Link can perform with the sword on his back. The cut animation
    // then plays with the sword model still parented to the sheath
    // (setSwordPos takes the hand joint only for mEquipItem == 0x103, stock
    // :5907-5914) - the reported "Link does not pull out his sword", and
    // louder on non-wooden swords because those are the ones with a visible
    // sheath to leave it in (draw pass, stock :19736-19742).
    //
    // THE ROUTE. There is no donor code for this: the fork never reaches the
    // case, so DN-10 step 1 lands on STOCK's own system rather than on
    // instance-authored logic. Stock's instant-equip idiom - request, commit,
    // cancel the equip animation - is changeItemTriggerKeepProc
    // (:14570-14581), which does exactly:
    //     itemEquip(sel_item); commonChangeItem(); resetUpperAnime(UPPER_2, -1);
    // for the swim-legal items. swordEquip(TRUE) is the sword's request half
    // (:11999-12035) and SELF-GATES on checkSwordGet, so a swordless Link is
    // left untouched and nothing here can invent a sword. commonChangeItem
    // puts away whatever was held, plays Z2SE_AL_SWORD_PULLOUT and calls
    // setSwordModel() (:12492-12499), so the draw reads and sounds as a draw
    // instead of the sword teleporting into his hand.
    //
    // POSITION. This is the adapter body, which stands exactly where the donor
    // calls commonProcInit(PROC_FLURRY_RUSH) - the first statement of
    // procFlurryRushInit's committed path (fork .inc:250) - and therefore ahead
    // of the same function's tail, where flurryBeginSwing(0) can fire on the
    // entry frame (fork .inc:274-277). The sword is in hand before the first
    // swing is armed, never after it.
    // ============================================
    if (mEquipItem != 0x103) {
        swordEquip(TRUE);
        if (field_0x2fde == 0x103) {  // the request took: checkSwordGet passed
            commonChangeItem();
            resetUpperAnime(UPPER_2, -1.0f);
        }
    }

    s_overlayActive = true;
#if ALBW_FLURRY_PROBE
    probe_reset();
    DuskLog.info("[flurry] overlay ENTER host=PROC_CUT_NORMAL modeFlg=0x101 target={} "
                 "equip {} -> {} (0x103 = sword in hand)",
                 (void*)dFlurryRush_getTargetActor(), equipBefore, (int)mEquipItem);
#endif
}

#include "flurry_port.inc"  // AlbwFlurry_c::* - verbatim fork bodies

// ============================================
// Public API
// ============================================
bool albw_flurry_proc_active() {
    return s_overlayActive;
}

void albw_flurry_proc_reset() {
    s_overlayActive = false;
#if ALBW_FLURRY_PROBE
    probe_reset();
#endif
}

int albw_flurry_proc_try_enter(daAlink_c* link) {
    if (link == nullptr || s_overlayActive || !s_procInstalled) {
        return 0;
    }
    // procFlurryRushInit self-gates on isEnabled / isActive / mode (fork
    // .inc:244-248) and returns 0 without touching Link's proc when unarmed -
    // the donor's own contract, which both call sites depend on.
    return static_cast<AlbwFlurry_c*>(link)->procFlurryRushInit();
}

// ============================================
// LIFETIME - layer 3 of 3. See MANIFEST "lifetime".
//
// Layers 1 and 2 are the donor's own two mechanisms (the commonProcInit
// chokepoint below and the dFlurryRush_update tail in flurry_rush.cpp). This
// third one exists only for the case neither can see: the player actor itself
// is gone or was rebuilt (stage change, death respawn, a transform that
// recreates the actor), so no commonProcInit ever fired on OUR Link and the
// module state would otherwise stand for the rest of the session. That is
// precisely the Hurricane Spin bug (hurricane_spin.cpp:403-422) and it is not
// repeated here.
// ============================================
void albw_flurry_proc_frame_watch() {
    if (!s_overlayActive) {
        return;  // inert whenever the overlay never started (toggle off == stock)
    }
    auto* link = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
    if (link != nullptr && link->mProcID == daAlink_c::PROC_CUT_NORMAL) {
        return;  // the overlay still owns Link's proc
    }
#if ALBW_FLURRY_PROBE
    DuskLog.info("[flurry] overlay EXIT via frame watch (link={} proc={})", (void*)link,
                 link != nullptr ? (int)link->mProcID : -1);
#endif
    s_overlayActive = false;
    if (dFlurryRush_isActive()) {
        dFlurryRush_end(dFlurryRushEnd_Interrupt);
    }
}

namespace {

DEFINE_HOOK(&daAlink_c::procCutNormal, FlurryCutNormal);
DEFINE_HOOK(&daAlink_c::procCutNormalInit, FlurryCutNormalInit);
DEFINE_HOOK(&daAlink_c::commonProcInit, FlurryCommonProcInit);

// ============================================
// Per-frame: the donor's procFlurryRush (fork .inc:280-405) runs INSTEAD of
// the stock cut-normal proc while the overlay owns Link.
//
// Position: this is not a "pre-hook standing in for a mid-function call". The
// donor's procFlurryRush is a WHOLE proc body dispatched by the proc table in
// place of procCutNormal; SKIP_ORIGINAL at PRE is the same thing - the stock
// body does not run at all, ours does, and the return value is the proc
// return the engine reads. The only reachable difference from a real table
// row is mpProcFunc's value, which nothing else consults.
// ============================================
HookAction on_proc_cut_normal_pre(ModContext*, void* args, void* retval, void*) {
    if (!s_overlayActive) {
        return HOOK_CONTINUE;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
#if ALBW_FLURRY_PROBE
    probe_sample(link, true);
#endif
    const int r = static_cast<AlbwFlurry_c*>(link)->procFlurryRush();
#if ALBW_FLURRY_PROBE
    if (s_overlayActive) {
        probe_sample(link, false);
    }
#endif
    if (retval != nullptr) {
        *static_cast<int*>(retval) = r;
    }
    return HOOK_SKIP_ORIGINAL;
}

// ============================================
// Re-entry guard. RECEIVER TRANSLATION (DN-10 step 2), and the proof that
// step 1 cannot close it: in the donor Link is in PROC_FLURRY_RUSH, and the
// engine simply never dispatches procCutNormalInit for a proc Link is not in
// - there is no donor code to port, because the donor's protection IS the
// distinct proc id. The overlay borrows a proc that ordinary play uses, so
// the same protection has to be stated: while the overlay owns Link, a stock
// cut-normal must not start underneath it (it would re-arm the animation, the
// AT profile and the HIO frame window mid-rush).
//
// Reached only if some path calls procCutNormalInit during a rush; the
// per-frame hook above already prevents the usual one, because the stock
// procCutNormal body - which is what runs checkNextAction - never executes.
// ============================================
HookAction on_proc_cut_normal_init_pre(ModContext*, void*, void* retval, void*) {
    if (!s_overlayActive) {
        return HOOK_CONTINUE;
    }
#if ALBW_FLURRY_PROBE
    DuskLog.warn("[flurry] refused a stock procCutNormalInit while the overlay owns Link");
#endif
    if (retval != nullptr) {
        *static_cast<int*>(retval) = 1;
    }
    return HOOK_SKIP_ORIGINAL;
}

// ============================================
// LIFETIME - layer 1, the donor's PRIMARY mechanism, PORTED.
//
//   fork src/d/actor/d_a_alink.cpp:16636-16639, inside commonProcInit:
//       if (mProcID == PROC_FLURRY_RUSH && i_procID != PROC_FLURRY_RUSH) {
//           dFlurryRush_end(dFlurryRushEnd_Interrupt);
//       }
//
// POSITION. The donor's insert is the FIRST STATEMENT of commonProcInit's
// body; the only thing ahead of it is `int i;`, a declaration with no
// initialiser and no side effect. A PRE hook therefore lands on exactly the
// donor's position - this is not a pre-hook substituting for a call the donor
// makes mid-function, which is the mistake that cost this project three
// regressions. And it is a real chokepoint: every proc change in daAlink_c
// goes through commonProcInit (it is where mProcID is assigned, stock
// d_a_alink.cpp:15160), which is precisely why the fork put the cleanup here
// and why wolf_howl_combat.cpp:233 already hooks it the same way for the
// fork's howl cleanup at the same site.
//
// OVERLAY TRANSLATION, and one deliberate TIGHTENING. The donor's guard is
// "we are in the rush proc and we are leaving it". Ours is "the overlay owns
// Link", which is the same predicate, and then it ends on ANY proc change,
// including one back to PROC_CUT_NORMAL. The donor cannot express that case
// (nothing re-enters PROC_FLURRY_RUSH mid-rush) and for the overlay it can
// only mean a foreign cut-normal has taken Link - the guard above refuses
// those, so this branch should be unreachable; if it is ever reached, ending
// is the outcome that cannot strand the overlay. The overlay's OWN entry is
// not caught by this: flurryOverlayProcInit sets s_overlayActive AFTER
// commonProcInit returns, so the flag is still false when this runs.
// ============================================
HookAction on_common_proc_init_pre(ModContext*, void* args, void*, void*) {
    if (!s_overlayActive) {
        return HOOK_CONTINUE;
    }
    const auto procID = mods::arg<daAlink_c::daAlink_PROC>(args, 1);
#if ALBW_FLURRY_PROBE
    DuskLog.info("[flurry] overlay EXIT via commonProcInit chokepoint (newProc={})", (int)procID);
#else
    (void)procID;
#endif
    s_overlayActive = false;
    // flurryExitToWait already called dFlurryRush_end with the donor's own
    // reason before reaching procWaitInit, and dFlurryRush_end early-returns
    // when the rush is already over - so a clean exit keeps its reason and
    // only an interruption is labelled Interrupt, as in the donor.
    if (dFlurryRush_isActive()) {
        dFlurryRush_end(dFlurryRushEnd_Interrupt);
    }
    return HOOK_CONTINUE;
}

// ============================================
// A hook that fails to resolve must NOT abort mod_initialize. Same doctrine as
// flurry_hooks.cpp / sim_time_scale_hooks.cpp: the miss is LOUD and SCOPED.
//
// It matters more than usual here, and asymmetrically: losing the per-frame
// hook only means no flurry. Losing the commonProcInit chokepoint while the
// per-frame hook is live would mean an overlay nothing can end. So a partial
// install is downgraded to NO install, the same way slow-motion refuses to run
// with only half its pair.
// ============================================
bool report(const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, name);
            svc_log->error(mod_ctx,
                           "hook above did NOT install - the Flurry Rush proc is inactive "
                           "this run");
        }
        return false;
    }
    return true;
}

}  // namespace

ModResult albw_flurry_proc_init(ModError*) {
    const bool frameOk =
        report("FlurryCutNormalPre",
               mods::hook::add_pre<FlurryCutNormal>(svc_hook, on_proc_cut_normal_pre));
    const bool initOk =
        report("FlurryCutNormalInitPre",
               mods::hook::add_pre<FlurryCutNormalInit>(svc_hook, on_proc_cut_normal_init_pre));
    const bool chokeOk =
        report("FlurryCommonProcInitPre",
               mods::hook::add_pre<FlurryCommonProcInit>(svc_hook, on_common_proc_init_pre));

    s_procInstalled = frameOk && initOk && chokeOk;
    if (!s_procInstalled) {
        s_overlayActive = false;
        if (frameOk) {
            // Take the detour back off rather than leave a callback that can
            // start an overlay nothing is able to end.
            mods::hook::uninstall<FlurryCutNormal>(svc_hook);
        }
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, "Flurry Rush proc DISABLED: the overlay body and its "
                                    "commonProcInit lifetime chokepoint must install together "
                                    "or neither takes effect");
        }
    }
    return MOD_OK;
}

#endif  // TARGET_PC
