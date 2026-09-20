// ============================================
// NEW CODE - ALBW Port (Wolf Art: "Midna's Grasp" - the ACTOR)
//
// Fork d_a_albw_midna_arm.cpp, verbatim: an auto-attacking helper actor - for
// ~15 s Midna's hair-hand repeatedly stretches to the current Z-lock target,
// strikes it as a SWORD hit, and retracts. Its own actor + its own AT collider
// (owner = THIS actor, not ALINK), so hits get the generic mHitType and can
// never feed the wolf charge counter - "special arts never build charges"
// holds structurally.
//
// TWO boundary translations, the rest is byte-faithful:
//  1. The fork appends the process profile to the host's table
//     (fpcNm_ALBW_MIDNA_ARM_e = 0x31A). A mod cannot extend stock's table, so
//     the profile is served from a pre-hook on fpcPf_Get - the same technique
//     the WW registry already runs on this host. The id value is the fork's.
//  2. The fork's draw-prio enum entry (fpcDwPi_ALBW_MIDNA_ARM_e) does not
//     exist in stock; the actor draws nothing (its Draw is `return 1`), so it
//     uses fpcDwPi_BOOMERANG_e, the helper-actor slot its own comment cites
//     as the pattern.
//
// The hair-reach visual bridge (dAlbwMidnaArm_setReachPos et al.) is already
// in wolf_combat.cpp; the daAlink_c::setNeckAngle re-apply is hooked here.
// Fork applies BEFORE the vanilla body (so a live Wchain aim would win); a mod
// can only re-apply AFTER, so while the art runs the arm's aim wins instead -
// noted divergence, only reachable if a cutscene chain-aim overlaps the art.
// ============================================

#include "global.h"
#include <os.h>

#include "SSystem/SComponent/c_math.h"
#include "d/d_com_inf_game.h"
#include "d/d_cc_d.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_profile.h"
#include "f_pc/f_pc_leaf.h"
#define private public
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#undef private

#include "albw_common.h"
#include "albw_game.h"
#include "albw_symbols.h"
#include "wolf_combat.h"
#include "mods/hook.hpp"

#if TARGET_PC

// fork f_pc_name.h:808 - appended PC-only process name.
static constexpr s16 kFpcNm_ALBW_MIDNA_ARM = 0x31A;

class daAlbwMidnaArm_c : public fopAc_ac_c {
public:
    enum State {
        STATE_IDLE_e,     // no target: hair hovers in the READY stance above Midna, waiting
        STATE_STRETCH_e,  // hair extending toward the target (striking)
        STATE_HIT_e,      // strike window: AT collider armed at the target (striking)
        STATE_RETRACT_e,  // release-return: eased glide back (no-snap)
        STATE_PAUSE_e,    // brief rest between punches (READY hover)
    };

    int create();
    int Delete();
    int Execute();
    int Draw();

    static bool isAlive();

private:
    fopAc_ac_c* getLockTarget();
    void        enterState(State i_state);

    dCcD_Stts mStts;
    dCcD_Cyl  mAtCyl;

    State mState;
    int   mStateTimer;
    int   mLifeFrames;
    cXyz  mReachFrom;
    cXyz  mReachTarget;
};

