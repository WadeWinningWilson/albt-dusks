#include "global.h"
#include "albw_save_flags.h"

#include "rental_eligibility.h"

#include "albw_common.h"
#include "albw_game.h"
#include "config_vars.h"

#include "helpers/string.hpp"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"

namespace {

static constexpr u16 kPostmanUnlockFlag = 0x4c01u;  // F_0625 — Talo rescued

// ============================================
// SAVE-DATA FIX #2 - eligibility moved OUT of saveBitLabels entirely.
//
// WHAT WAS WRONG. The previous version stored eligibility in
// saveBitLabels[673..687], on the fork's assertion that "indices 673-784 are
// confirmed free in the TP save layout" (fork d_meter2.cpp:792). That claim is
// FALSE, and we inherited it without checking. saveBitLabels is not an address
// space - it is an index into 822 REAL designer flags
// (dusklight-main/src/d/d_save.cpp:2098), and every index we took has a
// meaning:
//     F_0673  Goron Mines - heard the hint about Fyrus's weakness
//     F_0674..F_0678, F_0683, F_0687  Castle Town NPC conversations
//     F_0679  Fishing Pond - "cheated during Roll goal game"
//     F_0680, F_0681  Arbiter's Grounds - Midna's hints
//     F_0682  City in the Sky - first Oocca shopkeeper conversation
//     F_0684  Temple of Time - looked at the R00 statue with sense
//     F_0685  Cutscene 32 - Sage appears, FIRST MIRROR OF TWILIGHT SHARD
//     F_0686  Palace of Twilight - GET FUSED SHADOW PIECE (final mask)
// (labels: dusklight-main/include/d/d_save_bit_labels.inc:562-576)
//
// Two of those are read by STOCK every frame, in dMeter2_c
// (dusklight-main/src/d/d_meter2.cpp:278-286):
//     if (!isCollectMirror(0)  && isEventBit(F_0685)) onCollectMirror(0);
//     if (!isCollectCrystal(3) && isEventBit(F_0686)) onCollectCrystal(3);
// so breaking two shields handed the player a Mirror of Twilight shard and the
// final Fused Shadow piece. The rest corrupt dialogue and hint state.
//
// WHERE IT LIVES NOW. The event REGISTER window. dSv_event_c::mEvent is 256
// bytes; stock occupies 0-99 (bits) and 235-255 (registers), leaving 100-234
// genuinely unused, which is where this mod's other fourteen registers already
// live without incident. setEventReg encodes as `index << 8 | mask`
// (dusklight-main/src/d/d_save.cpp), so a register byte addresses individual
// bits and fifteen flags fit in two bytes. 114 and 115 are the next free ones
// after sword_atp's 106-113.
//
// WHAT IS DELIBERATELY *NOT* DONE: the old bits are not cleared. Unlike the
// wallet tier - an illegal value that could only have come from us - a set
// F_0686 is indistinguishable from a player who legitimately earned the Fused
// Shadow. Clearing them would TAKE AWAY real progress to tidy up our own mess,
// which is the worse error. They are left alone; eligibility simply re-arms on
// the next death strip or shield break. Players affected by the old build keep
// whatever those bits granted them.
// ============================================
// Allocation comes from the shared allocator (albw_save_flags.h) rather than
// being chosen here. Per-file allocation is what produced the collision in the
// first place, so the layout is derived, never picked.
int rentalEligibleFlag(int idx) { return ALBW_FLAG_RENTAL_0 + idx; }
int shieldEligibleFlag(int idx) { return ALBW_FLAG_SHIELD_0 + idx; }

static const u8 kRentalItems[] = {
    (u8)dItemNo_BOOMERANG_e,    (u8)dItemNo_SPINNER_e,      (u8)dItemNo_BOW_e,
    (u8)dItemNo_IRONBALL_e,     (u8)dItemNo_COPY_ROD_e,     (u8)dItemNo_HOOKSHOT_e,
    (u8)dItemNo_W_HOOKSHOT_e,   (u8)dItemNo_BOMB_BAG_LV1_e, (u8)dItemNo_BOMB_BAG_LV2_e,
    (u8)dItemNo_POKE_BOMB_e,    (u8)dItemNo_PACHINKO_e,     (u8)dItemNo_ARMOR_e,
};

static const u8 kShieldRentalItems[] = {
    (u8)dItemNo_WOOD_SHIELD_e,
    (u8)dItemNo_SHIELD_e,
    (u8)dItemNo_HYLIA_SHIELD_e,
};


int rentalIndex(u8 itemNo) {
    for (int i = 0; i < 12; ++i) {
        if (kRentalItems[i] == itemNo) {
            return i;
        }
    }
    return -1;
}

int shieldRentalIndex(u8 itemNo) {
    for (int i = 0; i < 3; ++i) {
        if (kShieldRentalItems[i] == itemNo) {
            return i;
        }
    }
    return -1;
}

void clearItemFromAllSlots(u8 itemNo) {
    for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
        if (g_dComIfG_gameInfo.info.getPlayer().getItem().getItem(slot, false) == itemNo) {
            g_dComIfG_gameInfo.info.getPlayer().getItem().setItem(slot, dItemNo_NONE_e);
        }
    }
}

