// ============================================
// NEW CODE - ALBW Port (build-then-swap clothes pipeline)
// See clothes_pipeline.h for provenance. loadModelDVD below is ported from the
// fork d_a_alink_swindow.inc:114-313, branch for branch.
// ============================================

#include "global.h"
#include <os.h>

#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/J3DGraphAnimator/J3DAnimation.h"
#include "SSystem/SComponent/c_phase.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_resorce.h"
#include "m_Do/m_Do_ext.h"
#define private public
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_midna.h"
#undef private

#include "Z2AudioLib/Z2SeMgr.h"  // Z2SE_AL_M_ARMER_* (P2a ALBW-arm SEs)

#include "clothes_pipeline.h"
#include "albw_common.h"
#include "albw_game.h"
#include "albw_dusk_compat.h"   // dusk::getSettings().game.armorRupeeDrain (P2 gates)
#include "albw_fork_compat.h"   // dMeter2_isALBWArmorDepleted (fork ALBW-arm dep)
#include "albw_symbols.h"       // ALBT_SYM_ALINK_DTOR (P0 - dtor is not member-pointer nameable)
#include "alink_compat.h"
#include "changelink.h"         // dispatch-gate latch (P1)
#include "outfit.h"
#include "sumo_test.h"
#include "albw_dusk_log.h"
#include "mods/hook.hpp"

#if TARGET_PC

namespace {

// fork d_a_alink.cpp:249-252, moved out of the actor into the mod.
JKRExpHeap*                    s_arcHeapB     = nullptr;  // alt heap; ping-pongs with mpArcHeap
request_of_phase_process_class s_phaseReqB    = {nullptr, 0};
const char*                    s_swapOldArc   = nullptr;
bool                           s_swapActive   = false;
bool                           s_forceRemount = false;

// ============================================
// NEW CODE - outfit-transition crash family P1 (transition scope flag)
// True only while control is inside on_load_model_dvd_pre. The completion
// branches there call changeLink/changeWolf AFTER zeroing the wait timer (and
// the same-arc path also clears s_swapActive first), so timer/swap alone
// cannot tell changelink.cpp those calls belong to the settling transition.
// ============================================
bool s_inLoadModelDvd = false;

struct AlbwLoadModelDvdScope {
    AlbwLoadModelDvdScope() { s_inLoadModelDvd = true; }
    ~AlbwLoadModelDvdScope() { s_inLoadModelDvd = false; }
};

// stock's l_mArcName (d_a_alink.cpp:85) is file-static; the resource manager keys
// on the string, so the literal is the same lookup.
const char* const ALBW_MMDL_ARC = "Mmdl";

DEFINE_HOOK(&daAlink_c::loadModelDVD, LoadModelDVD);
DEFINE_HOOK(&daAlink_c::setClothesChange, SetClothesChangeCloth);
DEFINE_HOOK(&daAlink_c::create, AlinkCreate);
DEFINE_HOOK(&daAlink_c::changeLink, ChangeLinkStamp);
DEFINE_HOOK(&daAlink_c::changeWolf, ChangeWolfStamp);
// fork d_a_alink.cpp:19744 - execute() drives the outfit reconciler every frame. Stock
// execute has no such call, so the mod must supply it: without this per-frame tick the
// sumo worn-bit gets set (shop buy / quick-swap) but syncLinkModel never runs and no
// clothes rebuild is ever kicked -> "buying sumo does nothing". This is the outfit
// counterpart to the shield reload driver's per-frame execute hook.
DEFINE_HOOK(&daAlink_c::execute, OutfitExecDriver);
DEFINE_HOOK(&daAlink_c::draw, AlinkDrawGuard);
DEFINE_HOOK(&daAlink_c::setMagicArmorBrk, SetMagicArmorBrk);
DEFINE_HOOK(&daAlink_c::setWaterDropColor, SetWaterDropColor);
// fork d_a_alink.cpp:14054-14074 - ALBW-mode heaviness override (P2b). Stock's
// checkMagicArmorHeavy is declared in d_a_alink.h:1683 (public const), exported
// (?checkMagicArmorHeavy@daAlink_c@@QEBAHXZ, stock exports:8053) and non-empty
// (a settings switch, stock d_a_alink.cpp:12748), so member-pointer hooking works.
DEFINE_HOOK(&daAlink_c::checkMagicArmorHeavy, CheckMagicArmorHeavy);
// ============================================
// NEW CODE - outfit-transition crash family P0 (destructor teardown)
// fork d_a_alink.cpp:22266-22277 - the fork's ~daAlink_c() frees the
// build-then-swap alt heap and resets the swap statics so the NEXT Link
// instance starts clean. The dusk lacked this teardown: after one completed
// swap + one Link recreation the file-statics above dangle, and a later swap
// COMPLEATEs against stale heap state -> initModel(NULL)
// (changelink_port.inc:71, symbolicated) and the ucrtbase-memcpy / heap
// fast-fail siblings. A destructor cannot be named by member pointer
// (&daAlink_c::~daAlink_c is ill-formed), so this is the DEFINE_HOOK_SYMBOL
// mangled-name route; the per-platform pair lives in albw_symbols.h.
// ============================================
DEFINE_HOOK_SYMBOL(ALBT_SYM_ALINK_DTOR, void(daAlink_c*), AlinkDtor);

// fork d_a_alink.cpp:199 - encodes the draw-relevant identity of Link's clothes
// models. changeLink/changeWolf stamp the token the models were BUILT for; draw
// compares it to the live token and skips the whole Link draw for the frame if
// they disagree (or the arc epoch moved). Because every crashing consumer runs
// inside draw(), this one choke point replaces per-consumer gating.
u32 s_builtModelState = 0xFFFFFFFF;

inline u32 albwModelStateToken(bool wolf, bool sumoBody, bool casual, bool zora, bool magic) {
    return (wolf ? 1u : 0u) | (sumoBody ? 2u : 0u) | (casual ? 4u : 0u) | (zora ? 8u : 0u) |
           (magic ? 16u : 0u);
}

inline u32 albwLiveModelStateToken(daAlink_c* link, bool wolf) {
    return albwModelStateToken(wolf, link->checkNoResetFlg2(daPy_py_c::FLG2_UNK_80000) != 0,
                               link->checkCasualWearFlg() != 0, link->checkZoraWearFlg() != 0,
                               link->checkMagicArmorWearFlg() != 0);
}

// fork d_a_alink_wolf.inc:656 - changeLink always builds HUMAN Link, so wolf=false
// even during metamorphose (FLG1_IS_WOLF may still be set until changeCommon).
void on_change_link_post(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) return;
    dAlbwAlink_resyncClothesEpoch();
    s_builtModelState = albwLiveModelStateToken(link, false);
}

