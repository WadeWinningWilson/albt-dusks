// ============================================
// Region multipliers + enemy HP scaling — VERBATIM fork port.
//
// Replaces the earlier hand-written region_table.cpp / region_hp.cpp (which had
// drifted from the fork). The province/dungeon table, gating, HP scaling and
// Link-damage-decrease logic are the fork's own d_albw_region_mult.cpp +
// d_albw_hp_mult.cpp, extracted verbatim by tools/port/port_tool.py into
// region_mult_port.inc / hp_mult_port.inc. Only the wiring lives here:
//   * fopAc_Execute post  -> dAlbwHP_tryApplyTrueMaxHp (per-enemy true max HP)
//   * fopAc_Delete  post  -> dAlbwHP_onActorDelete
//   * damageMagnification -> *= dAlbwRegionMult_getDamageMult() (fork line 173)
//   * setDamagePoint pre/post -> hold DamageScaleScope for COVER hits, reproducing
//     the fork's enemy/trap/boss-CC + hazard-poly scope sites (d_a_alink_damage.inc).
//
// The fork's extra edits in giants we cannot port (Morpheel d_a_b_oh, Deku Toad
// d_a_e_dt COVER scopes; the d_cc_uty applyMult intercept) stay follow-ups.
// port_tool omits aggregate arrays, so the category boss lists are hand-added
// below verbatim from d_albw_hp_mult.cpp.
// ============================================

#include "region_table.h"

#include "albw_common.h"
#include "config_vars.h"
#include "modules.h"
#include "region_mult_port.h"
#include "hp_mult_port.h"

#include "d/d_com_inf_game.h"
#include "d/d_stage.h"
#include "d/d_cc_uty.h"
#include "d/d_cc_d.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_nbomb.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "mods/svc/hook.hpp"

#include <algorithm>
#include <cstddef>
#include <unordered_set>

#if TARGET_PC

// ---- hand-added file-static boss lists (verbatim from d_albw_hp_mult.cpp;
//      port_tool does not emit aggregate-init arrays) ----
static const s16 sMidBoss[] = {
    fpcNm_B_GG_e,    fpcNm_B_TN_e,  fpcNm_B_ZANTM_e, fpcNm_E_DT_e,  fpcNm_E_GOB_e,
    fpcNm_E_MK_e,    fpcNm_E_PM_e,  fpcNm_E_PZ_e,    fpcNm_E_RDB_e, fpcNm_E_TH_e,
    fpcNm_E_VT_e,    fpcNm_E_YC_e,  fpcNm_E_YMB_e,
};

static const s16 sBoss[] = {
    fpcNm_B_BQ_e,    fpcNm_B_BH_e,    fpcNm_B_DS_e,    fpcNm_B_DR_e,    fpcNm_B_DRE_e,
    fpcNm_B_GM_e,    fpcNm_B_OB_e,    fpcNm_B_OH_e,    fpcNm_B_OH2_e,   fpcNm_B_YO_e,
    fpcNm_B_YOI_e,   fpcNm_B_ZANT_e,  fpcNm_B_ZANTS_e, fpcNm_B_ZANTZ_e, fpcNm_E_FM_e,
    fpcNm_E_HZELDA_e, fpcNm_NPC_KN_e,
};

static const s16 sFinalBoss[] = {
    fpcNm_B_GND_e, fpcNm_B_MGN_e,
};

// IN macro the ported dAlbwHP_getCategory uses (port_tool referenced but did not
// emit it). sizeof form avoids a std::size dependency.
#define IN(arr, name) inList((arr), sizeof(arr) / sizeof((arr)[0]), (name))

// The verbatim fork logic (config reads substituted to albw_cfg_* by port_tool).
#include "region_mult_port.inc"
#include "hp_mult_port.inc"

#undef IN

namespace {

// ---- COVER-source arming (reproduces the fork's DamageScaleScope sites at the
//      setDamagePoint seam, since we cannot edit d_a_alink_damage.inc) ----
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
        // Exclude Link-bag bombs (fork: scaleRegion = !isPlayerBomb).
        if (atObj != NULL && atObj->GetAtType() == AT_TYPE_BOMB &&
            dAlbwRegionMult_isPlayerBomb(atActor)) {
            return false;
        }
        return true;
    }
    // Hazard poly (fork d_a_alink_damage.inc:611 COVER scope).
    return dmgAmount == 1 && link->checkPolyDamage() != 0;
}

