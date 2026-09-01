#pragma once

// ============================================
// NEW CODE - ALBW Port (Colossal Wallet, 4th wallet tier)
// Ported from the fork, which spreads this across three stock files:
//   f_ap/f_ap_game.cpp:886   auto-grant on Cave of Ordeals clear (bit 0x1F9)
//   d/d_save.cpp:152         getRupeeMax() returns COLOSSAL_WALLET_MAX for it
//   d/d_item.cpp:866         item_func_WALLET_LV4 (save-editor completeness)
// Stock's wallet enum stops at GIANT_WALLET (2); COLOSSAL_WALLET is 3.
// Unblocks the shop's Deity Armor row, whose eligibility requires this tier.
// ============================================

#include "mods/api.h"

ModResult albw_colossal_wallet_init(ModError* error);
void albw_colossal_wallet_tick();

// fork d_save.h:66 - the tier value, and d_save.h:44 - its rupee cap.
static const int ALBW_COLOSSAL_WALLET = 3;
static const int ALBW_COLOSSAL_WALLET_MAX = 50000;
