#pragma once

// ============================================
// ALBW_SOUL_PROBE - Soul of Light / Tear of Light lifecycle trace.
//
// WHY THIS EXISTS. A player-reported softlock (tear effect not clearing on
// collect; a second death that never completes, Link alive and dead at once,
// warped, game stuck) could not be investigated from the logs AT ALL, because
// soul_of_light.cpp emits nothing on any success path - only error strings,
// and not one of them had fired. Three GAMEOVER processes were visible in the
// session log and the mod had said nothing about any of them.
//
// A feature that can softlock the game and produces no evidence when it does
// is the gap this closes. Trace the lifecycle: death, snapshot, spawn,
// collect, reset - and the Vessel of Light flags at every step, since those
// are save state this feature has been shown to write.
//
// MUST be 0 before any release (docs/RELEASE-PROCEDURE.md step 1).
// ============================================
#define ALBW_SOUL_PROBE 1
