#pragma once

#include <dolphin/types.h>

/** True while physical L1 / LB is held (not L2 / z-target via emulateTriggers). */
bool albw_l1_held(u32 port);

/** Rising edge of L1 / LB. */
bool albw_l1_trig(u32 port);
