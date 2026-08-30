#pragma once

#include "mods/api.h"

enum AlbwExtraItemSlotMode {
    ALBW_EXTRA_ITEM_SLOT_OFF = 0,
    ALBW_EXTRA_ITEM_SLOT_EXTRA_ONLY = 1,
    ALBW_EXTRA_ITEM_SLOT_EXTRA_AND_QUICK_SWAP = 2,
};

int albw_extra_item_slot_mode();
bool albw_is_extra_item_slot_enabled();
bool albw_is_dpad_quick_swap_enabled();
bool albw_quick_swap_field_active();

void albw_extra_item_slot_migrate_legacy_config();
void albw_extra_item_slot_config_tick();
void albw_extra_item_slot_sync_bools_from_mode();

ModResult albw_extra_item_slot_init(ModError* error);
ModResult albw_extra_item_slot_shutdown(ModError* error);
