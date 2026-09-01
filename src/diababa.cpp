// Diababa Boss Refinement wiring — part of dev.albt.albw.
// See diababa.h for the reachability map and method reference.

#include "diababa.h"

#include "albw_common.h"
#include "albw_game.h"
#include "boss_refinement.h"
#include "config_vars.h"
#include "modules.h"

#include "d/actor/d_a_b_bq.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor.h"
#include "mods/svc/hook.hpp"

namespace {

// ============================================
// Fork constants, verbatim from d_a_b_bq.cpp:489 / :498.
// ============================================
constexpr s16 kAlbwBqHangHardFrames = 450;  // 15 seconds

// ============================================
// Uniquely-named file statics in d_a_b_bq.cpp / d_a_b_bh.cpp. The host resolves
// these through the symgen manifest, which carries non-exported statics; the
// plain (demangled display) name is used so one string works on every platform.
// ============================================
DEFINE_HOOK_SYMBOL("daB_BQ_Create", int(fopAc_ac_c*), BqCreate);
DEFINE_HOOK_SYMBOL("daB_BQ_Execute", int(b_bq_class*), BqExecute);
DEFINE_HOOK_SYMBOL("b_bq_damage", void(b_bq_class*), BqDamage);
DEFINE_HOOK_SYMBOL("b_bq_wait", void(b_bq_class*), BqWait);

// ---- b_bq_damage: record/restore state around vanilla ----------------------
// Fork sites in this function are all state corrections, so pre records and
// post re-applies. Vanilla is left to run untouched in between.
//
//   :13  refinement keeps the continuous HP bar; vanilla snaps health = 50
//   :39  same snap on the second damage-back path
//   :54  hang vulnerability window is a 15s hard cap, not l_HIO.mChanceTime
s16 s_healthBefore = 0;
s16 s_modeBefore = 0;
bool s_damageArmed = false;

HookAction on_bq_damage_pre(ModContext*, void* args, void*, void*) {
    if (!dAlbwBossRefinement_isEnabled()) {
        s_damageArmed = false;
        return HOOK_CONTINUE;
    }
    auto* self = mods::arg<b_bq_class*>(args, 0);
    if (self == nullptr) {
        s_damageArmed = false;
        return HOOK_CONTINUE;
    }
    s_healthBefore = self->health;
    s_modeBefore = self->mMode;
    s_damageArmed = true;
    return HOOK_CONTINUE;
}

void on_bq_damage_post(ModContext*, void* args, void*, void*) {
    if (!s_damageArmed) {
        return;
    }
    s_damageArmed = false;
    auto* self = mods::arg<b_bq_class*>(args, 0);
    if (self == nullptr) {
        return;
    }

    // Vanilla opens a fresh 50-HP core pool on hang. Refinement keeps the single
    // continuous bar - the bomb already chipped it via kAlbwDiababaBombReceiveDamage
    // - so undo the snap. Only when vanilla actually assigned exactly 50 from a
    // different value; a real 50 that was already there is left alone.
    if (self->health == 50 && s_healthBefore != 50) {
        self->health = s_healthBefore;
    }

    // Refinement replaces the vanilla chance window (l_HIO.mChanceTime) with a
    // 15 second hard cap. Vanilla sets mTimers[0] as it enters mMode 2, so only
    // rewrite on that transition.
    if (self->mMode == 2 && s_modeBefore != 2) {
        self->mTimers[0] = kAlbwBqHangHardFrames;
    }
}

// ---- daB_BQ_Execute: per-frame phase update --------------------------------
// Fork d_a_b_bq.cpp: dAlbwBoss_diababaUpdatePhase(a_this) each frame while
// refinement is on. Pre-hook so the phase is current for everything vanilla
// does this frame.
HookAction on_bq_execute_pre(ModContext*, void* args, void*, void*) {
    if (!dAlbwBossRefinement_isEnabled()) {
        return HOOK_CONTINUE;
    }
    auto* self = mods::arg<b_bq_class*>(args, 0);
    if (self != nullptr) {
        dAlbwBoss_diababaUpdatePhase(self);
    }
    return HOOK_CONTINUE;
}

// ---- daB_BQ_Create: fight state reset + refinement HP pool -----------------
// Fork sets health/field_0x560 to 100 (vanilla's 50 is the hang-half convention)
// and resets our fight state. Post-hook, so vanilla's own Create has finished
// writing the actor before we adjust it.
void on_bq_create_post(ModContext*, void* args, void*, void*) {
    if (!dAlbwBossRefinement_isEnabled()) {
        return;
    }
    auto* actor = mods::arg<fopAc_ac_c*>(args, 0);
    if (actor == nullptr) {
        return;
    }
    dAlbwBoss_diababaResetFightState();
    actor->health = 100;
    actor->field_0x560 = 100;
}

// ---- b_bq_wait: poison spray notification ----------------------------------
// Fork calls dAlbwBoss_diababaOnPoisonSprayBegin() on the wait->attack handoff
// (field_0x11fc != 0 && mTimers[2] == 1). Detect the same condition pre-call so
// our brain is armed before vanilla transitions.
HookAction on_bq_wait_pre(ModContext*, void* args, void*, void*) {
    if (!dAlbwBossRefinement_isEnabled()) {
        return HOOK_CONTINUE;
    }
    auto* self = mods::arg<b_bq_class*>(args, 0);
    if (self != nullptr && self->field_0x11fc != 0 && self->mTimers[2] == 1) {
        dAlbwBoss_diababaOnPoisonSprayBegin();
    }
    return HOOK_CONTINUE;
}

// A boss hook that fails to resolve must not abort mod_initialize - one boss
// going missing is not worth taking the whole collective down. Loud, not silent.
void try_install(const char* what, ModResult result) {
    if (result != MOD_OK) {
        // LogService takes a plain message, so name the hook in the string.
        svc_log->error(mod_ctx, what);
        svc_log->error(mod_ctx, "diababa: hook above did NOT install - that "
                                "refinement site is inactive this run");
    }
}

}  // namespace

ModResult albw_diababa_init(ModError*) {
    try_install("daB_BQ_Create", mods::hook::add_post<BqCreate>(on_bq_create_post));
    try_install("daB_BQ_Execute", mods::hook::add_pre<BqExecute>(on_bq_execute_pre));
    try_install("b_bq_damage:pre", mods::hook::add_pre<BqDamage>(on_bq_damage_pre));
    try_install("b_bq_damage:post", mods::hook::add_post<BqDamage>(on_bq_damage_post));
    try_install("b_bq_wait", mods::hook::add_pre<BqWait>(on_bq_wait_pre));
    svc_log->info(mod_ctx, "albw diababa refinement hooks ready");
    return MOD_OK;
}

ModResult albw_diababa_shutdown(ModError*) {
    s_damageArmed = false;
    return MOD_OK;
}