// fork d_a_alink_wolf.inc:328 - wolf models also live in mpArcHeap.
// fork d_a_alink.cpp:19744 - the per-frame outfit reconciler, missing in stock execute.
// Runs BEFORE stock execute's body (so a swap it kicks is picked up by execute's own
// loadModelDVD the same frame). dAlbwSumoTest_exec self-gates (null + worn-bit/settings),
// so it is inert unless an outfit is in play.
HookAction on_outfit_exec_driver_pre(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link != nullptr) {
        dAlbwSumoTest_exec(link);

        // ============================================
        // NEW CODE - outfit-transition crash family P2a (ALBW rupee-drain arm)
        // fork d_a_alink.cpp:20755-20770 (the `armorMode == ALBW` arm of the
        // fork's execute driver), ported verbatim. STOCK's execute ALREADY runs
        // the NORMAL-mode drain + Brk-flip driver natively (stock
        // d_a_alink.cpp:18740-18762, gated on the HOST's armorRupeeDrain ==
        // NORMAL), and this pre-hook HOOK_CONTINUEs into it, so nothing here
        // duplicates NORMAL. The fork delta is the ALBW arm only.
        //
        // INERT-UNTIL-EXPOSED: the gate below reads the MOD's compat
        // armorRupeeDrain (albw_dusk_compat.h), which is pinned to NORMAL, so
        // this block is provably unreachable until the compat shim exposes an
        // ALBW mode. Timing note: the fork runs this arm mid-execute; as a
        // pre-hook it runs at frame start instead - a sub-frame phase shift on
        // a Brk on/off flip, with no ordering hazard (setMagicArmorBrk routes
        // through the always-on SetMagicArmorBrk hook either way).
        // ============================================
        if (dusk::getSettings().game.armorRupeeDrain.getValue() == dusk::MagicArmorMode::ALBW &&
            link->checkMagicArmorWearAbility() && link->mClothesChangeWaitTimer == 0)
        {
            if ((dMeter2_isALBWArmorDepleted() || dComIfGs_getRupee() < 500) &&
                link->field_0x2fd7 != 0)
            {
                link->setMagicArmorBrk(0);
                link->seStartOnlyReverb(Z2SE_AL_M_ARMER_TURNOFF);
                link->mZ2Link.setLinkState(5);
            } else if (!dMeter2_isALBWArmorDepleted() && dComIfGs_getRupee() >= 500 &&
                       link->field_0x2fd7 == 0)
            {
                link->setMagicArmorBrk(1);
                link->seStartOnlyReverb(Z2SE_AL_M_ARMER_RECOVER);
                link->mZ2Link.setLinkState(4);
            }
        }
    }
    return HOOK_CONTINUE;
}

void on_change_wolf_post(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) return;
    dAlbwAlink_resyncClothesEpoch();
    s_builtModelState = albwLiveModelStateToken(link, true);
}

// fork d_a_alink.cpp:21533 - the crash seatbelt. Skip the WHOLE Link draw for a
// frame whose models disagree with the state they were built for, or whose arc
// heap was freed under them. The build-then-swap path keeps the tokens in
// agreement during normal play, so this should not fire (no flicker); it exists
// for residual mismatch.
//
// NOT ported: the fork also gates on s_albwBuildIntegrityOk, set by a per-build
// material scan (albwFirstCorruptMat) it describes as a DETECTOR for collecting
// samples of a build-corruption bug. That is diagnostics, not the seatbelt, so
// the epoch/token half is ported and the scan is left in the fork.
// ============================================
// The guard below skipping is what "invisible Link" looks like. The fork's own
// note on this seatbelt (d_a_alink.cpp:21524) says the tokens are kept in
// agreement during normal play so "this does not fire" - so a skip that PERSISTS
// is never routine, it is a state that stopped reconciling. Silent it would be
// indistinguishable from a rendering bug, so report every distinct state once.
// ============================================
u32 s_lastSkipBuilt = 0xFFFFFFFF;
u32 s_lastSkipCur = 0xFFFFFFFF;
bool s_wasSkipping = false;

HookAction on_alink_draw_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    const u32 cur = albwLiveModelStateToken(link, link->checkWolf() != 0);
    const bool epochOk = dAlbwAlink_clothesEpochInSync();
    if (!epochOk || s_builtModelState != cur) {
        if (!s_wasSkipping || s_lastSkipBuilt != s_builtModelState || s_lastSkipCur != cur) {
            s_wasSkipping = true;
            s_lastSkipBuilt = s_builtModelState;
            s_lastSkipCur = cur;
            DuskLog.error("[Alink] draw SKIPPED (Link invisible): built={} live={} epochInSync={} "
                          "arc={} cloth={} timer={}",
                          s_builtModelState, cur, epochOk, 
                          link->mArcName != nullptr ? link->mArcName : "(null)",
                          (int)dComIfGs_getSelectEquipClothes(),
                          (int)link->mClothesChangeWaitTimer);
        }
        *static_cast<int*>(retval) = 1;
        return HOOK_SKIP_ORIGINAL;
    }
    if (s_wasSkipping) {
        s_wasSkipping = false;
        DuskLog.info("[Alink] draw resumed (state={})", cur);
    }
    return HOOK_CONTINUE;
}

