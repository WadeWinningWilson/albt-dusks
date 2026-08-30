#include "albw_combat.h"

#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"

dAlbwHelmBashTier dAlbwCombat_getHelmBashTier(fopAc_ac_c* actor) {
    if (actor == nullptr || fopAcM_GetGroup(actor) != fopAc_ENEMY_e) {
        return dAlbwHelmBash_THRESHOLD;
    }

    if (fopAcM_GetName(actor) == fpcNm_B_TN_e) {
        // daB_TN_c::mNextBreakPart @ 0x0A78 — unarmored when >= 11 (fork albwIsUnarmoredPhase).
        const int breakPart =
            *reinterpret_cast<const int*>(reinterpret_cast<const char*>(actor) + 0x0A78);
        if (breakPart >= 11) {
            return dAlbwHelmBash_THRESHOLD;
        }
        return dAlbwHelmBash_MAX;
    }

    return dAlbwHelmBash_THRESHOLD;
}

static bool inList(const s16* list, int count, s16 name) {
    for (int i = 0; i < count; ++i) {
        if (list[i] == name) {
            return true;
        }
    }
    return false;
}

static const s16 kMidBoss[] = {fpcNm_B_GG_e, fpcNm_B_TN_e, fpcNm_B_ZANTM_e, fpcNm_E_DT_e,
                               fpcNm_E_GOB_e, fpcNm_E_MK_e,  fpcNm_E_PM_e,  fpcNm_E_PZ_e,
                               fpcNm_E_RDB_e, fpcNm_E_TH_e,  fpcNm_E_VT_e,  fpcNm_E_YC_e,
                               fpcNm_E_YMB_e};

static const s16 kBoss[] = {
    fpcNm_B_BQ_e, fpcNm_B_BH_e, fpcNm_B_DS_e, fpcNm_B_DR_e, fpcNm_B_DRE_e, fpcNm_B_GM_e,
    fpcNm_B_OB_e, fpcNm_B_OH_e, fpcNm_B_OH2_e, fpcNm_B_YO_e, fpcNm_B_YOI_e, fpcNm_B_ZANT_e,
    fpcNm_B_ZANTS_e, fpcNm_B_ZANTZ_e, fpcNm_E_FM_e, fpcNm_E_HZELDA_e, fpcNm_NPC_KN_e,
};

static const s16 kFinal[] = {fpcNm_B_GND_e, fpcNm_B_MGN_e};

u16 dAlbwHP_applyDurabilityMult(s16 profName, u16 damage) {
    if (inList(kFinal, sizeof(kFinal) / sizeof(kFinal[0]), profName) ||
        inList(kBoss, sizeof(kBoss) / sizeof(kBoss[0]), profName))
    {
        return static_cast<u16>(damage * 2);
    }
    if (inList(kMidBoss, sizeof(kMidBoss) / sizeof(kMidBoss[0]), profName)) {
        return static_cast<u16>((damage * 3) / 2);
    }
    return damage;
}
