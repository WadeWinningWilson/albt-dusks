#include "global.h"

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
// SAVE-DATA FIX (reg-collision repair): eligibility storage now matches the FORK
// exactly — saveBitLabels[673+i] (fork d_meter2.cpp kRentalEligibleBase = 673,
// "indices 673-784 confirmed free in the TP save layout"). The dusk previously
// deviated onto event regs 106-120, which COLLIDED with sword_atp's fork-verbatim
// regs 106-113 (sword ATP writes marked rentals eligible and vice versa). Shields
// take the next free bits, 685-687. Old reg-stored eligibility is not migrated
// (the colliding bytes are unreliable); eligibility re-arms on the next death
// strip, and True ALBW is unaffected.
// ============================================
static constexpr int kRentalEligibleBase = 673;       // fork parity: 673..684
static constexpr int kShieldEligibleBase = 685;       // 685..687

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
        dComIfGs_onEventBit(dSv_event_flag_c::saveBitLabels[kRentalEligibleBase + idx]);
    }
}

bool albw_rental_is_eligible(u8 itemNo) {
    const int idx = rentalIndex(itemNo);
    if (idx < 0) {
        return false;
    }
    return albw_game::is_event_bit(dSv_event_flag_c::saveBitLabels[kRentalEligibleBase + idx]);
}

void albw_rental_on_shield_eligible(u8 itemNo) {
    const int idx = shieldRentalIndex(itemNo);
    if (idx >= 0) {
        dComIfGs_onEventBit(dSv_event_flag_c::saveBitLabels[kShieldEligibleBase + idx]);
    }
}

bool albw_rental_is_shield_eligible(u8 itemNo) {
    const int idx = shieldRentalIndex(itemNo);
    if (idx < 0) {
        return false;
    }
    return albw_game::is_event_bit(dSv_event_flag_c::saveBitLabels[kShieldEligibleBase + idx]);
}

void albw_rental_strip_all_on_death() {
    if (!albw_cfg_bool(g_postman_rental, true)) {
        return;
    }
    for (int i = 0; i < 12; ++i) {
        stripRentalItemOnDeath(kRentalItems[i]);
    }
}