// fork d_a_alink_swindow.inc:83 - ignore re-entrant requests while a reload is in
// flight (resetting the timer mid-sequence corrupts loadModelDVD and crashes
// under mash/warp), and never inherit a stuck swap flag.
HookAction on_set_clothes_change_pre(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link->checkWolf()) {
        return HOOK_CONTINUE;
    }
    if (link->mClothesChangeWaitTimer != 0) {
        // P1: deliberately NO latch re-sample on this path - a dropped
        // re-entrant request must not change the gate value the in-flight
        // transition was accepted under.
        return HOOK_SKIP_ORIGINAL;  // drop the re-entrant request entirely
    }
    s_swapActive = false;
    // ============================================
    // NEW CODE - outfit-transition crash family P1 (gate latch sample point)
    // This is the single accept point for a player-clothes change: sample the
    // changeLink dispatch gate ONCE here, so the settling rebuild is dispatched
    // by the state the change was accepted under even if a gate input flips
    // mid-transition (sumo worn bit cleared during decompose, outfit.cpp:274).
    // With every toggle off the gate is false now, so the latch stays false and
    // the whole transition is provably stock.
    // ============================================
    albw_changelink_latch_dispatch_gate();
    return HOOK_CONTINUE;  // let vanilla set the timer / FLG2 bit
}

// fork d_a_alink.cpp:5541 - allocate the alt heap alongside Link own arc heap.
// With the P0 destructor teardown below, the `s_arcHeapB != nullptr` early-out
// naturally becomes PER-LINK-LIFE, matching the fork's create-side guard
// (fork d_a_alink.cpp:5786-5791: `if (s_albwArcHeapB == NULL)`) - the dtor
// nulls the pointer, so each recreated Link gets a fresh alt heap instead of
// inheriting a dead one.
void on_alink_create_post(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || s_arcHeapB != nullptr) {
        return;
    }
    link->setOriginalHeap(&s_arcHeapB, 0x100000);
}

// ============================================
// NEW CODE - outfit-transition crash family P0 (destructor teardown, crash fix)
// fork d_a_alink.cpp:22266-22277 semantics, run as a POST-hook so it is
// additive AFTER stock's own ~daAlink_c work (stock has already deleted
// mPhaseReq/mArcName and destroyed mpArcHeap by the time this runs).
//
// Without this, the statics at the top of this file survive Link's death:
// after one completed swap + one Link recreation they describe a heap/arc
// world that no longer exists, and the next swap COMPLEATEs against it ->
// initModel(NULL) in the settling changeLink (changelink_port.inc:71,
// symbolicated) and the ucrtbase-memcpy / heap fast-fail siblings.
//
// SELF-GATING: when s_arcHeapB == nullptr the pipeline never armed for this
// Link life (hook not installed, or the create-side alloc failed) and every
// statement below is a no-op on already-reset state. HONESTY NOTE on the
// all-toggles-off case: albw_clothes_pipeline_init runs unconditionally at mod
// load (mod.cpp:222) and on_alink_create_post arms the alt heap regardless of
// config - exactly as the fork does on TARGET_PC (fork d_a_alink.cpp:5786) -
// so with the mod loaded and every toggle off this teardown still destroys and
// the next create re-arms the alt heap once per Link life. That is the fork's
// own unconditional dtor block, not feature behaviour; no player-visible state
// is touched by it.
//
// If the swap is still active (near-impossible: daAlink_Delete pumps
// loadModelDVD until the timer settles before destructing, stock
// d_a_alink.cpp daAlink_Delete), run the abort semantics minus the rebuild
// (mirror of albw_clothes_abort_stuck below - Link is dying, changeLink(1)
// must not run) and minus the deleteObjectResMain fallback: stock's dtor has
// ALREADY executed dComIfG_resDelete(&mPhaseReq, mArcName) on this same arc
// name, so an unconditional second name-delete would double-decrement the
// res-control refcount. dComIfG_resDelete itself no-ops unless s_phaseReqB
// tracked a completed load (d_com_inf_game.cpp:1368: id != 2 -> return 0).
// ============================================
void on_alink_dtor_post(ModContext*, void*, void*, void*) {
    // REVIEW EDIT: this is a POST hook on a DESTRUCTOR - the actor object is
    // already destroyed, so NOTHING may be read through the args pointer (its
    // mArcName is dead memory). Only mod-owned statics are touched below; the
    // swap-active edge just logs loudly (cPhs_Reset below retires the request
    // state; a leaked refcount on a dying Link is acceptable and visible).
    if (s_arcHeapB == nullptr) {
        return;  // pipeline never armed - provable no-op
    }
    if (s_swapActive) {
        DuskLog.warn("[Alink] ~daAlink_c with swap still active (oldArc={}) - "
                     "dropping the in-flight alt-heap load",
                     s_swapOldArc != nullptr ? s_swapOldArc : "(null)");
    }
    // fork d_a_alink.cpp:22269-22276, verbatim semantics on the mod-side statics.
    mDoExt_destroyExpHeap(s_arcHeapB);
    s_arcHeapB = nullptr;
    s_swapActive = false;
    s_swapOldArc = nullptr;
    s_forceRemount = false;
    cPhs_Reset(&s_phaseReqB);
    // Mod-side additions beyond the fork block: the fork's draw guards are
    // actor-internal and die with the actor; ours are file-statics here, so
    // reset the draw-guard token and resync the arc epoch (alink_compat.cpp:65-69)
    // so the NEXT Link's first changeLink stamp starts from a clean slate, and
    // clear the P1 dispatch latch (the transition it described died with Link).
    s_builtModelState = 0xFFFFFFFF;
    dAlbwAlink_resyncClothesEpoch();
    albw_changelink_clear_dispatch_latch();
}

}  // leave the anonymous namespace so albw_midna_reset_demo_bck has EXTERNAL linkage
   // (the ported changeLink in changelink.cpp calls it via clothes_pipeline.h).

