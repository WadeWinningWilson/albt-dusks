#pragma once

#include "mods/api.h"

/** Hide Z-slot item panes / reset cache so stock Midna Z button is safe after disable. */
void albw_z_item_hud_restore_stock();

ModResult albw_z_item_hud_hooks_init(ModError* error);
ModResult albw_z_item_hud_hooks_shutdown(ModError* error);
