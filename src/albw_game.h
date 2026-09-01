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

// ---- inventory / bottles (fork d_com_inf_game.cpp one-liners) ----
inline u8 get_item(int slotNo, int checkCombo = 1) {
    return g_dComIfG_gameInfo.info.getPlayer().getItem().getItem(slotNo, checkCombo);
}

inline void set_item(int slotNo, u8 itemNo) {
    g_dComIfG_gameInfo.info.getPlayer().getItem().setItem(slotNo, itemNo);
}

inline u8 get_bottle_num(int bottleIdx) {
    return g_dComIfG_gameInfo.info.getPlayer().getItemRecord().getBottleNum(bottleIdx);
}

inline void set_bottle_num(int bottleIdx, u8 bottleNum) {
    g_dComIfG_gameInfo.info.getPlayer().getItemRecord().setBottleNum(bottleIdx, bottleNum);
}

inline void add_bottle_num(int bottleIdx, int num) {
    g_dComIfG_gameInfo.info.getPlayer().getItemRecord().addBottleNum(bottleIdx, num);
}

inline u8 get_select_item_index(int no) {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectItemIndex(no);
}

inline void on_item_first_bit(u8 itemNo) {
    g_dComIfG_gameInfo.info.getPlayer().getGetItem().onFirstBit(itemNo);
}

inline u16 get_rupee() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupee();
}

inline void set_rupee(u16 rupees) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setRupee(rupees);
}

inline void set_life(u16 life) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setLife(life);
}

inline void set_restart_room(const cXyz& position, s16 angle, s8 roomNo) {
    g_dComIfG_gameInfo.info.getRestart().setRoom(position, angle, roomNo);
}

inline void set_restart_room_param(u32 param) {
    g_dComIfG_gameInfo.info.getRestart().setRoomParam(param);
}

inline u8 clear_count() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerInfo().getClearCount();
}

inline u8 poh_spirit_num() {
    return g_dComIfG_gameInfo.info.getPlayer().getCollect().getPohNum();
}

inline u16 life() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getLife();
}

inline bool is_pause_flag() {
    return g_dComIfG_gameInfo.play.isPauseFlag() != 0;
}

inline J2DGrafContext* current_graf_port() {
    return g_dComIfG_gameInfo.play.getCurrentGrafPort();
}

inline JKRArchive* item_icon_archive() {
    return g_dComIfG_gameInfo.play.getItemIconArchive();
}

inline JKRArchive* collect_res_archive() {
    return g_dComIfG_gameInfo.play.getCollectResArchive();
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