bool slotHasPossessionForm(u8 rentalItemNo) {
    if (albw_game::is_item_first_bit(rentalItemNo)) {
        return true;
    }
    for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
        if (g_dComIfG_gameInfo.info.getPlayer().getItem().getItem(slot, false) == rentalItemNo) {
            return true;
        }
    }
    if (rentalItemNo == (u8)dItemNo_BOMB_BAG_LV1_e) {
        for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
            if (g_dComIfG_gameInfo.info.getPlayer().getItem().getItem(slot, false) ==
                (u8)dItemNo_NORMAL_BOMB_e)
            {
                return true;
            }
        }
    } else if (rentalItemNo == (u8)dItemNo_BOMB_BAG_LV2_e) {
        for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
            if (g_dComIfG_gameInfo.info.getPlayer().getItem().getItem(slot, false) ==
                (u8)dItemNo_WATER_BOMB_e)
            {
                return true;
            }
        }
    } else if (rentalItemNo == (u8)dItemNo_COPY_ROD_e) {
        for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
            if (g_dComIfG_gameInfo.info.getPlayer().getItem().getItem(slot, false) ==
                (u8)dItemNo_COPY_ROD_2_e)
            {
                return true;
            }
        }
    }
    return false;
}

void clearAllPossessionForms(u8 rentalItemNo) {
    clearItemFromAllSlots(rentalItemNo);
    g_dComIfG_gameInfo.info.getPlayer().getGetItem().offFirstBit(rentalItemNo);
    switch (rentalItemNo) {
    case (u8)dItemNo_BOMB_BAG_LV1_e:
        clearItemFromAllSlots((u8)dItemNo_NORMAL_BOMB_e);
        g_dComIfG_gameInfo.info.getPlayer().getGetItem().offFirstBit((u8)dItemNo_NORMAL_BOMB_e);
        break;
    case (u8)dItemNo_BOMB_BAG_LV2_e:
        clearItemFromAllSlots((u8)dItemNo_WATER_BOMB_e);
        g_dComIfG_gameInfo.info.getPlayer().getGetItem().offFirstBit((u8)dItemNo_WATER_BOMB_e);
        break;
    case (u8)dItemNo_COPY_ROD_e:
        clearItemFromAllSlots((u8)dItemNo_COPY_ROD_2_e);
        g_dComIfG_gameInfo.info.getPlayer().getGetItem().offFirstBit((u8)dItemNo_COPY_ROD_2_e);
        break;
    default:
        break;
    }
}

bool playerOwnsRentalItem(u8 itemNo) {
    if (itemNo == (u8)dItemNo_WOOD_SHIELD_e || itemNo == (u8)dItemNo_SHIELD_e ||
        itemNo == (u8)dItemNo_HYLIA_SHIELD_e)
    {
        return albw_game::select_equip_shield() == itemNo;
    }
    if ((itemNo == (u8)dItemNo_ARMOR_e || itemNo == (u8)dItemNo_WEAR_CASUAL_e) &&
        g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(0) == itemNo)
    {
        return true;
    }
    return slotHasPossessionForm(itemNo);
}

void stripRentalItemOnDeath(u8 itemNo) {
    if (itemNo == (u8)dItemNo_ARMOR_e) {
        const u8 clothes =
            g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(0);
        const bool worn = clothes == itemNo;
        const bool owned = albw_game::is_item_first_bit(itemNo);
        if (!worn && !owned) {
            return;
        }
        albw_rental_on_eligible(itemNo);
        g_dComIfG_gameInfo.info.getPlayer().getGetItem().offFirstBit(itemNo);
        if (worn) {
            g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setSelectEquip(
                0, (u8)dItemNo_WEAR_KOKIRI_e);
        }
        return;
    }

    if (!playerOwnsRentalItem(itemNo)) {
        return;
    }
    albw_rental_on_eligible(itemNo);
    clearAllPossessionForms(itemNo);
}

}  // namespace

// ============================================
// NEW CODE - ALBT multiplatform
// Fork-named entry point onto playerOwnsRentalItem() for the ported outfit
// cluster (fork d_meter2.cpp:902 dMeter2_playerOwnsRentalItem). The impl lives
// in the anonymous namespace above, so it needs this external-linkage wrapper -
// a header declaration alone would compile and then fail at link.
// ============================================
bool albw_rental_player_owns_item(u8 itemNo) { return playerOwnsRentalItem(itemNo); }

// ============================================
// True ALBW (fork dusk::truetest::isTrueAlbwShopEnabled): when on, the Postman
// rental shop is unlocked at ANY point and its full catalog is available, without
// the story gate (Ravio's-shop style). The fork keys this off a game setting; the
// mod keys it off the True ALBW config toggle.
// ============================================
bool albw_is_true_albw_enabled() {
    return albw_cfg_bool(g_true_albw, false);
}

bool albw_rental_postman_unlocked() {
    // True ALBW bypasses the F_0625 (Talo-rescued) story gate.
    return albw_is_true_albw_enabled() || albw_game::is_event_bit(kPostmanUnlockFlag);
}

void albw_rental_on_eligible(u8 itemNo) {
    const int idx = rentalIndex(itemNo);
    if (idx >= 0) {
        albw_save_flag_set(rentalEligibleFlag(idx), true);
    }
}

bool albw_rental_is_eligible(u8 itemNo) {
    const int idx = rentalIndex(itemNo);
    if (idx < 0) {
        return false;
    }
    return albw_save_flag_get(rentalEligibleFlag(idx));
}

void albw_rental_on_shield_eligible(u8 itemNo) {
    const int idx = shieldRentalIndex(itemNo);
    if (idx >= 0) {
        albw_save_flag_set(shieldEligibleFlag(idx), true);
    }
}

bool albw_rental_is_shield_eligible(u8 itemNo) {
    const int idx = shieldRentalIndex(itemNo);
    if (idx < 0) {
        return false;
    }
    return albw_save_flag_get(shieldEligibleFlag(idx));
}

void albw_rental_strip_all_on_death() {
    if (!albw_cfg_bool(g_postman_rental, true)) {
        return;
    }
    for (int i = 0; i < 12; ++i) {
        stripRentalItemOnDeath(kRentalItems[i]);
    }
}
