// ============================================
// NEW CODE - ALBW Port (Soulbound Red Potion grant driver)
// The fork grants the soulbound red potion via its editor/settings callback
// (settings.cpp:1705 -> dAlbwPotion_editorSetSoulboundEnabled). The .dusk has no
// such editor, so this tick bridges the g_soulbound_potion config var to the same
// core function. Grant is idempotent (guarded on dAlbwPotion_isSoulboundRedInSlot)
// so a reload never overwrites the player's accumulated charges.
// ============================================
#include "helpers/string.hpp"  // TEXT_SPAN - must precede any d_save.h include (via potion.h)

#include "potion_grant.h"

#include "potion.h"
#include "config_vars.h"
#include "albw_common.h"

#include "d/d_com_inf_game.h"

#if TARGET_PC

namespace {
int s_lastEnabled = -1;  // -1 = not yet evaluated -> forces a first-pass check per load
}  // namespace

ModResult albw_potion_grant_init(ModError*) {
    s_lastEnabled = -1;
    return MOD_OK;
}

void albw_potion_grant_tick() {
    // Only touch save/inventory once a game is loaded (Link actor present); before
    // that getPlayer(0) is null and the status is not the player's.
    if (g_dComIfG_gameInfo.play.getPlayer(0) == nullptr) {
        return;
    }

    const bool on = albw_cfg_bool(g_soulbound_potion, false);
    const int cur = on ? 1 : 0;

    if (cur != s_lastEnabled) {
        s_lastEnabled = cur;
        if (!on) {
            dAlbwPotion_editorSetSoulboundEnabled(false);  // remove from SLOT_11
            return;
        }
    }

    // Toggle on: ensure the potion exists in SLOT_11. isSoulboundRedInSlot keeps this
    // idempotent, so it grants a fresh 2-charge bottle only when the slot lacks one
    // (fresh save / just enabled) and never clobbers existing charges on reload.
    if (on && !dAlbwPotion_isSoulboundRedInSlot(kAlbwPotionSoulboundSlot)) {
        dAlbwPotion_applyDefaultInventorySlot11();
    }
}

#else

ModResult albw_potion_grant_init(ModError*) { return MOD_OK; }
void albw_potion_grant_tick() {}

#endif  // TARGET_PC
