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

// ---- generalized parry-opening (DT §7) ----
//
// A parry on an ARMED (enraged) enemy opens an elongated guard window. This is
// the GENERIC primitive — not Darknut-specific: dShield_onShieldHit calls
// openGuardWindow for whatever enemy was parried, and each DT-enforced actor
// consults isGuardOpen to lift its enrage for the window's duration (the Darknut
// lifts its no-flinch immunity, so hits stagger it — the opening). No-op on an
// unarmed enemy: only an enraged one has an opening to force.
void dAlbwDevil_openGuardWindow(fopAc_ac_c* actor);
bool dAlbwDevil_isGuardOpen(fopAc_ac_c* actor);

// True exactly ONCE per parry (per openGuardWindow call): the actor's enforcement
// calls this to fire the bashed-reaction a single time, not every frame the
// window is open. Fixes the "reacts 3 times" repeat.
bool dAlbwDevil_consumeOpenReaction(fopAc_ac_c* actor);

// ---- per-actor DT compatibility profile ----
//
// The generic DT enforcement (arm -> not-dead -> not-mid-swing -> second execute)
// is identical for every enemy; only the ACTOR-SPECIFIC compat differs. A profile
// packages that difference so onboarding a new enemy is "fill a struct," and the
// eventual migration from typed per-actor hooks to one generic actor dispatcher
// only swaps the FRONT DOOR (who calls dAlbwDevil_execProfile), not this system.
// The health reader stays separate via dAlbwDevil_registerHealthOverride.
//
// See docs/DEVIL-TRIGGER-METHODS.md (profile abstraction) + CURRENT-STATE §6.
struct DTProfile {
    short profName;                        // fpcNm_* this profile applies to
    bool  redispatchSafe;                  // true: run a second execute; false: fallbackTick only
    bool  (*isDead)(fopAc_ac_c*);          // terminal/death state -> forget (null = never)
    void  (*preRedispatch)(fopAc_ac_c*);   // compat pre-pass before the 2nd execute (null = none)
    void  (*redispatch)(fopAc_ac_c*);      // THE second execute (required when redispatchSafe)
    void  (*postRedispatch)(fopAc_ac_c*);  // compat post-pass after (null = none)
    void  (*fallbackTick)(fopAc_ac_c*);    // per-frame treatment when !redispatchSafe (null = none)
};

// Register at init (idempotent-friendly; order-independent like the health table).
void dAlbwDevil_registerProfile(const DTProfile* profile);

// The generic DT enforcement, driven by the actor's profile. Call once per frame
// from the front door (today: each actor's typed execute-POST hook; later: one
// generic actor dispatcher). No-op for actors without a profile or when DT is off.
// The caller owns the re-entry latch (the redispatch re-enters the front door).
// Returns true iff it ran the second execute this frame (for the bring-up audit).
bool dAlbwDevil_execProfile(fopAc_ac_c* actor);

ModResult albw_devil_trigger_init(ModError* error);
void albw_devil_trigger_reset();

// Bokoblin (E_OC) DT profile — the second compatibility profile. Registers the
// profile + its execute front door. Defined in bokoblin_dt.cpp.
ModResult albw_bokoblin_dt_init(ModError* error);
