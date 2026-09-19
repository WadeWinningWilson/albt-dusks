#pragma once

#include "mods/api.h"

// ============================================
// NEW CODE - ALBW Port (Soulbound Red Potion grant driver)
// Drives dAlbwPotion_editorSetSoulboundEnabled from the g_soulbound_potion config
// var: grants the SLOT_11 soulbound red potion when the toggle is on (idempotent —
// never overwrites accumulated charges), and removes it when turned off.
// ============================================
ModResult albw_potion_grant_init(ModError* error);
void albw_potion_grant_tick();
