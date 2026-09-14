#pragma once

void albw_boss_hp_hud_draw();

// True for profiles that get the dedicated boss bar (so the per-enemy overlay can
// skip them and not double up). Defined in boss_hp_hud.cpp.
bool albw_boss_hp_is_boss_profile(short profName);
