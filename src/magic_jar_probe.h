#pragma once

// ============================================
// ALBW_MAGICJAR_PROBE - the magic-jar grant-chain probe switch.
//
// Lives in its own header (like flurry_probe.h) because the post-grant meter
// sampler is driven from the mod tick, so mod.cpp has to see both the switch
// and the tick declaration.
//
// WHAT THE FIRST RUN SETTLED. The grant chain WORKS: all four links fired and
// the meter moved 4885 -> 8518, which is exactly 10900/3, the L_MAGIC third.
// Rupees are 1/15 (727), so the arithmetic rules them out as the source. The
// open question is therefore no longer "does a jar give anything" but "why
// does the player not see it on the bar" - value reverted downstream, or a
// HUD that is not reading this meter.
//
// MUST be 0 before any release (docs/RELEASE-PROCEDURE.md step 1).
// ============================================
#define ALBW_MAGICJAR_PROBE 1

#if ALBW_MAGICJAR_PROBE
// Samples the meter for a few frames after a grant. No-op until armed.
void albw_magic_jar_probe_tick();
#endif
