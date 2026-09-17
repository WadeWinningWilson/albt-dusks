#ifndef ALBW_DEKU_LEAF_H
#define ALBW_DEKU_LEAF_H

#include "mods/api.h"

// ============================================
// Deku Leaf glide — port of the fork's WW Deku Leaf feature (d_a_alink.cpp:374-846
// + the procAutoJump/execute/draw seams). The fork re-gated the native cucco-glide
// chassis by adding `|| checkDekuLeafGlide()` to the INLINE checkGrabGlide(), which is
// compiled into the stock exe at ~10 physics/model/camera sites and cannot be edited.
//
// Native route (DN-10): the stock checkGrabGlide() inlines checkGrabRooster(), which is
// OUT-OF-LINE (a real, hookable symbol) and keys only on Link holding a cucco/TKJ2
// actor. We hook checkGrabRooster POST and force TRUE while the leaf is out, so the
// ENTIRE native cucco chassis (gravity, wind, chase, model-flag, camera) engages for the
// leaf with no reimplementation. The leaf-specific inserts (R+A takeoff lift, aerial bomb
// drop, meter cost, 1.20x glide speed, canopy model) are overlaid via hooks on
// procAutoJumpInit/procAutoJump/procLandInit/execute/draw/checkItemChangeFromButton.
//
// R+A takeoff reuses the Moon Jump cheat's lift shape (fork f_ap_game.cpp:822); the
// canopy model is the bundled res/dekuleaf/itemmdl_21.bmd (armogohma pattern).
// ============================================

// Installs every deku-leaf hook. No-op behaviour until the "deku_leaf" setting is on.
ModResult albw_deku_leaf_init(ModError* error);

// Per-frame driver (call from mod_update). Reproduces the fork's execute()-time glide
// truth-check + gust retire (d_a_alink.cpp:19751) and the f_ap_game R+A takeoff gesture
// (f_ap_game.cpp:822). Runs in every player state, so it lives outside the proc hooks.
void albw_deku_leaf_tick();

#endif  // ALBW_DEKU_LEAF_H
