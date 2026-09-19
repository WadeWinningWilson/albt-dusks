#ifndef ALBW_END_GAME_TRANSFORM_H
#define ALBW_END_GAME_TRANSFORM_H

// ============================================
// End-Game Midna Transform (port of fork editor "End-Game Transform (Midna + Crystal)")
// Grants Link the free wolf transform by setting the same save state the fork sets:
//   event bit M_077 (Get shadow crystal - can now transform)
//   event bit F_0250 (Midna revived / Hyrule Castle barrier)
//   transform LV 0..3
// Enabled ONLY by its own config toggle (g_end_game_transform). It is NOT tied to
// True ALBW: True ALBW is shop-unlocks only and must never write story save bits.
// ============================================
void albw_end_game_transform_tick();

#endif  // ALBW_END_GAME_TRANSFORM_H