// ---- hooks ----
DEFINE_HOOK_SYMBOL("fopAc_Execute", int(void*), AcExecute);
DEFINE_HOOK_SYMBOL("fopAc_Delete", int(void*), AcDelete);
DEFINE_HOOK(&daAlink_c::damageMagnification, DamageMag);
DEFINE_HOOK(&daAlink_c::setDamagePoint, SetDmgPt);

void on_execute_post(ModContext*, void* args, void*, void*) {
    if (fpcM_SearchByName(fpcNm_PLAY_SCENE_e) == NULL) {
        return;
    }
    dAlbwHP_tryApplyTrueMaxHp(static_cast<fopAc_ac_c*>(mods::arg<void*>(args, 0)));
}

void on_delete_post(ModContext*, void* args, void* retval, void*) {
    if (retval == NULL || *static_cast<int*>(retval) == 0) {
        return;
    }
    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(mods::arg<void*>(args, 0));
    if (actor != NULL) {
        dAlbwHP_onActorDelete(fpcM_GetID(actor));
    }
}

// Fork damageMagnification (d_a_alink_damage.inc:173): base_mag *= getDamageMult().
// getDamageMult self-gates on regionDamage + the COVER scope depth.
void on_damage_mag_post(ModContext*, void*, void* retval, void*) {
    if (retval == NULL) {
        return;
    }
    const f32 mult = dAlbwRegionMult_getDamageMult();
    if (mult <= 1.0f) {
        return;
    }
    *static_cast<f32*>(retval) *= mult;
}

// Hold the DamageScaleScope across setDamagePoint for COVER hits — reproduces the
// fork's `dAlbwRegionMult_DamageScaleScope regionDmg;` at the enemy/trap/boss-CC
// and hazard-poly sites. (No dmg>=4 gate: the fork scales every COVER hit.)
bool s_scoped = false;

HookAction on_set_damage_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    const int dmg = mods::arg<int>(args, 1);
    const BOOL timer = mods::arg<BOOL>(args, 3);
    s_scoped = isCoverSource(link, dmg, timer);
    if (s_scoped) {
        dAlbwRegionMult_pushDamageScale();
    }
    return HOOK_CONTINUE;
}

void on_set_damage_post(ModContext*, void*, void*, void*) {
    if (s_scoped) {
        dAlbwRegionMult_popDamageScale();
        s_scoped = false;
    }
}

}  // namespace

// ============================================
// Interface bridges — keep the mod's albw_region_* callers (enemy_rupees,
// parry_master, settings UI, mod.cpp) working against the ported logic.
// ============================================
f32 albw_region_resolve_table_mult() { return dAlbwRegionMult_getTableMult(); }
f32 albw_region_damage_mult() { return dAlbwRegionMult_getDamageMult(); }
f32 albw_region_rupee_mult() {
    return dAlbwRegionMult_getRupeeMult() * dAlbwRegionMult_getRegionDamageRupeeMult();
}
u16 albw_region_scale_rupees(u16 amount) { return dAlbwRegionMult_scaleRupees(amount); }
s16 albw_region_scale_hp(s16 hp) { return dAlbwRegionMult_scaleHp(hp); }
void albw_region_push_damage_scale() { dAlbwRegionMult_pushDamageScale(); }
void albw_region_pop_damage_scale() { dAlbwRegionMult_popDamageScale(); }

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
        mods::hook::add_post<SetDmgPt>(on_set_damage_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook setDamagePoint");
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_region_hp_shutdown(ModError*) {
    s_trueHpApplied.clear();
    return MOD_OK;
}

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
        albw_ui_add_number(panel, "Link damage decrease",
                           "Divide the damage Link's own hits deal (1x = vanilla).",
                           g_link_damage_decrease, 1, 16) != MOD_OK ||
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
                           "Scale enemy-death rupee grants by the region table (x3 extra when "
                           "Region damage is on).",
                           g_region_mult_rupees) != MOD_OK) {
        if (error != nullptr) {
            error->code = MOD_ERROR;
        }
        return MOD_ERROR;
    }
    return MOD_OK;
}

#endif  // TARGET_PC
