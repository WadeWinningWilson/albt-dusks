#include "parry_hooks.h"

#include "albw_common.h"
#include "config_vars.h"
#include "parry_master.h"

#include "d/actor/d_a_player.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_gameover.h"
#include "d/d_meter2.h"
#include "d/d_meter2_info.h"
#include "d/d_save.h"
#include "mods/hook.hpp"

#define private public
#include "d/actor/d_a_alink.h"
#undef private

#include "albw_game.h"
#include "meter_bridge.h"
#include "oocoo.h"
#include "rental_eligibility.h"

namespace {

DEFINE_HOOK(&daPy_py_c::setPlayerDamage, SetPlayerDamage);
DEFINE_HOOK_SYMBOL("cc_at_check", fopAc_ac_c*(fopAc_ac_c*, dCcU_AtInfo*), CcAtCheck);
DEFINE_HOOK(&dMeter2_c::moveKantera, MoveKanteraParry);
DEFINE_HOOK(&dGameover_c::_create, GameoverCreate);

static s16 sKanteraOldLife = 0;

HookAction on_set_player_damage_pre(ModContext*, void* args, void*, void*) {
    if (!dParryMaster_isEnabled()) {
        return HOOK_CONTINUE;
    }
    const int dmg = mods::arg<int>(args, 0);
    if (dmg > 0) {
        dParryMaster_onHpLoss(dmg);
    }
    return HOOK_CONTINUE;
}

void on_cc_at_check_post(ModContext*, void* args, void* retval, void*) {
    if (!dParryMaster_isEnabled() || retval == nullptr) {
        return;
    }
    auto* attacker = mods::arg<fopAc_ac_c*>(args, 0);
    if (attacker == nullptr || fopAcM_GetName(attacker) != fpcNm_ALINK_e) {
        return;
    }
    if (static_cast<fopAc_ac_c*>(retval) != nullptr) {
        dParryMaster_onDealtDamage();
    }
}

HookAction on_move_kantera_parry_pre(ModContext*, void*, void*, void*) {
    sKanteraOldLife =
        g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getLife();
    return HOOK_CONTINUE;
}

void on_move_kantera_parry_post(ModContext*, void*, void*, void*) {
    if (!dParryMaster_isEnabled()) {
        return;
    }
    const s16 newLife =
        g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getLife();
    const int delta = static_cast<int>(newLife) - static_cast<int>(sKanteraOldLife);
    if (delta > 0) {
        dParryMaster_onHeal(delta);
    } else if (newLife == 0) {
        dParryMaster_clearQueue();
    }
}

void on_gameover_create_post(ModContext*, void* args, void* retval, void*) {
    if (!albw_cfg_bool(g_postman_rental, true)) {
        return;
    }
    if (retval == nullptr || *static_cast<s32*>(retval) != cPhs_COMPLEATE_e) {
        return;
    }
    if (g_meter2_info.getGameOverType() != 0) {
        return;
    }

    const char* deathStage = g_dComIfG_gameInfo.play.getLastPlayStageName();
    const bool diedInDungeon =
        deathStage != nullptr && deathStage[0] == 'D' && deathStage[1] == '_' &&
        deathStage[2] == 'M' && deathStage[3] == 'N';
    albw_oocoo_on_death_context(deathStage, diedInDungeon);

    if (albw_rental_postman_unlocked()) {
        albw_rental_strip_all_on_death();
        albw_meter_fill_on_death();
    }
}

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace

ModResult albw_parry_master_init(ModError* error) {
    if (!install(error, "SetPlayerDamageParry",
                 mods::hook_add_pre<SetPlayerDamage>(svc_hook, on_set_player_damage_pre)) ||
        !install(error, "CcAtCheckParry",
                 mods::hook_add_post<CcAtCheck>(svc_hook, on_cc_at_check_post)) ||
        !install(error, "MoveKanteraParryPre",
                 mods::hook_add_pre<MoveKanteraParry>(svc_hook, on_move_kantera_parry_pre)) ||
        !install(error, "MoveKanteraParryPost",
                 mods::hook_add_post<MoveKanteraParry>(svc_hook, on_move_kantera_parry_post)) ||
        !install(error, "GameoverRental",
                 mods::hook_add_post<GameoverCreate>(svc_hook, on_gameover_create_post)))
    {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "parry master + death strip hooks ready");
    return MOD_OK;
}

ModResult albw_parry_master_shutdown(ModError*) {
    return MOD_OK;
}
