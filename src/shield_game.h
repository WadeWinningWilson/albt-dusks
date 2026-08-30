#pragma once

// Direct g_dComIfG access — avoids linking DUSK_NOINLINE dComIf* exports from the stub.

#include "d/d_com_inf_game.h"
#include "d/d_meter2.h"

class dMeter2_c;

namespace albw_shield_game {

void set_meter_class(dMeter2_c* meter);
dMeter2_c* get_meter_class();

inline u8 get_select_equip_shield() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(COLLECT_SHIELD);
}

inline int is_item_first_bit(u8 item_no) {
    return g_dComIfG_gameInfo.info.getPlayer().getGetItem().isFirstBit(item_no);
}

inline void off_item_first_bit(u8 item_no) {
    g_dComIfG_gameInfo.info.getPlayer().getGetItem().offFirstBit(item_no);
}

inline void set_select_equip_shield(u8 item_no) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setSelectEquip(COLLECT_SHIELD, item_no);
    g_dComIfG_gameInfo.play.setSelectEquip(COLLECT_SHIELD, item_no);
}

inline JKRArchive* get_main_2d_archive() {
    return g_dComIfG_gameInfo.play.getMain2DArchive();
}

inline JKRArchive* get_item_icon_archive() {
    return g_dComIfG_gameInfo.play.getItemIconArchive();
}

inline J2DGrafContext* get_current_graf_port() {
    return g_dComIfG_gameInfo.play.getCurrentGrafPort();
}

inline u8 is_pause_flag() {
    return g_dComIfG_gameInfo.play.isPauseFlag();
}

inline u8 is_heap_lock_flag() {
    return g_dComIfG_gameInfo.play.isHeapLockFlag();
}

inline void set_hit_mark(u16 hitmark, fopAc_ac_c* actor, const cXyz* pos, const csXyz* angle,
                        const cXyz* scale, u32 param_5) {
    g_dComIfG_gameInfo.play.getParticle()->setHitMark(hitmark, actor, pos, angle, scale, param_5);
}

inline void set_shield(u8 item_id, bool off_item_bit) {
    u8 shield_id = item_id;
    switch (shield_id) {
    case dItemNo_NONE_e:
    case dItemNo_WOOD_SHIELD_e:
    case dItemNo_SHIELD_e:
    case dItemNo_HYLIA_SHIELD_e:
        break;
    default:
        shield_id = dItemNo_NONE_e;
        off_item_bit = false;
        break;
    }

    if (off_item_bit && get_select_equip_shield() != 0xFF) {
        off_item_first_bit(get_select_equip_shield());
    }

    set_select_equip_shield(shield_id);
}

}  // namespace albw_shield_game
