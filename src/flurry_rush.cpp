#include "flurry_rush.h"

#include "albw_common.h"
#include "albw_dusk_log.h"
#include "albw_game.h"
#include "config_vars.h"
#include "flurry_probe.h"
#include "flurry_proc.h"
#include "focused_arts.h"
#include "shield.h"
#include "shield_adapt.h"
#include "sim_time_scale.h"

#include "d/d_attention.h"
#include "d/d_com_inf_game.h"
#define private public
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_e_oc.h"
#undef private
#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"

#include <chrono>
#include <cstdio>

namespace {

constexpr float kFlurryStartGateRealSeconds = 2.0f;
constexpr float kFlurrySimTimeScale = 0.1f;

// ============================================
// PORTED VERBATIM - fork src/d/d_albw_flurry_rush.cpp:27.
// The chain-gate width in Link's own animation frames (Link runs at 1.0x while
// the world is slowed), read by flurryBeginSwing to compute field_0x3480.
// ============================================
constexpr float kFlurryChainGateWidthFrames = 8.0f;

struct State {
    bool active = false;
    dFlurryRushMode mode = dFlurryRushMode_None;
    fopAc_ac_c* target = nullptr;
    fpc_ProcID targetId = fpcM_ERROR_PROCESS_ID_e;
    dFlurryRushSwordProfile profile = dFlurryRushProfile_Unknown;
    std::chrono::steady_clock::time_point startDeadline{};
    bool startGateArmed = false;
    bool hasStartedAttack = false;
    int hitCount = 0;
    bool pendingPerfectDodge = false;
    dFlurryPerfectDodgeKind pendingKind = dFlurryPerfectDodge_SideStep;
};

State s_state;

// ============================================
// NEW CODE - ALBW Port (Flurry Rush shop gate)
// The two toggles say the FEATURE exists; dFocusedArts_hasFlurryRushTier says
// the PLAYER has it, and that is a session-only purchase with no save bit (see
// the DN-10 ledger block in focused_arts_core.inc). Gating the shared
// predicate rather than each caller means every seam - the trigger, the proc
// entry, the sim-time scale and the per-frame update - picks the gate up at
// once, and the feature is provably inert before the purchase.
// The existing all-tiers cheat (g_focused_arts_cheat) grants it, so testing
// does not cost 1000 rupees.
// ============================================
bool flurry_enabled() {
    return albw_cfg_bool(g_focused_arts, false) && albw_cfg_bool(g_flurry_rush, false) &&
           dFocusedArts_hasFlurryRushTier();
}

dFlurryMeleeTelegraphAxis queryOcTelegraph(fopAc_ac_c* actor) {
    if (actor == nullptr || fopAcM_GetName(actor) != fpcNm_E_OC_e) {
        return dFlurryTelegraph_None;
    }

    auto* oc = static_cast<daE_OC_c*>(actor);
    constexpr int kOcActionAttack = 4;
    constexpr f32 kTelegraphEndFrame = 14.0f;
    constexpr f32 kVerticalStartFrame = 8.0f;
    constexpr f32 kHorizontalStartFrame = 6.0f;

    if (oc->getActionMode() != kOcActionAttack || oc->mpMorf == nullptr) {
        return dFlurryTelegraph_None;
    }
    if (oc->mOcState != 1 && oc->mOcState != 2) {
        return dFlurryTelegraph_None;
    }

    const f32 frame = oc->mpMorf->getFrame();
    if (frame < kTelegraphEndFrame) {
        if (oc->mOcState == 1 && frame >= kVerticalStartFrame) {
            return dFlurryTelegraph_Vertical;
        }
        if (oc->mOcState == 2 && frame >= kHorizontalStartFrame) {
            return dFlurryTelegraph_Horizontal;
        }
    }
    return dFlurryTelegraph_None;
}

dFlurryRushSwordProfile swordProfileFromEquip() {
    const u8 sword = albw_game::select_equip_sword();
    if (sword == dItemNo_WOOD_STICK_e) {
        return dFlurryRushProfile_Wood;
    }
    if (sword == dItemNo_SWORD_e) {
        return dFlurryRushProfile_Ordon;
    }
    if (sword == dItemNo_MASTER_SWORD_e) {
        return dFlurryRushProfile_Master;
    }
    if (sword == dItemNo_LIGHT_SWORD_e) {
        return dFlurryRushProfile_Light;
    }
    return dFlurryRushProfile_Unknown;
}

dFlurryRushProfile profileTable(dFlurryRushSwordProfile profile) {
    switch (profile) {
    case dFlurryRushProfile_Wood:
        return {0, 0, 10};
    case dFlurryRushProfile_Ordon:
        return {2, 2, 7};
    case dFlurryRushProfile_Master:
    case dFlurryRushProfile_Light:
        return {3, 3, 5};
    default:
        return {0, 0, 0};
    }
}

void refreshTarget() {
    if (s_state.target != nullptr && fopAcM_IsActor(s_state.target)) {
        return;
    }
    s_state.target = fopAcM_SearchByID(s_state.targetId);
}

bool beginMelee(fopAc_ac_c* target) {
    if (!flurry_enabled() || s_state.active || target == nullptr) {
        return false;
    }

    s_state.active = true;
    s_state.mode = dFlurryRushMode_Melee;
    s_state.target = target;
    s_state.targetId = fopAcM_GetID(target);
    s_state.profile = swordProfileFromEquip();
    s_state.hitCount = 0;
    s_state.hasStartedAttack = false;
    s_state.startGateArmed = false;
    s_state.pendingPerfectDodge = false;
    albw::set_sim_time_scale(kFlurrySimTimeScale);
    return true;
}

void armStartGate() {
    if (!s_state.active || s_state.hasStartedAttack) {
        return;
    }
    s_state.startGateArmed = true;
    s_state.startDeadline =
        std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<float>(kFlurryStartGateRealSeconds));
}

}  // namespace

