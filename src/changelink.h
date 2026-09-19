#ifndef ALBW_CHANGELINK_H
#define ALBW_CHANGELINK_H

#include "mods/api.h"

// ============================================
// ALBW Port — daAlink_c::changeLink whole-function replacement
//
// PROBLEM (Zora + Sumo quick-swap crash): stock daAlink_c::changeLink builds the
// sumo body's face model from getObjectRes(mArcName, "al_face.bmd"). Over the Zora
// base (mArcName == "Zmdl") that arc ships only zl_face — al_face is absent — so the
// lookup returns NULL, initModel(NULL) yields a NULL face model, and the immediately
// following eye-LOD block dereferences mpLinkFaceModel->getModelData()->getTexture()
// -> EXCEPTION_ACCESS_VIOLATION (stock d_a_alink_wolf.inc face + eye-LOD block).
//
// FIX: the fork hardened changeLink's face sourcing into a cascade — sumo body borrows
// Link's al_face from the private Kmdl donor (dAlbwSumoTest_sumoFaceData), else from the
// Kmdl base arc when the base is Zora, and never builds a face model from NULL. The DUSK
// already carries the primitives (sumo_test.cpp) and keeps Kmdl resident over the Zora
// base (resourcesReady), but nothing consumed them because changeLink was never replaced
// (see clothes_pipeline.cpp:418-427 / alink_compat.cpp:32-36 — the escalation this fixes).
//
// changeLink is an exported daAlink_c method, so it cannot be redefined (LNK2005). Same
// subclass + HOOK_SKIP_ORIGINAL technique as outfit_swim.cpp / mq_heart_meter.cpp: the
// verbatim fork body lives on a layout-compatible subclass AlbwChangeLink_c and a pre-hook
// dispatches every changeLink call to it. Coexists with clothes_pipeline's existing
// changeLink pre/post hooks (shared trampoline; all pre-hooks run, all post-hooks run).
//
// User-approved DN-10 escalation (replacing an exported engine function incl. its
// file-static deps).
// ============================================

ModResult albw_changelink_init(ModError* error);

#endif  // ALBW_CHANGELINK_H
