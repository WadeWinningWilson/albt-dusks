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
void dFlurryRush_onHitLanded(fopAc_ac_c* enemy);
void dFlurryRush_update();
void dFlurryRush_end(dFlurryRushEndReason reason);
bool dFlurryRush_shouldSuppressAlbwSpend();
bool dFlurryRush_isTargetActor(fopAc_ac_c* actor);
fopAc_ac_c* dFlurryRush_getTargetActor();

ModResult albw_flurry_init(ModError* error);
ModResult albw_flurry_shutdown(ModError* error);
void albw_flurry_tick();

ModResult albw_flurry_hooks_init(ModError* error);
ModResult albw_flurry_hooks_shutdown(ModError* error);