// fork daMidna_c::resetDemoBck + removeDemoBodyBck (d_a_midna.cpp). Both are
// fork ADDITIONS to a stock actor, so a mod cannot add them as members - but
// every field they touch is present in stock (mBckHeap / mBtpHeap / mBtkHeap /
// mpDemoFCBlendBrk / m_anmDataTable / mpMorf ... all in d_a_midna.h; resetArcNo
// and resetIdx in d_a_player.h), so the bodies port verbatim as a free function.
void albw_midna_reset_demo_bck(daMidna_c* midna) {
    if (midna == nullptr) {
        return;
    }
    // removeDemoBodyBck()
    if (midna->mpDemoHDTmpBck != nullptr && midna->mpDemoHDTmpBmd != nullptr) {
        midna->mpDemoHDTmpBck->remove(midna->mpDemoHDTmpBmd->getModelData());
    }
    midna->endHighModel();

    midna->mBckHeap[0].resetArcNo();
    midna->mBckHeap[0].resetIdx();
    midna->mBckHeap[1].resetArcNo();
    midna->mBckHeap[1].resetIdx();
    midna->mBckHeap[2].resetIdx();
    midna->mBtpHeap.resetArcNo();
    midna->mBtpHeap.resetIdx();
    midna->mBtkHeap.resetArcNo();
    midna->mBtkHeap.resetIdx();

    midna->offStateFlg1(
        (daMidna_c::daMidna_FLG1)(daMidna_c::FLG1_UNK_10 | daMidna_c::FLG1_UNK_40));

    if (midna->mLeftHandShapeIdx == 0xfd) {
        midna->mLeftHandShapeIdx = 0xfe;
    }
    if (midna->mRightHandShapeIdx == 0xfd) {
        midna->mRightHandShapeIdx = 0xfe;
    }

    if (midna->mpDemoFCBlendBrk != nullptr) {
        if (midna->mpDemoFCBlendBmd != nullptr) {
            midna->mpDemoFCBlendBmd->getModelData()->removeTevRegAnimator(midna->mpDemoFCBlendBrk);
        }
        midna->mpDemoFCBlendBrk = nullptr;
    }
    midna->field_0x668 = nullptr;

    J3DAnmTransform* bck = (J3DAnmTransform*)midna->mBckHeap[0].loadDataIdx(
        midna->m_anmDataTable[daMidna_c::ANM_WAITA].mResID);
    if (bck != nullptr) {
        midna->setBckAnime(bck, -1, 0.0f);
        midna->mUpperBck.init(midna->mpMorf->getAnm(), TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0,
                              -1, false);
        midna->mFaceBck.init(midna->mpMorf->getAnm(), TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0,
                             -1, false);
    }
}