bool dFlurryRush_shouldSuppressAlbwSpend() {
    return s_state.active;
}

bool dFlurryRush_isEnabled() {
    return flurry_enabled();
}

bool dFlurryRush_isActive() {
    return s_state.active;
}

// ============================================
// PORTED - fork d_albw_flurry_rush.cpp:353-355.
// procFlurryRushInit (fork .inc:245) refuses to enter unless the mode is
// Melee, so the proc needs this accessor.
// ============================================
dFlurryRushMode dFlurryRush_getMode() {
    return s_state.mode;
}

fopAc_ac_c* dFlurryRush_getTargetActor() {
    if (!s_state.active) {
        return nullptr;
    }
    refreshTarget();
    return s_state.target;
}

bool dFlurryRush_isTargetActor(fopAc_ac_c* actor) {
    if (!s_state.active || actor == nullptr) {
        return false;
    }
    refreshTarget();
    return actor == s_state.target;
}

void dFlurryRush_end(dFlurryRushEndReason reason) {
    if (!s_state.active) {
        return;
    }
#if ALBW_FLURRY_PROBE
    // Donor shape: fork d_albw_flurry_rush.cpp:220-247 logs "end (<reason>) hits=N".
    DuskLog.info("[flurry] END reason={} hits={} startedAttack={}", (int)reason, s_state.hitCount,
                 s_state.hasStartedAttack);
#else
    (void)reason;
#endif
    s_state = State{};
    albw::set_sim_time_scale(1.0f);
}

bool dFlurryRush_tryPerfectDodge(dFlurryPerfectDodgeKind kind) {
    if (!flurry_enabled() || s_state.active) {
        return false;
    }

    daPy_py_c* player = albw_game::link_player();
    if (player == nullptr || player->checkWolf() ||
        albw_game::select_equip_sword() == dItemNo_NONE_e || !player->checkAttentionLock())
    {
        return false;
    }

    dAttention_c* attn = albw_game::attention();
    fopAc_ac_c* target = attn != nullptr ? attn->LockonTarget(0) : nullptr;
    if (target == nullptr) {
        return false;
    }

    // ============================================
    // TRIGGER - DELIBERATE DIVERGENCE FROM THE FORK (user-directed).
    //
    // The fork gates entry on an ENEMY TELEGRAPH: an E_OC mid-attack on the
    // axis matching the dodge, inside a 6-8 frame window, paid for with a
    // Focused Arts bank charge (fork d_focused_arts.cpp:521-615). That is a
    // reaction test, and it is also why the fork's Flurry Rush only ever works
    // against Bokoblins - queryOcTelegraph returns None for every other actor,
    // which is the fork's own unfinished Phase 8.
    //
    // This build gates on PLAYER RESOURCE STATE instead: a backflip, while
    // Z-locked, with the shield's bash bar at its tier maximum - and the
    // backflip SPENDS a charge. Consequences, both intended:
    //   * it works against every enemy, since no telegraph is consulted;
    //   * bashing and flurrying draw on one bar, so spending bashes locks you
    //     out at max-1 until the bar refills;
    //   * entry is no longer a reaction test. If a skill check is wanted back,
    //     the fork already built two places for it - the 2s start gate and the
    //     recovery chain gate (d_a_alink_flurry.inc:383-404) - so it would move
    //     from entry to sustain rather than being reintroduced here.
    //
    // The FA perfect-dodge spend economy (canPerfectDodgeSpend /
    // onPerfectDodgeSpend) is therefore NOT consulted; bash charges replace it.
    // Recorded as divergence 1 and 2 in docs/FLURRY-RUSH-PLAN.md section 6.
    // ============================================
    if (kind != dFlurryPerfectDodge_BackJump) {
        return false;  // sidestep no longer opens a rush; the backflip is the input
    }
    if (!dShield_isBashBarFull()) {
        return false;
    }

    const dFlurryRushProfile profile = profileTable(swordProfileFromEquip());
    if (profile.maxHits <= 0) {
        return false;
    }

    if (!beginMelee(target)) {
        return false;
    }
    dShield_spendBashCharges(1);

    s_state.pendingPerfectDodge = true;
    s_state.pendingKind = kind;
    return true;
}

