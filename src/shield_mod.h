#pragma once

#include "global.h"
#include "mods/api.h"

ModResult albw_shield_init(ModError* error);
ModResult albw_shield_shutdown(ModError* error);

void albw_shield_on_block_while_bombling();
void albw_shield_add_bash_charge(unsigned char amount);
