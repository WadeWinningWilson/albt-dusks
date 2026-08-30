#include "flurry_rush.h"

#include "albw_common.h"
#include "sim_time_scale.h"
#include "mods/hook.hpp"

#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/actor/d_a_player.h"
#define private public
#include "d/actor/d_a_alink.h"
#undef private
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"

namespace {

DEFINE_HOOK(&daAlink_c::procSideStepInit, ProcSideStepInit);
DEFINE_HOOK(&daAlink_c::procBackJumpInit, ProcBackJumpInit);
DEFINE_HOOK(&daAlink_c::procSideStepLandInit, ProcSideStepLandInit);
DEFINE_HOOK(&daAlink_c::procBackJumpLandInit, ProcBackJumpLandInit);
DEFINE_HOOK(&daAlink_c::swordSwingTrigger, SwordSwingTrigger);
DEFINE_HOOK(fopAcM_posMove, FopAcMPosMove);
DEFINE_HOOK(cc_at_check, FlurryCcAtCheck);

void on_proc_side_step_init_post(ModContext*, void* args, void*, void*) {
    if (!dFlurryRush_isEnabled()) {
        return;
    }
    const int dir = mods::arg<int>(args, 1);
    if (dir != daAlink_c::DIR_BACKWARD) {
        dFlurryRush_tryPerfectDodge(dFlurryPerfectDodge_SideStep);
    }
}

void on_proc_back_jump_init_post(ModContext*, void* args, void*, void*) {
    if (!dFlurryRush_isEnabled()) {
        return;
    }
    const int param = mods::arg<int>(args, 1);
    if (param == 0) {
        dFlurryRush_tryPerfectDodge(dFlurryPerfectDodge_BackJump);
    }
}

void try_arm_flurry_after_dodge() {
    dFlurryRush_tryEnterFromDodge();
}

void on_proc_side_step_land_init_post(ModContext*, void*, void*, void*) {
    try_arm_flurry_after_dodge();
}

void on_proc_back_jump_land_init_post(ModContext*, void*, void*, void*) {
    try_arm_flurry_after_dodge();
}

void on_sword_swing_trigger_post(ModContext*, void* args, void* retval, void*) {
    if (!dFlurryRush_isActive()) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || retval == nullptr || *static_cast<BOOL*>(retval) == FALSE) {
        return;
    }
    dFlurryRush_onAttackStarted();
}

HookAction on_fop_ac_m_pos_move_pre(ModContext*, void* args, void*, void*) {
    const float scale = albw::get_sim_time_scale();
    if (scale >= 0.999f) {
        return HOOK_CONTINUE;
    }

    auto* actor = mods::arg<fopAc_ac_c*>(args, 0);
    if (actor != nullptr && fopAcM_GetName(actor) == fpcNm_ALINK_e) {
        return HOOK_CONTINUE;
    }

    cXyz* speed = fopAcM_GetSpeed_p(actor);
    if (speed != nullptr) {
        speed->x *= scale;
        speed->y *= scale;
        speed->z *= scale;
    }
    return HOOK_CONTINUE;
}

void on_fop_ac_m_pos_move_post(ModContext*, void* args, void*, void*) {
    const float scale = albw::get_sim_time_scale();
    if (scale >= 0.999f) {
        return;
    }

    auto* actor = mods::arg<fopAc_ac_c*>(args, 0);
    if (actor != nullptr && fopAcM_GetName(actor) == fpcNm_ALINK_e) {
        return;
    }

    cXyz* speed = fopAcM_GetSpeed_p(actor);
    if (speed != nullptr && scale > 0.0001f) {
        speed->x /= scale;
        speed->y /= scale;
        speed->z /= scale;
    }
}

void on_flurry_cc_at_check_post(ModContext*, void* args, void*, void*) {
    if (!dFlurryRush_isActive()) {
        return;
    }
    auto* enemy = mods::arg<fopAc_ac_c*>(args, 0);
    auto* info = mods::arg<dCcU_AtInfo*>(args, 1);
    if (enemy != nullptr && info != nullptr && info->mHitBit != 0) {
        dFlurryRush_onHitLanded(enemy);
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

ModResult albw_flurry_hooks_init(ModError* error) {
    if (!install(error, "FlurrySideStepPost",
                 mods::hook_add_post<ProcSideStepInit>(svc_hook, on_proc_side_step_init_post)) ||
        !install(error, "FlurryBackJumpPost",
                 mods::hook_add_post<ProcBackJumpInit>(svc_hook, on_proc_back_jump_init_post)) ||
        !install(error, "FlurrySideLandPost",
                 mods::hook_add_post<ProcSideStepLandInit>(svc_hook,
                                                           on_proc_side_step_land_init_post)) ||
        !install(error, "FlurryBackLandPost",
                 mods::hook_add_post<ProcBackJumpLandInit>(svc_hook,
                                                           on_proc_back_jump_land_init_post)) ||
        !install(error, "FlurrySwingPost",
                 mods::hook_add_post<SwordSwingTrigger>(svc_hook, on_sword_swing_trigger_post)) ||
        !install(error, "FlurryPosMovePre",
                 mods::hook_add_pre<FopAcMPosMove>(svc_hook, on_fop_ac_m_pos_move_pre)) ||
        !install(error, "FlurryPosMovePost",
                 mods::hook_add_post<FopAcMPosMove>(svc_hook, on_fop_ac_m_pos_move_post)) ||
        !install(error, "FlurryCcPost",
                 mods::hook_add_post<FlurryCcAtCheck>(svc_hook, on_flurry_cc_at_check_post)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_flurry_hooks_shutdown(ModError*) {
    return MOD_OK;
}
