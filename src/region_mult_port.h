// Copied verbatim from fork include/d/d_albw_region_mult.h — the Region Multiplier
// public API + DamageScaleScope RAII. Implementation ported into region_port.cpp
// (build/port/region_mult.inc). Do not hand-edit the API; re-copy from the fork.
#pragma once

#if TARGET_PC

#include "dolphin/types.h"

class fopAc_ac_c;

bool dAlbwRegionMult_isEnabled();

f32 dAlbwRegionMult_getTableMult();

f32 dAlbwRegionMult_getDamageMult();
f32 dAlbwRegionMult_getHealthMult();
f32 dAlbwRegionMult_getRupeeMult();
f32 dAlbwRegionMult_getRegionDamageRupeeMult();

s16 dAlbwRegionMult_scaleHp(s16 hp);

u16 dAlbwRegionMult_scaleRupees(u16 amount);

void dAlbwRegionMult_pushDamageScale();
void dAlbwRegionMult_popDamageScale();

bool dAlbwRegionMult_isPlayerBomb(fopAc_ac_c* i_actor);

struct dAlbwRegionMult_DamageScaleScope {
    dAlbwRegionMult_DamageScaleScope() { dAlbwRegionMult_pushDamageScale(); }
    ~dAlbwRegionMult_DamageScaleScope() { dAlbwRegionMult_popDamageScale(); }

    dAlbwRegionMult_DamageScaleScope(const dAlbwRegionMult_DamageScaleScope&) = delete;
    dAlbwRegionMult_DamageScaleScope& operator=(const dAlbwRegionMult_DamageScaleScope&) = delete;
};

#endif  // TARGET_PC
