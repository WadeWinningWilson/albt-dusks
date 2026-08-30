// Region HP / damage — part of dev.albt.albw.

#include "albw_common.h"
#include "config_vars.h"
#include "modules.h"
#include "region_table.h"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_nbomb.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "mods/svc/hook.hpp"

#include <algorithm>
#include <unordered_set>

namespace {

int s_armStack[8];
int s_armSp = 0;
std::unordered_set<fpc_ProcID> s_trueHpApplied;

DEFINE_HOOK_SYMBOL("fopAc_Execute", int(void*), AcExecute);
DEFINE_HOOK_SYMBOL("fopAc_Delete", int(void*), AcDelete);
DEFINE_HOOK(&daAlink_c::damageMagnification, DamageMag);
DEFINE_HOOK(&daAlink_c::setDamagePoint, SetDmgPt);
DEFINE_HOOK(&daPy_py_c::setPlayerDamage, SetPlayerDmg);

int clampMult(int value) {
    if (value < 1) {
        return 1;
    }
    if (value > 16) {
        return 16;
    }
    return value;
}

enum Category {
    CatNormal = 0,
    CatMidBoss = 1,
    CatBoss = 2,
    CatFinal = 3,
};

const s16 kMidBoss[] = {
    fpcNm_B_GG_e, fpcNm_B_TN_e, fpcNm_B_ZANTM_e, fpcNm_E_DT_e, fpcNm_E_GOB_e, fpcNm_E_MK_e,
    fpcNm_E_PM_e, fpcNm_E_PZ_e, fpcNm_E_RDB_e, fpcNm_E_TH_e, fpcNm_E_VT_e, fpcNm_E_YC_e,
    fpcNm_E_YMB_e,
};

const s16 kBoss[] = {
    fpcNm_B_BQ_e,  fpcNm_B_BH_e,  fpcNm_B_DS_e,    fpcNm_B_DR_e,    fpcNm_B_DRE_e, fpcNm_B_GM_e,
    fpcNm_B_OB_e,  fpcNm_B_OH_e,  fpcNm_B_OH2_e,   fpcNm_B_YO_e,    fpcNm_B_YOI_e, fpcNm_B_ZANT_e,
    fpcNm_B_ZANTS_e, fpcNm_B_ZANTZ_e, fpcNm_E_FM_e, fpcNm_E_HZELDA_e, fpcNm_NPC_KN_e,
};

const s16 kFinal[] = {fpcNm_B_GND_e, fpcNm_B_MGN_e};

bool inList(const s16* list, size_t count, s16 name) {
    for (size_t i = 0; i < count; i++) {
        if (list[i] == name) {
            return true;
        }
    }
    return false;
}

Category categoryOf(s16 profName) {
    if (inList(kFinal, sizeof(kFinal) / sizeof(kFinal[0]), profName)) {
        return CatFinal;
    }
    if (inList(kBoss, sizeof(kBoss) / sizeof(kBoss[0]), profName)) {
        return CatBoss;
    }
    if (inList(kMidBoss, sizeof(kMidBoss) / sizeof(kMidBoss[0]), profName)) {
        return CatMidBoss;
    }
    return CatNormal;
}

int categoryMult(s16 profName) {
    int mult = 1;
    switch (categoryOf(profName)) {
    case CatNormal:
        mult = albw_cfg_int(g_hp_normal, 1);
        break;
    case CatMidBoss:
        mult = albw_cfg_int(g_hp_midboss, 1);
        break;
    case CatBoss:
        mult = albw_cfg_int(g_hp_boss, 1);
        break;
    case CatFinal:
        mult = albw_cfg_int(g_hp_final, 1);
        break;
    }
    mult = clampMult(mult);
    return mult > 1 ? mult : 1;
}

s16 scaleHpField(s16 value, int mult) {
    if (value <= 1 || mult <= 1) {
        return value;
    }
    const int scaled = static_cast<int>(value) * mult;
    return static_cast<s16>(std::min(scaled, 32767));
}

void tryApplyTrueMaxHp(fopAc_ac_c* actor) {
    if (actor == NULL || !fopAcM_IsActor(actor)) {
        return;
    }
    const fpc_ProcID procId = fpcM_GetID(actor);
    if (procId == fpcM_ERROR_PROCESS_ID_e || !fpcM_IsExecuting(procId)) {
        return;
    }
    if (s_trueHpApplied.find(procId) != s_trueHpApplied.end()) {
        return;
    }
    if (fopAcM_GetGroup(actor) != fopAc_ENEMY_e) {
        s_trueHpApplied.insert(procId);
        return;
    }
    if (actor->health <= 0) {
        return;
    }
    if (actor->health <= 1) {
        s_trueHpApplied.insert(procId);
        return;
    }

    const int mult = categoryMult(fopAcM_GetName(actor));
    if (mult > 1) {
        actor->health = scaleHpField(actor->health, mult);
        if (actor->field_0x560 > 1) {
            actor->field_0x560 = scaleHpField(actor->field_0x560, mult);
        } else if (actor->field_0x560 > 0) {
            actor->field_0x560 = actor->health;
        }
    }

    actor->health = albw_region_scale_hp(actor->health);
    if (actor->field_0x560 > 1) {
        actor->field_0x560 = albw_region_scale_hp(actor->field_0x560);
    } else if (actor->field_0x560 > 0) {
        actor->field_0x560 = actor->health;
    }
    s_trueHpApplied.insert(procId);
}

dCcD_GObjInf* firstTgHit(daAlink_c* link) {
    if (link == NULL) {
        return NULL;
    }
    for (int i = 0; i < 3; i++) {
        if (link->mTgCyls[i].ChkTgHit()) {
            return &link->mTgCyls[i];
        }
    }
    return NULL;
}

bool isPlayerBombActor(fopAc_ac_c* actor) {
    if (actor == NULL || fopAcM_GetName(actor) != fpcNm_NBOMB_e) {
        return false;
    }
    daNbomb_c* bomb = static_cast<daNbomb_c*>(actor);
    if (bomb->checkPlayerMake()) {
        return true;
    }
    switch (bomb->mType) {
    case daNbomb_c::TYPE_NORMAL_PLAYER:
    case daNbomb_c::TYPE_WATER_PLAYER:
    case daNbomb_c::TYPE_INSECT_PLAYER:
        return true;
    default:
        return false;
    }
}

bool isCoverSource(daAlink_c* link, int dmgAmount, BOOL setDmgTimer) {
    if (!albw_cfg_bool(g_region_damage, false) || link == NULL) {
        return false;
    }
    if (!setDmgTimer) {
        return false;
    }
    if (link->checkEndResetFlg2(daPy_py_c::ERFLG2_UNK_40)) {
        return false;
    }

    dCcD_GObjInf* hit = firstTgHit(link);
    if (hit != NULL) {
        dCcD_GObjInf* atObj = hit->GetTgHitGObj();
        fopAc_ac_c* atActor = hit->GetTgHitAc();
        if (atObj != NULL && atObj->GetAtType() == AT_TYPE_BOMB && isPlayerBombActor(atActor)) {
            return false;
        }
        return true;
    }

    return dmgAmount == 1 && link->checkPolyDamage() != 0;
}

void armPush(bool armed) {
    if (s_armSp < 8) {
        s_armStack[s_armSp++] = armed ? 1 : 0;
    }
    if (armed) {
        albw_region_push_damage_scale();
    }
}

void armPop() {
    if (s_armSp <= 0) {
        return;
    }
    const bool armed = s_armStack[--s_armSp] != 0;
    if (armed) {
        albw_region_pop_damage_scale();
    }
}

void on_execute_post(ModContext*, void* args, void*, void*) {
    if (fpcM_SearchByName(fpcNm_PLAY_SCENE_e) == NULL) {
        return;
    }
    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(mods::arg<void*>(args, 0));
    tryApplyTrueMaxHp(actor);
}

void on_delete_post(ModContext*, void* args, void* retval, void*) {
    if (retval == NULL || *static_cast<int*>(retval) == 0) {
        return;
    }
    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(mods::arg<void*>(args, 0));
    if (actor != NULL) {
        s_trueHpApplied.erase(fpcM_GetID(actor));
    }
}

void on_damage_mag_post(ModContext*, void*, void* retval, void*) {
    if (retval == NULL) {
        return;
    }
    const f32 mult = albw_region_damage_mult();
    if (mult <= 1.0f) {
        return;
    }
    *static_cast<f32*>(retval) *= mult;
}

HookAction on_set_damage_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    const int dmg = mods::arg<int>(args, 1);
    const BOOL timer = mods::arg<BOOL>(args, 3);
    armPush(isCoverSource(link, dmg, timer));
    return HOOK_CONTINUE;
}

