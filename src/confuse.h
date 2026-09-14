#pragma once

#include "mods/api.h"

struct fopAc_ac_c;

// Dom Rod confuse (shared-seam). See confuse.cpp for the design writeup.
ModResult albw_confuse_init(ModError* error);

// True while the given enemy is the current confuse host (meter locked, timer live).
bool albw_confuse_is_confused(const fopAc_ac_c* i_enemy);

// The rival the confused host is currently steered onto, or NULL.
fopAc_ac_c* albw_confuse_get_target(const fopAc_ac_c* i_attacker);
