#pragma once

// ============================================
// NEW CODE - ALBT multiplatform (dusk:: host-settings compat shim)
//
// The outfit cluster (outfit / sumo_test / wardrobe / outfit_stats) is ported
// VERBATIM from the fork. Those files read fork-only HOST state:
//
//     dusk::getSettings().game.capWear / .sumoOutfitFists / .showWardrobeRecoveryDebug
//     dusk::isDpadQuickSwapEnabled()
//     dusk::custom_assets::overlay_generation() / overlay_path_for()
//
// None of it exists in stock dusklight (verified: `git grep` on origin/main
// returns 0 files for capWear, isDpadQuickSwapEnabled, overlay_generation and
// overlay_path_for). ConfigService is mod-scoped, so the mod cannot read host
// settings even where they do exist.
//
// Rather than rewrite ~40 call sites inside the ported files - which would make
// them diverge from the fork and defeat the point of a verbatim port - the
// translation lives HERE, at the consumption boundary (DO-NOT DN-10 step 2).
// The four .cpp files stay byte-identical to the fork except for #include paths.
// ============================================

#include "albw_common.h"
#include "config_vars.h"

namespace dusk {

// Verbatim from fork include/dusk/settings.h:125-131.
enum class CapWearMode : u8 {
    Off = 0,
    None = 1,
    Green = 2,
    Red = 3,
    Blue = 4,
};

// Minimal stand-in for the host's ConfigVar<T> - only .getValue() is used by
// the ported files.
template <typename T>
struct CompatVar {
    T value;
    T getValue() const { return value; }
};

struct CompatGameSettings {
    CompatVar<CapWearMode> capWear;
    CompatVar<bool>        sumoOutfitFists;
    CompatVar<bool>        showWardrobeRecoveryDebug;
};

struct CompatSettings {
    CompatGameSettings game;
};

inline CompatSettings getSettings() {
    CompatSettings s;
    // Cap Wear is a fork "Settings -> ALBW -> Visuals" feature backed by the
    // fork's alternate-heap cap/face donor pipeline inside d_a_alink.cpp. That
    // pipeline is not present in stock, so the mod exposes the setting but
    // defaults it Off; both consumers short-circuit on Off (see the port notes
    // in alink_compat.cpp).
    s.game.capWear.value =
        static_cast<CapWearMode>(albw_cfg_int(g_cap_wear, static_cast<int>(CapWearMode::Off)));
    s.game.sumoOutfitFists.value          = albw_cfg_bool(g_sumo_outfit_fists, false);
    s.game.showWardrobeRecoveryDebug.value = albw_cfg_bool(g_wardrobe_recovery_debug, false);
    return s;
}

// fork include/dusk/action_bindings.h:64 - a host binding in the fork, a mod
// config var here.
inline bool isDpadQuickSwapEnabled() { return albw_cfg_bool(g_dpad_quick_swap, false); }

// ---- Custom Models overlay signal -------------------------------------------
// The fork's Custom Models system re-mounts resident arcs from a user FST and
// bumps a generation counter so the outfit module can force ONE model rebuild.
// Stock dusklight has no such system (0 hits on origin/main), so there is never
// an overlay to react to: generation is constant and no arc has an overlay path.
// This is an accurate translation of "this host has no overlays", NOT a stubbed
// -out feature - if a Custom Models API ever lands in the host SDK, wire it here.
namespace custom_assets {
inline int overlay_generation() { return 0; }
inline const char* overlay_path_for(const char*) { return ""; }
}  // namespace custom_assets

}  // namespace dusk

// ============================================
// NEW CODE ENDS HERE
// ============================================
