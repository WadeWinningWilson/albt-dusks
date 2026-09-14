#pragma once

#include "mods/api.h"
#include "mods/svc/ui.h"

ModResult albw_stick_cycle_init(ModError* error);
ModResult albw_stick_cycle_shutdown(ModError* error);
ModResult albw_stick_cycle_build_panel(UiElementHandle panel, ModError* error);

ModResult albw_region_hp_init(ModError* error);
ModResult albw_region_hp_shutdown(ModError* error);
ModResult albw_region_hp_build_panel(UiElementHandle panel, ModError* error);

ModResult albw_soul_of_light_init(ModError* error);
ModResult albw_soul_of_light_shutdown(ModError* error);
ModResult albw_soul_of_light_build_panel(UiElementHandle panel, ModError* error);

ModResult albw_enemy_rupees_init(ModError* error);
ModResult albw_enemy_rupees_shutdown(ModError* error);
ModResult albw_enemy_rupees_build_panel(UiElementHandle panel, ModError* error);

ModResult albw_magic_jar_init(ModError* error);

// Enemy-death rupee "+n" HUD popup (rupee_popup.cpp, full fork port).
ModResult albw_rupee_popup_init(ModError* error);
void albw_rupee_popup_on_grant(unsigned short amount);

// Supplemental Tear-of-Light scene particles (tear_particles.cpp): loads the
// Pscene011 tear FX archive + slot-2 getRM_ID fallback so the recovery orb is
// visible in stages whose own scene archive lacks the tear FX.
ModResult albw_tear_particles_init(ModError* error);
bool albw_tear_ensure_scene_res();

// Public bridge: grant the one-time boss-defeat rupee reward for profName (deduped).
// Used by the armogohma whole-function port, which reproduces the fork's
// dAlbwEnemyRupees_tryGrantFightVictory call on the boss's death.
void albw_enemy_rupees_grant_fight_victory(short profName);
