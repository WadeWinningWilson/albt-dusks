#pragma once

#include "mods/api.h"

ModResult albw_rental_postman_hooks_init(ModError* error);
ModResult albw_rental_postman_hooks_shutdown(ModError* error);
void albw_rental_postman_tick();
