#include "quick_equip.h"
#include "albw_symbols.h"

#include "albw_common.h"
#include "albw_game.h"
#include "albw_l1_input.h"
#include "config_vars.h"
#include "extra_item_slot.h"
#include "mods/hook.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_menu_window.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_object.h"
#include "d/d_save.h"
#include "m_Do/m_Do_controller_pad.h"

#define private public
#include "d/actor/d_a_alink.h"
#include "d/d_meter2.h"
#undef private

namespace {

constexpr u16 kQuickHoldFrames = 8;  // ~250ms at 30Hz — fork value

bool s_session = false;
bool s_holdActive = false;
u16 s_holdFrames = 0;
bool s_openedQuick = false;
bool s_forceUpOnce = false;

bool feature_on() {
    return albw_cfg_bool(g_quick_equip_wheel, false) && albw_is_extra_item_slot_enabled();
}

bool l1_down() {
    return albw_l1_held(PAD_1);
}

bool l1_pressed() {
    return albw_l1_trig(PAD_1);
}

bool can_open_wheel() {
    if (g_dComIfG_gameInfo.play.isPauseFlag() || g_dComIfG_gameInfo.play.isEnableNextStage()) {
        return false;
    }
    // isItemOpenCheck() derefs the player with no null guard — boot/title AV at +0x66a
    // if we call it before Link exists (dusklight-20260828-231134.log).
    if (albw_game::link_player() == nullptr) {
        return false;
    }
    if (!dMeter2Info_isItemOpenCheck()) {
        return false;
    }
    const int mapStatus = g_meter2_info.getMapStatus();
    if (mapStatus != 0 && mapStatus != 1) {
        return false;
    }
    return true;
}

void open_item_wheel(bool quickSession) {
    dMsgObject_setKillMessageFlag();
    if (g_dComIfG_gameInfo.play.isHeapLockFlag() == 5) {
        dMeter2_c* meter = g_meter2_info.getMeterClass();
        if (meter != nullptr) {
            meter->emphasisButtonDelete();
        }
    }

    if (quickSession) {
        albw_quick_equip_begin_session();
    } else {
        albw_quick_equip_end_session();
    }

    // Extra item-slot UP post consumes this and forces dMw_UP_TRIGGER TRUE once.
    s_forceUpOnce = true;
}

DEFINE_HOOK_SYMBOL(ALBT_SYM_SET_SELECT_ITEM_INDEX, void(int, u8), SetSelectItemIndex);

HookAction on_set_select_item_index_pre(ModContext*, void* args, void*, void*) {
    if (!s_session || !feature_on()) {
        return HOOK_CONTINUE;
    }

    const int i_no = mods::arg<int>(args, 0);
    const u8 slot = mods::arg<u8>(args, 1);
    if (i_no != SELECT_ITEM_X && i_no != SELECT_ITEM_Y) {
        return HOOK_CONTINUE;
    }

    // Fork Quick Equip: release assigns Z only — X/Y stay unchanged.
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setSelectItemIndex(SELECT_ITEM_DOWN,
                                                                               slot);
    dComIfGp_setSelectItem(SELECT_ITEM_DOWN);
    return HOOK_SKIP_ORIGINAL;
}

// ============================================
// A hook that fails to resolve must NOT abort mod_initialize.
//
// This helper used to call mods::set_error(..., MOD_ERROR, ...) and return
// false, which made mod_initialize return MOD_ERROR - so ONE unresolved symbol
// unloaded the ENTIRE mod. That is how a single missing hook target reached
// players as "Failed - Reason: <hook name>" with nothing loaded at all, on a
// build where every other feature was fine. It is the same doctrine fyrus.cpp
// already states for the boss hooks.
//
// Now the miss is LOUD and SCOPED: the feature that needed the hook is
// inactive for the run and says so by name in the log, and everything else
// still loads. Never make this silent - a quiet miss turns "never bound" into
// "plausibly wrong forever".
// ============================================
bool install(ModError*, const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, name);
            svc_log->error(mod_ctx,
                           "hook above did NOT install - that feature is inactive this run");
        }
    }
    return true;
}

}  // namespace

bool albw_quick_equip_enabled() {
    return feature_on();
}

bool albw_quick_equip_session_active() {
    return s_session;
}

void albw_quick_equip_begin_session() {
    s_session = true;
}

void albw_quick_equip_end_session() {
    s_session = false;
}

bool albw_quick_equip_consume_force_up() {
    if (!s_forceUpOnce) {
        return false;
    }
    s_forceUpOnce = false;
    return true;
}

void albw_quick_equip_tick() {
    if (!feature_on()) {
        s_holdActive = false;
        s_holdFrames = 0;
        s_openedQuick = false;
        s_session = false;
        s_forceUpOnce = false;
        (void)albw_l1_held(PAD_1);  // keep edge tracker in sync while feature off
        return;
    }

    // End Z-only redirect when the ring closes — but not while force-open is still
    // pending (pause/heap are often still clear for 1+ frames after open_item_wheel).
    if (s_session && !s_forceUpOnce && !g_dComIfG_gameInfo.play.isPauseFlag() &&
        g_dComIfG_gameInfo.play.isHeapLockFlag() == 0)
    {
        s_session = false;
    }

    const bool pressed = l1_pressed();
    const bool down = l1_down();

    if (!can_open_wheel()) {
        if (!down) {
            s_holdActive = false;
            s_holdFrames = 0;
            s_openedQuick = false;
        }
        return;
    }

    if (pressed) {
        s_holdActive = true;
        s_holdFrames = 0;
        s_openedQuick = false;
    }

    if (s_holdActive && down) {
        if (s_holdFrames < 0xFFFF) {
            s_holdFrames++;
        }
        if (!s_openedQuick && s_holdFrames >= kQuickHoldFrames) {
            open_item_wheel(true);
            s_openedQuick = true;
        }
    } else if (s_holdActive && !down) {
        if (!s_openedQuick) {
            open_item_wheel(false);
        }
        s_holdActive = false;
        s_holdFrames = 0;
        s_openedQuick = false;
    }
}

ModResult albw_quick_equip_init(ModError* error) {
    if (!install(error, "QeSetSelectItemIndex",
                 mods::hook_add_pre<SetSelectItemIndex>(svc_hook, on_set_select_item_index_pre)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_quick_equip_shutdown(ModError*) {
    s_session = false;
    s_holdActive = false;
    s_forceUpOnce = false;
    mods::hook_uninstall<SetSelectItemIndex>(svc_hook);
    return MOD_OK;
}
