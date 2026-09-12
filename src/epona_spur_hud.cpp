// ============================================
// NEW CODE - ALBW Port (Epona dash-spur HUD gate — d_meter2.cpp / d_meter_hakusha.cpp)
//
// Fork feature: settings.game.showEponaSpurHud (default TRUE = vanilla behavior).
// When the player turns it OFF, the fork suppresses the Epona dash-spur meter
// sub-content at two seams:
//   1) dMeter2_c::checkSubContents  — skip *allocating* the dMeterHakusha_c
//      sub-content (killSubContents(1); return; before the create block).
//   2) dMeterHakusha_c::draw        — skip *drawing* it (early return at top).
//
// Seam (2) is the one that produces the observable result, and it ports cleanly:
// reproduced verbatim as a pre-hook that returns HOOK_SKIP_ORIGINAL, so the spur
// HUD is hidden exactly as the fork hides it. Seam (1) is a pure allocation
// optimization — it cannot be reproduced by a member/symbol hook without
// reimplementing checkSubContents (a multi-branch dispatch that also owns the
// scope/sumo/string sub-contents, so a whole-function skip would break those),
// and with the leaf draw suppressed its omission is unobservable (the hakusha
// sub-content's _execute only positions/animates internal HUD panes — no world
// side effects). So the draw seam alone is the faithful port of this feature.
// ============================================

#include "global.h"

#include "d/d_meter_hakusha.h"

#include "albw_common.h"
#include "config_vars.h"
#include "epona_spur_hud.h"
#include "mods/hook.hpp"

#if TARGET_PC

namespace {

// dMeterHakusha_c::draw — the Epona dash-spur meter render. Fork gates it at the
// top with `if (!showEponaSpurHud) return;`; we reproduce that as a skip pre-hook.
DEFINE_HOOK(&dMeterHakusha_c::draw, EponaSpurDraw);

HookAction on_epona_spur_draw_pre(ModContext*, void*, void*, void*) {
    // Default TRUE = show (vanilla). Only suppress when the user turned it off.
    if (!albw_cfg_bool(g_epona_spur_hud, true)) {
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace

ModResult albw_epona_spur_hud_init(ModError* error) {
    if (!install(error, "EponaSpurDraw",
                 mods::hook_add_pre<EponaSpurDraw>(svc_hook, on_epona_spur_draw_pre)))
    {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw epona spur hud gate ready");
    return MOD_OK;
}

#else  // TARGET_PC

ModResult albw_epona_spur_hud_init(ModError*) { return MOD_OK; }

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
