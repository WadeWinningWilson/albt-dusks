#pragma once

#include "mods/api.h"

ModResult albw_meter_init(ModError* error);
ModResult albw_meter_shutdown(ModError* error);
void albw_meter_update();
