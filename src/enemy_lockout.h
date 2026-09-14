#pragma once

#include "mods/api.h"

// Per-enemy slingshot ranged-open / stun wiring (Stage 2). Reproduces the fork's
// per-actor lockout edits (d_a_e_*, d_a_b_tn) as HOOK_SKIP_ORIGINAL whole-function
// replacements / top-of-function guards. See enemy_lockout.cpp.
ModResult albw_enemy_lockout_init(ModError* error);
