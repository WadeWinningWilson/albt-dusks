#pragma once

#include "mods/api.h"

// ============================================
// Darknut (B_TN) shield-bash parry / guard-open port - ACTOR half.
//
// Reproduces the fork's daB_TN_c guard-open window (field_0xaa2), its
// shield-bash guard-break decision core and the phase-1/phase-2 break paths, as
// typed hooks on the exported daB_TN_c members. See btn_parry.cpp for the hook
// ledger and the placement argument for every seam.
//
// SHARED SEAM: daB_TN_c::damage_check carries BOTH this feature and the meter
// lockout's per-enemy slingshot wiring, interleaved in one fork body. It is
// ported here ONCE and WHOLE, lockout hunks included - see the note at the top
// of enemy_lockout.cpp. The lockout lane VERIFIES rather than re-ports.
// ============================================
ModResult albw_btn_parry_init(ModError* error);
