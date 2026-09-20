#include "global.h"

#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_controller_pad.h"

#define private public
#include "d/actor/d_a_alink.h"
#undef private

#include "albw_common.h"
#include "config_vars.h"
#include "hold_a_crawl.h"
#include "mods/hook.hpp"

namespace {

DEFINE_HOOK(&daAlink_c::checkNormalAction, CheckNormalAction);

HookAction on_check_normal_action_pre(ModContext*, void* args, void* retval, void*) {
    if (!albw_cfg_bool(g_hold_a_crawl, false)) {
        return HOOK_CONTINUE;
    }

    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }

    static u16 s_hold_frames = 0;
    static bool s_hold_latched = false;
    constexpr u16 kHoldFrames = 30;

    const bool a_down = link->doButton();
    const bool gates =
        !link->checkWolf() && !link->checkEventRun() && link->mLinkAcch.ChkGroundHit() &&
        g_dComIfG_gameInfo.play.getDoStatus() == BUTTON_STATUS_NONE &&
        link->mStickValue <= link->getFrontRollRate() &&
        (link->mProcID == daAlink_c::PROC_WAIT || link->mProcID == daAlink_c::PROC_TIRED_WAIT ||
         link->mProcID == daAlink_c::PROC_SERVICE_WAIT);

    if (!a_down) {
        s_hold_frames = 0;
        s_hold_latched = false;
    } else if (!gates) {
        s_hold_frames = 0;
    } else if (!s_hold_latched) {
        if (s_hold_frames < 0xFFFF) {
            s_hold_frames++;
        }
        if (s_hold_frames >= kHoldFrames) {
            s_hold_latched = true;
            s_hold_frames = 0;
            link->field_0x306e = static_cast<s16>(link->shape_angle.y + 0x8000);
            link->field_0x34ec = link->current.pos;
            link->field_0x34ec.x -= 35.0f * cM_ssin(link->field_0x306e);
            link->field_0x34ec.z -= 35.0f * cM_scos(link->field_0x306e);
            link->procCrawlStartInit();
            if (retval != nullptr) {
                *static_cast<int*>(retval) = 1;
            }
            return HOOK_SKIP_ORIGINAL;
        }
    }

    return HOOK_CONTINUE;
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

ModResult albw_hold_a_crawl_init(ModError* error) {
    if (!install(error, "CheckNormalActionPre",
                 mods::hook_add_pre<CheckNormalAction>(svc_hook, on_check_normal_action_pre)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_hold_a_crawl_shutdown(ModError*) {
    return MOD_OK;
}
