#pragma once

#include "f_op/f_op_actor.h"
#include "mods/api.h"

class cCcD_Obj;
class daNpc_Post_c;

static constexpr int WOLF_STUN_FRAMES = 300;

bool dAlbwWolfCombat_isEnabled();

void dAlbwWolfCombat_onBiteConnect();
void dAlbwWolfCombat_onChestMashHit();
void dAlbwWolfCombat_fillCharges();

u8 albw_wolf_get_charge_count();
u8 albw_wolf_get_max_charges();
void albw_wolf_spend_charge(u8 amount);

ModResult albw_wolf_charge_art_init(ModError* error);

// wolf_howl_combat.cpp
ModResult albw_wolf_howl_combat_init(ModError* error);
void albw_wolf_howl_arm_combat_request();
bool albw_wolf_combat_howl_active();

// hair-reach visual bridge (defined in wolf_combat.cpp; consumed by the arm
// actor and the setNeckAngle hook - fork d_albw_wolf_stun.h:123)
class cXyz;
void dAlbwMidnaArm_setReachPos(const cXyz& i_pos, bool i_striking);
void dAlbwMidnaArm_clearReachPos();
bool dAlbwMidnaArm_getReachPos(cXyz* o_pos);
bool dAlbwMidnaArm_isReachStriking();

// midna_arm.cpp
class daAlink_c;
ModResult albw_midna_arm_init(ModError* error);
bool albw_midna_arm_is_alive();
bool albw_midna_arm_spawn(daAlink_c* link);

bool dAlbwWolfArts_isHowlUnlocked();
bool dAlbwWolfArts_isArmUnlocked();

// ============================================
// NEW CODE - ALBW Port (wolf-arts shop surface)
// These are already DEFINED at file scope in wolf_combat.cpp:682-806; they were
// simply never declared, so the shop could not call them. No new behaviour.
// ============================================
bool        dAlbwWolfArts_shouldShowHowlShopRow();
int         dAlbwWolfArts_getHowlShopPrice();
const char* dAlbwWolfArts_getHowlShopName();
const char* dAlbwWolfArts_getHowlShopDesc();
bool        dAlbwWolfArts_tryPurchaseHowl();

bool        dAlbwWolfArts_shouldShowArmShopRow();
int         dAlbwWolfArts_getArmShopPrice();
const char* dAlbwWolfArts_getArmShopName();
const char* dAlbwWolfArts_getArmShopDesc();
bool        dAlbwWolfArts_tryPurchaseArm();

bool        dAlbwWolfArts_shouldShowChargeShopRow();
int         dAlbwWolfArts_getChargeShopPrice();
const char* dAlbwWolfArts_getChargeShopName();
const char* dAlbwWolfArts_getChargeShopDesc();
bool        dAlbwWolfArts_tryPurchaseChargeUpgrade();

bool dAlbwWolfStun_isTwilightEnemy(s16 i_name);
void dAlbwWolfStun_apply(fopAc_ac_c* i_enemy);
void dAlbwWolfStun_applyHold(fopAc_ac_c* i_enemy);
void dAlbwWolfStun_thaw(fopAc_ac_c* i_enemy);
void dAlbwWolfStun_update();
bool dAlbwWolfStun_isStunned(fopAc_ac_c* i_enemy);
void dAlbwWolfStun_syncColliders(fopAc_ac_c* i_enemy, cCcD_Obj* const* i_objs, int i_count);
void dAlbwWolfStun_captureAfterExecute();
void dAlbwWolfStun_beforeMove();
void dAlbwWolfStun_afterMove();

ModResult albw_wolf_combat_init(ModError* error);
ModResult albw_wolf_combat_shutdown(ModError* error);
