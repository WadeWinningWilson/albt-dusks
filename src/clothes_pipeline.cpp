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

// stock's l_mArcName (d_a_alink.cpp:85) is file-static; the resource manager keys
// on the string, so the literal is the same lookup.
const char* const ALBW_MMDL_ARC = "Mmdl";

DEFINE_HOOK(&daAlink_c::loadModelDVD, LoadModelDVD);
DEFINE_HOOK(&daAlink_c::setClothesChange, SetClothesChangeCloth);
DEFINE_HOOK(&daAlink_c::create, AlinkCreate);
DEFINE_HOOK(&daAlink_c::changeLink, ChangeLinkStamp);
DEFINE_HOOK(&daAlink_c::changeWolf, ChangeWolfStamp);
DEFINE_HOOK(&daAlink_c::draw, AlinkDrawGuard);
DEFINE_HOOK(&daAlink_c::setMagicArmorBrk, SetMagicArmorBrk);
DEFINE_HOOK(&daAlink_c::setWaterDropColor, SetWaterDropColor);

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

bool s_albwMagicModelReady = false;

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
        DuskLog.error("setMagicArmorBrk: missing body BRK {} in {}", bodyBrkName[i_status],
                      ALBW_MMDL_ARC);
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
                 mods::hook_add_pre<AlinkDrawGuard>(svc_hook, on_alink_draw_pre)) ||
        !install(error, "ChangeLinkMagicReady",
                 mods::hook_add_pre<ChangeLinkStamp>(svc_hook, on_change_link_magic_pre)) ||
        !install(error, "SetMagicArmorBrkGuarded",
                 mods::hook_add_pre<SetMagicArmorBrk>(svc_hook, on_set_magic_armor_brk_pre)) ||
        !install(error, "SetWaterDropColorCapSafe",
                 mods::hook_add_pre<SetWaterDropColor>(svc_hook, on_set_water_drop_color_pre)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
