#pragma once

#include "mods/api.h"

bool albw_quick_equip_enabled();
bool albw_quick_equip_session_active();
void albw_quick_equip_begin_session();
void albw_quick_equip_end_session();

/** Consume one-shot force-open for dMw_UP_TRIGGER (Extra slot UP post owns the override). */
bool albw_quick_equip_consume_force_up();

/** Call each frame from mod_update while Extra+QuickSwap can open the wheel. */
void albw_quick_equip_tick();

ModResult albw_quick_equip_init(ModError* error);
ModResult albw_quick_equip_shutdown(ModError* error);
