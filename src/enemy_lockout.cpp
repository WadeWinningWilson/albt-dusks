// ============================================
// ALBW meter lockout — per-enemy slingshot ranged-open / stun wiring (Stage 2).
//
// The fork edits each enemy actor (d_a_e_*, d_a_b_tn) to consume the lockout
// slingshot debuff at the enemy's own targeting/damage sites — the central
// cc_at_check seam (reproduced in lockout.cpp) only applies the pause-based
// tag; these edits add the per-enemy "ranged-open vulnerability window" and the
// native-stun holds that pause alone does not give.
//
// Those enemy files are MODIFIED stock giants that port_tool cannot parse, so —
// per the modified-giant rule — each touched function is reproduced by hooking
// its symbol with HOOK_SKIP_ORIGINAL and running the fork body VERBATIM (never
// paraphrased). Pure top-of-function guards are reproduced as thin pre-hooks
// instead of replacing the whole body.
//
// Enemies wired here (incrementally):
//   E_WW (Deku Like / "ww")  — checkSideStep guard + damage_check whole-func.
// Pending (same pattern): E_OC, E_SM, B_TN (member fns); E_DN, E_ST (file-static
//   state fns — reproduced at the actor Execute seam).
// ============================================

#include "global.h"

#include "SSystem/SComponent/c_cc_d.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#include "d/actor/d_a_player.h"
#include "m_Do/m_Do_mtx.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"

#define private public
#include "d/actor/d_a_e_ww.h"
#undef private

#include "albw_common.h"
#include "enemy_lockout.h"
#include "lockout_port.h"
#include "meter_bridge.h"
#include "mods/hook.hpp"

#if TARGET_PC

namespace {

// File-local enum values reproduced from d_a_e_ww.cpp (defined in the .cpp, not
// the header, so the whole-func port cannot name them via the class).
constexpr int WW_ACTION_DAMAGE = 4;  // enum Action::ACTION_DAMAGE
constexpr int WW_ACTION_MODE_0 = 0;  // enum Action_Mode::ACTION_MODE_0
constexpr int WW_JNT_HEAD      = 4;  // enum Joint::JNT_HEAD

// ============================================
// E_WW — daE_WW_c::checkSideStep (top-of-function guard).
// Fork prepends: while ranged-opened the Deku Like never side-steps (stays open
// to the follow-up hit). Reproduced as a pre-hook — no whole-body replacement.
// ============================================
DEFINE_HOOK(&daE_WW_c::checkSideStep, WwCheckSideStep);

HookAction on_ww_check_side_step_pre(ModContext*, void* args, void* retval, void*) {
    auto* self = mods::arg<daE_WW_c*>(args, 0);
    if (self != nullptr && retval != nullptr && dAlbwLockout_isRangedOpened(self)) {
        *static_cast<bool*>(retval) = false;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

// ============================================
// E_WW — daE_WW_c::damage_check (whole-function replacement).
// Fork body VERBATIM (d_a_e_ww.cpp): the only fork change is the
// `if (!dAlbwLockout_isRangedOpened(this))` guard around the iron-ball block, but
// the modified-giant rule requires reproducing the whole function + SKIP_ORIGINAL.
// ============================================
DEFINE_HOOK(&daE_WW_c::damage_check, WwDamageCheck);

void ww_damage_check(daE_WW_c* self) {
    if (self->field_0x724 == 0) {
        self->mCcStts.Move();
        if (self->mSph1[1].ChkAtShieldHit() != 0) {
            self->mSph1[1].OffAtShieldHit();
            if (daPy_getPlayerActorClass()->checkPlayerGuard()) {
                self->setActionMode(WW_ACTION_DAMAGE, WW_ACTION_MODE_0);
                return;
            }
        }

        cCcD_Obj* var_r29 = NULL;  // ObjHit ? Where damage is applied ?
        if (self->mSph2[0].ChkTgHit() != 0) {
            var_r29 = self->mSph2[0].GetTgHitObj();
        }

        if (self->mSph2[1].ChkTgHit() != 0) {
            var_r29 = self->mSph2[1].GetTgHitObj();
        }

        if (var_r29 != NULL) {
            self->mAtInfo.mpCollider = var_r29;
            if (!dAlbwLockout_isRangedOpened(self))
            if (self->mAtInfo.mpCollider->ChkAtType(AT_TYPE_IRON_BALL) != 0) {
                if (fopAcM_GetName(dCc_GetAc(self->mAtInfo.mpCollider->GetAc())) ==
                    fpcNm_Obj_Carry_e) {
                    S16_ADD(self->health, 150);
                } else if (dComIfGp_checkPlayerStatus0(0, 0x400) != 0) {
                    S16_ADD(self->health, 180);
                } else {
                    self->health = 0;
                }
            }

            cc_at_check(self, &self->mAtInfo);
            if (self->mAtInfo.mpCollider->GetAtAtp() >= 1) {
                cXyz temp_r1;
                mDoMtx_stack_c::copy(self->mpModelMorf->getModel()->getAnmMtx(WW_JNT_HEAD));
                mDoMtx_stack_c::transM(-10.0f, -20.0f, 0.0f);
                mDoMtx_stack_c::multVecZero(&temp_r1);

                if (self->mAtInfo.mHitStatus == 0) {
                    dComIfGp_setHitMark(1, self, &temp_r1, NULL, NULL, 0);
                } else {
                    dComIfGp_setHitMark(3, self, &temp_r1, NULL, NULL, 0);
                }
            }

            if (self->mAtInfo.mpCollider->ChkAtType(AT_TYPE_UNK) != 0) {
                self->field_0x724 = 20;
            } else {
                self->field_0x724 = 10;
            }

            if (self->mAtInfo.mAttackPower <= 1) {
                self->field_0x724 = (u8)(KREG_S(8) + 10);
            }

            self->setActionMode(WW_ACTION_DAMAGE, WW_ACTION_MODE_0);
        }
    }
}

HookAction on_ww_damage_check_pre(ModContext*, void* args, void*, void*) {
    ww_damage_check(mods::arg<daE_WW_c*>(args, 0));
    return HOOK_SKIP_ORIGINAL;
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

ModResult albw_enemy_lockout_init(ModError* error) {
    if (!install(error, "WwCheckSideStep",
                 mods::hook_add_pre<WwCheckSideStep>(svc_hook, on_ww_check_side_step_pre)) ||
        !install(error, "WwDamageCheck",
                 mods::hook_add_pre<WwDamageCheck>(svc_hook, on_ww_damage_check_pre))) {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw enemy lockout (ww) ready");
    return MOD_OK;
}

#endif  // TARGET_PC
