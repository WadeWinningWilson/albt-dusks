// Ported VERBATIM from the fork's src/d/d_albw_potion.cpp.
// TARGET_PC is defined in the mod build (see shield.cpp), so the guards are kept
// exactly as the fork has them - including the #else branch, which must stay
// inert rather than be deleted or accidentally activated.
// The only edits are include paths and ABI translations noted inline.

// ============================================
// NEW CODE — ALBW Port
// Soulbound red potion bottle (SLOT_11).
// ============================================

#include "helpers/string.hpp"  // TEXT_SPAN - must precede any d_save.h include
#include "potion.h"
#include "albw_common.h"
#include "albw_game.h"
#include "albw_symbols.h"
#include "config_vars.h"
#include "mods/svc/hook.hpp"

#if TARGET_PC

#include "shield.h"
#include "d/d_com_inf_game.h"


// ============================================
// NEW CODE - ALBW Port
// dComIfGp_setSelectItem is DUSK_NOINLINE with real logic (not a data accessor),
// so it cannot go through albw_game.h and is absent from the Windows stub.
// Resolve by name at runtime, per-platform via albw_symbols.h - the same pattern
// rental_postman_hooks.cpp uses for fopAcM_create.
// ============================================
namespace {
using SetSelectItemFn = void (*)(int);
SetSelectItemFn s_setSelectItem = nullptr;
bool ensure_set_select_item() {
    if (s_setSelectItem != nullptr) return true;
    if (svc_hook == nullptr) return false;
    void* addr = nullptr;
    if (svc_hook->resolve(mod_ctx, ALBT_SYM_SET_SELECT_ITEM, &addr, nullptr) != MOD_OK ||
        addr == nullptr) return false;
    s_setSelectItem = reinterpret_cast<SetSelectItemFn>(addr);
    return true;
}
}  // namespace

namespace {

// Full byte of dSv_event_c::mEvent[] — index 105 (confirmed unused by vanilla).
static constexpr u16 kAlbwPotionCapacityTierReg = static_cast<u16>(105 << 8) | 0xFF;
static constexpr u8 kAlbwPotionCapacityShopTiers = 1;
static constexpr int kAlbwPotionCapacityShopPrice = 80;

static u8 readCapacityTier() {
    const int tier = albw_game::get_event_reg(kAlbwPotionCapacityTierReg);
    if (tier < 0) {
        return 0;
    }
    if (tier > kAlbwPotionCapacityShopTiers) {
        return kAlbwPotionCapacityShopTiers;
    }
    return static_cast<u8>(tier);
}

static void writeCapacityTier(u8 tier) {
    albw_game::set_event_reg(kAlbwPotionCapacityTierReg, tier);
}

}  // namespace

bool dAlbwPotion_isSoulboundBottleSlot(u8 slot) {
    return slot == kAlbwPotionSoulboundSlot;
}

bool dAlbwPotion_isSoulboundRedItem(u8 itemNo) {
    return itemNo == dItemNo_RED_BOTTLE_e || itemNo == dItemNo_RED_BOTTLE_2_e ||
           itemNo == dItemNo_CHUCHU_RED_e;
}

bool dAlbwPotion_isSoulboundRedInSlot(u8 slot) {
    // ============================================
    // FUNDAMENTAL FIX (deliberate divergence from the fork): gate the whole
    // soulbound behavior on the feature toggle. SLOT_11 is the FIRST VANILLA
    // BOTTLE SLOT; without this gate, ANY ordinary red potion / red chu jelly
    // there armed every soulbound seam (count getters/setters SKIP the stock
    // originals) even with all toggles off - breaking vanilla item assignment
    // and menu close. The fork ships the same ungated predicate, but there the
    // branches are compiled INTO the engine functions as inherent fork behavior;
    // a stock-side mod must be opt-in. Every soulbound seam (count hooks, ring
    // counts, drink dispatch, shop row, refills) self-gates through this
    // predicate, so this one check restores vanilla behavior when off.
    // ============================================
    if (!albw_cfg_bool(g_soulbound_potion, false)) {
        return false;
    }
    if (!dAlbwPotion_isSoulboundBottleSlot(slot)) {
        return false;
    }

    const u8 item = albw_game::get_item(slot, true);
    return dAlbwPotion_isSoulboundRedItem(item);
}

u8 dAlbwPotion_getMaxUses() {
    if (readCapacityTier() >= 1) {
        return kAlbwPotionCapacityUpgradeUses;
    }
    return kAlbwPotionDefaultStartCharges;
}

s16 dAlbwPotion_getHealQuarters(int i_selItemIdx, u8 i_itemNo) {
    if (!dAlbwPotion_isSoulboundRedItem(i_itemNo)) {
        return 32;
    }

    const u8 slot = albw_game::get_select_item_index(i_selItemIdx);
    if (!dAlbwPotion_isSoulboundBottleSlot(slot)) {
        return 32;
    }

    // Tier 0 soulbound bottle; shop upgrades can raise this later.
    return kAlbwPotionDefaultHealQuarters;
}

