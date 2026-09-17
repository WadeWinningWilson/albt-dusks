// ============================================
// End-Game Midna Transform - part of dev.albt.albw.
// Port of the fork editor toggle "End-Game Transform (Midna + Crystal)"
// (src/dusk/ui/editor.cpp): it grants the free wolf transform by setting
// event bits M_077 + F_0250 and transform levels 0..3. Here it is applied
// each frame in gameplay when its own toggle or True ALBW is on, so the Wolf
// Combat that True ALBW unlocks in the shop is actually usable.
//
// Grant-only: we never CLEAR the bits when the toggle is turned off, so a legit
// save that already has the shadow crystal / Midna-revived story state is never
// clobbered (unlike the fork's editor toggle, which is a manual set/clear).
// ============================================

#include "end_game_transform.h"

#include "albw_common.h"
#include "config_vars.h"

#include "d/d_com_inf_game.h"
#include "d/d_save.h"

// ============================================
// Grant the end-game transform save state, once, when in gameplay.
// ============================================
void albw_end_game_transform_tick() {
    const bool enabled =
        albw_cfg_bool(g_end_game_transform, false) || albw_cfg_bool(g_true_albw, false);
    if (!enabled) {
        return;
    }

    // Only touch save state once a game is actually loaded (Link actor present).
    // Before that, getPlayer(0) is null and the save/status is not the player's.
    if (g_dComIfG_gameInfo.play.getPlayer(0) == nullptr) {
        return;
    }

    // Already granted -> nothing to write (avoids marking the save dirty each frame).
    if (dComIfGs_isEventBit(dSv_event_flag_c::M_077) &&
        dComIfGs_isEventBit(dSv_event_flag_c::M_067) &&
        dComIfGs_isEventBit(dSv_event_flag_c::F_0250) && dComIfGs_isTransformLV(3)) {
        return;
    }

    // The transform only fires when daMidna_c::checkMetamorphoseEnableBase() passes, which
    // needs BOTH the shadow crystal (M_077) AND Midna actually riding Link
    // (checkMidnaRide() -> M_067). The fork's editor "End-Game Transform" sets M_077 +
    // F_0250 + LV0-3; its d_s_menu debug enable sets M_067 (riding) + M_011 (wolf chains
    // removed). Combine both so the transform is usable on any save.
    dComIfGs_onEventBit(dSv_event_flag_c::M_077);   // shadow crystal - can transform
    dComIfGs_onEventBit(dSv_event_flag_c::M_067);   // Midna riding (checkMidnaRide)
    dComIfGs_onEventBit(dSv_event_flag_c::M_011);   // Midna removed wolf's chains
    dComIfGs_onEventBit(dSv_event_flag_c::F_0250);  // Midna revived
    for (int i = 0; i <= 3; ++i) {
        dComIfGs_onTransformLV(i);
    }

    if (svc_log != nullptr) {
        svc_log->info(mod_ctx,
                      "[albt] end-game transform granted (M_077 + M_067 + M_011 + F_0250 + LV0-3)");
    }
}
