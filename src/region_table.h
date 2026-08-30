#pragma once

#include "dolphin/types.h"

f32 albw_region_resolve_table_mult();
f32 albw_region_rupee_mult();
u16 albw_region_scale_rupees(u16 amount);

void albw_region_push_damage_scale();
void albw_region_pop_damage_scale();
f32 albw_region_damage_mult();

s16 albw_region_scale_hp(s16 hp);