// ============================================
// Tuning - fork values verbatim (30 Hz sim tick: N frames = N/30 s).
// ============================================
namespace {
constexpr int kArmLifeFrames    = 450;   // 15 s
constexpr int kArmStretchFrames = 8;     // jab OUT: 0.27 s
constexpr int kArmHitFrames     = 4;     // strike window (~0.13 s)
constexpr int kArmRetractFrames = 11;    // jab RETURN: 0.37 s
constexpr int kArmPauseFrames   = 5;     // rest between punches: 0.17 s
constexpr f32 kArmIdleForward   = 150.0f;
constexpr f32 kArmIdleHoverY    = 20.0f;
constexpr int kArmAtp           = 4;     // sword attack power (Hurricane initCutTurnAt uses 4)
constexpr f32 kArmStrikeRadius  = 80.0f;
constexpr f32 kArmStrikeHeight  = 120.0f;
constexpr f32 kArmReachYOffset  = 80.0f;
constexpr f32 kArmMaxRange      = 480.0f;  // gameplay range to the target's eyePos

bool s_armAlive = false;  // one-at-a-time gate

// Sword-typed AT source (mirrors daAlink's l_atCylSrc; radius/height overridden
// per strike). Tg/Co lines are empty - the arm itself can't be hit.
static dCcD_SrcCyl l_armAtCylSrc = {
    {
        {0, {{AT_TYPE_NORMAL_SWORD, 2, 0x1B}, {0, 0}, 0}},
        {dCcD_SE_SWORD, 3, 1, 0, {1}},
        {dCcD_SE_NONE, 0, 0, 0, {0}},
        {0},
    },
    {
        {
            {0.0f, 0.0f, 0.0f},
            80.0f,
            120.0f,
        },
    }
};
}  // namespace

bool daAlbwMidnaArm_c::isAlive() {
    return s_armAlive;
}

fopAc_ac_c* daAlbwMidnaArm_c::getLockTarget() {
    fopAc_ac_c* tgt = albw_game::attention()->LockonTarget(0);
    if (tgt == NULL) {
        return NULL;
    }
    if (fopAcM_GetGroup(tgt) != fopAc_ENEMY_e) {
        return NULL;
    }
    daPy_py_c* link = daPy_getPlayerActorClass();
    if (link != NULL && link->current.pos.abs(tgt->eyePos) > kArmMaxRange) {
        return NULL;
    }
    return tgt;
}

void daAlbwMidnaArm_c::enterState(State i_state) {
    mState = i_state;
    switch (i_state) {
    case STATE_STRETCH_e: mStateTimer = kArmStretchFrames; break;
    case STATE_HIT_e:     mStateTimer = kArmHitFrames;     break;
    case STATE_RETRACT_e: mStateTimer = kArmRetractFrames; break;
    case STATE_PAUSE_e:   mStateTimer = kArmPauseFrames;   break;
    default:              mStateTimer = 0;                 break;
    }
}

int daAlbwMidnaArm_c::create() {
    // fopAcM_ct minus its JUT_ASSERT (OSPanic is not exported): the macro's real
    // work is the placement construction over the fopAc allocation.
    fopAcM_ct_placement(this, daAlbwMidnaArm_c);

    // Collider owned by THIS actor - the load-bearing line.
    mStts.Init(60, 0xFF, this);
    mAtCyl.Set(l_armAtCylSrc);
    mAtCyl.SetStts(&mStts);
    mAtCyl.SetAtAtp(kArmAtp);
    mAtCyl.SetR(kArmStrikeRadius);
    mAtCyl.SetH(kArmStrikeHeight);

    mLifeFrames = kArmLifeFrames;
    enterState(STATE_IDLE_e);

    s_armAlive = true;
    return cPhs_COMPLEATE_e;
}

int daAlbwMidnaArm_c::Delete() {
    dAlbwMidnaArm_clearReachPos();
    s_armAlive = false;
    return 1;
}

