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
#include "d/d_meter2_info.h"
#include "JSystem/J2DGraph/J2DPicture.h"

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

// ---- oil / warp / touch ------------------------------------------------------
u16 dComIfGs_getMaxOil() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxOil();
}
u16 dComIfGs_getOil() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getOil();
}
u8 dMeter2Info_getWarpStatus() { return g_meter2_info.getWarpStatus(); }
bool dMeter2Info_isTouchKeyCheck(int i_status) { return g_meter2_info.isTouchKeyCheck(i_status); }
u8 dMeter2Info_getRentalBombBag() { return g_meter2_info.getRentalBombBag(); }

// ---- item wheel textures -----------------------------------------------------
u8 dComIfGs_getSelectItemIndex(int i_no) {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectItemIndex(i_no);
}
int dMeter2Info_readItemTexture(u8 i_itemNo, void* i_texBuf1, J2DPicture* i_pic1, void* i_texBuf2,
                                J2DPicture* i_pic2, void* i_texBuf3, J2DPicture* i_pic3,
                                void* i_texBuf4, J2DPicture* i_pic4, int param_9) {
    return g_meter2_info.readItemTexture(i_itemNo, i_texBuf1, i_pic1, i_texBuf2, i_pic2, i_texBuf3,
                                         i_pic3, i_texBuf4, i_pic4, param_9);
}
void dMeter2Info_setItemColor(u8 i_itemNo, J2DPicture* i_pic1, J2DPicture* i_pic2,
                              J2DPicture* i_pic3, J2DPicture* i_pic4) {
    g_meter2_info.setItemColor(i_itemNo, i_pic1, i_pic2, i_pic3, i_pic4);
}

// ---- events / meter window ---------------------------------------------------
BOOL dComIfGp_event_runCheck() {
    return g_dComIfG_gameInfo.play.getEvent()->runCheck();
}
dMw_c* dMeter2Info_getMenuWindowClass() {
    return g_meter2_info.getMenuWindowClass();
}
void dMeter2Info_setWindowStatus(u8 i_status) {
    g_meter2_info.setWindowStatus(i_status);
}

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
void* dComIfG_getObjectRes(const char* i_arcName, const char* i_resName) {
    return albw_game::object_res(i_arcName, i_resName);
}
// stock d_com_inf_game.h:5303 / :3830 (#else inline bodies)
dCcS* dComIfG_Ccsp() {
    return &g_dComIfG_gameInfo.play.mCcs;
}
// stock d_com_inf_game.h:5299
dBgS& dComIfG_Bgsp() {
    return g_dComIfG_gameInfo.play.mBgs;
}
// stock d_com_inf_game.h — the 4-arg particle_set overload (id,pos,rot,scale).
JPABaseEmitter* dComIfGp_particle_set(u16 i_resID, const cXyz* i_pos, const csXyz* i_rotation,
                                      const cXyz* i_scale) {
    return g_dComIfG_gameInfo.play.getParticle()->setNormal(
        i_resID, i_pos, NULL, i_rotation, i_scale, 0xFF, NULL, -1, NULL, NULL, NULL, 1.0f);
}
dVibration_c& dComIfGp_getVibration() {
    return g_dComIfG_gameInfo.play.getVibration();
}
// stock d_com_inf_game.h:2366
BOOL dComIfGs_isTransformLV(int i_no) {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusB().isTransformLV(i_no);
}

// ============================================
// Focused Arts port dependencies. These are DUSK_NOINLINE-declared on PC (no
// body ships), so the module's verbatim dComIfGs_* calls need out-of-line
// definitions here. Bodies from stock d_com_inf_game.h inline #else branch.
// ============================================
// stock d_com_inf_game.h:2222
u16 dComIfGs_getLife() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getLife();
}
// stock d_com_inf_game.h:2226
void dComIfGs_setLife(u16 i_life) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setLife(i_life);
}
// stock d_com_inf_game.h:2900
void dComIfGs_setEventReg(u16 reg, u8 value) {
    g_dComIfG_gameInfo.info.getEvent().setEventReg(reg, value);
}
// stock d_com_inf_game.h:2904
u8 dComIfGs_getEventReg(u16 reg) {
    return g_dComIfG_gameInfo.info.getEvent().getEventReg(reg);
}
// stock d_com_inf_game.h:2558 (armogohma egg-gate: force arrow count > 3)
u8 dComIfGs_getArrowNum() {
    return g_dComIfG_gameInfo.info.getPlayer().getItemRecord().getArrowNum();
}
// stock d_com_inf_game.h:2562
void dComIfGs_setArrowNum(u8 i_arrowNum) {
    g_dComIfG_gameInfo.info.getPlayer().getItemRecord().setArrowNum(i_arrowNum);
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