namespace {  // reopen the anonymous namespace (all anon namespaces in a TU merge,
             // so s_arcHeapB / s_swapActive / the hook aliases stay in scope)

// fork d_a_alink_swindow.inc:114-313, ported branch for branch.
HookAction on_load_model_dvd_pre(ModContext*, void* args, void* retval, void*) {
    auto* i_this = mods::arg<daAlink_c*>(args, 0);
    if (i_this == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    // P1: the changeLink/changeWolf calls made from the completion branches
    // below run after the timer is zeroed (and same-arc also clears
    // s_swapActive first) - mark them as part of the settling transition so
    // changelink.cpp's dispatch uses the latched gate, not a live re-read.
    AlbwLoadModelDvdScope inLoadModelDvdScope;
    auto ret = [retval](int v) {
        *static_cast<int*>(retval) = v;
        return HOOK_SKIP_ORIGINAL;
    };

    if (i_this->mClothesChangeWaitTimer == 0) {
        return ret(1);  // fork: the trailing else branch
    }

    i_this->mClothesChangeWaitTimer--;

    if (i_this->mClothesChangeWaitTimer == 2) {
        i_this->mEyeHL1.remove();
        i_this->mEyeHL2.remove();
        i_this->mpWlMidnaModel = nullptr;
        i_this->mpWlMidnaMaskModel = nullptr;
        i_this->mpWlMidnaHandModel = nullptr;
        i_this->mpWlMidnaHairModel = nullptr;
        {
            daMidna_c* midna = daPy_py_c::getMidnaActor();
            if (midna != nullptr) {
                albw_midna_reset_demo_bck(midna);
            }
        }
        if (!i_this->checkNoResetFlg2(daPy_py_c::FLG2_UNK_280000)) {
            const bool isMeta = (i_this->mProcID == daAlink_c::PROC_METAMORPHOSE ||
                                 i_this->mProcID == daAlink_c::PROC_METAMORPHOSE_ONLY);
            // P1: metamorphose sets the wait timer DIRECTLY (stock
            // d_a_alink.cpp:17626 / 17724), bypassing setClothesChange, so the
            // latch was never sampled for this transition - sample it at the
            // first pipeline tick that recognizes the metamorphose, before its
            // in-flight changeLink/changeWolf calls consult a stale latch.
            if (isMeta) {
                albw_changelink_latch_dispatch_gate();
            }
            // BUILD-THEN-SWAP prep: keep the OLD models fully valid - do NOT free
            // here. Remember the old arc and point mArcName at the new one.
            if (!isMeta && s_arcHeapB != nullptr) {
                if (!s_swapActive) {
                    s_swapOldArc = i_this->mArcName;
                    i_this->setArcName(i_this->checkWolf());
                    dAlbwSumoTest_sanitizeClothesArc(i_this->mArcName);
                    cPhs_Reset(&s_phaseReqB);
                    s_swapActive = true;
                }
            } else {
                // In-place free-then-load: wolf metamorphose, or no alt heap.
                if (!dComIfG_resDelete(&i_this->mPhaseReq, i_this->mArcName)) {
                    dComIfG_deleteObjectResMain(i_this->mArcName);
                }
                cPhs_Reset(&i_this->mPhaseReq);
                i_this->mpArcHeap->freeAll();
                dAlbwAlink_invalidateClothesEpoch();  // prior-epoch models are stale
                i_this->setArcName(isMeta ? !i_this->checkWolf() : i_this->checkWolf());
            }
        }
        return ret(0);
    }

    if (i_this->mClothesChangeWaitTimer == 1) {
        if (i_this->checkNoResetFlg2(daPy_py_c::FLG2_UNK_280000)) {
            i_this->mClothesChangeWaitTimer = 0;
            i_this->changeLink(1);
            return ret(0);
        }
        const bool isMeta = (i_this->mProcID == daAlink_c::PROC_METAMORPHOSE ||
                             i_this->mProcID == daAlink_c::PROC_METAMORPHOSE_ONLY);
        if (s_swapActive) {
            // Same arc re-equipped: still resident in mpArcHeap, rebuild in place.
            // (Custom-Model-API section 9: same-arc deliberately does NOT remount.)
            if (i_this->mArcName == s_swapOldArc) {
                i_this->mClothesChangeWaitTimer = 0;
                s_swapActive = false;
                s_forceRemount = false;
                i_this->changeLink(1);
                return ret(1);
            }
            const int loadSt = dComIfG_resLoad(&s_phaseReqB, i_this->mArcName, s_arcHeapB);
            if (loadSt == cPhs_COMPLEATE_e) {
                i_this->mClothesChangeWaitTimer = 0;
                i_this->changeLink(1);  // builds from the alt heap; repoints refs
                // Live ptrs now reference the alt heap, so the old heap can drop.
                if (!dComIfG_resDelete(&i_this->mPhaseReq, s_swapOldArc)) {
                    dComIfG_deleteObjectResMain(s_swapOldArc);
                }
                cPhs_Reset(&i_this->mPhaseReq);
                i_this->mpArcHeap->freeAll();
                {
                    JKRExpHeap* l_th = i_this->mpArcHeap;
                    i_this->mpArcHeap = s_arcHeapB;
                    s_arcHeapB = l_th;
                }
                {
                    request_of_phase_process_class l_tr = i_this->mPhaseReq;
                    i_this->mPhaseReq = s_phaseReqB;
                    s_phaseReqB = l_tr;
                }
                s_swapActive = false;
                s_forceRemount = false;
                dAlbwAlink_resyncClothesEpoch();
                return ret(1);
            }
            if (loadSt == cPhs_ERROR_e) {
                // Keep LIVE models, purge abandoned rows, timer=2 retry. Do NOT
                // reconcile the outfit to Ordon (that pinned the D-pad ring).
                const char* failedArc = i_this->mArcName;
                dRes_info_c* info =
                    failedArc != nullptr ? dComIfG_getObjectResInfo(failedArc) : nullptr;
                const bool inFlight = info != nullptr && info->getArchive() == nullptr &&
                                      info->getDMCommand() != nullptr;
                const bool zombie = info != nullptr && info->getArchive() == nullptr &&
                                    info->getDMCommand() == nullptr;
                DuskLog.warn("[Alink] build-then-swap resLoad ERROR for {} (old={}) - "
                             "inFlight={} zombie={}",
                             failedArc ? failedArc : "(null)",
                             s_swapOldArc ? s_swapOldArc : "(null)",
                             inFlight ? 1 : 0, zombie ? 1 : 0);
                if (!inFlight) {
                    cPhs_Reset(&s_phaseReqB);
                    if (s_arcHeapB != nullptr) {
                        s_arcHeapB->freeAll();
                    }
                    if (zombie) {
                        dAlbwSumoTest_sanitizeClothesArc(failedArc);
                    }
                }
                i_this->mClothesChangeWaitTimer = 2;
                return ret(0);
            }
            i_this->mClothesChangeWaitTimer = 2;  // alt arc not loaded yet; retry
            return ret(0);
        }
        // In-place load: wolf metamorphose, or fallback (no alt heap).
        const int loadSt =
            dComIfG_resLoad(&i_this->mPhaseReq, i_this->mArcName, i_this->mpArcHeap);
        if (loadSt == cPhs_COMPLEATE_e) {
            i_this->mClothesChangeWaitTimer = 0;
            if (isMeta) {
                if (i_this->checkWolf()) {
                    i_this->changeLink(0);
                    dAlbwOutfit_onMetamorphoseToHuman(i_this);
                } else {
                    i_this->changeWolf();
                }
            } else {
                i_this->changeLink(1);
            }
            return ret(1);
        }
        if (loadSt == cPhs_ERROR_e) {
            // In-place already freed the live heap at timer==2 - stop spinning
            // and unhide; outfit sync may retry later.
            DuskLog.warn("[Alink] in-place clothes resLoad ERROR for {} - clearing timer",
                         i_this->mArcName ? i_this->mArcName : "(null)");
            i_this->mClothesChangeWaitTimer = 0;
            dAlbwAlink_resyncClothesEpoch();
            return ret(0);
        }
        i_this->mClothesChangeWaitTimer = 2;
    }

    return ret(0);
}

// ============================================
// NEW CODE - ALBW Port (Magic-Armor model readiness)
//
// Ports fork commit 3a345b5b5d ("fix Magic-Armor-buy crash"), which hardened the
// Magic path against the clothes pipeline making Mmdl non-resident. Its own
// write-up of the failure:
//
//   changeLink's Magic branch can fall back to the Kmdl (Hero's) body when Mmdl
//   isn't resolvable, but the wear flag stays Magic, so draw() ran Magic-only
//   material/Brk ops on a Hero's-layout model and read off the end.
//
// The playtest crash on 2026-09-01 is the Brk half of exactly that:
//   J3DAnmTevRegKey::searchUpdateMaterialID  <- setMagicArmorBrk  <- execute
//   EXCEPTION_ACCESS_VIOLATION, fault addr 0x10   (a NULL `this` + 0x10)
// dComIfG_getObjectRes("Mmdl", "ml_body_power_down.brk") returned NULL and stock
// dereferences it unconditionally.
//
// Ported here: setMagicArmorBrk and setWaterDropColor, both verbatim, plus the
// s_albwMagicModelReady predicate that ties them together.
//
// NOT ported - changeLink's Kmdl fallback. changeLink cannot be replaced from a
// mod: its param_0==0 branch needs five file-statics in stock's d_a_alink.cpp
// with no external linkage (l_jntColData, l_crawlSideOffset, l_crawlTopUpOffset,
// l_autoUpHeight, l_autoDownHeight) plus daAlink_kandelaarModelCallBack, and
// l_autoUpHeight/l_autoDownHeight are MUTABLE state that other d_a_alink.cpp
// code reads back - a mod-side copy would write a duplicate the game never sees.
// So the flag is computed in a changeLink PRE-hook using the fork's own
// predicate, at the same moment and off the same three lookups. When it comes
// out false the miss is logged as an error rather than passed over quietly: the
// fallback that would have rescued it is the part that cannot be ported.
//
// Translation: l_mArcName is likewise file-static, so the literal "Mmdl" is used
// - the resource manager keys on the string, so the lookup is identical.
// ============================================

}  // leave the anonymous namespace: s_albwMagicModelReady needs EXTERNAL linkage.

// ============================================
// NEW CODE - outfit-transition crash family P3 (single magic-ready flag)
// Previously changelink.cpp carried a second, file-static s_albwMagicModelReady
// that the ported changeLink body wrote (changelink_port.inc:93) while the draw
// consumer albw_setWaterDropColor below read THIS one - body and draw could
// diverge. This is now the ONE definition (donor-native name kept, fork
// d_a_alink.cpp file-static), declared extern in clothes_pipeline.h; both sync
// sites (on_change_link_magic_pre below, and the ported body's Magic branch)
// write the same flag.
// ============================================
bool s_albwMagicModelReady = false;

namespace {  // reopen (anon namespaces in a TU merge; statics stay in scope)

int s_lastBrkMissStatus = -1;

// fork d_a_alink.cpp:13756 - stock's is void and unguarded; the fork's returns
// BOOL so changeLink only touches the Brks when they actually resolved.
BOOL albw_setMagicArmorBrk(daAlink_c* i_this, int i_status) {
    static const char* bodyBrkName[3] = {
        "ml_body_power_down.brk",
        "ml_body_power_up_a.brk",
        "ml_body_power_up_b.brk",
    };

    static const char* headBrkName[3] = {
        "ml_head_power_down.brk",
        "ml_head_power_up_a.brk",
        "ml_head_power_up_b.brk",
    };

    i_this->mMagicArmorBodyBrk = NULL;
    i_this->mMagicArmorHeadBrk = NULL;

    if (i_status < 0 || i_status > 2) {
        return FALSE;
    }
    if (i_this->mpLinkModel == NULL || i_this->mpLinkHatModel == NULL) {
        DuskLog.error("setMagicArmorBrk: missing link model (body={} hat={})",
                      (const void*)i_this->mpLinkModel,
                      (const void*)i_this->mpLinkHatModel);
        return FALSE;
    }

    J3DModelData* modelData = i_this->mpLinkModel->getModelData();
    i_this->mMagicArmorBodyBrk =
        (J3DAnmTevRegKey*)dComIfG_getObjectRes(ALBW_MMDL_ARC, bodyBrkName[i_status]);
    if (i_this->mMagicArmorBodyBrk == NULL || modelData == NULL) {
        // Once per (status, arc-residency) state - execute() retries this every
        // frame while the mismatch holds, and 60 identical lines a second buries
        // whatever else the log was about to tell us.
        if (s_lastBrkMissStatus != i_status) {
            s_lastBrkMissStatus = i_status;
            DuskLog.error("setMagicArmorBrk: missing body BRK {} in {} (arc={} cloth={})",
                          bodyBrkName[i_status], ALBW_MMDL_ARC,
                          i_this->mArcName != nullptr ? i_this->mArcName : "(null)",
                          (int)dComIfGs_getSelectEquipClothes());
        }
        i_this->mMagicArmorBodyBrk = NULL;
        return FALSE;
    }
    i_this->mMagicArmorBodyBrk->searchUpdateMaterialID(modelData);
    modelData->entryTevRegAnimator(i_this->mMagicArmorBodyBrk);
    i_this->mMagicArmorBodyBrk->setFrame(0.0f);

    modelData = i_this->mpLinkHatModel->getModelData();
    i_this->mMagicArmorHeadBrk =
        (J3DAnmTevRegKey*)dComIfG_getObjectRes(ALBW_MMDL_ARC, headBrkName[i_status]);
    if (i_this->mMagicArmorHeadBrk == NULL || modelData == NULL) {
        DuskLog.error("setMagicArmorBrk: missing head BRK {} in {}", headBrkName[i_status],
                      ALBW_MMDL_ARC);
        i_this->mMagicArmorBodyBrk = NULL;
        i_this->mMagicArmorHeadBrk = NULL;
        return FALSE;
    }
    i_this->mMagicArmorHeadBrk->searchUpdateMaterialID(modelData);
    modelData->entryTevRegAnimator(i_this->mMagicArmorHeadBrk);
    i_this->mMagicArmorHeadBrk->setFrame(0.0f);

    i_this->field_0x2fd7 = i_status;
    s_lastBrkMissStatus = -1;  // resolved - let a later miss report itself
    return TRUE;
}

// fork d_a_alink.cpp:21372 - carries BOTH of the fork's changes to this function:
// the ALBW_HAT_TEV bound (Cap Wear can put a foreign cap with fewer materials on
// Link) and the s_albwMagicModelReady gate on the Magic branch.
void albw_setWaterDropColor(daAlink_c* i_this, const J3DGXColorS10* i_color) {
    static const GXColorS10 notColor0 = {0x00, 0x00, 0x00, 0xFF};
    J3DGXColorS10* var_r31;

    // ============================================
    // NEW CODE - ALBW Port (Cap Wear recolor safety)
    // The recolor below indexes the HAT material by the wear-flag branch (Magic touches hat
    // mats 1+2, Zora mat 1, Hero's/casual mat 0).  With Cap Wear a foreign cap can be on Link
    // (e.g. the green al_head, 2 mats, over a Magic body whose branch writes hat mat 2) -> an
    // off-the-end read.  Guard every mpLinkHatModel material write by the cap's ACTUAL count so
    // a mismatched cap recolors only what it has.  ALBW_HAT_TEV is also used by the existing
    // wear-flag branches (harmless: a native cap always has the expected material).
    // ============================================
    J3DModelData* const albwHatMd =
        (i_this->mpLinkHatModel != NULL) ? i_this->mpLinkHatModel->getModelData() : NULL;
    const u16 albwHatMatNum = (albwHatMd != NULL) ? albwHatMd->getMaterialNum() : 0;
#define ALBW_HAT_TEV(idx)                                                        \
    do {                                                                         \
        if (albwHatMd != NULL && albwHatMatNum > (u16)(idx))                     \
            albwHatMd->getMaterialNodePointer(idx)->setTevColor(1, i_color);      \
    } while (0)

    if (&i_this->field_0x32a0[0] == i_color) {
        if (i_this->checkNoResetFlg2(daPy_py_c::FLG2_UNK_80000) || i_this->checkZoraWearAbility() ||
            i_this->checkMagicArmorWearAbility())
        {
            var_r31 = (J3DGXColorS10*)&notColor0;
            i_color = (J3DGXColorS10*)&notColor0;
        } else {
            var_r31 = (J3DGXColorS10*)&i_color[1];
        }
    } else {
        var_r31 = (J3DGXColorS10*)i_color;
    }

    if (!i_this->checkNoResetFlg2(daPy_py_c::FLG2_UNK_80000)) {
        if (i_this->checkZoraWearAbility()) {
            if (i_this->field_0x064C->getMaterialNum() >= 14)
            {
            i_this->field_0x064C->getMaterialNodePointer(13)->setTevColor(1, i_color);
            i_this->field_0x064C->getMaterialNodePointer(0)->setTevColor(1, i_color);
            i_this->field_0x064C->getMaterialNodePointer(1)->setTevColor(1, i_color);
            ALBW_HAT_TEV(1);
            }
        // Only run the Magic Armor material ops on the REAL Mmdl model - the Kmdl
        // fallback body lacks this material layout and getMaterialNodePointer(2) on its
        // hat reads off the end (the shop-buy-Magic draw crash).  Fall through to the
        // Hero's branch below, which matches the fallback body.
        } else if (i_this->checkMagicArmorWearAbility() && s_albwMagicModelReady) {
            if (i_this->field_0x064C->getMaterialNum() >= 12)
            {
            i_this->field_0x064C->getMaterialNodePointer(11)->setTevColor(1, i_color);
            i_this->field_0x064C->getMaterialNodePointer(10)->setTevColor(1, i_color);
            i_this->field_0x064C->getMaterialNodePointer(9)->setTevColor(1, i_color);
            i_this->field_0x064C->getMaterialNodePointer(8)->setTevColor(1, i_color);
            i_this->field_0x064C->getMaterialNodePointer(6)->setTevColor(1, i_color);
            ALBW_HAT_TEV(2);
            ALBW_HAT_TEV(1);
            }
        } else if (i_this->checkCasualWearFlg()) {
            if (i_this->field_0x064C->getMaterialNum() >= 8)
            {
            i_this->field_0x064C->getMaterialNodePointer(7)->setTevColor(1, i_color);
            ALBW_HAT_TEV(0);
            i_this->field_0x064C->getMaterialNodePointer(5)->setTevColor(1, var_r31);
            }
        } else {
            if (i_this->field_0x064C->getMaterialNum() >= 18)
            {
            i_this->field_0x064C->getMaterialNodePointer(17)->setTevColor(1, i_color);
            i_this->field_0x064C->getMaterialNodePointer(9)->setTevColor(1, i_color);
            i_this->field_0x064C->getMaterialNodePointer(0)->setTevColor(1, i_color);
            i_this->field_0x064C->getMaterialNodePointer(1)->setTevColor(1, i_color);
            i_this->field_0x064C->getMaterialNodePointer(2)->setTevColor(1, i_color);
            ALBW_HAT_TEV(0);
            i_this->field_0x064C->getMaterialNodePointer(16)->setTevColor(1, var_r31);
            i_this->field_0x064C->getMaterialNodePointer(15)->setTevColor(1, var_r31);
            i_this->field_0x064C->getMaterialNodePointer(14)->setTevColor(1, var_r31);
            }
        }
    }
#undef ALBW_HAT_TEV
}

HookAction on_set_magic_armor_brk_pre(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) return HOOK_CONTINUE;
    (void)albw_setMagicArmorBrk(link, mods::arg<int>(args, 1));
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_set_water_drop_color_pre(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) return HOOK_CONTINUE;
    albw_setWaterDropColor(link, mods::arg<const J3DGXColorS10*>(args, 1));
    return HOOK_SKIP_ORIGINAL;
}

// ============================================
// NEW CODE - outfit-transition crash family P2b (ALBW heaviness override)
// fork d_a_alink.cpp:14054-14074 - the fork's checkMagicArmorHeavy adds ONE arm
// to stock's settings switch: `case ALBW: return dMeter2_isALBWArmorDepleted()`.
// STOCK's own switch (stock d_a_alink.cpp:12748-12766) already handles NORMAL /
// ON_DAMAGE / DOUBLE_DEFENSE / INVINCIBLE / COSMETIC natively, so anything
// except ALBW falls through to the original (HOOK_CONTINUE).
//
// INERT-UNTIL-EXPOSED: gated on the MOD's compat armorRupeeDrain
// (albw_dusk_compat.h, pinned NORMAL) - identical gate to the P2a arm above;
// unreachable until the compat shim exposes an ALBW mode.
// ============================================
HookAction on_check_magic_armor_heavy_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<const daAlink_c*>(args, 0);
    if (link == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    if (dusk::getSettings().game.armorRupeeDrain.getValue() != dusk::MagicArmorMode::ALBW) {
        return HOOK_CONTINUE;  // stock's native switch owns every non-ALBW mode
    }
    // fork 14055-14057 + 14060-14061, verbatim semantics.
    *static_cast<BOOL*>(retval) =
        (link->checkMagicArmorWearAbility() && dMeter2_isALBWArmorDepleted()) ? TRUE : FALSE;
    return HOOK_SKIP_ORIGINAL;
}

// fork d_a_alink_wolf.inc:334-347 - the head of changeLink, which the mod cannot
// replace (see the note at the top of this block). Two of its lines are portable
// at the boundary and both are load-bearing:
//
//  1. The sumo trigger. dAlbwSumoTest_prepareChangeLink()'s whole contract is
//     "true when resources are resident and FLG2_UNK_200000 may be set", and
//     this is the fork's only place that sets it. Without it stock's sumo branch
//     is unreachable - every other reference in this mod clears the flag.
//  2. s_albwMagicModelReady, off the fork's own three Mmdl lookups. Assigned
//     only when the Magic branch is the one stock will take, matching the fork,
//     where the assignment lives inside that branch.
HookAction on_change_link_magic_pre(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) return HOOK_CONTINUE;

