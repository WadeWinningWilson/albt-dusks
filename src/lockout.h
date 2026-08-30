#pragma once

#include "mods/api.h"

struct fopAc_ac_c;

ModResult albw_lockout_init(ModError* error);
ModResult albw_lockout_shutdown(ModError* error);

void albw_lockout_on_begin();
void albw_lockout_on_end();

bool albw_lockout_can_fire_bow();
bool albw_lockout_can_fire_bomb_arrow();
bool albw_lockout_can_use_bombling();
bool albw_lockout_can_use_double_hookshot();

void albw_lockout_on_arrow_fired();
void albw_lockout_on_bomb_arrow_fired();
void albw_lockout_on_hookshot_fired();
void albw_lockout_on_bombling_deployed(fopAc_ac_c* bombling);

// True while lockout is active and the orbiting bombling actor still exists.
// Fork: dAlbwLockout_isBomblingActive — gates the +1 bash bonus on block/parry.
bool albw_lockout_is_bombling_active();

// When true, player insect-bomb spawns are rejected (bombling lockout gate).
void albw_lockout_set_block_insect_bomb_create(bool block);
