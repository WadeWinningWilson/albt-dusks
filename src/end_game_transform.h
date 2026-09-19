#ifndef ALBW_END_GAME_TRANSFORM_H
#define ALBW_END_GAME_TRANSFORM_H

// ============================================
// End-Game Transform - RUNTIME-ONLY free wolf transform, ZERO save writes.
// D-pad Down toggles human <-> wolf via the native forced-transform flags
// (onForceWolfChange / ERFLG0_UNK_1 + the ground-special dispatchers), the same
// mechanism Focused Arts' Mortal Draw window uses. No story bits, no transform
// LV, nothing persisted. Enabled ONLY by its own toggle (g_end_game_transform);
// it is NOT tied to True ALBW (shop-unlocks only, never writes save state).
// ============================================
void albw_end_game_transform_tick();

#endif  // ALBW_END_GAME_TRANSFORM_H
