// ============================================
// NEW CODE — ALBW Port — "Midna's Shield" (wolf guard / parry) — hooks
//
// Slice 1 (parry-first). Two hooks on daAlink_c:
//   * execute (POST)            — advance the shared parry window in wolf form
//                                 (the human updateGuardTracking never runs here)
//                                 and tick the Midna visual placeholder.
//   * checkDamageAction (PRE)   — the wolf parry seam. The human guard/parry
//                                 sub-branch of checkDamageAction only runs when
//                                 human Link guards, so the wolf falls through to
//                                 procDamageInit. This PRE hook reproduces the
//                                 function's own hit-collider scan, then reuses
//                                 the SHARED parry engine (dShield_onShieldHit),
//                                 which is now wolf-aware via guardRaised().
//
// On a successful parry we clear the consumed collider so stock's checkDamageAction
// takes its no-hit (no-damage) path — clearing state, never faking the return, so
// the dangling-collider class of bug (the DT crash) stays off the table.
//
// Design + architecture: docs/WOLF-GUARD-SCOPE.md §8b.
// ============================================

#include "global.h"

#include "mods/hook.hpp"
#include "d/d_cc_d.h"
#include "f_op/f_op_actor.h"

#define private public
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"  // daPy_py_c::getMidnaActor
#include "d/actor/d_a_midna.h"   // daMidna_c::mpModel (shield-at-face attach)
#undef private

#include "m_Do/m_Do_mtx.h"        // mDoMtx_stack_c (shield seat on Midna's face)

#include "albw_common.h"
#include "devil_trigger.h"
#include "parry_master.h"
#include "shield.h"
#include "wolf_combat.h"
#include "wolf_guard.h"

namespace {

// Durability drains once per ~swing, not once per frame. The block hook fires
// every frame a swing overlaps, so without this a single swing drained the whole
// bar ("shield destroyed in one hit"). Ticked down in execute-post.
u16 s_blockDrainCooldown = 0;
constexpr u16 kBlockDrainCooldownFrames = 24;

DEFINE_HOOK(&daAlink_c::execute, WolfGuardExecute);
DEFINE_HOOK(&daAlink_c::checkDamageAction, WolfGuardDamageAction);
DEFINE_HOOK(&daAlink_c::setWolfItemMatrix, WolfGuardItemMatrix);

// Advance the parry window in wolf form + drive the (placeholder) visual.
void on_wolf_link_execute_post(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }
    dShield_updateWolfGuardTracking(link);
    dWolfGuard_tick(link);
    if (s_blockDrainCooldown > 0) {
        s_blockDrainCooldown--;
    }

    // §4a — guarded-attack wolf-charge is DEFERRED. Neither the enemy's cc_at_check
    // (guarded/armored hits never reach it) nor the wolf's own mAtCyl AT-flags
    // (ChkAtHit / ChkAtShieldHit) fired on a guarding Darknut in testing. The
    // likely-better seam is a WOLF-side "any connecting hit" detector rather than
    // an actor/AT-Tg one. See CURRENT-STATE §4a / WOLF-GUARD-SCOPE §4a.
}

HookAction on_wolf_check_damage_action_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || !link->checkWolf() || !dWolfGuard_isActive(link)) {
        return HOOK_CONTINUE;
    }

    // Mirror checkDamageAction's own hit-collider selection (fork
    // d_a_alink_damage.inc:575-581): body cylinders first, then mAtSph for wolf.
    dCcD_GObjInf* hit = nullptr;
    for (int i = 0; i < 3; ++i) {
        if (link->mTgCyls[i].ChkTgHit()) {
            hit = &link->mTgCyls[i];
            break;
        }
    }
    if (hit == nullptr && link->mAtSph.ChkTgHit()) {
        hit = &link->mAtSph;
    }
    if (hit == nullptr) {
        return HOOK_CONTINUE;  // no incoming hit this frame
    }

    dCcD_GObjInf* tg_gobj = hit->GetTgHitGObj();
    const int at_spl = tg_gobj != nullptr ? static_cast<int>(tg_gobj->GetAtSpl())
                                          : static_cast<int>(link->mCcStts.GetAtSpl());
    fopAc_ac_c* attacker = hit->GetTgHitAc();

    // PARRY first, then held block. onShieldHit is the shared parry decision
    // (guard raised + inside the parry window + not a non-deferrable guard-break).
    const bool parried = dShield_onShieldHit(link, at_spl, attacker);

    // Clear the consumed collider (dangling-collider hygiene) AND force the
    // "no damage" return via SKIP_ORIGINAL — more reliable than relying on the
    // stock scan re-reading a cleared collider.
    auto consumeAndBlock = [&]() {
        hit->ClrTgHit();
        link->mCcStts.ClrTg();
        *static_cast<int*>(retval) = 0;  // checkDamageAction: no damage action taken
    };

    if (parried) {
        dShield_playWolfParryFeedback(link, hit->GetTgHitPosP());  // spark + metallic clang
        dAlbwWolfCombat_onParry();  // +1/15 wolf charge (mash-parity, skill-gated)
        // Generalized DT parry-opening (belt-and-suspenders: onShieldHit already
        // calls this, but keep it explicit at the wolf seam too).
        dAlbwDevil_openGuardWindow(attacker);
        consumeAndBlock();
        return HOOK_SKIP_ORIGINAL;
    }

    // HELD GUARD (step 2): block the hit — NEGATE ONLY.
    //
    // ⚠ DO NOT call the human failed-block chain here. dShield_onFailedGuardBlock
    // ends in `link->procGuardBreakInit()` (shield.cpp) — the HUMAN guard-break
    // proc — which, driven onto the WOLF skeleton, was CONFIRMED in-game to deform
    // the wolf, void it out of the world, and kill it (the many at_spl=10 block
    // frames hammered it). dShield_onBlockHit is a no-op for the wolf anyway
    // (requires checkShieldGet()). Durability drain + Parry-Master chip for the
    // wolf need a wolf-SAFE variant that omits procGuardBreakInit — deferred.
    // Non-perfect block: drain the equipped shield's durability (and break it at
    // 0), so wolf blocks wear the shield down like human blocks. Uses the EXISTING
    // durability path — dShield_onBlockHit is now wolf-aware (shield.cpp). We still
    // do NOT call dShield_onFailedGuardBlock (its procGuardBreakInit deforms/kills
    // the wolf); onBlockHit + destroyFromDurability carry no such call.
    if (dShield_isDurabilityEnabled() && s_blockDrainCooldown == 0) {
        if (dShield_onBlockHit(link, at_spl, false, attacker)) {
            dShield_destroyFromDurability(link);
        }
        s_blockDrainCooldown = kBlockDrainCooldownFrames;  // one drain per ~swing, not per frame
    }
    consumeAndBlock();
    return HOOK_SKIP_ORIGINAL;
}

