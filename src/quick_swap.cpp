#include "quick_swap.h"

#include "global.h"

#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_meter2_info.h"
#include "d/d_save.h"
#include "m_Do/m_Do_controller_pad.h"

#define private public
#include "d/actor/d_a_alink.h"
#undef private

#include "Z2AudioLib/Z2AudioMgr.h"
#include "Z2AudioLib/Z2SeMgr.h"

#include "albw_game.h"
#include "albw_common.h"
#include "config_vars.h"
#include "extra_item_slot.h"
#include "outfit.h"
#include "shield_game.h"

namespace {

static constexpr u8 kShieldOrder[] = {dItemNo_WOOD_SHIELD_e, dItemNo_SHIELD_e,
                                      dItemNo_HYLIA_SHIELD_e};

static bool shield_is_item(u8 itemNo) {
    return itemNo == dItemNo_WOOD_SHIELD_e || itemNo == dItemNo_SHIELD_e ||
           itemNo == dItemNo_HYLIA_SHIELD_e;
}

// Mirror dComIfGs_setSelectEquipShield collect bits (save bitfield, not replace).
static void set_collect_shield_for_item(u8 itemNo) {
    switch (itemNo) {
    case dItemNo_WOOD_SHIELD_e:
        g_dComIfG_gameInfo.info.getPlayer().getCollect().setCollect(COLLECT_SHIELD,
                                                                    COLLECT_WOODEN_SHIELD);
        break;
    case dItemNo_SHIELD_e:
        g_dComIfG_gameInfo.info.getPlayer().getCollect().setCollect(COLLECT_SHIELD,
                                                                    COLLECT_ORDON_SHIELD);
        break;
    case dItemNo_HYLIA_SHIELD_e:
        g_dComIfG_gameInfo.info.getPlayer().getCollect().setCollect(COLLECT_SHIELD,
                                                                    COLLECT_HYLIAN_SHIELD);
        break;
    default:
        break;
    }
}

bool shield_is_owned(u8 itemNo) {
    if (!shield_is_item(itemNo)) {
        return false;
    }
    if (albw_game::is_item_first_bit(itemNo)) {
        return true;
    }
    // Save editor may equip without syncing the first bit.
    return albw_game::select_equip_shield() == itemNo;
}

u8 next_owned_shield(u8 current) {
    int start = 0;
    for (int i = 0; i < 3; ++i) {
        if (kShieldOrder[i] == current) {
            start = i + 1;
            break;
        }
    }
    for (int step = 0; step < 3; ++step) {
        const u8 candidate = kShieldOrder[(start + step) % 3];
        if (candidate != current && shield_is_owned(candidate)) {
            return candidate;
        }
    }
    return dItemNo_NONE_e;
}

// Port of fork dMeter2_equipOwnedShield — never invent first bits; sync collect +
// select equip locally (avoid relying on DUSK_NOINLINE dMeter2Info_setShield).
bool equip_owned_shield(u8 itemNo) {
    if (!shield_is_item(itemNo) || itemNo == dItemNo_NONE_e) {
        return false;
    }
    if (albw_game::select_equip_shield() == itemNo) {
        return false;
    }
    if (!shield_is_owned(itemNo)) {
        return false;
    }

    daAlink_c* link = static_cast<daAlink_c*>(albw_game::link_player());
    if (link != nullptr && link->getShieldChangeWaitTimer() != 0) {
        return false;
    }

    set_collect_shield_for_item(itemNo);
    albw_shield_game::set_shield(itemNo, false);

    if (link != nullptr) {
        link->setShieldChange();
    }
    return true;
}

bool can_use_quick_swap() {
    if (!albw_is_dpad_quick_swap_enabled()) {
        return false;
    }

    const int heapLock = g_dComIfG_gameInfo.play.isHeapLockFlag();
    if (heapLock != 0 && heapLock != 5) {
        return false;
    }

    if (g_dComIfG_gameInfo.play.isPauseFlag() || g_dComIfG_gameInfo.play.getMesgStatus() != 0 ||
        g_dComIfG_gameInfo.play.isEnableNextStage())
    {
        return false;
    }

    return true;
}

bool is_owned_sword(u8 itemNo) {
    return albw_game::is_item_first_bit(itemNo);
}

void cycle_next_sword() {
    if (!can_use_quick_swap()) {
        return;
    }

    daPy_py_c* player = albw_game::link_player();
    if (player == nullptr || player->checkWolf()) {
        return;
    }

    if (player->getSwordChangeWaitTimer() != 0) {
        return;
    }

    static constexpr u8 kOrder[] = {dItemNo_WOOD_STICK_e, dItemNo_SWORD_e, dItemNo_MASTER_SWORD_e,
                                    dItemNo_LIGHT_SWORD_e};
    const u8 current = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(
        COLLECT_SWORD);
    u8 next = current;
    int start = 0;
    for (int i = 0; i < 4; ++i) {
        if (kOrder[i] == current) {
            start = i + 1;
            break;
        }
    }
    for (int step = 0; step < 4; ++step) {
        const u8 candidate = kOrder[(start + step) % 4];
        if (is_owned_sword(candidate) && candidate != current) {
            next = candidate;
            break;
        }
    }
    if (next == current || next == dItemNo_NONE_e) {
        return;
    }

    dMeter2Info_setSword(next, false);
    Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    dMeter2Info_set2DVibration();
}

void cycle_next_shield() {
    if (!can_use_quick_swap()) {
        return;
    }

    daPy_py_c* player = albw_game::link_player();
    if (player == nullptr || player->checkWolf()) {
        return;
    }

    if (player->getShieldChangeWaitTimer() != 0) {
        return;
    }

    const u8 current = albw_game::select_equip_shield();
    const u8 next = next_owned_shield(current);
    if (next == dItemNo_NONE_e || next == current) {
        return;
    }
    if (!equip_owned_shield(next)) {
        return;
    }

    Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    dMeter2Info_set2DVibration();
}

void cycle_next_outfit() {
    if (!can_use_quick_swap()) {
        return;
    }

    daPy_py_c* player = albw_game::link_player();
    if (player == nullptr || player->checkWolf()) {
        return;
    }
    if (player->getClothesChangeWaitTimer() != 0) {
        return;
    }
    if (albw_outfit_is_swap_blocked()) {
        Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_USE_CANCEL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        return;
    }

    const AlbwOutfitKind current = albw_outfit_get_active();
    const AlbwOutfitKind next = albw_outfit_get_next_owned(current);
    if (next == current) {
        return;
    }
    if (!albw_outfit_equip(next)) {
        return;
    }

    Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    dMeter2Info_set2DVibration();
}

}  // namespace

void albw_quick_swap_tick() {
    if (!can_use_quick_swap()) {
        return;
    }
    if (mDoCPd_c::getTrigUp(PAD_1) != 0) {
        cycle_next_sword();
    }
    if (mDoCPd_c::getTrigRight(PAD_1) != 0) {
        cycle_next_shield();
    }
    if (mDoCPd_c::getTrigDown(PAD_1) != 0) {
        cycle_next_outfit();
    }
}

ModResult albw_quick_swap_init(ModError*) {
    return MOD_OK;
}

ModResult albw_quick_swap_shutdown(ModError*) {
    return MOD_OK;
}