static void sync_select_items_for_slot(u8 slot) {
    for (int j = 0; j < 3; j++) {
        if (albw_game::get_select_item_index(j) == slot) {
            if (ensure_set_select_item()) {
                s_setSelectItem(j);
            }
        }
    }
}

bool dAlbwPotion_canDrinkSelectItem(int i_selItemIdx, u8 i_itemNo) {
    // FUNDAMENTAL FIX: with the feature off this must ALWAYS be true - a vanilla
    // red potion in bottle slot 1 otherwise read the mod's charge counter (0 on a
    // vanilla save) and drinking was blocked entirely.
    if (!albw_cfg_bool(g_soulbound_potion, false)) {
        return true;
    }
    if (!dAlbwPotion_isSoulboundRedItem(i_itemNo)) {
        return true;
    }

    const u8 slot = albw_game::get_select_item_index(i_selItemIdx);
    if (!dAlbwPotion_isSoulboundBottleSlot(slot)) {
        return true;
    }

    return albw_game::get_bottle_num(kAlbwPotionSoulboundBottleIdx) > 0;
}

void dAlbwPotion_consumeSoulboundDrink(int i_selItemIdx) {
    const u8 slot = albw_game::get_select_item_index(i_selItemIdx);
    if (!dAlbwPotion_isSoulboundRedInSlot(slot)) {
        return;
    }

    if (albw_game::get_bottle_num(kAlbwPotionSoulboundBottleIdx) > 0) {
        albw_game::add_bottle_num(kAlbwPotionSoulboundBottleIdx, -1);
    }

    // Soulbound drink → +20% equipped shield max durability (when durability On).
    dShield_repairDurabilityFraction(1, 5);

    sync_select_items_for_slot(slot);
}

void dAlbwPotion_refillSoulboundToMax() {
    if (!dAlbwPotion_isSoulboundRedInSlot(kAlbwPotionSoulboundSlot)) {
        return;
    }

    // Unconditional top-up to the current max charges (death respawn + Shade
    // Watcher rest). Refills a partial count, not just a fully-empty bottle.
    albw_game::set_bottle_num(kAlbwPotionSoulboundBottleIdx, dAlbwPotion_getMaxUses());
    sync_select_items_for_slot(kAlbwPotionSoulboundSlot);
}

void dAlbwPotion_applyDefaultInventorySlot11() {
    albw_game::set_item(kAlbwPotionSoulboundSlot, dItemNo_RED_BOTTLE_e);
    albw_game::set_bottle_num(kAlbwPotionSoulboundBottleIdx, dAlbwPotion_getMaxUses());
    albw_game::on_item_first_bit(dItemNo_RED_BOTTLE_e);
    sync_select_items_for_slot(kAlbwPotionSoulboundSlot);
}

void dAlbwPotion_editorSetSoulboundEnabled(bool enabled) {
    if (enabled) {
        dAlbwPotion_applyDefaultInventorySlot11();
        return;
    }

    const u8 item = albw_game::get_item(kAlbwPotionSoulboundSlot, true);
    if (dAlbwPotion_isSoulboundRedItem(item)) {
        albw_game::set_item(kAlbwPotionSoulboundSlot, dItemNo_EMPTY_BOTTLE_e);
        albw_game::set_bottle_num(kAlbwPotionSoulboundBottleIdx, 0);
        sync_select_items_for_slot(kAlbwPotionSoulboundSlot);
    }
}

bool dAlbwPotion_shouldShowCapacityShopRow() {
    return dAlbwPotion_isSoulboundRedInSlot(kAlbwPotionSoulboundSlot);
}

bool dAlbwPotion_canPurchaseCapacityShop() {
    if (!dAlbwPotion_shouldShowCapacityShopRow()) {
        return false;
    }
    return readCapacityTier() < kAlbwPotionCapacityShopTiers;
}

int dAlbwPotion_getCapacityShopPrice() {
    if (!dAlbwPotion_canPurchaseCapacityShop()) {
        return 0;
    }
    return kAlbwPotionCapacityShopPrice;
}

bool dAlbwPotion_tryPurchaseCapacityShop() {
    if (!dAlbwPotion_canPurchaseCapacityShop()) {
        return false;
    }

    writeCapacityTier(static_cast<u8>(readCapacityTier() + 1));

    if (dAlbwPotion_isSoulboundRedInSlot(kAlbwPotionSoulboundSlot)) {
        albw_game::set_bottle_num(kAlbwPotionSoulboundBottleIdx, dAlbwPotion_getMaxUses());
        sync_select_items_for_slot(kAlbwPotionSoulboundSlot);
    }

    return true;
}

const char* dAlbwPotion_getCapacityShopName() {
    return "Red Potion Capacity";
}

const char* dAlbwPotion_getCapacityShopDesc() {
    if (!dAlbwPotion_canPurchaseCapacityShop()) {
        return "Sold out.";
    }
    return "I can expand that vessel for you if you like. My father taught me all "
           "about his craft....maybe I should have followed in his footsteps...";
}

#endif
