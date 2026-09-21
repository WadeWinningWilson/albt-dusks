// ============================================
// NEW CODE - ALBW Port (Dusklight 2.0 custom-actor Tear of Light)
// Behavior half: spawn at the death spot, float/bob, collect on proximity
// (wolf OR human form), grant half-of-lost rupees, play the vanilla tear SEs.
// The visual is drawn separately by tear_glow.cpp via GfxService, reading the
// active tear's position published here. See tear_actor.hpp.
// ============================================
#include "tear_actor.hpp"

#include "albw_common.h"

#include "d/d_com_inf_game.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "Z2AudioLib/Z2AudioMgr.h"
#include "Z2AudioLib/Z2Instances.h"

#include <cmath>

IMPORT_SERVICE(ActorService, svc_actor);

namespace {

// Published state for the GfxService glow hook (tear_glow.cpp reads these).
bool  g_tearActive = false;
bool  g_tearCollected = false;  // latched on pickup; soul_of_light stops respawning
ActorId g_tearId = 0;           // tracked spawn (for despawn on a new death)
cXyz  g_tearPos = {0.0f, 0.0f, 0.0f};
f32   g_tearScale = 150.0f;  // glow half-extent in world units (~6x the initial speck)
f32   g_tearAlpha = 0.0f;    // fade-in / fade-out

static constexpr f32 kPickupRange = 250.0f;   // fork checkGetArea distance
static constexpr f32 kFloatAmp = 15.0f;
static constexpr f32 kFloatSpeed = 0.12f;
static constexpr s16 kArmFrames = 20;         // brief delay before collectable

daPy_py_c* playerActor() {
    return static_cast<daPy_py_c*>(g_dComIfG_gameInfo.play.getPlayer(0));
}

void playSe(u32 se) {
    if (Z2AudioMgr* audio = Z2GetAudioMgr()) {
        audio->seStart(se, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }
}

void grantRecovery(u16 recovery) {
    if (recovery == 0) {
        return;
    }
    dSv_player_status_a_c& st = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA();
    u32 next = static_cast<u32>(st.getRupee()) + recovery;
    if (next > st.getRupeeMax()) {
        next = st.getRupeeMax();
    }
    st.setRupee(static_cast<u16>(next));
    playSe(Z2SE_SY_LIGHT_DROP_GET);
}

}  // namespace

// ---- actor callbacks --------------------------------------------------------

cPhs_Step maAlbwTear_c::create() {
    fopAcM_ct(this, maAlbwTear_c);

    mHomeY = current.pos.y;
    mFloatPhase = 0.0f;
    mRecovery = static_cast<u16>(fopAcM_GetParam(this) & 0xFFFF);
    mArmTimer = kArmFrames;
    mPicked = false;

    g_tearActive = true;
    g_tearPos = current.pos;
    g_tearAlpha = 0.0f;

    playSe(Z2SE_OBJ_LIGHTDROP_APPEAR);
    return cPhs_COMPLEATE_e;
}

int maAlbwTear_c::Execute() {
    if (mArmTimer > 0) {
        --mArmTimer;
    }

    // Float / bob around the spawn height.
    mFloatPhase += kFloatSpeed;
    current.pos.y = mHomeY + std::sin(mFloatPhase) * kFloatAmp;

    // Publish for the glow hook (fade in over the first ~0.5s).
    g_tearActive = true;
    g_tearPos = current.pos;
    if (g_tearAlpha < 1.0f) {
        g_tearAlpha += 0.06f;
        if (g_tearAlpha > 1.0f) {
            g_tearAlpha = 1.0f;
        }
    }

    // Proximity pickup — form-agnostic (works in wolf or human form).
    if (!mPicked && mArmTimer == 0) {
        if (daPy_py_c* player = playerActor()) {
            if (current.pos.abs(player->current.pos) < kPickupRange) {
                grantRecovery(mRecovery);
                // ============================================
                // DO NOT call player->onWolfLightDropGet() here. It was added
                // for cosmetics ("douse Link in the blue light-absorb glow")
                // with the comment "clears naturally in wolf form" - and that
                // assumption is false for this actor, which is collected in
                // HUMAN form during normal play. It caused a softlock.
                //
                // onWolfLightDropGet (d_a_player.h:1198) sets FLG3_UNK_200000,
                // a NO-RESET flag. The only code in the whole tree that clears
                // it is daAlink_c::resetWolfBallGrab (d_a_alink_wolf.inc:6927),
                // which is WOLF-ONLY. In human form nothing clears it, so the
                // glow loop at d_a_alink_effect.inc:407-428 runs forever - and
                // once the tear count reaches the needed count that loop calls
                // changeDemoMode(DEMO_UNK_94_e) on every frame an event is
                // running. A death is an event, so the death sequence gets
                // hijacked into a demo pose: Link dead and in a demo at once,
                // model deformed, softlocked.
                //
                // THE FORK DOES NOT DO THIS. Its only onWolfLightDropGet call
                // is the stock one inside daObjDrop_c (fork d_a_obj_drop.cpp:
                // 506 == stock :448), reached only by the real wolf tear. The
                // custom death-tear never touches Link's wolf absorb state.
                //
                // If a collect flourish is wanted, draw it from tear_glow.cpp,
                // which already owns this actor's visuals - do not borrow a
                // wolf-only player state machine to get it.
                // ============================================
                mPicked = true;
                g_tearCollected = true;
            }
        }
    }

    if (mPicked) {
        g_tearActive = false;
        fopAcM_delete(this);
    }
    return 1;
}

int maAlbwTear_c::Draw() {
    // Visual is drawn by tear_glow.cpp (GfxService). Nothing to do in the game
    // render pass.
    return 1;
}

int maAlbwTear_c::Delete() {
    g_tearActive = false;
    return 1;
}

// ---- callback thunks --------------------------------------------------------

namespace {
cPhs_Step tear_create(void* self) { return static_cast<maAlbwTear_c*>(self)->create(); }
int tear_delete(void* self) { return static_cast<maAlbwTear_c*>(self)->Delete(); }
int tear_execute(void* self) { return static_cast<maAlbwTear_c*>(self)->Execute(); }
int tear_is_delete(void*) { return 1; }
int tear_draw(void* self) { return static_cast<maAlbwTear_c*>(self)->Draw(); }
}  // namespace

s16 maAlbwTear_c::sProcName = -1;
ActorHandle maAlbwTear_c::sActorHandle = static_cast<ActorHandle>(-1);
const ActorProfileDesc maAlbwTear_c::sProfile = {
    .name = MA_ALBW_TEAR_NAME,
    .priority_group = 7,
    .process_size = sizeof(maAlbwTear_c),
    .draw_priority = fpcDwPi_OBJ_LBOX_e,
    // No CULL flag: the tear always ticks so it can be collected and keep its
    // glow published even at the screen edge.
    .status = fopAcStts_UNK_0x40000_e | fopAcStts_UNK_0x4000_e,
    .group = fopAc_ACTOR_e,
    .cull_type = fopAc_CULLBOX_CUSTOM_e,
    .create_function = tear_create,
    .delete_function = tear_delete,
    .execute_function = tear_execute,
    .is_delete_function = tear_is_delete,
    .draw_function = tear_draw,
};

// ---- public API -------------------------------------------------------------

ModResult albw_tear_actor_init(ModError*) {
    if (svc_actor->register_actor(mod_ctx, &maAlbwTear_c::sProfile, &maAlbwTear_c::sProcName,
                                  &maAlbwTear_c::sActorHandle) != MOD_OK) {
        svc_log->error(mod_ctx, "tear actor: register_actor failed");
        return MOD_ERROR;
    }
    return MOD_OK;
}

ActorId albw_tear_actor_spawn(const cXyz& pos, s8 room, u16 recovery) {
    ActorSpawnParams params = {};
    params.parameters = recovery;
    params.argument = 0;
    params.room_num = room;
    params.position.x = pos.x;
    params.position.y = pos.y;
    params.position.z = pos.z;
    params.angle.x = 0;
    params.angle.y = 0;
    params.angle.z = 0;
    params.scale.x = 1.0f;
    params.scale.y = 1.0f;
    params.scale.z = 1.0f;
    params.create_function = nullptr;

    g_tearCollected = false;  // fresh tear for this death
    ActorId id = 0;
    ModResult r = svc_actor->create_actor_from_name(mod_ctx, MA_ALBW_TEAR_NAME, &params, &id);
    if (r != MOD_OK) {
        return 0;
    }
    g_tearId = id;
    return id;
}

void albw_tear_actor_despawn() {
    if (g_tearId != 0 && g_tearActive) {
        svc_actor->delete_actor(mod_ctx, g_tearId);
    }
    g_tearId = 0;
    g_tearActive = false;
    g_tearCollected = false;  // new death = fresh tear cycle (fixes "happened once, never again")
}

bool albw_tear_actor_is_active() {
    return g_tearActive;
}

bool albw_tear_actor_was_collected() {
    return g_tearCollected;
}

bool albw_tear_actor_get_draw(cXyz* outPos, f32* outScale, f32* outAlpha) {
    if (!g_tearActive) {
        return false;
    }
    if (outPos != nullptr) {
        *outPos = g_tearPos;
    }
    if (outScale != nullptr) {
        *outScale = g_tearScale;
    }
    if (outAlpha != nullptr) {
        *outAlpha = g_tearAlpha;
    }
    return true;
}