    if (dAlbwOutfit_isSumoWorn() && dAlbwSumoTest_prepareChangeLink()) {
        link->onNoResetFlg2(daPy_py_c::FLG2_UNK_200000);
    }

    if (!link->checkNoResetFlg2(daPy_py_c::FLG2_UNK_200000) && !link->checkCasualWearFlg() &&
        !link->checkZoraWearFlg() && link->checkMagicArmorWearFlg())
    {
        const bool ready = dComIfG_getObjectRes(ALBW_MMDL_ARC, "ml.bmd") != NULL &&
                           dComIfG_getObjectRes(ALBW_MMDL_ARC, "ml_head.bmd") != NULL &&
                           dComIfG_getObjectRes(ALBW_MMDL_ARC, "al_hands.bmd") != NULL;
        if (!ready && s_albwMagicModelReady) {
            // LOUD: the fork answers this by rebuilding from Kmdl, which needs
            // changeLink itself. Here stock's branch is about to build from a
            // NULL J3DModelData, so say so instead of letting it look handled.
            DuskLog.error("ALBW changeLink: Mmdl parts unresolvable - stock's Magic branch has "
                          "no fallback (the fork's needs changeLink, which cannot be replaced); "
                          "Magic draw ops now gated off");
        }
        s_albwMagicModelReady = ready;
    }
    return HOOK_CONTINUE;
}

