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

// ============================================
// A hook that fails to resolve must NOT abort mod_initialize.
//
// This helper used to call mods::set_error(..., MOD_ERROR, ...) and return
// false, which made mod_initialize return MOD_ERROR - so ONE unresolved symbol
// unloaded the ENTIRE mod. That is how a single missing hook target reached
// players as "Failed - Reason: <hook name>" with nothing loaded at all, on a
// build where every other feature was fine. It is the same doctrine fyrus.cpp
// already states for the boss hooks.
//
// Now the miss is LOUD and SCOPED: the feature that needed the hook is
// inactive for the run and says so by name in the log, and everything else
// still loads. Never make this silent - a quiet miss turns "never bound" into
// "plausibly wrong forever".
// ============================================
bool install(ModError*, const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, name);
            svc_log->error(mod_ctx,
                           "hook above did NOT install - that feature is inactive this run");
        }
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
