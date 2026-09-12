#pragma once

// ============================================
// NEW CODE — ALBW Port (Hidden-skill rework: Jump Strike charge gate)
// Ported from the fork's d_albw_combat.cpp jump-strike-charge state. Jump Strike
// only fires if its charge was held to completion: the charge proc sets "ready"
// on anim-end, and procCutLargeJumpInit consumes it (denying an under-charged
// release). All gated by dAlbw_isHiddenSkillReworkEnabled().
// ============================================

void dAlbw_resetJumpStrikeChargeReady();
void dAlbw_setJumpStrikeChargeReady();
bool dAlbw_isJumpStrikeChargeReady();
bool dAlbw_tryConsumeJumpStrikeChargeReady();
