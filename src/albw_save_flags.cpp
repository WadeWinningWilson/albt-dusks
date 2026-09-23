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
// COUNTERS. One named config var each - see the header for why these left the
// event registers even though those were safe.
//
// The table is indexed by AlbwSaveCounter, so its order is load-bearing for
// the two sword_atp runs, which are addressed as base + swordId. The
// static_assert below is what stops a reordered enum from silently writing a
// sword's attack bonus into the potion tier.
// ============================================
namespace {

ConfigVarHandle* const kCounterVars[] = {
    &g_ctr_fa_tier,
    &g_ctr_potion_tier,
    &g_ctr_heart_shop_tier,
    &g_ctr_meter_shop_tier,
    &g_ctr_bonus_half_hearts,
    &g_ctr_bonus_quarter_hearts,
    &g_ctr_sword_atp_bonus_0, &g_ctr_sword_atp_bonus_1,
    &g_ctr_sword_atp_bonus_2, &g_ctr_sword_atp_bonus_3,
    &g_ctr_sword_atp_step_0,  &g_ctr_sword_atp_step_1,
    &g_ctr_sword_atp_step_2,  &g_ctr_sword_atp_step_3,
};

static_assert(sizeof(kCounterVars) / sizeof(kCounterVars[0]) == ALBW_CTR_COUNT,
              "counter var table is out of step with AlbwSaveCounter - the sword_atp runs "
              "are addressed as base + swordId, so a mismatch misroutes a write");

}  // namespace

int albw_save_counter_get(int counter) {
    if (counter < 0 || counter >= ALBW_CTR_COUNT) {
        return 0;
    }
    const int v = albw_cfg_int(*kCounterVars[counter], 0);
    // Clamp on READ as well as write: config.json is user-editable, so a
    // hand-typed 9999 must not reach code that assumed a u8 register.
    if (v < 0) {
        return 0;
    }
    return v > 255 ? 255 : v;
}

void albw_save_counter_set(int counter, int value) {
    if (counter < 0 || counter >= ALBW_CTR_COUNT) {
        return;
    }
    const ConfigVarHandle var = *kCounterVars[counter];
    if (var == 0 || svc_config == nullptr) {
        return;
    }
    const int clamped = value < 0 ? 0 : (value > 255 ? 255 : value);
    if (albw_cfg_int(var, 0) == clamped) {
        return;  // debounced, not free - do not dirty the file for a no-op
    }
    svc_config->set_int(mod_ctx, var, static_cast<int64_t>(clamped));
}

// ============================================
// "NEW GAME" RESET. The control the store's per-install trade always required
// (block comment above, and the header). Rewrites both flag ints and every
// counter to 0 - the registered default of all of them (mod.cpp:74-88). Writes
// go only to the mod's config vars; the player's save file is never touched.
// ============================================
void albw_save_flags_reset_all() {
    if (svc_config == nullptr) {
        return;
    }
    if (g_progress_flags_a != 0 && albw_cfg_int(g_progress_flags_a, 0) != 0) {
        svc_config->set_int(mod_ctx, g_progress_flags_a, 0);
    }
    if (g_progress_flags_b != 0 && albw_cfg_int(g_progress_flags_b, 0) != 0) {
        svc_config->set_int(mod_ctx, g_progress_flags_b, 0);
    }
    for (ConfigVarHandle* const var : kCounterVars) {
        if (var != nullptr && *var != 0 && albw_cfg_int(*var, 0) != 0) {
            svc_config->set_int(mod_ctx, *var, 0);
        }
    }
}

// ============================================
// NEW CODE ENDS HERE
// ============================================
