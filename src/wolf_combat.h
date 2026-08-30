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

bool dAlbwWolfArts_isHowlUnlocked();
bool dAlbwWolfArts_isArmUnlocked();

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
