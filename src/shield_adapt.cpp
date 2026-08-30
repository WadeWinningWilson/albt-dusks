#include "global.h"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/actor/d_a_player.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter_HIO.h"
#include "d/d_pane_class.h"
#include "m_Do/m_Do_controller_pad.h"

#define private public
#include "d/actor/d_a_alink.h"
#undef private

#include "albw_common.h"
#include "config_vars.h"
#include "shield_adapt.h"
#include "wolf_combat.h"

#include <cmath>

namespace {

// Fork d_meter2_draw.cpp — bash/wolf cluster right of Midna when LoP row mode on.
static constexpr f32 kLopSpurAnchorOffX = 36.0f;
static constexpr f32 kLopSpurAnchorOffY = -16.0f;

bool pane_center(CPaneMgr* pane, Vec* o_center) {
    if (pane == nullptr || pane->getPanePtr() == nullptr || o_center == nullptr) {
        return false;
    }
    *o_center = pane->getGlobalVtxCenter(false, 0);
    return true;
}

}  // namespace

bool albw_link_in_guard_slip(const daAlink_c* link) {
    return link != nullptr && link->mProcID == daAlink_c::PROC_GUARD_SLIP;
}

daAlink_c* albw_link_actor() {
    return static_cast<daAlink_c*>(g_dComIfG_gameInfo.play.getPlayer(0));
}

bool albw_manual_shield_enabled() {
    return albw_cfg_bool(g_manual_shield, false);
}

bool albw_manual_shield_button(const daAlink_c* link) {
    if (link == nullptr || !link->checkShieldGet()) {
        return false;
    }
    return mDoCPd_c::getHoldLockR(PAD_1) != 0;
}

bool albw_manual_shield_attack_trigger(daAlink_c* link) {
    return link != nullptr && albw_manual_shield_button(link) &&
           link->itemTriggerCheck(daAlink_c::BTN_B);
}

bool albw_manual_shield_blocks_sword(const daAlink_c* link) {
    return albw_manual_shield_enabled() && albw_manual_shield_button(link);
}

bool albw_shield_parry_enabled() {
    return albw_cfg_bool(g_shield_parry, false);
}

bool albw_shield_features_active() {
    return albw_manual_shield_enabled() || albw_shield_parry_enabled();
}

namespace {

int clamp_int(int value, int min, int max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

}  // namespace

namespace albw_shield_ui {

ParryIcons parry_icons_mode() {
    return static_cast<ParryIcons>(clamp_int(albw_cfg_int(g_parry_icons_mode, 0), 0, 2));
}

ShieldHudVisibility shield_hud_visibility_mode() {
    return static_cast<ShieldHudVisibility>(clamp_int(albw_cfg_int(g_shield_hud_visibility, 0), 0, 3));
}

LopHudMode lop_hud_mode() {
    return static_cast<LopHudMode>(clamp_int(albw_cfg_int(g_lop_hud_mode, 0), 0, 2));
}

}  // namespace albw_shield_ui

bool dLopHudOn() {
    return albw_shield_ui::lop_hud_mode() != albw_shield_ui::LopHudMode::Off;
}

bool albw_get_rupee_anchor_center(dMeter2Draw_c* draw, Vec* o_center) {
    if (draw == nullptr || o_center == nullptr) {
        return false;
    }
    return pane_center(draw->mpRupeeParent[0], o_center);
}

bool albw_get_shield_hud_anchor_center(dMeter2Draw_c* draw, Vec* o_center) {
    if (draw == nullptr || o_center == nullptr) {
        return false;
    }

    // LoP row modes only reanchor bash/wolf icons until Tier C2 full meter layout.
    if (dLopHudOn()) {
        if (pane_center(draw->mpButtonCrossParent, o_center)) {
            o_center->x += kLopSpurAnchorOffX;
            o_center->y += kLopSpurAnchorOffY;
            return true;
        }
        if (pane_center(draw->mpLifeParent, o_center)) {
            o_center->x += kLopSpurAnchorOffX;
            o_center->y += 360.0f;
            return true;
        }
    }

    if (pane_center(draw->mpRupeeKeyParent, o_center)) {
        return true;
    }
    return albw_get_rupee_anchor_center(draw, o_center);
}

f32 albw_get_rupee_hud_reference_size(dMeter2Draw_c* draw) {
    if (draw == nullptr || draw->mpRupeeParent[0] == nullptr ||
        draw->mpRupeeParent[0]->getPanePtr() == nullptr)
    {
        return g_drawHIO.mRupeeScale * 24.0f;
    }

    Mtx m;
    J2DPane* pane = draw->mpRupeeParent[0]->getPanePtr();
    const Vec v0 = draw->mpRupeeParent[0]->getGlobalVtx(pane, &m, 0, false, 0);
    const Vec v3 = draw->mpRupeeParent[0]->getGlobalVtx(pane, &m, 3, false, 0);
    const f32 w = std::fabs(v3.x - v0.x);
    const f32 h = std::fabs(v3.y - v0.y);
    if (h < 1.0f) {
        return g_drawHIO.mRupeeScale * 24.0f;
    }
    if (w > h * 2.5f) {
        return h;
    }
    return (w < h) ? w : h;
}
