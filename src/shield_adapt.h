#pragma once

#include "global.h"

#include "albw_common.h"

#include "config_vars.h"

#include "albw_combat.h"

#include "parry_master.h"
#include "rental_eligibility.h"
#include "rental_shop.h"

class daAlink_c;

class fopAc_ac_c;

class fopEn_enemy_c;

namespace albw_shield_ui {

enum class ParryIcons : u8 {
    SpurOnly = 0,
    SpurShield = 1,
    ShieldOnly = 2,
};

enum class ShieldHudVisibility : u8 {
    Off = 0,
    DurabilityAlways = 1,
    ParryAlways = 2,
    BothAlways = 3,
};

enum class LopHudMode : u8 {
    Off = 0,
    VanillaHearts = 1,
    HealthBar = 2,
};

ParryIcons parry_icons_mode();
ShieldHudVisibility shield_hud_visibility_mode();
LopHudMode lop_hud_mode();

}  // namespace albw_shield_ui

daAlink_c* albw_link_actor();

bool albw_manual_shield_enabled();

bool albw_manual_shield_button(const daAlink_c* link);

bool albw_manual_shield_attack_trigger(daAlink_c* link);

bool albw_manual_shield_blocks_sword(const daAlink_c* link);

bool albw_shield_parry_enabled();

bool albw_shield_features_active();

bool albw_link_in_guard_slip(const daAlink_c* link);

bool dLopHudOn();

class dMeter2Draw_c;
struct Vec;

// Fork getRupeeAnchorCenter / getShieldHudAnchorCenter — live pane centers
// (raw draw-HIO mRupeePos* are not screen coords on stock HUD).
bool albw_get_rupee_anchor_center(dMeter2Draw_c* draw, Vec* o_center);
bool albw_get_shield_hud_anchor_center(dMeter2Draw_c* draw, Vec* o_center);
f32 albw_get_rupee_hud_reference_size(dMeter2Draw_c* draw);

inline bool dAlbw_isHiddenSkillReworkEnabled() {
    return albw_cfg_bool(g_focused_arts, false);
}

// dALBWRental_isOpen / menu-res generation live in rental_shop.h/.cpp.

inline void dMeter2_onShieldDestroyedForRental(u8 itemNo) {
    albw_rental_on_shield_eligible(itemNo);
}
inline void dMeter2_onALBWHiddenSkill() {}
inline void dMeter2_commitALBWHiddenSkillIfPending() {}
