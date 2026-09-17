#ifndef ALBW_HURRICANE_SPIN_H
#define ALBW_HURRICANE_SPIN_H

#include "mods/api.h"

// ============================================
// Hurricane Spin overlay - port of the fork's Focused-Arts Great-Spin finisher
// (d_a_alink_hurricane.inc). The fork added new daAlink_c procs
// (PROC_CUT_GS_HURRICANE / _TIRED); the stock exe has no proc-table slot for those,
// so instead of a new proc we OVERLAY the hurricane onto the existing great-spin proc
// (PROC_CUT_TURN): keep Link's mProcID valid, skip the stock proc's per-frame logic,
// and run the hurricane update ourselves for the duration, then exit to a wait proc.
//
// Driven from the meter's procCutTurn hooks (see meter.cpp).
// ============================================

class daAlink_c;

// Called from the procCutTurn(Init) hook. If the FA Great-Spin hurricane finisher is
// armed, begin the hurricane overlay and return true (caller SKIPs the stock proc).
bool albw_hurricane_try_begin(daAlink_c* link);

// Called each frame from the procCutTurn hook while the overlay owns the proc.
// Returns true while the overlay is running (caller SKIPs the stock proc). When it
// returns false the overlay has ended and normal proc flow resumes.
bool albw_hurricane_tick(daAlink_c* link);

// True while the hurricane overlay currently owns Link's proc.
bool albw_hurricane_is_active();

// Installs the per-frame procCutTurn overlay hook.
ModResult albw_hurricane_init(ModError* error);

#endif  // ALBW_HURRICANE_SPIN_H
