#pragma once

// ============================================
// NEW CODE - ALBW Port (Dusklight 2.0 custom-actor Tear of Light)
// The vanilla tear (daObjDrop_c) has a NULL draw slot; its visuals are scene
// particles (Pscene011) absent from most stages, so it was invisible + drove a
// crash-prone supplemental-archive workaround. Dusklight 2.0's ActorService lets
// us register our own actor, and GfxService draws its glow procedurally, so the
// tear is visible everywhere with no scene-particle dependency. Behavior
// (spawn-at-death / float / wolf+human pickup / sounds / half-of-lost recovery)
// is reproduced here to match the fork's daObjDrop tear.
// ============================================

#include "mods/svc/actor.h"

#include "f_op/f_op_actor.h"
#include "SSystem/SComponent/c_phase.h"
#include "SSystem/SComponent/c_xyz.h"

#define MA_ALBW_TEAR_NAME "altear"  // <=7 chars: ActorProfileDesc::name is char[8]

class maAlbwTear_c : public fopAc_ac_c {
public:
    request_of_phase_process_class mPhase;
    f32   mHomeY;        // float centre (world Y the bob oscillates around)
    f32   mFloatPhase;   // bob phase accumulator
    u16   mRecovery;     // rupees granted on pickup (half of what death lost)
    s16   mArmTimer;     // short delay before the tear is collectable
    bool  mPicked;       // set once collected -> queues deletion

    cPhs_Step create();
    int Delete();
    int Execute();
    int Draw();

    static s16 sProcName;
    static ActorHandle sActorHandle;
    static const ActorProfileDesc sProfile;
};

// Registration + dynamic spawn API used by soul_of_light.cpp.
ModResult albw_tear_actor_init(ModError*);                          // mod init: register the actor
ActorId albw_tear_actor_spawn(const cXyz& pos, s8 room, u16 recovery);
void  albw_tear_actor_despawn();                                   // delete the tracked tear (new death / reset)
bool  albw_tear_actor_is_active();                                  // currently spawned + ticking
bool  albw_tear_actor_was_collected();                             // picked up (soul_of_light stops respawning)
bool  albw_tear_actor_get_draw(cXyz* outPos, f32* outScale, f32* outAlpha);  // for the gfx glow hook

// Visual half (tear_glow.cpp): GfxService billboard. Declared here so mod.cpp
// can register it in the init chain (needs FEATURES webgpu). ModResult/ModError
// come from mods/api.h via mods/svc/actor.h above.
// Ends the native collect glow. MUST be ticked: the stock loop it uses has
// no exit of its own. See the comment on the definition.
void albw_tear_collect_glow_tick();

ModResult albw_tear_glow_init(ModError*);
ModResult albw_tear_glow_shutdown(ModError*);
