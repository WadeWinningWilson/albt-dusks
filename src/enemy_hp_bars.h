#pragma once

// Per-enemy floating HP bars over regular (non-boss) enemies. Call from the 2D
// meter-draw-post seam (alongside the boss bar). Gated by g_enemy_hp_bars.
void albw_enemy_hp_bars_draw();
