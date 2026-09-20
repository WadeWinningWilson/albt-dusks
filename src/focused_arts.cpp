// ============================================
// NEW CODE — ALBW Port (Focused Arts — full module)
//
// The module body (tiers, bank/fill/spend state machine, special-finisher logic,
// melee/item damage resolution, debug overlay) is ported VERBATIM from the fork's
// src/d/d_focused_arts.cpp via tools/port/port_tool.py (config tools/port/
// focused_arts.json), included below as focused_arts_core.inc. Only the host ABI
// was substituted:
//   dusk::getSettings().game.focusedArts      -> albw_cfg_bool(g_focused_arts, false)
//   dusk::getSettings().game.focusedArtsCheat -> albw_cfg_int(g_focused_arts_cheat, 0)
//   dusk::FocusedArtsCheatMode::{Off,On,WithDebug,OnMaxBank,WithDebugMaxBank} -> 0..4
//   dMeter2_isALBWLocked()        -> albw_meter_is_locked()
//   dMeter2_drainALBWToLockout()  -> albw_meter_drain_to_lockout()
//
// Consumption wiring is below the include: the fork applies the module's outputs
// inside cc_at_check, so this reproduces that at the mod's cc_at_check seam
// (melee resolve + EB great-spin AOE + item damage boost + sword/item fill). The
// mod's stock cc_at_check already applies the vanilla sword multipliers, so FA
// resolve runs POST (scaling the computed power before the enemy consumes it),
// matching the fork order. FA melee resolve is human-form-only (!checkNowWolf),
// so it does not collide with wolf_combat's wolf-form power overrides.
//
// DEFERRED (Group C — needs alink-side actor spawning; state is ported and live):
//   the finisher *visual* launches — GS hurricane proc + Ending-Blow great-spin
//   AOE actor spawn (fork d_a_alink_hurricane.inc / d_a_alink_cut.inc). The
//   finisher STATE + damage all resolve; only the spawn flourish is pending.
//   Also pending: meter.cpp passes CUT_TYPE_TWIRL to onHiddenSkillProcStarted for
//   every large-turn skill; per-skill cutType precision (JS/MD/Helm/GS finisher
//   effects) is a meter-lane follow-up.
// ============================================

#include "global.h"

#include "focused_arts.h"

#include "albw_common.h"
#include "albw_game.h"
#include "config_vars.h"
#include "meter_bridge.h"
#include "outfit_stats.h"
#include "shield.h"
#include "shield_adapt.h"
#include "wolf_combat.h"
#include "mods/hook.hpp"

#include <algorithm>
#include <cstdio>

#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_save.h"
#include "SSystem/SComponent/c_cc_d.h"
#include "f_op/f_op_actor_mng.h"
#include "d/actor/d_a_player.h"

#define private public
#include "d/actor/d_a_alink.h"
#undef private

#if TARGET_PC

// Dev trace macro the fork body calls; a no-op here (the mod has no conav trace).
#ifndef CONAV_LOG
#define CONAV_LOG(tag, ...) ((void)0)
#endif

// port_tool drops the fork's forward declaration of this file-static; restore it
// (logFaEvent is used by helpers defined above its own definition).
static void logFaEvent(const char* i_msg);

// The mechanically-extracted module (statics + every dFocusedArts_* function),
// verbatim from the fork with the ABI substitutions above. File-static helpers
// keep internal linkage; the public dFocusedArts_* keep external linkage (declared
// in focused_arts.h) — so this sits at file scope, NOT inside an anon namespace.
#include "focused_arts_core.inc"

