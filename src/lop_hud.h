// ============================================
// NEW CODE - ALBW Port (Lies of Link HUD)
// Port of the fork's game.lopHud relayout into the .dusk.
//
// Fork sources: d_meter2_draw.cpp (11 sites), d_meter2.cpp (3 toggle sites),
// d_albw_shield.cpp (2 sites, already shipped via shield_adapt).
//
// The fork edits dMeter2Draw_c methods inline. A mod cannot, so each site is
// reproduced from a hook on the stock method that contained it:
//
//   fork site                          -> hook target
//   exec()      751-793                -> dMeter2Draw_c::exec        (post)
//   draw()      867-1087               -> dMeter2Draw_c::draw        (pre/post)
//   drawButtonCross() 4104             -> dMeter2Draw_c::drawButtonCross (post)
//   drawRupee() 3381                   -> dMeter2Draw_c::drawRupee   (post)
//   drawKanteraScreen gating 1022      -> dMeter2Draw_c::drawKanteraScreen (pre)
//
// All seven pane members the fork uses (mpButtonParent, mpButtonCrossParent,
// mpButtonMidona, mpButtonB, mpLifeParent, mpRupeeKeyParent, mpItemXY) are
// public in the stock header, so the hooks can read them directly.
// ============================================

#pragma once

#include "albw_common.h"

#include "d/d_meter2_draw.h"

namespace albw_lop {

enum class Mode : u8 {
    Off = 0,            // vanilla TP corner layout
    VanillaHearts = 1,  // LoP relayout, keep the heart containers
    HealthBar = 2,      // LoP relayout, Lies-of-P health bar instead of hearts
};

Mode mode();
bool active();       // mode != Off
bool health_bar();   // mode == HealthBar

// ============================================
// Fork parity note - user HUD scale.
//
// The fork's dGetUserHudScale() reads dusk settings game.hudScale. ConfigService
// is explicitly scoped to the calling mod ("Registrations are owned by the
// calling mod"), and no SDK service exposes host settings, so a mod cannot read
// it. dAnchorHudScale computes half = (1 - scale) * 0.5, which is exactly 0 at
// the default scale of 1.0 - so this port is pixel-accurate at default HUD scale
// and drifts proportionally for a player who changed it. Not fixable mod-side;
// needs the host to expose hudScale.
// ============================================
f32 hud_scale();

enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };
void anchor_hud_scale(CPaneMgr* pane, Corner corner, f32* io_x, f32* io_y, f32 pull = 1.0f);

// Shield/wolf HUD anchor: the centre the fork caches in mLopShieldAnchor
// (right of the Midna icon, with cross/life fallbacks). Returns false when the
// LoP layout is off or no pane resolved.
bool shield_anchor(Vec* o_center);

}  // namespace albw_lop

ModResult albw_lop_hud_init(ModError* error);
ModResult albw_lop_hud_shutdown(ModError* error);
// ============================================
// NEW CODE ENDS HERE
// ============================================
