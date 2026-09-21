// ============================================
// NEW CODE - ALBT: the mod's persistent-flag store. See albw_save_flags.h for
// what this replaced and why.
//
// THE STORE IS config.json, NOT THE PLAYER'S SAVE FILE.
//
// Two routes were on the table. v0.2.7 shipped the interim one - event
// registers 114-118, inside the mEvent window stock does not use - because the
// urgent problem was that our flags were landing on REAL designer flags and
// granting the Fused Shadow. That stopped the corruption, but it did not stop
// us writing to the player's save, which is the thing actually asked for.
//
// This is the intended route. ConfigService persists to config.json
// ("Values are saved to config.json. Writes are debounced, not flushed per
// set." - sdk/include/mods/svc/config.h) and the file belongs to the mod, so
// uninstalling removes our state with it and the save is never touched at all.
//
// THE TRADE, STATED PLAINLY: config.json is per-INSTALL, not per-save-file.
// Two save files share these flags, and starting a new game does not clear
// them. That is a real behaviour change from a save-backed flag, and the
// answer to it is the deliberate reset control (a "New Game" button that
// rewrites these to defaults) rather than putting the data back in the save.
//
// The vars carry no UI control anywhere, which is what makes them invisible -
// visibility comes from the mod building a pane for a var, not from a property
// of the var itself.
//
// 36 flags across two 32-bit ints. albw_cfg_int returns `int`, so one var
// cannot hold them; the split is by bit index, not by meaning.
// ============================================

#include "albw_save_flags.h"

#include <cstdint>

#include "albw_common.h"
#include "config_vars.h"

namespace {

constexpr int kBitsPerVar = 32;

ConfigVarHandle varFor(int flag) {
    return flag < kBitsPerVar ? g_progress_flags_a : g_progress_flags_b;
}

// Stock's register window is no longer involved at all, so the only bound that
// matters is how many bits the two ints hold.
static_assert(ALBW_FLAG_COUNT <= 2 * kBitsPerVar,
              "albw save flags no longer fit in two 32-bit config ints - add a third var "
              "rather than spilling into the player's save file");

}  // namespace

bool albw_save_flag_get(int flag) {
    if (flag < 0 || flag >= ALBW_FLAG_COUNT) {
        return false;
    }
    const uint32_t bits = static_cast<uint32_t>(albw_cfg_int(varFor(flag), 0));
    return (bits & (1u << (flag % kBitsPerVar))) != 0;
}

void albw_save_flag_set(int flag, bool on) {
    if (flag < 0 || flag >= ALBW_FLAG_COUNT) {
        return;
    }
    const ConfigVarHandle var = varFor(flag);
    if (var == 0 || svc_config == nullptr) {
        return;  // unregistered: fail quiet rather than writing somewhere wrong
    }

    const uint32_t mask = 1u << (flag % kBitsPerVar);
    const uint32_t before = static_cast<uint32_t>(albw_cfg_int(var, 0));
    const uint32_t after = on ? (before | mask) : (before & ~mask);
    if (after == before) {
        return;  // writes are debounced, not free - do not dirty the file for a no-op
    }
    svc_config->set_int(mod_ctx, var, static_cast<int64_t>(after));
}

// ============================================
// NEW CODE ENDS HERE
// ============================================