// ============================================
// NEW CODE ENDS HERE
// ============================================

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace

// fork daAlink_c::albwAbortStuckClothesChange (d_a_alink.cpp:169). Now that the
// alt-heap pipeline actually exists in the mod, this recovery is real rather
// than the log-only placeholder alink_compat carried before.
void albw_clothes_abort_stuck(daAlink_c* link) {
    if (link == nullptr) {
        return;
    }
    if (link->mClothesChangeWaitTimer == 0 && !s_swapActive) {
        dAlbwAlink_resyncClothesEpoch();
        return;
    }
    DuskLog.warn("[Alink] abort stuck clothes change (timer={} swapActive={} arc={} oldArc={})",
                 (int)link->mClothesChangeWaitTimer, s_swapActive ? 1 : 0,
                 link->mArcName ? link->mArcName : "(null)",
                 s_swapOldArc ? s_swapOldArc : "(null)");
    const char* liveArc = s_swapOldArc != nullptr ? s_swapOldArc : link->mArcName;
    if (s_swapActive) {
        // Drop the hung alt-heap load; live models still reference mpArcHeap.
        if (link->mArcName != nullptr) {
            if (!dComIfG_resDelete(&s_phaseReqB, link->mArcName)) {
                dComIfG_deleteObjectResMain(link->mArcName);
            }
        }
        cPhs_Reset(&s_phaseReqB);
        if (s_arcHeapB != nullptr) {
            s_arcHeapB->freeAll();
        }
        // Point mArcName back at the arc still resident in the live heap.
        if (s_swapOldArc != nullptr) {
            link->mArcName = s_swapOldArc;
        }
        s_swapActive = false;
        s_swapOldArc = nullptr;
    }
    s_forceRemount = false;
    link->mClothesChangeWaitTimer = 0;
    // Equip/flags may still say Hero while models are Ordon; reconcile to the
    // live arc and rebuild.
    dAlbwOutfit_onClothesLoadFailed(liveArc != nullptr ? liveArc : link->mArcName, "abort");
    dAlbwAlink_resyncClothesEpoch();
    link->changeLink(1);
}

