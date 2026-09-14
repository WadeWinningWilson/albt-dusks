// Enemy death rupees — part of dev.albt.albw.

#include "albw_common.h"
#include "config_vars.h"
#include "modules.h"
#include "region_table.h"

#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "d/actor/d_a_obj_bemos.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_pane_class.h"
#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter2_info.h"
#include "d/d_meter_HIO.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "mods/svc/hook.hpp"
#include "SSystem/SComponent/c_math.h"

namespace {

constexpr u16 kNormalPoeRupees = 20;
constexpr u16 kImpPoeRupees = 100;
constexpr int kGrantedKillIdCap = 64;
constexpr s16 kPopupFrames = 120;
constexpr s16 kPopupFadeFrames = 15;
constexpr int kPopupMaxDigits = 4;

s16 sVictoryGranted[32];
int sVictoryGrantedCount = 0;
u32 sGrantedKillActorIds[kGrantedKillIdCap];
int sGrantedKillIdCount = 0;
int sGrantedKillEvictIdx = 0;

u16 sPopupAmount = 0;
s16 sPopupFramesLeft = 0;
J2DPicture* sPlusPic = NULL;
J2DPicture* sDigitPic[10] = {};

DEFINE_HOOK(&cc_at_check, CcAtCheck);
DEFINE_HOOK_SYMBOL("fopAc_Execute", int(void*), AcExecute);
DEFINE_HOOK(&daObjBm_c::mode_dead_init, BeamosDead);
DEFINE_HOOK(&dMeter2Draw_c::draw, MeterDraw);

bool grantsEnabled() {
    return albw_cfg_bool(g_kill_rupees, true);
}

bool wasVictoryGranted(s16 profName) {
    for (int i = 0; i < sVictoryGrantedCount; ++i) {
        if (sVictoryGranted[i] == profName) {
            return true;
        }
    }
    return false;
}

void markVictoryGranted(s16 profName) {
    if (sVictoryGrantedCount >= (int)(sizeof(sVictoryGranted) / sizeof(sVictoryGranted[0]))) {
        return;
    }
    sVictoryGranted[sVictoryGrantedCount++] = profName;
}

bool wasKillGranted(fopAc_ac_c* enemy) {
    if (enemy == NULL) {
        return false;
    }
    const u32 actorId = fopAcM_GetID(enemy);
    for (int i = 0; i < sGrantedKillIdCount; ++i) {
        if (sGrantedKillActorIds[i] == actorId) {
            return true;
        }
    }
    return false;
}

void markKillGranted(fopAc_ac_c* enemy) {
    if (enemy == NULL) {
        return;
    }
    const u32 actorId = fopAcM_GetID(enemy);
    for (int i = 0; i < sGrantedKillIdCount; ++i) {
        if (sGrantedKillActorIds[i] == actorId) {
            return;
        }
    }
    if (sGrantedKillIdCount >= kGrantedKillIdCap) {
        sGrantedKillActorIds[sGrantedKillEvictIdx] = actorId;
        sGrantedKillEvictIdx = (sGrantedKillEvictIdx + 1) % kGrantedKillIdCap;
        return;
    }
    sGrantedKillActorIds[sGrantedKillIdCount++] = actorId;
}

void armPopup(u16 amount) {
    if (amount == 0) {
        return;
    }
    sPopupAmount = amount;
    sPopupFramesLeft = kPopupFrames;
}

void grantRupees(u16 amount) {
    amount = albw_region_scale_rupees(amount);
    if (amount == 0) {
        return;
    }
    g_dComIfG_gameInfo.play.setItemRupeeCount(static_cast<s32>(amount));
    armPopup(amount);
}

bool isBulblinKingSpawn(fopAc_ac_c* enemy) {
    const u8 arg0 = static_cast<u8>(fopAcM_GetParam(enemy) & 0xFF);
    return arg0 == 4 || arg0 == 5 || arg0 == 11 || arg0 == 12;
}

u16 resolveBulblinKillRupees(fopAc_ac_c* enemy) {
    if (isBulblinKingSpawn(enemy)) {
        return 0;
    }
    return 15;
}

u16 resolvePoeKillRupees(fopAc_ac_c* enemy) {
    u8 arg0 = static_cast<u8>(fopAcM_GetParam(enemy) & 0xFF);
    if (arg0 == 0xFF) {
        arg0 = 1;
    }
    if (arg0 == 0) {
        return kImpPoeRupees;
    }
    return kNormalPoeRupees;
}

bool isKillExcludedProfile(s16 profName) {
    switch (profName) {
    case fpcNm_B_BQ_e:
    case fpcNm_B_BH_e:
    case fpcNm_B_DS_e:
    case fpcNm_B_DR_e:
    case fpcNm_B_DRE_e:
    case fpcNm_B_GM_e:
    case fpcNm_B_GND_e:
    case fpcNm_B_MGN_e:
    case fpcNm_B_OB_e:
    case fpcNm_B_OH_e:
    case fpcNm_B_OH2_e:
    case fpcNm_B_YO_e:
    case fpcNm_B_YOI_e:
    case fpcNm_B_ZANT_e:
    case fpcNm_B_ZANTS_e:
    case fpcNm_B_ZANTZ_e:
    case fpcNm_B_ZANTM_e:
    case fpcNm_B_TN_e:
    case fpcNm_B_GG_e:
    case fpcNm_E_FM_e:
    case fpcNm_E_GOB_e:
    case fpcNm_E_TH_e:
    case fpcNm_E_VT_e:
    case fpcNm_E_DT_e:
    case fpcNm_E_MK_e:
    case fpcNm_E_RDB_e:
    case fpcNm_E_PZ_e:
    case fpcNm_E_YMB_e:
    case fpcNm_E_HZELDA_e:
    case fpcNm_E_MB_e:
    case fpcNm_E_GS_e:
    case fpcNm_E_IS_e:
    case fpcNm_E_MM_MT_e:
    case fpcNm_E_DB_LEAF_e:
    case fpcNm_E_HB_LEAF_e:
    case fpcNm_E_YD_LEAF_e:
    case fpcNm_E_BI_LEAF_e:
    case fpcNm_E_PH_e:
    case fpcNm_E_SB_e:
    case fpcNm_E_WW_e:
    case fpcNm_E_MD_e:
    case fpcNm_E_CR_EGG_e:
    case fpcNm_E_TK_BALL_e:
    case fpcNm_E_DF_e:
    case fpcNm_E_BEE_e:
    case fpcNm_E_GA_e:
        return true;
    default:
        return false;
    }
}

u16 lookupKillRupees(s16 profName, fopAc_ac_c* enemy) {
    if (isKillExcludedProfile(profName)) {
        return 0;
    }

    switch (profName) {
    case fpcNm_E_AI_e:
        return 15;
    case fpcNm_E_GM_e:
        return 1;
    case fpcNm_E_DK_e:
        return 5;
    case fpcNm_E_GB_e:
        return 15;
    case fpcNm_E_HB_e:
        return 5;
    case fpcNm_E_HM_e:
        return 3;
    case fpcNm_E_KR_e:
        return 15;
    case fpcNm_E_ST_e:
        return 5;
    case fpcNm_E_OC_e:
        return 5;
    case fpcNm_E_BG_e:
        return 3;
    case fpcNm_E_CR_e:
        return 3;
    case fpcNm_E_BU_e:
        return 1;
    case fpcNm_E_RD_e:
        return resolveBulblinKillRupees(enemy);
    case fpcNm_E_KK_e:
        return 25;
    case fpcNm_E_SM2_e:
        return 3;
    case fpcNm_E_SM_e:
        return 3;
    case fpcNm_E_DB_e:
        return 1;
    case fpcNm_E_MF_e:
        return 50;
    case fpcNm_E_FB_e:
    case fpcNm_E_FZ_e:
        return 10;
    case fpcNm_E_NZ_e:
        return 1;
    case fpcNm_E_GE_e:
        return 3;
    case fpcNm_E_MM_e:
        return 3;
    case fpcNm_E_PO_e:
        return resolvePoeKillRupees(enemy);
    case fpcNm_E_HP_e:
        return kImpPoeRupees;
    case fpcNm_E_BA_e:
        return 1;
    case fpcNm_E_RB_e:
        return 1;
    case fpcNm_E_DN_e:
        return 25;
    case fpcNm_E_SW_e:
        return 1;
    case fpcNm_E_FK_e:
        return 20;
    case fpcNm_E_BUG_e:
        return 1;
    case fpcNm_E_FS_e:
        return 1;
    case fpcNm_E_MS_e:
        return 1;
    case fpcNm_E_GI_e:
        return 10;
    case fpcNm_E_S1_e:
        return 15;
    case fpcNm_E_RDY_e:
        return 15;
    case fpcNm_E_YD_e:
        return 1;
    case fpcNm_E_YH_e:
        return 1;
    case fpcNm_E_YM_e:
        return 1;
    case fpcNm_E_YC_e:
        return 15;
    case fpcNm_E_YR_e:
        return 15;
    case fpcNm_E_YK_e:
        return 1;
    case fpcNm_E_YG_e:
        return 1;
    case fpcNm_E_PM_e:
        return 64;
    case fpcNm_E_SG_e:
        return 1;
    case fpcNm_E_WS_e:
        return 1;
    case fpcNm_E_BS_e:
        return 1;
    case fpcNm_E_SF_e:
        return 25;
    case fpcNm_E_SH_e:
        return 10;
    case fpcNm_E_ZS_e:
        return 5;
    case fpcNm_E_TT_e:
        return 3;
    case fpcNm_E_OT_e:
        return 1;
    case fpcNm_E_TK_e:
    case fpcNm_E_TK2_e:
        return 3;
    case fpcNm_E_ZM_e:
        return 3;
    case fpcNm_E_KG_e:
        return 1;
    case fpcNm_E_BI_e:
        return 1;
    case fpcNm_E_HZ_e:
        return 4;
    default:
        return 0;
    }
}

u16 lookupFightVictoryRupees(s16 profName) {
    switch (profName) {
    case fpcNm_B_BQ_e:
    case fpcNm_E_FM_e:
    case fpcNm_B_OB_e:
        return 200;
    case fpcNm_B_DS_e:
    case fpcNm_B_YO_e:
    case fpcNm_B_GM_e:
    case fpcNm_B_DR_e:
        return 300;
    case fpcNm_B_ZANT_e:
        return 500;
    case fpcNm_B_MGN_e:
        return 500;
    case fpcNm_E_HZELDA_e:
        return 200;
    case fpcNm_B_TN_e:
        return 200;
    case fpcNm_E_GOB_e:
    case fpcNm_E_TH_e:
    case fpcNm_E_VT_e:
    case fpcNm_E_DT_e:
    case fpcNm_E_RDB_e:
    case fpcNm_E_MK_e:
    case fpcNm_E_PZ_e:
    case fpcNm_B_GG_e:
    case fpcNm_E_YMB_e:
        return 100;
    default:
        return 0;
    }
}

void onEnemyKill(fopAc_ac_c* enemy) {
    if (!grantsEnabled() || enemy == NULL) {
        return;
    }
    if (fopAcM_GetGroup(enemy) != fopAc_ENEMY_e) {
        return;
    }
    if (wasKillGranted(enemy)) {
        return;
    }

    const s16 profName = fopAcM_GetName(enemy);
    const u16 amount = lookupKillRupees(profName, enemy);
    if (amount == 0) {
        return;
    }

    markKillGranted(enemy);
    grantRupees(amount);
}

void tryKillAfterDamage(fopAc_ac_c* enemy, s32 attackPower) {
    if (!grantsEnabled() || enemy == NULL || attackPower == 0) {
        return;
    }
    if (fopAcM_GetGroup(enemy) != fopAc_ENEMY_e) {
        return;
    }
    if (enemy->health > 0) {
        return;
    }
    onEnemyKill(enemy);
}

// ============================================
// NEW CODE - ALBW Port (magic-jar bonus drop, fork d_cc_uty.cpp death block)
//
// The fork spawns a large-magic jar on common-enemy death, right inside the
// `mAttackPower != 0 && health <= 0` block of cc_at_check's damage apply, at a
// 10% rate (matches the ALBW orange-rupee bonus-drop rate). fopAcM_createItem
// with itemNo dItemNo_L_MAGIC_e -> daItem_c, whose get-path (d_a_obj_item.cpp)
// is already ported to fill the ALBW meter via item_func_L_MAGIC. The drop
// itself was never ported, so the meter refill had no in-world source -> "the
// jar never spawns". Reproduced here at the same cc_at_check seam.
//
// Intentionally NOT gated on the kill-rupees toggle: in the fork this drop is
// unconditional on enemy kill, and the meter is always live in this mod.
// ============================================
void tryDropMagicJar(fopAc_ac_c* enemy, s32 attackPower) {
    if (enemy == NULL || attackPower == 0) {
        return;
    }
    if (fopAcM_GetGroup(enemy) != fopAc_ENEMY_e) {
        return;
    }
    if (enemy->health > 0) {
        return;
    }
    if (cM_rndF(1.0f) < 0.10f) {
        static const cXyz s_jarDropScale(1.3f, 1.3f, 1.3f);
        fopAcM_createItem(&enemy->current.pos, dItemNo_L_MAGIC_e, -1,
                          fopAcM_GetRoomNo(enemy), NULL, &s_jarDropScale, 0);
    }
}
// ============================================
// NEW CODE ENDS HERE
// ============================================

void tryGrantFightVictory(s16 profName) {
    if (!grantsEnabled()) {
        return;
    }
    if (wasVictoryGranted(profName)) {
        return;
    }
    const u16 amount = lookupFightVictoryRupees(profName);
    if (amount == 0) {
        return;
    }
    markVictoryGranted(profName);
    grantRupees(amount);
}

bool ensurePics() {
    if (sPlusPic != NULL) {
        return true;
    }

    JKRArchive* arc = g_dComIfG_gameInfo.play.getMain2DArchive();
    if (arc == NULL) {
        return false;
    }

    for (int i = 0; i < 10; i++) {
        if (sDigitPic[i] != NULL) {
            continue;
        }
        ResTIMG* timg = static_cast<ResTIMG*>(arc->getResource('TIMG', dMeter2Info_getNumberTextureName(i)));
        if (timg == NULL) {
            return false;
        }
        sDigitPic[i] = JKR_NEW J2DPicture(timg);
        if (sDigitPic[i] == NULL) {
            return false;
        }
    }

    ResTIMG* plusTimg =
        static_cast<ResTIMG*>(arc->getResource('TIMG', dMeter2Info_getPlusTextureName()));
    if (plusTimg == NULL) {
        return false;
    }
    sPlusPic = JKR_NEW J2DPicture(plusTimg);
    return sPlusPic != NULL;
}

void drawPopup() {
    if (sPopupFramesLeft <= 0 || !grantsEnabled()) {
        if (!grantsEnabled()) {
            sPopupFramesLeft = 0;
        }
        return;
    }

    if (g_dComIfG_gameInfo.play.isPauseFlag() || g_dComIfG_gameInfo.play.isHeapLockFlag() != 0) {
        return;
    }

    if (!ensurePics()) {
        return;
    }

    J2DGrafContext* grafCtx = g_dComIfG_gameInfo.play.getCurrentGrafPort();
    if (grafCtx == NULL) {
        return;
    }

    sPopupFramesLeft--;

    f32 fade = 1.0f;
    if (sPopupFramesLeft < kPopupFadeFrames) {
        fade = (f32)sPopupFramesLeft / (f32)kPopupFadeFrames;
    }
    const u8 alpha = (u8)(255.0f * fade);
    if (alpha == 0) {
        return;
    }

    grafCtx->setup2D();

    const f32 digitW = g_drawHIO.mRupeeCountScale * 32.0f;
    const f32 digitH = digitW;
    const f32 advance = digitW * 0.6f;

    // ============================================
    // NEW CODE - ALBW Port
    // Anchor to the LIVE rupee pane, not the fixed HIO position - port of the
    // fork's dMeter2Draw_c::getRupeeAnchorCenter (d_meter2_draw.cpp). The wallet
    // moves (the LoP layout relocates it to the top-right corner), and a popup
    // pinned to g_drawHIO.mRupeePos* would be left behind at the vanilla spot.
    // HIO stays the fallback, exactly as the fork does it.
    // ============================================
    f32 anchorX = g_drawHIO.mRupeePosX;
    f32 anchorY = g_drawHIO.mRupeePosY;
    {
        // g_meter2_info, not dMeter2Info_getMeterClass(): the accessor is
        // DUSK_NOINLINE and absent from the Windows stub (same rule as
        // albw_game.h). The data global is exported.
        dMeter2_c* meter = g_meter2_info.getMeterClass();
        dMeter2Draw_c* meterDraw = (meter != NULL) ? meter->getMeterDrawPtr() : NULL;
        if (meterDraw != NULL && meterDraw->mpRupeeParent[0] != NULL &&
            meterDraw->mpRupeeParent[0]->getPanePtr() != NULL)
        {
            const Vec c = meterDraw->mpRupeeParent[0]->getGlobalVtxCenter(false, 0);
            anchorX = c.x;
            anchorY = c.y;
        }
    }
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================

    f32 posX = anchorX - advance * 6.5f;
    const f32 centerY = anchorY;

    int digits[kPopupMaxDigits];
    int digitCount = 0;
    u16 value = sPopupAmount;
    do {
        digits[digitCount++] = value % 10;
        value /= 10;
    } while (value != 0 && digitCount < kPopupMaxDigits);

    const f32 plusSize = digitH * 0.8f;
    sPlusPic->setAlpha(alpha);
    sPlusPic->draw(posX - plusSize * 0.5f, centerY - plusSize * 0.5f, plusSize, plusSize, false,
                   false, false);
    posX += advance;

    for (int i = digitCount - 1; i >= 0; i--) {
        J2DPicture* pic = sDigitPic[digits[i]];
        pic->setAlpha(alpha);
        pic->draw(posX - digitW * 0.5f, centerY - digitH * 0.5f, digitW, digitH, false, false,
                  false);
        posX += advance;
    }
}

void on_cc_at_post(ModContext*, void* args, void*, void*) {
    fopAc_ac_c* enemy = mods::arg<fopAc_ac_c*>(args, 0);
    dCcU_AtInfo* info = mods::arg<dCcU_AtInfo*>(args, 1);
    if (info == NULL) {
        return;
    }
    tryKillAfterDamage(enemy, info->mAttackPower);
    tryDropMagicJar(enemy, info->mAttackPower);
}

void on_execute_post(ModContext*, void* args, void*, void*) {
    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(mods::arg<void*>(args, 0));
    if (actor == NULL || actor->health > 0) {
        return;
    }
    if (fopAcM_GetGroup(actor) != fopAc_ENEMY_e) {
        return;
    }
    onEnemyKill(actor);
    tryGrantFightVictory(fopAcM_GetName(actor));
}

void on_beamos_post(ModContext*, void*, void*, void*) {
    if (!grantsEnabled()) {
        return;
    }
    grantRupees(5);
}

void on_meter_draw_post(ModContext*, void*, void*, void*) {
    drawPopup();
}

}  // namespace

