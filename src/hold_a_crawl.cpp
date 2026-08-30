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

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
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
