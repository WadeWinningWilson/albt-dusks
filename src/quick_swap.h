#pragma once

#include "mods/api.h"

ModResult albw_quick_swap_init(ModError* error);
ModResult albw_quick_swap_shutdown(ModError* error);
void albw_quick_swap_tick();
