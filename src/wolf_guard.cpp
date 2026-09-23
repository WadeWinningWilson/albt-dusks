// ============================================
// NEW CODE — ALBW Port — "Midna's Shield" (wolf guard / parry) — state module
//
// See docs/WOLF-GUARD-SCOPE.md §8b for the architecture. This module owns the
// wolf-guard-active predicate; shield.cpp's guardRaised() consults it when
// checkWolf(), so the existing parry engine (dShield_onShieldHit /
// updateGuardTracking) accepts the wolf with the human path byte-identical.
// ============================================

#include "global.h"

#include "m_Do/m_Do_controller_pad.h"

#define private public
#include "d/actor/d_a_alink.h"
#undef private

#include "d/actor/d_a_player.h"  // daPy_py_c::getMidnaActor
#include "d/actor/d_a_midna.h"   // daMidna_c + daMidna_ANM (S_TAKES/S_WAITS/S_PACKAWAY)

#include "albw_common.h"
#include "albw_game.h"
#include "albw_save_flags.h"
#include "config_vars.h"
#include "rental_eligibility.h"
#include "shield_adapt.h"
#include "wolf_arts.h"
#include "wolf_combat.h"
#include "wolf_guard.h"

bool dWolfGuard_isUnlocked() {
    // Purchase is the ONLY grant — config flag, never the save (WOLF-GUARD §8 "B").
    // True ALBW does NOT auto-grant; it only makes the shop ROW appear early
    // (see dWolfGuard_shouldShowShopRow), matching the sibling wolf arts.
    return albw_save_flag_get(ALBW_FLAG_WOLF_GUARD_PURCHASED);
}

void dWolfGuard_unlock() {
    albw_save_flag_set(ALBW_FLAG_WOLF_GUARD_PURCHASED, true);  // config.json, not the save
}

bool dWolfGuard_shouldShowShopRow() {
    // Available as soon as the wolf-arts shop itself opens — same first-twilight
    // gate as the howl row (DarkClearLV bit 0) — not gated behind the arm. Pure
    // read; True ALBW bypasses the story gate.
    return albw_cfg_bool(g_wolf_combat, false) && !dWolfGuard_isUnlocked() &&
           (albw_game::is_dark_clear_lv(0) || albw_is_true_albw_enabled());
}

int dWolfGuard_getShopPrice() {
    return 100;  // matches the other wolf-art rows
}

const char* dWolfGuard_getShopName() {
    return "Midna's Shield";
}

const char* dWolfGuard_getShopDesc() {
    return "Careful, I came across a beast in Hyrule. Hiding away, the creature seemed..."
           "sad. It slunked away, dropping this foreign scroll. The imagery depicts a form "
           "of defense.";
}

bool dWolfGuard_tryPurchase() {
    if (dWolfGuard_isUnlocked()) {
        return false;
    }
    dWolfGuard_unlock();  // config.json flag, never the save file
    return true;
}

bool dWolfGuard_isEnabled() {
    // No toggle of its own: part of Wolf Link combat. Needs wolf combat on, the
    // shield parry engine it reuses (dShield_onShieldHit early-returns without it,
    // which would leave a half-state where the guard raises but never parries),
    // and the unlock.
    return albw_cfg_bool(g_wolf_combat, false) && albw_shield_parry_enabled() &&
           dWolfGuard_isUnlocked();
}

bool dWolfGuard_isActive(const daAlink_c* i_link) {
    if (i_link == nullptr || !dWolfGuard_isEnabled()) {
        return false;
    }
    if (!i_link->checkWolf()) {
        return false;
    }
    // Held-R — the same guard input the human manual shield uses (getHoldLockR
    // doubles as Z-lock in both forms; TP-style target+guard). A grounded/facing
    // refinement is a follow-up; slice 1 keeps the const-safe minimal predicate.
    return mDoCPd_c::getHoldLockR(PAD_1) != 0;
}

// ============================================
// NEW CODE — ALBW Port ("Midna's Shield") — the raise/hold/stow visual.
// Drives the companion daMidna_c through her purpose-built shield animes on the
// guard's rising/falling edges: ANM_S_TAKES (raise) → ANM_S_WAITS (hold) →
// ANM_S_PACKAWAY (stow). Upper-body animes, so her ride/cling pose is preserved.
// See docs/WOLF-GUARD-SCOPE.md §4. First visual pass — raise duration is a
// tunable guess.
// ============================================
namespace {
bool s_guardVisualActive = false;
int  s_raiseFramesLeft = 0;
constexpr int kRaiseFrames = 10;  // approx ANM_S_TAKES length before the S_WAITS hold
}  // namespace

void dWolfGuard_tick(daAlink_c* i_link) {
    // Midna's body-lean pose (ANM_S_TAKES/S_WAITS/S_PACKAWAY) was removed (user):
    // now that the shield is drawn on her face during guard, the lean served no
    // purpose and didn't aid readability. The shield-on-face visual is driven
    // entirely by the setWolfItemMatrix hook, independent of any Midna anime.
    (void)i_link;
    (void)s_guardVisualActive;
    (void)s_raiseFramesLeft;
}
