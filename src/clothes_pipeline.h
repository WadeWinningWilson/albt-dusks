#pragma once

// ============================================
// NEW CODE - ALBW Port (build-then-swap clothes pipeline)
//
// Ports the fork's alternate-heap clothes loader out of the player actor. In the
// fork this is edits INSIDE stock code:
//
//   d_a_alink_swindow.inc:114  daAlink_c::loadModelDVD()      (the pipeline)
//   d_a_alink_swindow.inc:83   daAlink_c::setClothesChange()  (re-entrancy guard)
//   d_a_alink.cpp:5541         create()      - allocates the alt heap
//   d_a_alink.cpp:21968        ~daAlink_c()  - frees it
//   d_a_alink.cpp:249-252      the swap state file-statics
//
// Every one of those functions IS declared in stock's d_a_alink.h, so each is
// hookable by member pointer and needs no albw_symbols entry. The state lives
// here instead of in the actor.
//
// WHY IT MATTERS: without this, a clothes change frees Link's arc heap and
// reloads into the same address while the old models are still referenced -
// the freed-window crash the fork built this to avoid. It is also what makes
// the visual outfit swap work at all.
// ============================================

#include "mods/api.h"

class daAlink_c;
class daMidna_c;

ModResult albw_clothes_pipeline_init(ModError* error);

// fork daMidna_c::resetDemoBck + removeDemoBodyBck, ported as a free function (both
// are fork ADDITIONS absent from stock daMidna_c). Exposed so the ported changeLink
// (changelink.cpp) can reach it where the fork body calls midna->resetDemoBck().
void albw_midna_reset_demo_bck(daMidna_c* midna);

// Real implementations of two fork helpers that alink_compat previously could
// only log about, now that the alt-heap pipeline exists in the mod.
void albw_clothes_abort_stuck(daAlink_c* link);
void albw_clothes_request_remount();

// ============================================
// NEW CODE - outfit-transition crash family P1 (gate latch support)
//
// True while the clothes pipeline owns an in-flight transition: the alt-heap
// swap is active (fork s_albwSwapActive, d_a_alink.cpp:249-252) OR control is
// currently inside the ported loadModelDVD (so the changeLink/changeWolf calls
// its completion branches make - including the same-arc path, which clears the
// swap flag just before calling changeLink(1) - still count as part of the
// transition). changelink.cpp combines this with mClothesChangeWaitTimer to
// decide latched-vs-live gate evaluation.
// ============================================
bool albw_clothes_transition_in_flight();

// ============================================
// NEW CODE - outfit-transition crash family P3 (single magic-ready flag)
//
// THE one s_albwMagicModelReady (fork d_a_alink.cpp file-static; donor name
// kept). Defined in clothes_pipeline.cpp with EXTERNAL linkage; consumed by
// albw_setWaterDropColor (draw side) and WRITTEN by both sync sites:
//   - on_change_link_magic_pre   (clothes_pipeline.cpp - pre-hook, fires first)
//   - the ported changeLink body (changelink_port.inc:93 - Magic branch)
// changelink.cpp previously carried its OWN static copy (its writes were dead),
// so body and draw consumers could diverge; now they cannot.
// ============================================
extern bool s_albwMagicModelReady;
