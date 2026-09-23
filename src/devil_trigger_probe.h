#pragma once

// ============================================
// ALBW_DEVIL_PROBE - Devil Trigger bring-up trace.
//
// Per the scope's sequencing, the first thing to confirm is that the mode ARMS
// on the right enemies at the right health and never flickers - before any
// speed change is trusted. This also carries the check that decides the whole
// design: whether the generic "is an attack live" signal (the AT-collider
// registry scan) agrees with the Darknut's own action modes, which we already
// know. If those disagree, the generic route is out and per-enemy state lists
// are back in.
//
// MUST be 0 before any release (docs/RELEASE-PROCEDURE.md step 1).
// ============================================
#define ALBW_DEVIL_PROBE 0
