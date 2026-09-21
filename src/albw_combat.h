#pragma once

#include "dolphin/types.h"

enum dAlbwHelmBashTier {
    dAlbwHelmBash_THRESHOLD = 0,
    dAlbwHelmBash_MAX = 1,
};

class fopAc_ac_c;

dAlbwHelmBashTier dAlbwCombat_getHelmBashTier(fopAc_ac_c* actor);
u16 dAlbwHP_applyDurabilityMult(s16 profName, u16 amount);

// ============================================
// Guard-opener classification (fork d/d_albw_combat.h:31-45, verbatim intent).
// True when the hit collider belongs to one of the attacks that OPEN physical
// enemy guards (drop the shield for a short window) instead of clanking: the
// Hurricane finisher, the wolf Combat Howl AOE, and the Midna arm strike.
// Great Spin is deliberately EXCLUDED (it shares LARGE_TURN cut types with
// Hurricane, so this keys on the Hurricane state, not the cut type).
// Normal swings / charged spin stay non-openers so blocking still reads.
// ============================================
class cCcD_Obj;
bool dAlbwCombat_isGuardOpenerHit(cCcD_Obj* i_hitObj);

// Shared open-window length for guard-opener hits - deliberately shorter than
// the shield-bash windows (90/75) so a sustained Hurricane re-hit chain is
// self-limiting rather than a permanent guard-stunlock.
// Fork d/d_albw_combat.h:45.
constexpr u8 kAlbwGuardOpenerWindowFrames = 40;
