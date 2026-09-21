#pragma once

// ============================================
// ALBW_FLURRY_PROBE - the ONE Flurry Rush probe switch.
//
// It used to live as a bare `#define ALBW_FLURRY_PROBE 0` at the top of
// sim_time_scale_hooks.cpp. The attack proc needs the same switch, and a
// second macro would mean two things to remember to turn off before a
// release, so the definition moved here and both TUs include it:
//
//   sim_time_scale_hooks.cpp  world slow-motion: scale transitions, Link
//                             exemption, "hooked but inert" detection
//   flurry_proc.cpp           the attack proc: overlay enter/exit, phase
//                             transitions, swing index, hit registration
//                             (and whether a hit came from the collision
//                             primitives or from the slow-mo fallback)
//
// Both log ON CHANGE only, never per frame. Set to 0 before release
// (docs/RELEASE-PROCEDURE.md step 1); at 0 every probe block compiles out
// and neither TU carries probe state.
// ============================================
#define ALBW_FLURRY_PROBE 0
