#include "wolf_arts.h"

#include "albw_common.h"
#include "albw_game.h"
#include "config_vars.h"
#include "extra_item_slot.h"
#include "quick_swap.h"
#include "wolf_combat.h"
#include "wolf_charge_hud.h"
#include "d/d_save.h"

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

// Fork wolf howl BGM pool (d_a_alink_dusk.cpp:20-42): 3 always-on solos +
// each LEARNED duet (per-save Golden Wolf event flags).
static constexpr u32 s_wolfHowlSolo[] = {
    Z2BGM_HOWL_TOBIKUSA,
    Z2BGM_HOWL_UMAKUSA,
    Z2BGM_HOWL_ZELDASONG,
};
struct WolfHowlDuo { u32 bgm; int eventFlag; };
static constexpr WolfHowlDuo s_wolfHowlDuos[] = {
    {Z2BGM_HEALING_DUO, 472},     // Golden Wolf 2
    {Z2BGM_SOUL_REQ_DUO, 473},    // Golden Wolf 3
    {Z2BGM_LIGHT_PRLD_DUO, 474},  // Golden Wolf 4
    {Z2BGM_NEW_01_DUO, 475},      // Golden Wolf 5
    {Z2BGM_NEW_02_DUO, 476},      // Golden Wolf 6
    {Z2BGM_NEW_03_DUO, 477},      // Golden Wolf 7
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

    // fork handleWolfHowlBurst: not mid-flight/airborne is checked above; arm the
    // one-shot combat request the procWolfHowlInit hook consumes (this is what
    // was missing - without it the init played a PLAIN howl over the song).
    albw_wolf_howl_arm_combat_request();

    // fork: pool = 3 solos + each learned duo, one picked at random, started
    // once here (not in init) so the pose re-loop cannot restart the song.
    u32 pool[sizeof(s_wolfHowlSolo) / sizeof(s_wolfHowlSolo[0]) +
             sizeof(s_wolfHowlDuos) / sizeof(s_wolfHowlDuos[0])];
    int n = 0;
    for (u32 solo : s_wolfHowlSolo) {
        pool[n++] = solo;
    }
    for (const WolfHowlDuo& duo : s_wolfHowlDuos) {
        if (albw_game::is_event_bit(dSv_event_flag_c::saveBitLabels[duo.eventFlag])) {
            pool[n++] = duo.bgm;
        }
    }
    int idx = static_cast<int>(cM_rndF(static_cast<f32>(n)));
    if (idx < 0 || idx >= n) {
        idx = 0;
    }
    mDoAud_subBgmStart(pool[idx]);
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

    // fork handleWolfArmBurst: not during a cutscene; one arm at a time.
    if (link->checkEventRun() || albw_midna_arm_is_alive()) {
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

    // fork: spawn the arm helper (it manages its own 15 s lifetime + targeting +
    // collider). The actor now lives in midna_arm.cpp, served via fpcPf_Get.
    if (!albw_midna_arm_spawn(link) && svc_log != nullptr) {
        svc_log->error(mod_ctx, "wolf Midna arm: spawn failed");
    }
    (void)s_loggedArmActorMissing;
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