// Public bridge for the armogohma whole-function port (fork
// dAlbwEnemyRupees_tryGrantFightVictory). tryGrantFightVictory dedups internally.
void albw_enemy_rupees_grant_fight_victory(short profName) {
    tryGrantFightVictory(static_cast<s16>(profName));
}

ModResult albw_enemy_rupees_build_panel(UiElementHandle panel, ModError*) {
    return albw_ui_add_toggle(
        panel, "Kill rupees",
        "Add rupees when enemies die and when listed bosses are defeated. Region multipliers "
        "apply when enabled. Vanilla drops stay.",
        g_kill_rupees);
}

ModResult albw_enemy_rupees_init(ModError*) {
    if (mods::hook::add_post<CcAtCheck>(on_cc_at_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook cc_at_check (enemy rupees)");
        return MOD_ERROR;
    }
    if (mods::hook::add_post<AcExecute>(on_execute_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook fopAc_Execute (enemy rupees)");
        return MOD_ERROR;
    }
    if (mods::hook::add_post<BeamosDead>(on_beamos_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook Beamos mode_dead_init");
        return MOD_ERROR;
    }
    if (mods::hook::add_post<MeterDraw>(on_meter_draw_post) != MOD_OK) {
        svc_log->warn(mod_ctx, "meter draw hook unavailable for rupee popup");
    }
    return MOD_OK;
}

ModResult albw_enemy_rupees_shutdown(ModError*) {
    sVictoryGrantedCount = 0;
    sGrantedKillIdCount = 0;
    sPopupFramesLeft = 0;
    return MOD_OK;
}
