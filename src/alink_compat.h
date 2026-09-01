#pragma once

// ============================================
// NEW CODE - ALBT multiplatform (dAlbwAlink_* compat layer)
// Declares the three fork helpers the ported outfit cluster calls. In the fork
// these live INSIDE src/d/actor/d_a_alink.cpp and read file-static state that
// stock dusklight does not have. See alink_compat.cpp for the full port note.
// ============================================

class daAlink_c;

void dAlbwAlink_resyncClothesEpoch();
void dAlbwAlink_invalidateClothesEpoch();
void dAlbwAlink_requestClothesRemount();
bool dAlbwAlink_nativeCapResolved();

// True when the clothes models were built in the CURRENT arc epoch (the arc
// heap has not been freed under them). fork d_a_alink.cpp:21536.
bool dAlbwAlink_clothesEpochInSync();
void dAlbwAlink_abortStuckClothesChange(daAlink_c* link);

// True once the abort path has fired at least once (a stuck clothes swap was
// observed). Surfaced so the outfit module's behaviour is auditable in the log
// rather than silently differing from the fork.
bool dAlbwAlink_sawStuckClothesChange();