int daAlbwMidnaArm_c::Execute() {
    daPy_py_c* link = daPy_getPlayerActorClass();

    mLifeFrames--;
    if (mLifeFrames <= 0 || link == NULL || !daPy_py_c::checkNowWolf() ||
        !dAlbwWolfCombat_isEnabled())
    {
        dAlbwMidnaArm_clearReachPos();
        fopAcM_delete(this);
        return 1;
    }

    // Ride along with wolf Link (keeps room/cull bookkeeping sane).
    current.pos = link->current.pos;
    fopAcM_SetRoomNo(this, fopAcM_GetRoomNo(static_cast<fopAc_ac_c*>(link)));

    // The reach origin: Midna's perch on the wolf's back.
    mReachFrom = link->current.pos;
    mReachFrom.y += kArmReachYOffset;

    fopAc_ac_c* tgt = getLockTarget();

    // The READY rest point: ahead of Midna's perch along the wolf's facing.
    cXyz hoverPos = mReachFrom;
    hoverPos.x += cM_ssin(link->shape_angle.y) * kArmIdleForward;
    hoverPos.z += cM_scos(link->shape_angle.y) * kArmIdleForward;
    hoverPos.y += kArmIdleHoverY;

    switch (mState) {
    case STATE_IDLE_e:
        dAlbwMidnaArm_setReachPos(hoverPos, false);
        if (tgt != NULL) {
            enterState(STATE_STRETCH_e);
        }
        break;

    case STATE_STRETCH_e: {
        if (tgt == NULL) {
            enterState(STATE_RETRACT_e);
            break;
        }
        mReachTarget = tgt->eyePos;
        // Ease-out jab: most of the distance in the first frames.
        const f32 lin = 1.0f - (f32)mStateTimer / (f32)kArmStretchFrames;
        const f32 t   = 1.0f - (1.0f - lin) * (1.0f - lin);
        cXyz reach    = hoverPos + (mReachTarget - hoverPos) * t;
        dAlbwMidnaArm_setReachPos(reach, true);
        if (--mStateTimer <= 0) {
            enterState(STATE_HIT_e);
        }
        break;
    }

    case STATE_HIT_e: {
        if (tgt == NULL) {
            enterState(STATE_RETRACT_e);
            break;
        }
        mReachTarget = tgt->eyePos;
        dAlbwMidnaArm_setReachPos(mReachTarget, true);
        // Strike: the sword-typed cylinder sits AT the target for the window.
        mAtCyl.SetC(tgt->current.pos);
        dComIfG_Ccsp()->Set(&mAtCyl);
        if (--mStateTimer <= 0) {
            enterState(STATE_RETRACT_e);
        }
        break;
    }

    case STATE_RETRACT_e: {
        // NO-SNAP RETRACT (fork, user-diagnosed): keep publishing an eased
        // glide from the strike point back to the READY hover, striking=false,
        // so the aim flag never drops mid-cycle and the pose can never snap.
        const f32 lin = 1.0f - (f32)mStateTimer / (f32)kArmRetractFrames;
        const f32 t   = 1.0f - (1.0f - lin) * (1.0f - lin);
        cXyz reach    = mReachTarget + (hoverPos - mReachTarget) * t;
        dAlbwMidnaArm_setReachPos(reach, false);
        if (--mStateTimer <= 0) {
            enterState(STATE_PAUSE_e);
        }
        break;
    }

    case STATE_PAUSE_e:
        dAlbwMidnaArm_setReachPos(hoverPos, false);
        if (--mStateTimer <= 0) {
            enterState(tgt != NULL ? STATE_STRETCH_e : STATE_IDLE_e);
        }
        break;
    }

    return 1;
}

int daAlbwMidnaArm_c::Draw() {
    return 1;  // no model - the visual is Midna's own hair (reach bridge)
}

// ============================================
// Profile boilerplate - fork verbatim (pattern: helper-actor per d_a_boomerang).
// ============================================
namespace {

int daAlbwMidnaArm_Create(void* a_this) {
    return static_cast<daAlbwMidnaArm_c*>(a_this)->create();
}
int daAlbwMidnaArm_Delete(void* a_this) {
    return static_cast<daAlbwMidnaArm_c*>(a_this)->Delete();
}
int daAlbwMidnaArm_Execute(void* a_this) {
    return static_cast<daAlbwMidnaArm_c*>(a_this)->Execute();
}
int daAlbwMidnaArm_Draw(void* a_this) {
    return static_cast<daAlbwMidnaArm_c*>(a_this)->Draw();
}
int daAlbwMidnaArm_IsDelete(void*) {
    return 1;
}

DUSK_CONST actor_method_class daAlbwMidnaArm_MethodTable = {
    (process_method_func)daAlbwMidnaArm_Create,
    (process_method_func)daAlbwMidnaArm_Delete,
    (process_method_func)daAlbwMidnaArm_Execute,
    (process_method_func)daAlbwMidnaArm_IsDelete,
    (process_method_func)daAlbwMidnaArm_Draw,
};

actor_process_profile_definition DUSK_CONST g_profile_ALBW_MIDNA_ARM = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 6,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ kFpcNm_ALBW_MIDNA_ARM,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daAlbwMidnaArm_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_BOOMERANG_e,  // helper-actor slot; fork's own enum entry is fork-only
    /* Actor SubMtd */ &daAlbwMidnaArm_MethodTable,
    /* Status       */ fopAcStts_UNK_0x40000_e,
    /* Group        */ fopAc_UNK_GROUP_5_e,
    /* Cull Type    */ fopAc_CULLBOX_0_e,
};

