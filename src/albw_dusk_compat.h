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

class J3DModelData;  // for custom_assets::try_load's return type (pointer only)

namespace dusk {

// Verbatim from fork include/dusk/settings.h:125-131.
enum class CapWearMode : u8 {
    Off = 0,
    None = 1,
    Green = 2,
    Red = 3,
    Blue = 4,
};

// ============================================
// NEW CODE - ALBT multiplatform (Magic Armor rupee-drain mode compat)
// Verbatim from fork include/dusk/settings.h:55-62. The ported changeLink Magic
// branch (src/changelink_port.inc) reads dusk::getSettings().game.armorRupeeDrain
// against these values. Stock dusklight has no such setting, so getSettings()
// below defaults it to NORMAL: with NORMAL, the fork's `== ALBW` arm is dead and
// its `!= NORMAL` arms fold to the stock `rupee != 0` behaviour the DUSK runs
// today (accurate "this host has no ALBW rupee-drain mode", NOT a stubbed value).
// ============================================
enum class MagicArmorMode : u8 {
    NORMAL = 0,
    ON_DAMAGE = 1,
    DOUBLE_DEFENSE = 2,
    INVINCIBLE = 3,
    COSMETIC = 4,
    ALBW = 5,
};

// Minimal stand-in for the host's ConfigVar<T> - only .getValue() is used by
// the ported files.
template <typename T>
struct CompatVar {
    T value;
    T getValue() const { return value; }
};

struct CompatGameSettings {
    CompatVar<CapWearMode>     capWear;
    CompatVar<bool>            sumoOutfitFists;
    CompatVar<bool>            showWardrobeRecoveryDebug;
    CompatVar<bool>            outfitStats;
    CompatVar<bool>            albwSoulboundRedPotion;
    CompatVar<MagicArmorMode>  armorRupeeDrain;
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
    s.game.outfitStats.value               = albw_cfg_bool(g_outfit_stats, false);
    s.game.albwSoulboundRedPotion.value    = albw_cfg_bool(g_soulbound_potion, false);
    // Magic Armor rupee-drain is a fork-only "Settings -> ALBW" mode; stock has no
    // such setting, so it is fixed at NORMAL here. The ported changeLink then takes
    // the same Magic-Brk path stock dusklight already runs (see the enum note above).
    s.game.armorRupeeDrain.value           = MagicArmorMode::NORMAL;
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
// fork custom_assets.cpp - loads a loose per-arc override model ("<arc>_<idx>.bmd")
// from an enabled Custom Models folder. The ported changeLink boots section calls
// try_load for the WW iron-boots skin. Stock has no Custom Models system (see
// overlay_generation above), so there is never an override: return nullptr and the
// fork body falls back to the vanilla al_bootsH (accurate, not a stub).
inline J3DModelData* try_load(const char*, int) { return nullptr; }
}  // namespace custom_assets

}  // namespace dusk

// ============================================
// NEW CODE ENDS HERE
// ============================================
