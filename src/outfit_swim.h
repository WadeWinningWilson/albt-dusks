#ifndef ALBW_OUTFIT_SWIM_H
#define ALBW_OUTFIT_SWIM_H

#include "mods/api.h"

// ============================================
// Outfit Stats — swim mechanics (the aquatic half of the outfit-stats port).
//
// The fork lets outfit-wearing humans dive and swim underwater like a Zora, by ORing
// dAlbwOutfitStats_isSubmergedHumanSwim() into ~25 sites across the swim state machine
// AND inside posMove()/execute() — the two biggest shared actor functions, which the
// stock exe compiles in and the .dusk cannot overlay.
//
// Native route (DN-10, step 2 boundary translation — a whole-function overlay of
// posMove/execute + 11 swim procs is proven infeasible here): the stock swim machine
// already routes ALL of diving — entry, Zora dive controls, buoyancy sustain in
// execute() (d_a_alink.cpp:18320), posMove vertical speed — through getZoraSwim(), an
// OUT-OF-LINE, hookable predicate. Hooking it true for an outfit-wearing human in water
// turns the entire stock machine into treating them as a Zora swimmer, giving diving
// natively. getZoraSwim() gates on the Zora swim ANIMATION, not on any state flag, so
// re-gating it does not fire recursively and does not force a Zora ability check
// (checkOxygenTimer still gates on checkZoraWearAbility(), so the human still drowns).
// The fork's proc rewrites are polish (resurface-anim skips, soft-surface) layered on
// top; they are intentionally not reproduced.
// ============================================

ModResult albw_outfit_swim_init(ModError* error);

#endif  // ALBW_OUTFIT_SWIM_H
