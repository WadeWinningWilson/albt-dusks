// Armogohma refined fight (actor side) — see armogohma.cpp.
#pragma once

#include "albw_common.h"

ModResult albw_armogohma_init(ModError* error);

// Releases the bundled reveal BMD buffer. The fork loads the same model onto the
// boss's SOLID HEAP (d_a_b_gm.cpp:3094 try_load_uncached inside useHeapInit), so
// it dies with the actor and needs no explicit free; the mod's host-resource
// bridge owns a process-lifetime ResourceBuffer instead, so the mod must return
// it at the only other moment its statics die - mod_shutdown.
ModResult albw_armogohma_shutdown(ModError* error);
