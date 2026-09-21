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

// ============================================
// MODIFIED CODE - ALBW Port (Focused Arts: hit-reset seam correction)
//
// FORK SEAM. dFocusedArts_onDamageTaken() is called from exactly ONE place in
// the fork: daAlink_c::setDamagePoint, d_a_alink_damage.inc:239-241 - inside
// the life-deducting else-branch, AFTER the DOUBLE_DEFENSE halving (:236-238),
// gated `if (i_dmgAmount > 0)`, immediately before
// dComIfGp_setItemLifeCount(-i_dmgAmount, 0) (:243). Its sibling
// dParryMaster_onHpLoss sits under the identical gate three lines later
// (:245-247). What it resets: the CURRENT TIER'S PARTIAL FILL ONLY -
// clearFillProgress() zeroes s_fillNumerator and nothing else (fork
// d_focused_arts.cpp:336-338, called at :739). Banked charges, the purchased
// tier and every timer survive. There is no separate idle/decay timeout
// anywhere in the fork module.
//
// WHAT THIS REPLACES. A post-hook on daAlink_c::procDamageInit. That is not
// the fork's seam and it is wrong in BOTH directions:
//
//  * UNDER-fires. procDamageInit is only ONE of the damage REACTIONS that
//    checkDamageAction dispatches AFTER it has already called setDamagePoint
//    (fork d_a_alink_damage.inc:863/866/871). Every other reaction left the
//    fill intact: procCoLargeDamageInit (:934 and :952 - every large/huge
//    attack, i.e. most real hits), procWolfDamageInit (:959),
//    procSwimDamageInit (:929), procCoElecDamageInit (:902),
//    procHorseDamageInit / procHorseHangInit (:920 / :922), setDashDamage
//    (:955), setWolfHeadDamage (:957), the no-reaction 0x4000000 arm
//    (:894-900) and procCoPolyDamageInit (:659 / :666) - plus every
//    setDamagePoint caller outside checkDamageAction entirely: fall/land
//    damage (:101, :103, :1770, :1778), electric return damage (:670), enemy
//    grab (d_a_alink_grab.inc:2047/2049), swim (d_a_alink_swim.inc:1685),
//    scene damage (d_a_alink.cpp:5479), the field_0x318c throw settle
//    (d_a_alink.cpp:18998) and daPy_py_c::setPlayerDamage
//    (d_a_player.cpp:581).
//
//  * OVER-fires. procDamageInit also runs with NO life lost:
//    d_a_alink_wolf.inc:1994 (checkWolfBarrierHitReverse stagger - no
//    setDamagePoint anywhere on that path), and the armor-absorbed ICE arm
//    (fork :953 `!armor_no_dmg || at_mtrl == dCcD_MTRL_ICE` -> :961), which
//    wiped fill on a hit the Magic Armor had fully paid for.
//
// WHY A TRANSLATION AND NOT THE FORK'S OWN LINE (DN-10 order of resort,
// step 2). The fork's call sits inside a setDamagePoint body the mod cannot
// edit, and the whole-function replacement route is already refused IN WRITING
// for this exact symbol - see the ALBW Magic Armor block in meter.cpp:
// dusk::AchievementSystem is not in dusklight_exports.def, and setDamagePoint
// already carries composed pre/post hooks from two modules (meter.cpp:233,
// region_port.cpp:122). So the fork's GATE is reproduced at that same seam.
//
// THE GATE IS MEASURED, NOT RE-DERIVED. The fork's condition is exactly "the
// life-deducting branch ran with a positive amount". setItemLifeCount is a
// plain accumulator (`mItemInfo.mItemLifeCount += hearts`,
// d_com_inf_game.h:611-614), so snapshotting it across the call and testing
// delta < 0 answers that question without duplicating damageMagnification,
// the checkMagicArmorNoDamage branch or the DOUBLE_DEFENSE halving - three
// pieces of host logic that would silently drift out of sync. This is the
// same technique meter.cpp already uses on the rupee accumulator for this very
// function. Coverage check: the heal path (fork :188-191) queues a POSITIVE
// amount -> delta >= 0, no fire; an armor-absorbed hit queues RUPEES, not life
// -> delta == 0, no fire; a DEBUG invincible build skips the whole block, and
// so does the fork's own call.
//
// PLACEMENT (a post-hook is a different position from the fork's line, so it
// must be argued, not assumed). The fork calls onDamageTaken immediately
// BEFORE setItemLifeCount; this fires after the body returns. The call's only
// effect is a write to the file-static s_fillNumerator; nothing in the
// remainder of setDamagePoint reads FA state, and no frame boundary is
// crossed. The two other modules on this symbol touch the rupee accumulator
// (meter.cpp) and the region damage-scale scope (region_port.cpp), never the
// life accumulator - so the measured delta is attributable to the host body
// alone, whatever order the hooks run in.
//
// Toggle off == provably stock: both halves sit behind dFocusedArts_isEnabled()
// and neither writes any host state in either state.
// ============================================
DEFINE_HOOK(&daAlink_c::setDamagePoint, FaSetDamagePoint);

f32 s_fa_life_queue_snap = 0.0f;
bool s_fa_damage_armed = false;

void on_cut_large_jump_charge_post(ModContext*, void*, void*, void*) {
    if (dFocusedArts_isEnabled() && dAlbw_isHiddenSkillReworkEnabled()) {
        dFocusedArts_onHiddenSkillChargeStart();
    }
}

HookAction on_set_damage_point_pre(ModContext*, void*, void*, void*) {
    s_fa_damage_armed = false;
    if (!dFocusedArts_isEnabled()) {
        return HOOK_CONTINUE;
    }
    s_fa_life_queue_snap = g_dComIfG_gameInfo.play.getItemLifeCount();
    s_fa_damage_armed = true;
    return HOOK_CONTINUE;
}

void on_set_damage_point_post(ModContext*, void*, void*, void*) {
    if (!s_fa_damage_armed) {
        return;
    }
    s_fa_damage_armed = false;
    // delta < 0 <=> the host ran `dComIfGp_setItemLifeCount(-i_dmgAmount, 0)`
    // with i_dmgAmount > 0, i.e. exactly the branch the fork's call sits in.
    const f32 delta = g_dComIfG_gameInfo.play.getItemLifeCount() - s_fa_life_queue_snap;
    if (delta < 0.0f) {
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

    // --- fill: sword hits fill by sword step; item hits fill by the item step. ---
    if (info->mpCollider->ChkAtType(AT_TYPE_NORMAL_SWORD | AT_TYPE_MASTER_SWORD)) {
        dFocusedArts_onConnectedSwordHit();
    } else if (!info->mpCollider->ChkAtType(AT_TYPE_WOLF_ATTACK) &&
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
        !install(error, "FaSetDamagePointPre",
                 mods::hook_add_pre<FaSetDamagePoint>(svc_hook, on_set_damage_point_pre)) ||
        !install(error, "FaSetDamagePointPost",
                 mods::hook_add_post<FaSetDamagePoint>(svc_hook, on_set_damage_point_post)))
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
