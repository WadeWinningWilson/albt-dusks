#pragma once

#include "global.h"

// ============================================
// NEW CODE - ALBT: the mod's persistent-flag allocator.
//
// WHY THIS EXISTS. Every mod-owned persistent flag used to be an ad-hoc index
// into dSv_event_flag_c::saveBitLabels, allocated per-file on the fork's claim
// that "indices 673-784 are confirmed free in the TP save layout" (fork
// d_meter2.cpp:792). That claim is false. saveBitLabels is not an address
// space - it indexes 822 REAL designer flags (dusklight-main/src/d/d_save.cpp:
// 2098, labels in include/d/d_save_bit_labels.inc). Indices we had taken
// included F_0685 and F_0686, which stock's dMeter2_c reads EVERY FRAME to
// grant the first Mirror of Twilight shard and the final Fused Shadow piece
// (dusklight-main/src/d/d_meter2.cpp:278-286), plus a dozen NPC-conversation
// and hint flags, plus - at 710-714 - indices that are REGISTERS rather than
// bits, corrupting in both directions.
//
// Three files allocated independently and none of them checked. A central
// allocator is the fix for the allocation bug, not just for the bad addresses:
// adding a flag now means adding an enumerator here, and the layout is derived
// rather than chosen.
//
// WHERE IT LIVES. dSv_event_c::mEvent is 256 bytes. Stock occupies 0-99 (bits)
// and 235-255 (registers); 100-234 is genuinely unused, and this mod's other
// fourteen registers (mq_hearts 100-102/104, focused arts 103, potion 105,
// sword_atp 106-113) already live there without incident. setEventReg encodes
// as `index << 8 | mask` (dusklight-main/src/d/d_save.cpp), so a register byte
// addresses individual bits.
//
// Base 114 is the next free byte after sword_atp's 106-113. Thirty-six flags
// occupy 114-118 and leave 119-234 free. If you add flags past ALBW_FLAG_COUNT
// growing beyond byte 234, the static_assert in the .cpp fires.
//
// NOT A MIGRATION. The old saveBitLabels bits are deliberately left untouched.
// A set F_0686 is indistinguishable from a player who legitimately earned the
// Fused Shadow, so clearing it would take away real progress to tidy up our
// own mess. Affected saves simply re-arm their mod flags through normal play.
// ============================================

enum AlbwSaveFlag {
    // Rental eligibility, one per entry of kRentalItems (rental_eligibility.cpp).
    ALBW_FLAG_RENTAL_0 = 0,
    ALBW_FLAG_RENTAL_11 = ALBW_FLAG_RENTAL_0 + 11,

    // Shield rental eligibility, one per entry of kShieldRentalItems.
    ALBW_FLAG_SHIELD_0,
    ALBW_FLAG_SHIELD_1,
    ALBW_FLAG_SHIELD_2,

    // Sumo outfit (sumo_test.cpp, outfit.cpp).
    ALBW_FLAG_SUMO_OWNED,
    ALBW_FLAG_SUMO_WRESTLER_MET,
    ALBW_FLAG_SUMO_WORN,

    // Outfit ownership stash (outfit.cpp).
    ALBW_FLAG_STASH_ORDON,
    ALBW_FLAG_STASH_HEROS,
    ALBW_FLAG_STASH_ZORA,
    ALBW_FLAG_STASH_MAGIC,
    ALBW_FLAG_STASH_DEITY,

    // Postman storage (wardrobe.cpp).
    ALBW_FLAG_STORE_WOOD_SWORD,
    ALBW_FLAG_STORE_ORDON_SWORD,
    ALBW_FLAG_STORE_MASTER_SWORD,
    ALBW_FLAG_STORE_LIGHT_SWORD,
    ALBW_FLAG_STORE_ORDON_SHIELD,
    ALBW_FLAG_STORE_WOODEN_SHIELD,
    ALBW_FLAG_STORE_HYLIAN_SHIELD,
    ALBW_FLAG_STORE_SUMO_OUTFIT,
    ALBW_FLAG_STORE_ORDON_OUTFIT,
    ALBW_FLAG_STORE_HEROS_OUTFIT,
    ALBW_FLAG_STORE_ZORA_OUTFIT,
    ALBW_FLAG_STORE_MAGIC_OUTFIT,
    ALBW_FLAG_STORE_DEITY_OUTFIT,

    ALBW_FLAG_COUNT,
};

// A negative flag reads false and writes nothing, so callers that map an
// unknown item to -1 need no extra guard.
bool albw_save_flag_get(int flag);
void albw_save_flag_set(int flag, bool on);
