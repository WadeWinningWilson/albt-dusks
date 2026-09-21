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
// WHERE IT LIVES: config.json, via ConfigService - NOT the player's save file.
// The mod owns that file, so uninstalling takes our state with it and the save
// is never written at all. See the block comment in the .cpp for the store's
// mechanics and for the one real trade (config.json is per-INSTALL, not
// per-save-file).
//
// v0.2.7 shipped an interim version of this that used event registers 114-118.
// That fixed the corruption - those bytes are outside anything stock reads -
// but it was still writing to the save. Anyone who played 0.2.7 has flags in
// those registers which this version does not read; they re-arm through normal
// play, and the stale bytes are inert.
//
// NOT A MIGRATION. The original saveBitLabels bits are deliberately left
// untouched too. A set F_0686 is indistinguishable from a player who
// legitimately earned the Fused Shadow, so clearing it would take away real
// progress to tidy up our own mess.
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

    // Focused Arts IV / Flurry Rush. Was a session-only static, from before
    // this allocator existed and config.json was an option - which meant
    // re-buying it every launch. It is a purchase, so it persists; and it
    // persists HERE, not in the save, like every other mod-owned flag.
    ALBW_FLAG_FLURRY_TIER,

    ALBW_FLAG_COUNT,
};

// A negative flag reads false and writes nothing, so callers that map an
// unknown item to -1 need no extra guard.
bool albw_save_flag_get(int flag);
void albw_save_flag_set(int flag, bool on);

// ============================================
// COUNTERS - the same store, for the small 0-255 progression counters that
// used to live in event registers 100-113.
//
// Those registers were SAFE (100-234 is outside anything stock reads, unlike
// the saveBitLabels disaster) but they were still bytes in the player's save
// file. Every one of them is read back only by the mod and applied through a
// mod hook - mq_hearts, for instance, never writes the stock heart count, it
// hooks dComIfGs_getMaxLifeGauge and adds its bonus at read time
// (mq_hearts.cpp:111) - so nothing in the game engine needs to see them and
// they belong in config.json with the flags.
//
// They get one NAMED config var each rather than being packed, so config.json
// stays readable and a "New Game" reset can rewrite them individually.
//
// NOT MOVED, deliberately: anything STOCK reads stays in the save. Granting a
// purchased item, spending rupees, draining life, setting a bottle or an equip
// slot are all invisible to the engine if written to config.json - that would
// not remove a write, it would remove the effect.
// ============================================
enum AlbwSaveCounter {
    ALBW_CTR_FA_TIER = 0,          // was event reg 103
    ALBW_CTR_POTION_TIER,          // was 105
    ALBW_CTR_HEART_SHOP_TIER,      // was 100
    ALBW_CTR_METER_SHOP_TIER,      // was 101
    ALBW_CTR_BONUS_HALF_HEARTS,    // was 102
    ALBW_CTR_BONUS_QUARTER_HEARTS, // was 104

    // Four swords x (bonus, step) - were 106-109 and 110-113. Indexed by
    // swordId, so these two must stay contiguous and in order.
    ALBW_CTR_SWORD_ATP_BONUS_0,
    ALBW_CTR_SWORD_ATP_BONUS_3 = ALBW_CTR_SWORD_ATP_BONUS_0 + 3,
    ALBW_CTR_SWORD_ATP_STEP_0,
    ALBW_CTR_SWORD_ATP_STEP_3 = ALBW_CTR_SWORD_ATP_STEP_0 + 3,

    ALBW_CTR_COUNT,
};

// Clamped to 0-255, matching the u8 the event registers held. An out-of-range
// id reads 0 and writes nothing.
int  albw_save_counter_get(int counter);
void albw_save_counter_set(int counter, int value);