// ============================================
// PORTED - fork d_albw_flurry_rush.cpp:508-515
// (dFlurryRush_tryEnterProcFromPerfectDodge).
//
// The armStartGate() call that used to be here is REMOVED. It was a receiver
// invention from the era with no proc: the donor arms the 2s gate in
// dFlurryRush_onSnapToTargetComplete, i.e. when Link has actually finished
// lunging and the attack window opens (fork :373-384). Arming it at the dodge
// landing started the clock during the snap-lunge and spent part of the
// window on travel.
// ============================================
bool dFlurryRush_tryEnterFromDodge() {
    if (!s_state.active || s_state.mode != dFlurryRushMode_Melee || !s_state.pendingPerfectDodge) {
        return false;
    }
    s_state.pendingPerfectDodge = false;
    return true;
}

// ============================================
// PORTED - fork d_albw_flurry_rush.cpp:365-371.
// The donor logs here and nothing else; the state transition it announces
// (snap pending) is already carried by the proc's own phase variable. Kept as
// the donor's seam so procFlurryRushInit (fork .inc:251) stays verbatim, and
// so the probe has the entry edge.
// ============================================
void dFlurryRush_onMeleeProcEntered() {
    if (!s_state.active || s_state.mode != dFlurryRushMode_Melee || s_state.hasStartedAttack) {
        return;
    }
#if ALBW_FLURRY_PROBE
    DuskLog.info("[flurry] melee proc entered (snap pending)");
#endif
}

// ============================================
// PORTED - fork d_albw_flurry_rush.cpp:373-384.
// This, not the dodge landing, is where the donor arms the 2s start gate.
// ============================================
void dFlurryRush_onSnapToTargetComplete() {
    if (!s_state.active || s_state.mode != dFlurryRushMode_Melee || s_state.hasStartedAttack) {
        return;
    }
    if (s_state.startGateArmed) {
        return;
    }
    armStartGate();
#if ALBW_FLURRY_PROBE
    DuskLog.info("[flurry] snap complete (start gate armed, {}s)", kFlurryStartGateRealSeconds);
#endif
}

void dFlurryRush_onAttackStarted() {
    if (!s_state.active || s_state.hasStartedAttack) {
        return;
    }
    s_state.hasStartedAttack = true;
    s_state.startGateArmed = false;
}

// ============================================
// PORTED - fork d_albw_flurry_rush.cpp:395-403.
// The donor only COUNTS here; the hit cap is enforced once per frame in
// dFlurryRush_update below. The previous receiver version took the enemy
// actor, re-validated it, and ended the rush inline - all three of which came
// from the cc_at_check stand-in hook that the donor's own flurryCheckSwordHit
// (fork .inc:133-154) now replaces.
// ============================================
void dFlurryRush_onHitLanded() {
    if (!s_state.active || !s_state.hasStartedAttack) {
        return;
    }
    s_state.hitCount++;
#if ALBW_FLURRY_PROBE
    DuskLog.info("[flurry] hit {} / cap {}", s_state.hitCount,
                 profileTable(s_state.profile).maxHits);
#endif
}

