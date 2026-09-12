// ============================================
// NEW CODE — ALBW Port (Hidden-skill rework: Jump Strike charge gate)
// Verbatim from the fork's d_albw_combat.cpp jump-strike-charge helpers, with the
// settings gate mapped to the mod's dAlbw_isHiddenSkillReworkEnabled(). The
// charge-proc / init hooks that drive these live in meter.cpp (the cut-proc hook
// owner).
// ============================================

#include "global.h"

#include "hidden_skill_charge.h"
#include "shield_adapt.h"  // dAlbw_isHiddenSkillReworkEnabled()

#if TARGET_PC

static bool s_jumpStrikeChargeReady = false;

void dAlbw_resetJumpStrikeChargeReady() {
    s_jumpStrikeChargeReady = false;
}

void dAlbw_setJumpStrikeChargeReady() {
    s_jumpStrikeChargeReady = true;
}

bool dAlbw_isJumpStrikeChargeReady() {
    if (!dAlbw_isHiddenSkillReworkEnabled()) {
        return true;
    }
    return s_jumpStrikeChargeReady;
}

bool dAlbw_tryConsumeJumpStrikeChargeReady() {
    if (!dAlbw_isHiddenSkillReworkEnabled()) {
        return true;
    }
    if (!s_jumpStrikeChargeReady) {
        return false;
    }
    s_jumpStrikeChargeReady = false;
    return true;
}

#else  // TARGET_PC

void dAlbw_resetJumpStrikeChargeReady() {}
void dAlbw_setJumpStrikeChargeReady() {}
bool dAlbw_isJumpStrikeChargeReady() { return true; }
bool dAlbw_tryConsumeJumpStrikeChargeReady() { return true; }

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
