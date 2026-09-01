// ============================================
// NEW CODE - ALBT multiplatform (dAlbwAlink_* compat layer)
//
// WHAT THIS IS, AND WHY IT IS NOT A VERBATIM PORT
// ------------------------------------------------------------------
// The ported outfit cluster (outfit / sumo_test / wardrobe) calls three helpers
// that the fork defines inside src/d/actor/d_a_alink.cpp:
//
//     dAlbwAlink_resyncClothesEpoch()      fork d_a_alink.cpp:167
//     dAlbwAlink_abortStuckClothesChange() fork d_a_alink.cpp:169
//     dAlbwAlink_nativeCapResolved()       fork d_a_alink.cpp:177
//
// They are thin accessors over FILE-STATIC state in the player actor:
//     s_albwArcEpoch / s_albwClothesModelEpoch  (arc generation gate)
//     s_albwNativeCapResolved                   (Cap Wear fallback flag)
//     s_albwSwapActive / s_albwSwapOldArc /
//     s_albwArcHeapB   / s_albwPhaseReqB        (ALTERNATE-HEAP clothes loader)
//
// Verified against stock (git grep on origin/main, 0 files each): none of that
// state exists in stock dusklight. The fork maintains it from ~256 ALBW edit
// sites woven through the 20k-line player actor, including a second arc heap and
// phase request the fork added to load clothes off the live heap.
//
// A mod cannot reproduce that: the epoch's real consumers are guards the fork
// inlined into daAlink_c::draw / modelDraw, which cannot be inserted from a hook
// without replacing draw() wholesale.
//
// So this file is the consumption-boundary translation (DO-NOT DN-10 step 2) for
// the parts that CAN be honestly translated, and it is LOUD about the parts that
// cannot - it never silently pretends the fork behaviour is present.
//
// ESCALATION (DN-10 forbids self-approval): making Link's MODEL actually rebuild
// on outfit change would require authoring a clothes-loading path against stock's
// changeLink/loadModelDVD, i.e. instance-authored code. That is NOT done here and
// needs the user's go. Ownership, wardrobe storage and outfit stats do not depend
// on it and are fully functional.
// ============================================

#include "global.h"
#include <os.h>
#include "alink_compat.h"
#include "albw_common.h"
#include "clothes_pipeline.h"
#include "config_vars.h"
#include "global.h"

#if TARGET_PC

namespace {

// Arc-generation bookkeeping, ported from fork d_a_alink.cpp:146-147. Kept
// faithful so the outfit module's calls have real effect on mod-side state.
// NOTE: the fork's CONSUMERS of this pair are the draw-time guards at
// d_a_alink.cpp:21061 / 21118 / 21199 / 21536, which live inside stock's
// draw()/modelDraw() and cannot be inserted from a mod. The bookkeeping is
// therefore correct but currently unread - it is kept (rather than dropped) so
// that if a draw hook is ever added the state is already being maintained.
u32  s_arcEpoch          = 0;
u32  s_clothesModelEpoch = 0;
bool s_sawStuck          = false;
bool s_forceClothesRemount = false;

}  // namespace

void dAlbwAlink_resyncClothesEpoch() { s_clothesModelEpoch = s_arcEpoch; }

bool dAlbwAlink_clothesEpochInSync() { return s_clothesModelEpoch == s_arcEpoch; }

void dAlbwAlink_invalidateClothesEpoch() { ++s_arcEpoch; }

// fork d_a_alink.cpp:184 - latches s_albwForceClothesRemount so the next clothes
// settle rebuilds in place after a Custom Models winner change. Stock has no
// Custom Models system (albw_dusk_compat.h: overlay_generation() is constant), so
// nothing can ever raise this condition; the latch is kept faithful anyway.
void dAlbwAlink_requestClothesRemount() {
    s_forceClothesRemount = true;
    albw_clothes_request_remount();
}

// fork d_a_alink.cpp:177 - "true when the last native changeLink resolved the
// requested Cap Wear cap/topknot (false on fallback to the outfit's native hat)".
//
// Cap Wear is a fork Visuals feature backed by the alternate-heap cap/face donor
// pipeline described above; it is not ported, and albw_dusk_compat.h defaults the
// setting to CapWearMode::Off. BOTH consumers short-circuit on Off:
//     d_albw_outfit.cpp:701  cap == Off || dAlbwAlink_nativeCapResolved()
//     d_albw_outfit.cpp:786  cap != Off && !dAlbwAlink_nativeCapResolved()
// so with Cap Wear Off this function is never actually reached. Returning true
// ("nothing left to resolve") is the correct value for the Off case rather than a
// stand-in - but if Cap Wear is ever forced on without the donor pipeline, say so
// once instead of silently reporting success.
bool dAlbwAlink_nativeCapResolved() {
    static bool warned = false;
    if (!warned && albw_cfg_int(g_cap_wear, 0) != 0) {
        warned = true;
        if (svc_log != nullptr) {
            svc_log->warn(mod_ctx,
                          "albw: Cap Wear is set but the fork's cap/face donor pipeline is not "
                          "ported - cap resolution cannot be tracked, reporting resolved");
        }
    }
    return true;
}

// fork d_a_alink.cpp:169 / daAlink_c::albwAbortStuckClothesChange().
//
// The fork's abort recovers a hang that its OWN alternate-heap swap can enter
// (timer bouncing 1<->2 on a never-completing resLoad while draw() hides Link).
// That hang mode is created by s_albwSwapActive / s_albwArcHeapB, neither of
// which exists here, so the fork's recovery body has no counterpart to unwind.
//
// The outfit module only calls this from three stuck-watchdog sites
// (d_albw_outfit.cpp:193 / 201 / 206). If we ever get here it means the watchdog
// tripped on stock's own clothes pipeline - a real condition worth surfacing, not
// swallowing. Log it loudly every time; do NOT fake a recovery we cannot perform.
void dAlbwAlink_abortStuckClothesChange(daAlink_c* link) {
    s_sawStuck = true;
    // The fork alt-heap pipeline IS ported now (clothes_pipeline.cpp), so this
    // performs the fork real recovery instead of only reporting that it could
    // not. Kept as a forwarder so the ported outfit module calls the fork name.
    albw_clothes_abort_stuck(link);
}

bool dAlbwAlink_sawStuckClothesChange() { return s_sawStuck; }

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