// ============================================
// PORTED VERBATIM - fork d_albw_flurry_rush.cpp:406-412.
// ============================================
float dFlurryRush_getChainGateWidthFrames() {
    return kFlurryChainGateWidthFrames;
}

void dFlurryRush_onChainGateMissed() {
#if ALBW_FLURRY_PROBE
    DuskLog.info("[flurry] chain gate missed (recovery window closed)");
#endif
}

void dFlurryRush_update() {
    if (!flurry_enabled()) {
        if (s_state.active) {
            dFlurryRush_end(dFlurryRushEnd_Interrupt);
        }
        return;
    }
    if (!s_state.active) {
        return;
    }

    refreshTarget();
    if (s_state.target == nullptr) {
        dFlurryRush_end(dFlurryRushEnd_TargetLost);
        return;
    }

    dAttention_c* attn = albw_game::attention();
    if (attn != nullptr) {
        attn->keepLock(30);
    }

    if (!s_state.hasStartedAttack) {
        daPy_py_c* player = albw_game::link_player();
        daAlink_c* link = static_cast<daAlink_c*>(player);
        if (player == nullptr || !player->checkAttentionLock()) {
            dFlurryRush_end(dFlurryRushEnd_Interrupt);
            return;
        }

        if (link != nullptr && s_state.mode == dFlurryRushMode_Melee) {
            const u16 linkProc = link->mProcID;
            if (linkProc == daAlink_c::PROC_SIDESTEP || linkProc == daAlink_c::PROC_BACK_JUMP) {
                // The !manualShieldBlocksSwordInput() clause is the donor's
                // (fork d_albw_flurry_rush.cpp:287) and was missing here: with
                // manual shield on, holding guard must not silently reserve a
                // flurry swing. albw_manual_shield_blocks_sword is the mod's
                // existing equivalent (shield_adapt.h:56).
                if (link->swordSwingTrigger() && !albw_manual_shield_blocks_sword(link)) {
                    link->onNoResetFlg2(daPy_py_c::FLG2_COMBO_RESERB);
                }
            }
        }

        if (s_state.startGateArmed && std::chrono::steady_clock::now() >= s_state.startDeadline) {
            dFlurryRush_end(dFlurryRushEnd_StartGateExpired);
            return;
        }
    }

    if (s_state.hasStartedAttack) {
        const dFlurryRushProfile profile = profileTable(s_state.profile);
        if (profile.maxHits > 0 && s_state.hitCount >= profile.maxHits) {
            dFlurryRush_end(dFlurryRushEnd_HitCap);
            return;
        }
    }

    // ============================================
    // LIFETIME - layer 2 of 3. PORTED, fork d_albw_flurry_rush.cpp:310-314:
    //
    //     if (s_state.mode == dFlurryRushMode_Melee && s_state.hasStartedAttack) {
    //         if (link != nullptr && linkProc != daAlink_c::PROC_FLURRY_RUSH) {
    //             dFlurryRush_end(dFlurryRushEnd_Interrupt);
    //         }
    //     }
    //
    // The donor's predicate is "Link is no longer in the rush proc". The
    // overlay's equivalent is albw_flurry_proc_active(), NOT
    // mProcID != PROC_CUT_NORMAL: the host proc is one ordinary play uses all
    // the time, so the proc id alone cannot tell the overlay from a plain
    // sword swing. Layer 1 (the commonProcInit chokepoint, flurry_proc.cpp) is
    // what normally fires first; this is the once-per-frame backstop the donor
    // also keeps.
    // ============================================
    if (s_state.mode == dFlurryRushMode_Melee && s_state.hasStartedAttack &&
        !albw_flurry_proc_active())
    {
        dFlurryRush_end(dFlurryRushEnd_Interrupt);
    }
}

ModResult albw_flurry_init(ModError* error) {
    if (albw_flurry_hooks_init(error) != MOD_OK) {
        return MOD_ERROR;
    }
    if (albw_flurry_proc_init(error) != MOD_OK) {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_flurry_shutdown(ModError*) {
    dFlurryRush_end(dFlurryRushEnd_Interrupt);
    albw_flurry_proc_reset();
    return MOD_OK;
}

void albw_flurry_tick() {
    // LIFETIME layer 3 runs BEFORE the state update so that a rush whose
    // player actor vanished is already ended when dFlurryRush_update looks.
    albw_flurry_proc_frame_watch();
    dFlurryRush_update();
}
