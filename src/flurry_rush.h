#pragma once

#include "mods/api.h"

class fopAc_ac_c;

enum dFlurryPerfectDodgeKind {
    dFlurryPerfectDodge_SideStep = 0,
    dFlurryPerfectDodge_BackJump = 1,
};

enum dFlurryMeleeTelegraphAxis {
    dFlurryTelegraph_None = 0,
    dFlurryTelegraph_Vertical = 1,
    dFlurryTelegraph_Horizontal = 2,
};

enum dFlurryRushEndReason {
    dFlurryRushEnd_Interrupt = 0,
    dFlurryRushEnd_StartGateExpired = 1,
    dFlurryRushEnd_HitCap = 2,
    dFlurryRushEnd_TargetLost = 3,
};

// ============================================
// PORTED - fork include/d/d_albw_flurry_rush.h (dFlurryRushMode).
// The module already tracked a mode, but as a private `FlurryMode` enum inside
// flurry_rush.cpp. The ported proc reads it by the donor's name
// (procFlurryRushInit, fork d_a_alink_flurry.inc:245), so the donor's enum
// replaces the private one rather than a second mode being introduced.
// dFlurryRushMode_AerialBow is kept because it is part of the donor type; it
// is never set here (the fork's own Phase 7 is unfinished - plan doc section 8).
// ============================================
enum dFlurryRushMode {
    dFlurryRushMode_None = 0,
    dFlurryRushMode_Melee,
    dFlurryRushMode_AerialBow,
};

enum dFlurryRushSwordProfile {
    dFlurryRushProfile_Unknown = 0,
    dFlurryRushProfile_Wood = 1,
    dFlurryRushProfile_Ordon = 2,
    dFlurryRushProfile_Master = 3,
    dFlurryRushProfile_Light = 4,
};

struct dFlurryRushProfile {
    int spendGate;
    int barCost;
    int maxHits;
};

bool dFlurryRush_isEnabled();
bool dFlurryRush_isActive();
bool dFlurryRush_tryPerfectDodge(dFlurryPerfectDodgeKind kind);
bool dFlurryRush_tryEnterFromDodge();
void dFlurryRush_onAttackStarted();
void dFlurryRush_update();
void dFlurryRush_end(dFlurryRushEndReason reason);
bool dFlurryRush_shouldSuppressAlbwSpend();
bool dFlurryRush_isTargetActor(fopAc_ac_c* actor);
fopAc_ac_c* dFlurryRush_getTargetActor();

// ============================================
// PORTED - the state callbacks the fork's proc drives, fork
// include/d/d_albw_flurry_rush.h + src/d/d_albw_flurry_rush.cpp:353-412.
// Every one of these is called from d_a_alink_flurry.inc and had no
// counterpart here, which is why the proc could not be ported before.
//
// dFlurryRush_onHitLanded lost its fopAc_ac_c* parameter: that argument was a
// receiver invention for the cc_at_check stand-in hook (flurry_hooks.cpp),
// which the donor's own hit path now replaces. The donor's signature takes no
// target because flurryCheckSwordHit (fork .inc:133-154) has already validated
// the hit against the pinned rush target before calling it.
// ============================================
dFlurryRushMode dFlurryRush_getMode();
void dFlurryRush_onMeleeProcEntered();
void dFlurryRush_onSnapToTargetComplete();
void dFlurryRush_onHitLanded();
float dFlurryRush_getChainGateWidthFrames();  // donor spells this f32; same type
void dFlurryRush_onChainGateMissed();

ModResult albw_flurry_init(ModError* error);
ModResult albw_flurry_shutdown(ModError* error);
void albw_flurry_tick();

ModResult albw_flurry_hooks_init(ModError* error);
ModResult albw_flurry_hooks_shutdown(ModError* error);