namespace {

// ============================================
// Consumption wiring (reproduces the fork's cc_at_check FA hooks).
// ============================================
DEFINE_HOOK(cc_at_check, FaCcAtCheck);
// fork calls dFocusedArts_onHiddenSkillChargeStart() ONLY from procCutLargeJumpChargeInit
// (d_a_alink_cut.inc:2732 - the Jump Strike hidden-skill charge). Hooking the turn/spin
// charge instead drained fill during ordinary sword combos, which pass through it.
DEFINE_HOOK(&daAlink_c::procCutLargeJumpChargeInit, CutLargeJumpChargeInit);
DEFINE_HOOK(&daAlink_c::procDamageInit, ProcDamageInit);

void on_cut_large_jump_charge_post(ModContext*, void*, void*, void*) {
    if (dFocusedArts_isEnabled() && dAlbw_isHiddenSkillReworkEnabled()) {
        dFocusedArts_onHiddenSkillChargeStart();
    }
}

void on_damage_init_post(ModContext*, void*, void*, void*) {
    if (dFocusedArts_isEnabled()) {
        dFocusedArts_onDamageTaken();
    }
}

// Post-hook: vanilla cc_at_check has already applied the sword multipliers, so we
// scale the resolved power (fork order) before the enemy consumes it.
void on_cc_at_check_post(ModContext*, void* args, void*, void*) {
    if (!dFocusedArts_isEnabled()) {
        return;
    }

    auto* enemy = mods::arg<fopAc_ac_c*>(args, 0);
    auto* info = mods::arg<dCcU_AtInfo*>(args, 1);
    if (enemy == nullptr || info == nullptr || info->mpCollider == nullptr) {
        return;
    }

    // --- FA melee damage resolve: human-form Link normal attacks (hidden skills). ---
    if (info->mHitType == HIT_TYPE_LINK_NORMAL_ATTACK && !daPy_py_c::checkNowWolf()) {
        daPy_py_c* player = daPy_getPlayerActorClass();
        if (player != nullptr) {
            const int faCutType = player->getCutType();
            const u16 faVanillaPower = info->mAttackPower;
            info->mAttackPower =
                dFocusedArts_resolveMeleeDamage(info->mAttackPower, faCutType);
            // ============================================
            // TEMP DIAG — FA-HSDMG (hidden-skill damage scaling). STRIP before release.
            // For each hidden-skill hit, logs the tier state and vanilla->resolved power so we
            // can see whether the bank/tier decrease actually applies at the moment of the hit
            // (tierSteps = s_bankCount while building, = maxBank during a spend/maintain window).
            // ============================================
            if (isHiddenSkillCutType(faCutType) && svc_log != nullptr) {
                char faBuf[224];
                std::snprintf(faBuf, sizeof(faBuf),
                    "[FA-HSDMG] cut=%d insta=%d tierSteps=%d bank=%d maxBank=%d spend=%d "
                    "maint=%d fnb=%d van=%u res=%u",
                    faCutType, static_cast<int>(isInstaKillFinisherCutType(faCutType)),
                    getDamageTierSteps(), static_cast<int>(s_bankCount),
                    dFocusedArts_getMaxBank(), static_cast<int>(s_inSpendSequence),
                    static_cast<int>(s_maintainStackedFrames),
                    static_cast<int>(s_forceNewBaseDamage),
                    static_cast<unsigned>(faVanillaPower),
                    static_cast<unsigned>(info->mAttackPower));
                svc_log->info(mod_ctx, faBuf);
            }

            // --- Ending Blow -> Great Spin AOE: the ALINK atSph carries the AOE. ---
            if (dFocusedArts_isEndingBlowGreatSpinAoeActive()) {
                daAlink_c* link = static_cast<daAlink_c*>(player);
                if (info->mpCollider == static_cast<cCcD_Obj*>(&link->mAtSph)) {
                    info->mAttackPower =
                        dFocusedArts_getEndingBlowGreatSpinAoePower(info->mAttackPower);
                }
            }

            // --- Outfit outgoing damage mult (Sumo offensive kit) — fork cc_at_check:488,
            //     applied after the FA/EB resolve. Self-gates on the sumo kit (human-only),
            //     so it is inert unless that outfit is worn. ---
            info->mAttackPower = dAlbwOutfitStats_applyOutgoingDamageMult(info->mAttackPower);
        }
    }

    // --- item damage boost (arrow/bomb/iron ball/slingshot/spinner). Safe to call
    //     broadly: the boost self-gates on bank/spend state. Lockout's own item
    //     boost is applied separately by lockout.cpp. ---
    if (info->mAttackPower > 0 &&
        info->mpCollider->ChkAtType(AT_TYPE_ARROW | AT_TYPE_BOMB | AT_TYPE_IRON_BALL |
                                    AT_TYPE_SLINGSHOT | AT_TYPE_SPINNER))
    {
        dFocusedArts_applyItemDamageBoost(info->mAttackPower);
    }

    // ============================================
    // --- fill ---
    // SWORD fill is NOT a cc_at_check site in the fork. Its donor host is
    // daAlink_c::setSwordHitVibration (d_a_alink_cut.inc:367-370), where it is one of
    // two additions in a single hunk whose other half the mod already ports (the
    // magic-armor encounter registration). It has been returned there — see
    // meter.cpp on_sword_hit_vibration_post — because the donor's three conditions
    // (enemy group, !checkWolf, !ChkAtShieldHit) have no equivalent at this seam: here
    // every sword contact filled, pots and blocked hits included, which is what let a
    // freshly-emptied bank refill from a single multi-hit swing.
    //
    // ITEM fill IS a cc_at_check site in the fork (d_cc_uty.cpp:648-671) — kept, plus
    // the donor's enclosing enemy-group gate (:648), which was dropped here.
    // ============================================
    if (fopAcM_GetGroup(enemy) == fopAc_ENEMY_e &&
        !info->mpCollider->ChkAtType(AT_TYPE_NORMAL_SWORD) &&
        !info->mpCollider->ChkAtType(AT_TYPE_MASTER_SWORD) &&
        !info->mpCollider->ChkAtType(AT_TYPE_WOLF_ATTACK) &&
        !info->mpCollider->ChkAtType(AT_TYPE_WOLF_CUT_TURN) &&
        !info->mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK) &&
        info->mpCollider->ChkAtType(AT_TYPE_ARROW | AT_TYPE_BOMB | AT_TYPE_SLINGSHOT |
                                    AT_TYPE_IRON_BALL | AT_TYPE_40))
    {
        dFocusedArts_onConnectedItemHit();
    }
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

ModResult albw_focused_arts_init(ModError* error) {
    if (!install(error, "FaCcAtCheckPost",
                 mods::hook_add_post<FaCcAtCheck>(svc_hook, on_cc_at_check_post)) ||
        !install(error, "FaCutLargeJumpChargePost",
                 mods::hook_add_post<CutLargeJumpChargeInit>(svc_hook, on_cut_large_jump_charge_post)) ||
        !install(error, "FaDamageInitPost",
                 mods::hook_add_post<ProcDamageInit>(svc_hook, on_damage_init_post)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_focused_arts_shutdown(ModError*) {
    return MOD_OK;
}

void albw_focused_arts_tick() {
    dFocusedArts_update();
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