void albw_clothes_request_remount() { s_forceRemount = true; }

// ============================================
// NEW CODE - outfit-transition crash family P1 (transition query)
// See clothes_pipeline.h. Reads the anon-namespace swap state; changelink.cpp
// combines this with the wait timer to pick latched vs live gate evaluation.
// ============================================
bool albw_clothes_transition_in_flight() { return s_swapActive || s_inLoadModelDvd; }

ModResult albw_clothes_pipeline_init(ModError* error) {
    if (!install(error, "AlinkCreateClothesHeap",
                 mods::hook_add_post<AlinkCreate>(svc_hook, on_alink_create_post)) ||
        !install(error, "SetClothesChangeReentrancy",
                 mods::hook_add_pre<SetClothesChangeCloth>(svc_hook, on_set_clothes_change_pre)) ||
        !install(error, "LoadModelDVDBuildThenSwap",
                 mods::hook_add_pre<LoadModelDVD>(svc_hook, on_load_model_dvd_pre)) ||
        !install(error, "ChangeLinkStampToken",
                 mods::hook_add_post<ChangeLinkStamp>(svc_hook, on_change_link_post)) ||
        !install(error, "ChangeWolfStampToken",
                 mods::hook_add_post<ChangeWolfStamp>(svc_hook, on_change_wolf_post)) ||
        !install(error, "OutfitExecDriver",
                 mods::hook_add_pre<OutfitExecDriver>(svc_hook, on_outfit_exec_driver_pre)) ||
        !install(error, "AlinkDrawConsistencyGuard",
                 mods::hook_add_pre<AlinkDrawGuard>(svc_hook, on_alink_draw_pre)) ||
        !install(error, "ChangeLinkMagicReady",
                 mods::hook_add_pre<ChangeLinkStamp>(svc_hook, on_change_link_magic_pre)) ||
        !install(error, "SetMagicArmorBrkGuarded",
                 mods::hook_add_pre<SetMagicArmorBrk>(svc_hook, on_set_magic_armor_brk_pre)) ||
        !install(error, "SetWaterDropColorCapSafe",
                 mods::hook_add_pre<SetWaterDropColor>(svc_hook, on_set_water_drop_color_pre)) ||
        // P2b - fork d_a_alink.cpp:14054-14074, inert until compat exposes ALBW.
        !install(error, "CheckMagicArmorHeavyALBW",
                 mods::hook_add_pre<CheckMagicArmorHeavy>(svc_hook, on_check_magic_armor_heavy_pre)))
    {
        return MOD_ERROR;
    }
    // ============================================
    // P0 - fork d_a_alink.cpp:22266-22277. REVIEW EDIT: installed tolerant-but-
    // LOUD. The Windows name is verified against exports.def:1232 and the
    // Itanium D1 name against the shipped Linux v2.0.0 ELF (llvm-nm: T
    // _ZN9daAlink_cD1Ev, aliased D2 at the same address) - but the macOS / iOS /
    // android manifests were not inspected, and one failed hook otherwise
    // unloads the entire mod. A miss here degrades ONE guard (teardown), logged
    // as an error; it must never brick the mod.
    // ============================================
    if (mods::hook_add_post<AlinkDtor>(svc_hook, on_alink_dtor_post) != MOD_OK) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx,
                           "AlinkDtorClothesTeardown failed to bind (~daAlink_c symbol miss?) - "
                           "outfit swaps after Link recreation may crash on this platform; report this");
        }
    }
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
