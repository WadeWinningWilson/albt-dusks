#pragma once

#include "d/d_com_inf_game.h"
#include "d/d_stage.h"
#include "d/actor/d_a_player.h"
#include "SSystem/SComponent/c_cc_s.h"

// TARGET_PC game builds expose dComIfGs_* / dComIfGp_* as DUSK_NOINLINE exports only.
// Mod code must read through g_dComIfG_gameInfo so we link without game symbols.

namespace albw_game {

inline daPy_py_c* link_player() {
    return static_cast<daPy_py_c*>(g_dComIfG_gameInfo.play.getPlayer(0));
}

inline dCcS* cc_system() {
    return &g_dComIfG_gameInfo.play.mCcs;
}

inline const char* stage_name() {
    return g_dComIfG_gameInfo.play.getStartStageName();
}

inline int stay_room() {
    return dStage_roomControl_c::getStayNo();
}

inline dAttention_c* attention() {
    return g_dComIfG_gameInfo.play.getAttention();
}

inline bool is_event_bit(u16 flag) {
    return g_dComIfG_gameInfo.info.getEvent().isEventBit(flag) != 0;
}

inline void on_event_bit(u16 flag) {
    g_dComIfG_gameInfo.info.getEvent().onEventBit(flag);
}

inline void off_event_bit(u16 flag) {
    g_dComIfG_gameInfo.info.getEvent().offEventBit(flag);
}

inline bool is_item_first_bit(u8 itemNo) {
    return g_dComIfG_gameInfo.info.getPlayer().getGetItem().isFirstBit(itemNo) != 0;
}

inline u8 select_equip_shield() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(COLLECT_SHIELD);
}

inline u8 select_equip_sword() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(COLLECT_SWORD);
}

inline u8 get_event_reg(u16 reg) {
    return g_dComIfG_gameInfo.info.getEvent().getEventReg(reg);
}

inline void set_event_reg(u16 reg, u8 value) {
    g_dComIfG_gameInfo.info.getEvent().setEventReg(reg, value);
}

inline u16 max_life_gauge() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxLife();
}

inline bool is_dark_clear_lv(int idx) {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusB().isDarkClearLV(idx) != 0;
}

inline bool is_letter_get(int idx) {
    return g_dComIfG_gameInfo.info.getPlayer().getLetterInfo().isLetterGetFlag(idx) != 0;
}

inline bool is_switch(int sw, int room) {
    return g_dComIfG_gameInfo.info.isSwitch(sw, room) != 0;
}

inline void off_switch(int sw, int room) {
    g_dComIfG_gameInfo.info.offSwitch(sw, room);
}

inline void set_hit_mark(u16 mark, fopAc_ac_c* actor, const cXyz* pos, const csXyz* angle,
                         const cXyz* param4, u32 atType) {
    g_dComIfG_gameInfo.play.getParticle()->setHitMark(mark, actor, pos, angle, param4, atType);
}

inline bool is_wolf_form() {
    daPy_py_c* player = link_player();
    return player != nullptr && player->checkWolf();
}

inline bool sword_get() {
    return select_equip_sword() != dItemNo_NONE_e;
}

inline bool master_sword_equip() {
    const u8 sword = select_equip_sword();
    return sword == dItemNo_MASTER_SWORD_e || sword == dItemNo_LIGHT_SWORD_e;
}

inline void off_stage_boss_enemy() {
    g_dComIfG_gameInfo.info.getMemory().getBit().offStageBossEnemy();
}

inline void off_one_zone_switch(int swBit, int roomNo) {
    int room = roomNo;
    if (room < 0) {
        room = stay_room();
        if (room < 0) {
            return;
        }
    }
    const int zone = dStage_roomControl_c::getZoneNo(room);
    g_dComIfG_gameInfo.info.getZone(zone).getBit().offOneSwitch(swBit);
}

inline void on_one_zone_switch(int swBit, int roomNo) {
    int room = roomNo;
    if (room < 0) {
        room = stay_room();
        if (room < 0) {
            return;
        }
    }
    const int zone = dStage_roomControl_c::getZoneNo(room);
    g_dComIfG_gameInfo.info.getZone(zone).getBit().onOneSwitch(swBit);
}

inline bool is_one_zone_switch(int swBit, int roomNo) {
    int room = roomNo;
    if (room < 0) {
        room = stay_room();
        if (room < 0) {
            return false;
        }
    }
    const int zone = dStage_roomControl_c::getZoneNo(room);
    return g_dComIfG_gameInfo.info.getZone(zone).getBit().isOneSwitch(swBit) != 0;
}

inline JPABaseEmitter* particle_set(u16 resId, const cXyz* pos, const dKy_tevstr_c* tev,
                                    const csXyz* rot, const cXyz* scale) {
    return g_dComIfG_gameInfo.play.getParticle()->setNormal(resId, pos, tev, rot, scale, 255, nullptr,
                                                           -1, nullptr, nullptr, nullptr, 1.0f);
}

inline JPABaseEmitter* particle_get_emitter(u32 id) {
    return g_dComIfG_gameInfo.play.getParticle()->getEmitter(id);
}

inline void* object_res(const char* arcName, int index) {
    return g_dComIfG_gameInfo.mResControl.getObjectRes(arcName, index);
}

inline int evt_is_addvance(int staffId) {
    return g_dComIfG_gameInfo.play.getEvtManager().getIsAddvance(staffId);
}

inline int evt_my_act_idx(int staffId, DUSK_CONST char* DUSK_CONST* actions, int actionNum,
                          BOOL p3, BOOL p4) {
    return g_dComIfG_gameInfo.play.getEvtManager().getMyActIdx(staffId, actions, actionNum, p3, p4);
}

}  // namespace albw_game