void on_set_damage_post(ModContext*, void*, void*, void*) {
    armPop();
}

HookAction on_player_damage_pre(ModContext*, void* args, void*, void*) {
    const int dmg = mods::arg<int>(args, 0);
    armPush(albw_cfg_bool(g_region_damage, false) && dmg >= 4);
    return HOOK_CONTINUE;
}

void on_player_damage_post(ModContext*, void*, void*, void*) {
    armPop();
}

}  // namespace

ModResult albw_region_hp_build_panel(UiElementHandle panel, ModError* error) {
    if (albw_ui_add_number(panel, "Common HP",
                           "True max-HP multiplier for ordinary enemies. 1x is vanilla.",
                           g_hp_normal, 1, 16) != MOD_OK ||
        albw_ui_add_number(panel, "Mid-boss HP", "True max-HP for mid-bosses.", g_hp_midboss, 1,
                           16) != MOD_OK ||
        albw_ui_add_number(panel, "Boss HP", "True max-HP for dungeon bosses.", g_hp_boss, 1, 16) !=
            MOD_OK ||
        albw_ui_add_number(panel, "Final HP", "True max-HP for Ganondorf / Beast Ganon.", g_hp_final,
                           1, 16) != MOD_OK ||
        albw_ui_add_toggle(panel, "Region HP",
                           "Multiply spawn HP by the province/dungeon table after category HP.",
                           g_region_hp) != MOD_OK ||
        albw_ui_add_toggle(panel, "Region damage",
                           "Multiply incoming COVER damage to Link by the same table.",
                           g_region_damage) != MOD_OK ||
        albw_ui_add_toggle(panel, "Region multipliers (master)",
                           "Master switch for region table on HP and rupee axes.", g_region_mult) !=
            MOD_OK ||
        albw_ui_add_toggle(panel, "Region rupees",
                           "Scale enemy-death rupee grants by the region table (×3 extra when "
                           "Region damage is on).",
                           g_region_mult_rupees) != MOD_OK)
    {
        if (error != nullptr) {
            error->code = MOD_ERROR;
        }
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_region_hp_init(ModError*) {
    if (mods::hook::add_post<AcExecute>(on_execute_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook fopAc_Execute (region hp)");
        return MOD_ERROR;
    }
    if (mods::hook::add_post<AcDelete>(on_delete_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook fopAc_Delete");
        return MOD_ERROR;
    }
    if (mods::hook::add_post<DamageMag>(on_damage_mag_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook damageMagnification");
        return MOD_ERROR;
    }
    if (mods::hook::add_pre<SetDmgPt>(on_set_damage_pre) != MOD_OK ||
        mods::hook::add_post<SetDmgPt>(on_set_damage_post) != MOD_OK)
    {
        svc_log->error(mod_ctx, "failed to hook setDamagePoint");
        return MOD_ERROR;
    }
    if (mods::hook::add_pre<SetPlayerDmg>(on_player_damage_pre) != MOD_OK ||
        mods::hook::add_post<SetPlayerDmg>(on_player_damage_post) != MOD_OK)
    {
        svc_log->error(mod_ctx, "failed to hook setPlayerDamage");
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_region_hp_shutdown(ModError*) {
    s_trueHpApplied.clear();
    s_armSp = 0;
    return MOD_OK;
}
