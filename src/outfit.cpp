#include "outfit.h"

#include "albw_game.h"

#define private public
#include "d/actor/d_a_alink.h"
#undef private

#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_meter2_info.h"

namespace {

static constexpr u8 kOutfitItems[ALBW_OUTFIT_COUNT] = {
    (u8)dItemNo_WEAR_CASUAL_e,
    (u8)dItemNo_WEAR_KOKIRI_e,
    (u8)dItemNo_WEAR_ZORA_e,
    (u8)dItemNo_ARMOR_e,
};

bool isOwnedKind(AlbwOutfitKind kind) {
    if (kind < 0 || kind >= ALBW_OUTFIT_COUNT) {
        return false;
    }
    const u8 item = kOutfitItems[kind];
    if (kind == ALBW_OUTFIT_HEROS) {
        return true;
    }
    return albw_game::is_item_first_bit(item);
}

AlbwOutfitKind kindFromClothes(u8 clothes) {
    for (int i = 0; i < ALBW_OUTFIT_COUNT; ++i) {
        if (kOutfitItems[i] == clothes) {
            return static_cast<AlbwOutfitKind>(i);
        }
    }
    return ALBW_OUTFIT_HEROS;
}

}  // namespace

AlbwOutfitKind albw_outfit_get_active() {
    const u8 clothes =
        g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(COLLECT_CLOTHING);
    return kindFromClothes(clothes);
}

AlbwOutfitKind albw_outfit_get_next_owned(AlbwOutfitKind current) {
    int owned = 0;
    AlbwOutfitKind only = current;
    for (int step = 1; step <= ALBW_OUTFIT_COUNT; ++step) {
        const AlbwOutfitKind candidate =
            static_cast<AlbwOutfitKind>((static_cast<int>(current) + step) % ALBW_OUTFIT_COUNT);
        if (isOwnedKind(candidate)) {
            only = candidate;
            owned++;
        }
    }
    return owned <= 1 ? current : only;
}

bool albw_outfit_is_swap_blocked() {
    daAlink_c* link = static_cast<daAlink_c*>(albw_game::link_player());
    if (link == nullptr) {
        return true;
    }
    return link->checkBootsOrArmorHeavy();
}

bool albw_outfit_equip(AlbwOutfitKind kind) {
    if (!isOwnedKind(kind)) {
        return false;
    }
    const u8 item = kOutfitItems[kind];
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setSelectEquip(COLLECT_CLOTHING, item);
    g_dComIfG_gameInfo.play.setSelectEquip(COLLECT_CLOTHING, item);
    daAlink_c* link = static_cast<daAlink_c*>(albw_game::link_player());
    if (link != nullptr) {
        link->setClothesChange(0);
    }
    return true;
}