// ---------------------------------------------------------------------------
// Boundary translation 1: serve the profile from fpcPf_Get. Stock's table
// (f_pc_profile.cpp:19) does not know the fork-appended id; every other id
// falls through to vanilla untouched.
// ---------------------------------------------------------------------------
// DEFINE_HOOK, not DEFINE_HOOK_SYMBOL: fpcPf_Get is header-declared
// (f_pc/f_pc_profile.h:29), so the compiler mangles it per target and the
// signature is type-checked. A bare-name string target depends on the host's
// symbol manifest carrying that name, which is not guaranteed off Windows -
// see the note in albw_symbols.h. Twilit Realm's own randomizer uses the typed
// form for all but one of its hooks for the same reason.
DEFINE_HOOK(&fpcPf_Get, FpcPfGet);
DEFINE_HOOK(&daAlink_c::setNeckAngle, SetNeckAngle);

HookAction on_fpc_pf_get_pre(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr || mods::arg<s16>(args, 0) != kFpcNm_ALBW_MIDNA_ARM) {
        return HOOK_CONTINUE;
    }
    *static_cast<const void**>(retval) = &g_profile_ALBW_MIDNA_ARM;
    return HOOK_SKIP_ORIGINAL;
}

// fork d_a_alink.cpp:3915 (setNeckAngle insert): re-apply the arm actor's
// published reach into FLG1_MIDNA_HAIR_ATN_POS / mMidnaHairAtnPos after the
// per-frame clear, so daMidna_c's hair reaches toward the strike point.
void on_set_neck_angle_post(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }
    cXyz reachPos;
    if (dAlbwMidnaArm_getReachPos(&reachPos)) {
        link->onNoResetFlg1(daPy_py_c::FLG1_MIDNA_HAIR_ATN_POS);
        link->mMidnaHairAtnPos = reachPos;
    }
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

// Spawn/alive surface for wolf_arts.cpp (fork handleWolfArmBurst's tail).
bool albw_midna_arm_is_alive() {
    return daAlbwMidnaArm_c::isAlive();
}

bool albw_midna_arm_spawn(daAlink_c* link) {
    if (link == nullptr || daAlbwMidnaArm_c::isAlive()) {
        return false;
    }
    // fopAcM_create is not exported; resolve it by symbol the same way
    // boss_refinement.cpp spawns its golem.
    if (svc_hook == nullptr) {
        return false;
    }
    void* addr = nullptr;
    if (svc_hook->resolve(mod_ctx, ALBT_SYM_FOPACM_CREATE, &addr, nullptr) != MOD_OK ||
        addr == nullptr)
    {
        return false;
    }
    using CreateFn = fpc_ProcID (*)(s16, u32, const cXyz*, int, const csXyz*, const cXyz*, s8);
    return reinterpret_cast<CreateFn>(addr)(kFpcNm_ALBW_MIDNA_ARM, 0, &link->current.pos,
                                            fopAcM_GetRoomNo(link), NULL, NULL,
                                            -1) != fpcM_ERROR_PROCESS_ID_e;
}

ModResult albw_midna_arm_init(ModError* error) {
    if (!install(error, "FpcPfGetMidnaArm",
                 mods::hook_add_pre<FpcPfGet>(svc_hook, on_fpc_pf_get_pre)) ||
        !install(error, "SetNeckAngleArmReach",
                 mods::hook_add_post<SetNeckAngle>(svc_hook, on_set_neck_angle_post)))
    {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw midna-arm actor + profile ready");
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
