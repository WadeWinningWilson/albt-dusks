#include "albw_combat.h"

#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"

#include "SSystem/SComponent/c_cc_d.h"

#include "hurricane_spin.h"
#include "wolf_combat.h"

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

// ============================================
// Guard-opener classification. Port of fork src/d/d_albw_combat.cpp:72-98.
// Disambiguation is the whole job here:
//  - Hurricane shares CUT_TYPE_LARGE_TURN_* with Great Spin, so the check keys
//    on the hurricane STATE - Great Spin stays a clank by design.
//  - The Combat Howl AOE rides AT_TYPE_WOLF_CUT_TURN on the ALINK collider, the
//    same AT type as the ordinary wolf spin; the howl-active flag disambiguates
//    (Link cannot wolf-spin mid-howl).
//  - The Midna arm is its own actor, so its name is sufficient.
// ============================================
bool dAlbwCombat_isGuardOpenerHit(cCcD_Obj* i_hitObj) {
    if (i_hitObj == nullptr) {
        return false;
    }

    fopAc_ac_c* attacker = i_hitObj->GetAc();
    if (attacker == nullptr) {
        return false;
    }

    const s16 name = fopAcM_GetName(attacker);

    // [PORT-TRANSLATED] fork fpcNm_ALBW_MIDNA_ARM_e (fork f_pc_name.h). The stock
    // proc-name table has no slot for it, so the mod spawns the arm under the raw
    // id 0x031A - src/midna_arm.cpp:51 kFpcNm_ALBW_MIDNA_ARM. Same translation
    // already used at src/wolf_uty_port.inc:375.
    if (name == 0x031A) {
        return true;
    }

    if (name != fpcNm_ALINK_e) {
        return false;
    }

    // [PORT-TRANSLATED] fork daAlink_c::mWolfCombatHowlActive (fork-only field,
    // fork include/d/actor/d_a_alink.h:4681) -> the DUSK's own howl-active
    // accessor (src/wolf_howl_combat.cpp:307-310). The fork's NULL check on
    // daAlink_getAlinkActorClass() is folded into the fpcNm_ALINK_e gate above:
    // a collider owned by ALINK cannot exist without the ALINK actor.
    if (albw_wolf_combat_howl_active() && i_hitObj->ChkAtType(AT_TYPE_WOLF_CUT_TURN)) {
        return true;
    }

    // [PORT-TRANSLATED] fork `link->mProcID == daAlink_c::PROC_CUT_GS_HURRICANE ||
    // link->mProcID == daAlink_c::PROC_CUT_GS_HURRICANE_TIRED`. The stock proc
    // table has no slot for either, so the mod overlays the hurricane onto
    // PROC_CUT_TURN (src/hurricane_spin.cpp:195-202) and mProcID never carries the
    // fork value. albw_hurricane_is_active() is `s_phase != HP_NONE` over
    // HurricanePhase { HP_NONE, HP_SPIN, HP_TIRED } (src/hurricane_spin.cpp:197,
    // :360) - HP_SPIN and HP_TIRED are exactly the fork's two procs, so this is a
    // 1:1 reproduction of the disjunction, not a narrowing.
    return albw_hurricane_is_active();
}
