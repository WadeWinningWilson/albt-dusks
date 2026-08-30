#include "wolf_arts.h"

#include "albw_common.h"
#include "albw_game.h"
#include "config_vars.h"
#include "extra_item_slot.h"
#include "quick_swap.h"
#include "wolf_combat.h"
#include "wolf_charge_hud.h"

#include "Z2AudioLib/Z2AudioMgr.h"
#include "Z2AudioLib/Z2SeqMgr.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_controller_pad.h"

#define private public
#include "d/actor/d_a_alink.h"
#undef private

namespace {

static bool s_loggedArmActorMissing = false;

// Fork wolf howl BGM pool (solos always available).
static constexpr u32 s_wolfHowlSolo[] = {
    Z2BGM_HOWL_TOBIKUSA,
    Z2BGM_HOWL_UMAKUSA,
    Z2BGM_HOWL_ZELDASONG,
};

bool wolf_arts_enabled() {
    return albw_cfg_bool(g_wolf_combat, false) && albw_is_dpad_quick_swap_enabled();
}

bool dev_test_bypass() {
    return albw_cfg_bool(g_wolf_arts_dev_test, false);
}

void try_wolf_howl_burst() {
    if (!wolf_arts_enabled()) {
        return;
    }

    daAlink_c* link = static_cast<daAlink_c*>(albw_game::link_player());
    if (link == nullptr || !link->checkWolf() || !dAlbwWolfCombat_isEnabled()) {
        return;
    }

    if (!dev_test_bypass() && !dAlbwWolfArts_isHowlUnlocked()) {
        return;
    }

    if (link->checkEventRun()) {
        Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        return;
    }
    if (!link->mLinkAcch.ChkGroundHit() || link->checkModeFlg(daAlink_c::MODE_PLAYER_FLY)) {
        return;
    }

    if (!dev_test_bypass()) {
        if (albw_wolf_get_charge_count() < 2) {
            Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            dAlbwWolfChargeHud_notifyDeny();
            return;
        }
        albw_wolf_spend_charge(1);
        dAlbwWolfChargeHud_notify();
    }

    mDoCPd_c::getCpadInfo(PAD_1).mPressedButtonFlags = 0;

    const int poolSize = static_cast<int>(sizeof(s_wolfHowlSolo) / sizeof(s_wolfHowlSolo[0]));
    int idx = static_cast<int>(cM_rndF(static_cast<f32>(poolSize)));
    if (idx < 0 || idx >= poolSize) {
        idx = 0;
    }
    mDoAud_subBgmStart(s_wolfHowlSolo[idx]);
    link->procWolfHowlInit(0);
}

void try_wolf_arm_burst() {
    if (!wolf_arts_enabled()) {
        return;
    }

    daAlink_c* link = static_cast<daAlink_c*>(albw_game::link_player());
    if (link == nullptr || !link->checkWolf() || !dAlbwWolfCombat_isEnabled()) {
        return;
    }

    const bool devTest = dev_test_bypass();
    if (!devTest && !dAlbwWolfArts_isArmUnlocked()) {
        return;
    }

    if (link->checkEventRun()) {
        Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        return;
    }

    if (!devTest) {
        if (albw_wolf_get_charge_count() < 2) {
            Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            dAlbwWolfChargeHud_notifyDeny();
            return;
        }
        albw_wolf_spend_charge(2);
        dAlbwWolfChargeHud_notify();
    }

    mDoCPd_c::getCpadInfo(PAD_1).mPressedButtonFlags = 0;

    // Midna arm actor (d_a_albw_midna_arm) ships with the fork host profile only.
    // Gate + charge economy are wired; spawn lands in Phase 2B when the actor TU ports.
    if (!s_loggedArmActorMissing) {
        svc_log->info(mod_ctx,
                      "wolf Midna arm: unlock/charge gates ready; actor spawn pending host actor port");
        s_loggedArmActorMissing = true;
    }
    if (devTest) {
        Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }
}

}  // namespace

ModResult albw_wolf_arts_init(ModError*) {
    return MOD_OK;
}

ModResult albw_wolf_arts_shutdown(ModError*) {
    return MOD_OK;
}

void albw_wolf_arts_tick() {
    if (!wolf_arts_enabled()) {
        return;
    }

    daPy_py_c* player = albw_game::link_player();
    if (player == nullptr || !player->checkWolf()) {
        return;
    }

    if (mDoCPd_c::getTrigUp(PAD_1) != 0) {
        try_wolf_howl_burst();
    }
    if (mDoCPd_c::getTrigRight(PAD_1) != 0) {
        try_wolf_arm_burst();
    }
}
