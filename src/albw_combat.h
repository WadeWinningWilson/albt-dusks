#pragma once

#include "dolphin/types.h"

enum dAlbwHelmBashTier {
    dAlbwHelmBash_THRESHOLD = 0,
    dAlbwHelmBash_MAX = 1,
};

class fopAc_ac_c;

dAlbwHelmBashTier dAlbwCombat_getHelmBashTier(fopAc_ac_c* actor);
u16 dAlbwHP_applyDurabilityMult(s16 profName, u16 amount);
