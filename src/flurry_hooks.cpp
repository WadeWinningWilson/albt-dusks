#include "flurry_rush.h"

#include "albw_common.h"
#include "flurry_proc.h"
#include "devil_trigger.h"  // Devil Trigger shares this movement seam
#include "sim_time_scale.h"
#include "mods/hook.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"  // dMeter2Info_setSword - the sword-change seam
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
DEFINE_HOOK(fopAcM_posMove, FopAcMPosMove);
DEFINE_HOOK(dMeter2Info_setSword, Meter2InfoSetSword);

// Captured in PRE so POST divides by exactly what PRE multiplied by, even if
// the actor armed or a swing started between the two.
float s_posMoveScale = 1.0f;

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

// ============================================
// PROC ENTRY - PORTED, and moved from POST to PRE.
//
// Donor, the FIRST STATEMENT of both land procs:
//     fork d_a_alink.cpp:17613-17620   procSideStepLandInit
//     fork d_a_alink.cpp:18377-18386   procBackJumpLandInit
//
//     if (dFlurryRush_tryEnterProcFromPerfectDodge()) {
//         const int flurryResult = procFlurryRushInit();
//         if (flurryResult != 0) {
//             return flurryResult;
//         }
//         dFlurryRush_end(dFlurryRushEnd_Interrupt);
//     }
//     commonProcInit(PROC_..._LAND);   <- donor never reaches this on success
//
// POSITION - and why the old POST hook was the WRONG position, not merely a
// later one. The donor RETURNS before commonProcInit, so on a successful
// entry the land proc never runs at all. A POST hook let the whole land init
// run first - commonProcInit(PROC_BACK_JUMP_LAND), setSingleAnimeParam,
// mNormalSpeed = 0, the foot effect - and only then armed the rush; the
// overlay's own commonProcInit would then have torn that half-started proc
// down from underneath itself. PRE + SKIP_ORIGINAL with the donor's return
// value in retval is the donor's position exactly: nothing of the land proc
// executes, and the value the engine reads is the value the donor returned.
//
// On failure (procFlurryRushInit returned 0 - not armed, Link's proc
// untouched, the donor's own contract) the hook falls through to the stock
// land proc, which is what the donor's fall-through does.
// ============================================
HookAction on_flurry_land_init_pre(void* args, void* retval) {
    if (!dFlurryRush_tryEnterFromDodge()) {
        return HOOK_CONTINUE;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    const int flurryResult = albw_flurry_proc_try_enter(link);
    if (flurryResult != 0) {
        if (retval != nullptr) {
            *static_cast<int*>(retval) = flurryResult;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    dFlurryRush_end(dFlurryRushEnd_Interrupt);
    return HOOK_CONTINUE;
}

HookAction on_proc_side_step_land_init_pre(ModContext*, void* args, void* retval, void*) {
    return on_flurry_land_init_pre(args, retval);
}

HookAction on_proc_back_jump_land_init_pre(ModContext*, void* args, void* retval, void*) {
    return on_flurry_land_init_pre(args, retval);
}

// ============================================
// Two features share this seam. Flurry Rush slows the WORLD (world factor
// below 1.0, Link exempt); Devil Trigger speeds ONE armed actor (boost above
// 1.0). They compose by multiplication - an enraged enemy inside a flurry
// window is still slowed by it. Direct scaling drives DT movement HERE (not
// by re-running execute), so it pairs with the animation boost in
// btn_parry.cpp and there is no double-count.
// ============================================
HookAction on_fop_ac_m_pos_move_pre(ModContext*, void* args, void*, void*) {
    auto* actor = mods::arg<fopAc_ac_c*>(args, 0);
    if (actor != nullptr && fopAcM_GetName(actor) == fpcNm_ALINK_e) {
        return HOOK_CONTINUE;
    }

    const float scale = albw::get_sim_time_scale() * dAlbwDevil_boostFor(actor);
    if (scale >= 0.999f && scale <= 1.001f) {
        return HOOK_CONTINUE;
    }
    s_posMoveScale = scale;

    cXyz* speed = fopAcM_GetSpeed_p(actor);
    if (speed != nullptr) {
        speed->x *= scale;
        speed->y *= scale;
        speed->z *= scale;
    }
    return HOOK_CONTINUE;
}

void on_fop_ac_m_pos_move_post(ModContext*, void* args, void*, void*) {
    const float scale = s_posMoveScale;
    s_posMoveScale = 1.0f;
    if (scale >= 0.999f && scale <= 1.001f) {
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

// ============================================
// RETIRED with the proc port - two receiver stand-ins the donor replaces.
//
// 1. swordSwingTrigger POST -> dFlurryRush_onAttackStarted().
//    In the donor, onAttackStarted has exactly ONE gameplay caller:
//    flurryBeginSwing(0) (fork d_a_alink_flurry.inc:128-130), i.e. the moment
//    the FIRST swing actually begins. The hook fired on any sword-swing
//    trigger while a rush was pending, so hasStartedAttack went true before
//    Link had swung - which both cleared the 2s start gate early and, with
//    the donor's update tail now live, would immediately report "started but
//    not in the rush proc" and interrupt.
//
// 2. cc_at_check POST -> dFlurryRush_onHitLanded(enemy).
//    The donor registers hits from inside the proc, via flurryCheckSwordHit
//    (fork .inc:133-154) guarded by mProcVar3.field_0x300e so at most one hit
//    counts per swing, and it is that function - not a collision callback -
//    that carries the slow-mo fallback. Leaving the collision hook installed
//    would double-count every swing that also resolved through the AT
//    primitives, and would keep counting hits landed outside the attack
//    window.
//
// Nothing else called either callback, so both hooks and their DEFINE_HOOK
// entries are gone rather than left dormant.
// ============================================

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
// ============================================
// NEW CODE - ALBW Port (cancel a live rush when the sword changes under it)
//
// FORK SPEC: d_meter2_info.cpp:1732-1737, inside dMeter2Info_setSword, just
// above the two setters:
//     const u8 prevSword = dComIfGs_getSelectEquipSword();
//     if (prevSword != i_itemId) dFlurryRush_cancelOnSwordEquipChange();
//     dComIfGs_setSelectEquipSword(i_itemId);
//
// POSITION: PRE is the donor's position here, not a convenience. The donor
// reads the OLD equipped sword, and the only statement ahead of its insert
// that touches save state is offItemFirstBit, which clears an item's
// first-acquire bit and does not change the equipped sword - so a pre-hook
// observes exactly the value the donor observes. Post would read the NEW
// sword and the comparison would always be equal.
//
// The id validation is reproduced because the donor compares against the
// SANITISED id: the function's leading switch rewrites an unrecognised id to
// dItemNo_NONE_e before the comparison runs. A pre-hook sees the raw
// argument, so without this an invalid id would compare differently than in
// the donor. Error path only, but it is free to be exact.
//
// This writes nothing - it only reads the equipped sword and may end a rush.
// ============================================
HookAction on_meter2_info_set_sword_pre(ModContext*, void* args, void*, void*) {
    if (!dFlurryRush_isActive()) {
        return HOOK_CONTINUE;
    }
    u8 itemId = mods::arg<u8>(args, 0);
    switch (itemId) {
    case dItemNo_NONE_e:
    case dItemNo_WOOD_STICK_e:
    case dItemNo_SWORD_e:
    case dItemNo_MASTER_SWORD_e:
    case dItemNo_LIGHT_SWORD_e:
        break;
    default:
        itemId = dItemNo_NONE_e;
        break;
    }

    if (dComIfGs_getSelectEquipSword() != itemId) {
        dFlurryRush_cancelOnSwordEquipChange();
    }
    // Always CONTINUE: the donor leaves the two setters below to run.
    return HOOK_CONTINUE;
}

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

ModResult albw_flurry_hooks_init(ModError* error) {
    if (!install(error, "FlurrySideStepPost",
                 mods::hook_add_post<ProcSideStepInit>(svc_hook, on_proc_side_step_init_post)) ||
        !install(error, "FlurryBackJumpPost",
                 mods::hook_add_post<ProcBackJumpInit>(svc_hook, on_proc_back_jump_init_post)) ||
        !install(error, "FlurrySideLandPre",
                 mods::hook_add_pre<ProcSideStepLandInit>(svc_hook,
                                                          on_proc_side_step_land_init_pre)) ||
        !install(error, "FlurryBackLandPre",
                 mods::hook_add_pre<ProcBackJumpLandInit>(svc_hook,
                                                          on_proc_back_jump_land_init_pre)) ||
        !install(error, "FlurryPosMovePre",
                 mods::hook_add_pre<FopAcMPosMove>(svc_hook, on_fop_ac_m_pos_move_pre)) ||
        !install(error, "FlurryPosMovePost",
                 mods::hook_add_post<FopAcMPosMove>(svc_hook, on_fop_ac_m_pos_move_post)) ||
        !install(error, "FlurrySwordEquipChangePre",
                 mods::hook_add_pre<Meter2InfoSetSword>(svc_hook, on_meter2_info_set_sword_pre)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_flurry_hooks_shutdown(ModError*) {
    return MOD_OK;
}
