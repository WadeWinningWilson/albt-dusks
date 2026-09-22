#pragma once

namespace albw {

// ============================================
// PORTED FROM FORK - include/dusk/sim_time_scale.h:1-16
//
// The fork keeps the world pace in a PLAIN GLOBAL that JSystem reads directly:
//
//     extern "C" float dusk_world_sim_time_scale;   // fork sim_time_scale.h:5
//     ...
//     updateWithRateScale(dusk_world_sim_time_scale);  // fork J3DAnimation.cpp:142
//
// with the fork's own comment saying why: "C linkage so JSystem can read world
// pace without pulling in game headers." The shape matters as much as the
// value - the hottest reader in the game (J3DFrameCtrl::update, once per
// animation per frame) must pay ONE LOAD, not a cross-TU function call.
//
// This mod cannot define a symbol the host's JSystem reads, so the global is
// mirrored mod-side and read through an inline accessor. The hook's early-out
// then compiles to load + compare + branch, the same as the fork's.
//
// g_world_sim_time_scale is the fork's dusk_world_sim_time_scale. Renamed only
// because the fork's name is an engine-side C export; the meaning, the range
// and the "1.0 == stock" contract are unchanged.
// ============================================
extern float g_world_sim_time_scale;

// Hot-path reader. Inline on purpose: see the block above.
inline float world_sim_time_scale() {
    return g_world_sim_time_scale;
}

// ============================================
// PER-ACTOR ANIMATION BOOST (Devil Trigger). The flurry slows the WORLD via
// g_world_sim_time_scale; this is the reverse and the narrow case - a single
// actor advancing FASTER than the world, only while its own animation update
// is running.
//
// It reuses the exact seam the flurry already owns (J3DFrameCtrl::update):
// bracket the actor's play()/action() with begin/end, and every frame
// controller that advances inside the bracket is scaled by the boost. This is
// the Link-exemption mechanism (sim_time_scale_hooks.cpp) run in reverse, and
// it is immune to the setAnm re-seed that made the setPlaySpeed approach
// intermittent: it multiplies mRate at the instant of advance, whatever mRate
// currently is.
//
// Depth-counted so nested brackets are safe. boost > 1.0 speeds up; the
// bracket composes with the world scale by multiplication in the hook.
// ============================================
void anim_boost_begin(float boost);
void anim_boost_end();
float anim_boost_current();  // 1.0 when no bracket is open

float get_sim_time_scale();
void set_sim_time_scale(float scale);

}  // namespace albw

// ============================================
// World slow-motion hooks (sim_time_scale_hooks.cpp).
// Scales every J3D animation frame controller by the world sim scale and
// exempts Link, so Flurry Rush slows the world but not the player.
// ============================================
#include "mods/api.h"

ModResult albw_sim_time_scale_hooks_init(ModError* error);
ModResult albw_sim_time_scale_hooks_shutdown(ModError* error);
