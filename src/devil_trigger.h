#pragma once

#include "mods/api.h"

class fopAc_ac_c;

// ============================================
// NEW CODE - Devil Trigger (INVENTED, no donor).
//
// An enemy below a health threshold enrages: it moves and animates faster,
// and ignores knockback. Design and scope in docs/DEVIL-TRIGGER-SCOPE.md.
//
// Plain float/short in the public surface on purpose: depending on the f32/s16
// typedefs made this header order-sensitive, which is the same trap
// d_a_b_tn.h set earlier (it uses member types it does not include).
//
// This header is the shared POLICY - who qualifies, when it arms, and whether
// an attack is live. The per-enemy wiring (what "faster" means for a given
// actor) lives with that actor; the Darknut's is in btn_parry.cpp.
// ============================================

// ---- the policy ----

// Tier + toggle gate. Common enemies and mid-bosses only, per the scope.
bool dAlbwDevil_isEligible(fopAc_ac_c* actor);

// Health fraction remaining, 0.0..1.0, or a negative value when this actor's
// health cannot be read meaningfully.
//
// The generic reading is actor->health / field_0x560. That is WRONG for some
// actors - the Darknut being the one we know about, where `health` is a
// scratch register holding one hit's damage, not a pool. Such actors register
// an override below.
float dAlbwDevil_healthFraction(fopAc_ac_c* actor);

// Per-actor health override. Return true and fill *outFraction to replace the
// generic reading for this actor. Registered at init by the actor's module.
typedef bool (*dAlbwDevilHealthFn)(fopAc_ac_c* actor, float* outFraction);
void dAlbwDevil_registerHealthOverride(short profName, dAlbwDevilHealthFn fn);

// Armed state. LATCHED: once an actor arms it stays armed until it dies or is
// rebuilt, so a health value hovering at the threshold cannot flicker the mode
// on and off. Call the tick once per frame from the actor's execute seam.
bool dAlbwDevil_isArmed(fopAc_ac_c* actor);
void dAlbwDevil_tickActor(fopAc_ac_c* actor);
void dAlbwDevil_forget(fopAc_ac_c* actor);  // death / delete

// True when this actor currently has an attack collider registered with the
// collision system - i.e. a swing is live and must NOT be sped up.
//
// Generic: scans cCcS::mpObjAt for a collider whose GetAc() is this actor. See
// the scope's caveats - it reads the PREVIOUS frame's registrations, and any
// actor that holds an AT collider permanently will never qualify for Devil
// Trigger (a safe failure, but one that silently excludes it).
bool dAlbwDevil_isAttackLive(fopAc_ac_c* actor);

// The enrage speed multiplier. One tunable, read by both halves (animation
// bracket + movement scale) so they can never drift apart.
float dAlbwDevil_boost();

// Per-actor movement multiplier: dAlbwDevil_boost() if this actor is armed,
// else 1.0. Read by the fopAcM_posMove hook - the movement half of direct
// scaling, paired with the animation bracket in btn_parry.cpp.
float dAlbwDevil_boostFor(fopAc_ac_c* actor);


ModResult albw_devil_trigger_init(ModError* error);
void albw_devil_trigger_reset();
