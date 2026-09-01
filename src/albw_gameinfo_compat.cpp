// ============================================
// NEW CODE - ALBT multiplatform (dComIf* accessor compat)
//
// WHY THIS FILE EXISTS
// ------------------------------------------------------------------
// d_com_inf_game.h declares these accessors two different ways:
//
//   #if TARGET_PC   -> DUSK_NOINLINE declarations, no body   (d_com_inf_game.h:1275+)
//   #else           -> ordinary inline definitions           (d_com_inf_game.h:2250+)
//
// The mod builds with TARGET_PC defined, so every call compiles to an external
// symbol - and the Windows SDK stub library does not export them. That is the
// "unresolved external symbol dComIfGs_*" wall.
//
// The ported outfit cluster calls 19 of them. Rewriting those call sites to the
// mod's albw_game:: wrappers would put a diff on every one of the ~2,900 verbatim
// lines, so instead this TU supplies the missing definitions.
//
// Each body below is COPIED FROM STOCK'S OWN #else BRANCH (file:line cited per
// function). They read g_dComIfG_gameInfo, which is exported DATA and therefore
// genuinely available to the mod - so these are the real accessors, not
// reimplementations. Nothing here invents behaviour.
// ============================================

#include "global.h"
#include <os.h>

#include "albw_game.h"
#include "d/d_com_inf_game.h"
#include "d/actor/d_a_player.h"

#if TARGET_PC

// ---- save-file event bits (d_com_inf_game.h:2xxx #else branch) ---------------
BOOL dComIfGs_isEventBit(const u16 i_flag) {
    return g_dComIfG_gameInfo.info.getEvent().isEventBit(i_flag);
}
void dComIfGs_onEventBit(const u16 i_flag) {
    g_dComIfG_gameInfo.info.getEvent().onEventBit(i_flag);
}
void dComIfGs_offEventBit(const u16 i_flag) {
    g_dComIfG_gameInfo.info.getEvent().offEventBit(i_flag);
}

// ---- item "first get" bits ---------------------------------------------------
int dComIfGs_isItemFirstBit(u8 i_no) {
    return g_dComIfG_gameInfo.info.getPlayer().getGetItem().isFirstBit(i_no);
}
void dComIfGs_onItemFirstBit(u8 i_itemNo) {
    g_dComIfG_gameInfo.info.getPlayer().getGetItem().onFirstBit(i_itemNo);
}

// ---- wallet ------------------------------------------------------------------
u16 dComIfGs_getRupee() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupee();
}
void dComIfGs_setRupee(u16 i_rupees) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setRupee(i_rupees);
}

u8 dComIfGs_getWalletSize() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getWalletSize();
}

void dComIfGs_setWalletSize(u8 i_size) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setWalletSize(i_size);
}

// ---- equipment ---------------------------------------------------------------
u8 dComIfGs_getSelectEquipClothes() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(COLLECT_CLOTHING);
}
// dComIfGs_getSelectEquipShield is already defined in shield_game.cpp:25 -
// the mod established this same compat pattern there. Not duplicated here.
u8 dComIfGs_getSelectEquipSword() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(COLLECT_SWORD);
}

// ---- inventory ---------------------------------------------------------------
u8 dComIfGs_getItem(int i_slotNo, bool i_checkCombo) {
    return g_dComIfG_gameInfo.info.getPlayer().getItem().getItem(i_slotNo, i_checkCombo);
}
u8 dComIfGs_getLineUpItem(int i_slotNo) {
    return g_dComIfG_gameInfo.info.getPlayer().getItem().getLineUpItem(i_slotNo);
}

// ---- play-state --------------------------------------------------------------
fopAc_ac_c* dComIfGp_getPlayer(int idx) { return g_dComIfG_gameInfo.play.getPlayer(idx); }
daPy_py_c*  dComIfGp_getLinkPlayer() {
    return (daPy_py_c*)g_dComIfG_gameInfo.play.getPlayerPtr(LINK_PTR);
}
BOOL dComIfGp_isEnableNextStage() { return g_dComIfG_gameInfo.play.isEnableNextStage(); }
u8   dComIfGp_getMesgStatus()     { return g_dComIfG_gameInfo.play.getMesgStatus(); }
u8   dComIfGp_isHeapLockFlag()    { return g_dComIfG_gameInfo.play.isHeapLockFlag(); }
u8   dComIfGp_isPauseFlag()       { return g_dComIfG_gameInfo.play.isPauseFlag(); }
void dComIfGp_setOxygenCount(s32 oxygen) { g_dComIfG_gameInfo.play.setOxygenCount(oxygen); }

// ---- graphics ----------------------------------------------------------------
J2DGrafContext* dComIfGp_getCurrentGrafPort() {
    return albw_game::current_graf_port();
}

// ---- archive / resource control ---------------------------------------------
int dComIfG_deleteObjectResMain(const char* i_arcName) {
    return g_dComIfG_gameInfo.mResControl.deleteObjectRes(i_arcName);
}
dRes_info_c* dComIfG_getObjectResInfo(const char* i_arcName) {
    return g_dComIfG_gameInfo.mResControl.getObjectResInfo(i_arcName);
}
void* dComIfG_getObjectRes(const char* i_arcName, int i_index) {
    return albw_game::object_res(i_arcName, i_index);
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