// ============================================
// "Midna's Shield" visual — put the equipped shield on Midna's FACE during the
// guard. POST on setWolfItemMatrix (the WOLF's item-draw — the human setItemMatrix
// is a different function that never runs in wolf form, which is why the first
// attempt did nothing). setWolfItemMatrix attaches the shield to the wolf's back
// (joint 2) + modelCalc's it; we override the base matrix to Midna's head joint
// and re-run modelCalc, so the ONE shield model draws at her face instead of the
// wolf's back. Same model (no double-draw — calc is separate from the draw pass).
// Low risk: matrix + recalc only, no procs/skeleton. First pass attaches at the
// raw head joint (JNT_HEAD = 4); a forward/up offset to seat it in front of her
// face is a tuning follow-up once it reads on-screen.
// ============================================
void on_wolf_set_item_matrix_post(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }
    if (!link->checkWolf() || !dWolfGuard_isActive(link)) {
        return;
    }
    if (link->mShieldModel == nullptr) {
        return;
    }
    daMidna_c* midna = daPy_py_c::getMidnaActor();
    if (midna == nullptr || midna->mpModel == nullptr) {
        return;
    }
    // Seat the shield on Midna's face. The raw head-joint frame draws it
    // upside-down and off to the side, so build from the head joint and apply a
    // corrective rotation + offset. TUNABLE — adjust the constants below from
    // in-game feedback (first pass: 180° flip to fix the upside-down).
    static constexpr s16 kRotX = 0;
    static constexpr s16 kRotY = static_cast<s16>(0x4000);  // +90° yaw — face it front (tune sign/axis)
    static constexpr s16 kRotZ = static_cast<s16>(0x8000);  // 180° roll — keep upright
    // Small horizontal nudge left, applied in the shield's FINAL (post-rotation)
    // frame so X reads as screen-horizontal. Flip the sign if it goes right.
    static constexpr f32 kOffX = 15.0f;  // +10 was right direction; +5 more per user
    static constexpr f32 kOffY = 0.0f;
    static constexpr f32 kOffZ = 0.0f;
    mDoMtx_stack_c::copy(midna->mpModel->getAnmMtx(4 /* JNT_HEAD */));
    mDoMtx_stack_c::XrotM(kRotX);
    mDoMtx_stack_c::YrotM(kRotY);
    mDoMtx_stack_c::ZrotM(kRotZ);
    mDoMtx_stack_c::transM(kOffX, kOffY, kOffZ);
    link->mShieldModel->setBaseTRMtx(mDoMtx_stack_c::get());
    link->modelCalc(link->mShieldModel);
}

// A hook miss must be LOUD and SCOPED, never fatal (soul_of_light lesson): the
// feature goes inactive and says so by name; the rest of the mod still loads.
bool install(ModError*, const char* name, ModResult r) {
    if (r != MOD_OK && svc_log != nullptr) {
        svc_log->error(mod_ctx, name);
        svc_log->error(mod_ctx, "wolf-guard hook did NOT install - feature inactive this run");
    }
    return true;
}

}  // namespace

ModResult albw_wolf_guard_init(ModError* error) {
    if (!install(error, "WolfGuardExecute",
                 mods::hook_add_post<WolfGuardExecute>(svc_hook, on_wolf_link_execute_post)) ||
        !install(error, "WolfGuardDamageAction",
                 mods::hook_add_pre<WolfGuardDamageAction>(svc_hook,
                                                           on_wolf_check_damage_action_pre)) ||
        !install(error, "WolfGuardItemMatrix",
                 mods::hook_add_post<WolfGuardItemMatrix>(svc_hook,
                                                          on_wolf_set_item_matrix_post))) {
        return MOD_ERROR;
    }
    if (svc_log != nullptr) {
        svc_log->info(mod_ctx, "albw wolf-guard (Midna's Shield) parry hooks ready");
    }
    return MOD_OK;
}
