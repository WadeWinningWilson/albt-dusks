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

ModResult albw_confuse_init(ModError* error);
