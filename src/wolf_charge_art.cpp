// ============================================
// NEW CODE - ALBW Port (Wolf Art: Midna-charge attack gating)
//
// Fork d_a_alink_wolf.inc adds two gates inside procWolfRollAttackMove:
//   :7779 - at DOME FORMATION: without >=2 charges (and no open spend chain)
//     the Midna lock dome never forms, so holding the button behaves like
//     vanilla pre-Midna-charge wolf. Deny sound + HUD deny, once.
//   :7731 - at LAUNCH: opening a chain requires >=2 charges and SPENDS 1 per
//     hop; once open the chain may continue until charges hit 0. On
//     insufficient charges: deny + fall through to a normal roll attack.
//
// Both host sites are mid-function, but each sits immediately before a
// header-declared method - setWolfLockDomeModel() and procWolfLockAttackInit()
// - so pre-hooks on those land at the fork's exact insertion points.
// procWolfLockAttackInit is also the CHAIN-HOP entry (wolf.inc:8795, called
// from PROC_WOLF_LOCK_ATTACK's own move); the fork gates only the OPEN, so the
// hook passes hops through untouched by checking the current proc.
//
// State translation (same one the rest of the wolf module already uses): the
// fork's fork-added members mWolfChargeCount / mWolfSpendChainActive live
// module-side here (albw_wolf_* accessors / s_spendChainActive).
// ============================================

#include "global.h"
#include <os.h>

#include "d/d_com_inf_game.h"
#define private public
#include "d/actor/d_a_alink.h"
#undef private
#include "Z2AudioLib/Z2Instances.h"

#include "albw_common.h"
#include "wolf_combat.h"
#include "wolf_charge_hud.h"
#include "mods/hook.hpp"

#if TARGET_PC

namespace {

DEFINE_HOOK(&daAlink_c::setWolfLockDomeModel, SetWolfLockDomeModel);
DEFINE_HOOK(&daAlink_c::procWolfLockAttackInit, ProcWolfLockAttackInit);

// fork daAlink_c::mWolfSpendChainActive (fork-added member), module-side.
bool s_spendChainActive = false;

// fork wolf.inc:7779 - dome-formation gate.
HookAction on_set_wolf_lock_dome_model_pre(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || !link->checkWolf()) {
        return HOOK_CONTINUE;
    }
    if (dAlbwWolfCombat_isEnabled() && albw_wolf_get_charge_count() < 2 && !s_spendChainActive) {
        Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_USE_CANCEL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        dAlbwWolfChargeHud_notifyDeny();
        return HOOK_SKIP_ORIGINAL;  // fork: the else-branch skips setWolfLockDomeModel()
    }
    return HOOK_CONTINUE;
}

// fork wolf.inc:7731 - launch gate + per-hop spend.
HookAction on_proc_wolf_lock_attack_init_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || retval == nullptr || !dAlbwWolfCombat_isEnabled()) {
        return HOOK_CONTINUE;
    }
    // ============================================
    // HOP OVER-SPEND FIX: chain hops actually re-enter from
    // PROC_WOLF_LOCK_ATTACK_TURN (stock wolf.inc:8158/8211), NOT
    // PROC_WOLF_LOCK_ATTACK - the old skip never matched, so every hop was
    // gated AND spent 1 on top of the launch, truncating multi-target chains.
    // The fork hosts this logic inside procWolfRollAttackMove only (fork
    // wolf.inc:7733-7748), so gate exactly the OPEN and let every other caller
    // (hops included) pass free, matching the fork's spend-per-LAUNCH model.
    // ============================================
    if (link->mProcID != daAlink_c::PROC_WOLF_ROLL_ATTACK_MOVE) {
        return HOOK_CONTINUE;
    }

    const bool canOpen  = (albw_wolf_get_charge_count() >= 2);
    const bool canChain = s_spendChainActive;
    if (!canOpen && !canChain) {
        Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_USE_CANCEL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        dAlbwWolfChargeHud_notifyDeny();
        *static_cast<int*>(retval) = link->procWolfRollAttackInit(1, 0);
        return HOOK_SKIP_ORIGINAL;
    }
    if (!s_spendChainActive) {
        s_spendChainActive = true;
    }
    albw_wolf_spend_charge(1);
    if (albw_wolf_get_charge_count() == 0) {
        s_spendChainActive = false;
    }
    dAlbwWolfChargeHud_notify();
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

ModResult albw_wolf_charge_art_init(ModError* error) {
    if (!install(error, "WolfLockDomeGate",
                 mods::hook_add_pre<SetWolfLockDomeModel>(svc_hook, on_set_wolf_lock_dome_model_pre)) ||
        !install(error, "WolfLockAttackOpenGate",
                 mods::hook_add_pre<ProcWolfLockAttackInit>(svc_hook, on_proc_wolf_lock_attack_init_pre)))
    {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw wolf charge-art gates ready");
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
