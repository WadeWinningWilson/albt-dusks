// ============================================
// NEW CODE - ALBW Port (build-then-swap clothes pipeline)
// See clothes_pipeline.h for provenance. loadModelDVD below is ported from the
// fork d_a_alink_swindow.inc:114-313, branch for branch.
// ============================================

#include "global.h"
#include <os.h>

#include "JSystem/JKernel/JKRExpHeap.h"
#include "SSystem/SComponent/c_phase.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_resorce.h"
#include "m_Do/m_Do_ext.h"
#define private public
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_midna.h"
#undef private

#include "clothes_pipeline.h"
#include "albw_common.h"
#include "albw_game.h"
#include "alink_compat.h"
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

DEFINE_HOOK(&daAlink_c::loadModelDVD, LoadModelDVD);
DEFINE_HOOK(&daAlink_c::setClothesChange, SetClothesChangeCloth);
DEFINE_HOOK(&daAlink_c::create, AlinkCreate);
DEFINE_HOOK(&daAlink_c::changeLink, ChangeLinkStamp);
DEFINE_HOOK(&daAlink_c::changeWolf, ChangeWolfStamp);
DEFINE_HOOK(&daAlink_c::draw, AlinkDrawGuard);

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
HookAction on_alink_draw_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    const u32 cur = albwLiveModelStateToken(link, link->checkWolf() != 0);
    if (!dAlbwAlink_clothesEpochInSync() || s_builtModelState != cur) {
        *static_cast<int*>(retval) = 1;
        return HOOK_SKIP_ORIGINAL;
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
        return HOOK_SKIP_ORIGINAL;  // drop the re-entrant request entirely
    }
    s_swapActive = false;
    return HOOK_CONTINUE;  // let vanilla set the timer / FLG2 bit
}

// fork d_a_alink.cpp:5541 - allocate the alt heap alongside Link own arc heap.
void on_alink_create_post(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || s_arcHeapB != nullptr) {
        return;
    }
    link->setOriginalHeap(&s_arcHeapB, 0x100000);
}

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

// fork d_a_alink_swindow.inc:114-313, ported branch for branch.
HookAction on_load_model_dvd_pre(ModContext*, void* args, void* retval, void*) {
    auto* i_this = mods::arg<daAlink_c*>(args, 0);
    if (i_this == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }
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
        !install(error, "AlinkDrawConsistencyGuard",
                 mods::hook_add_pre<AlinkDrawGuard>(svc_hook, on_alink_draw_pre)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
