// ============================================
// NEW CODE - ALBW Port (Dominion Rod confuse — shared-seam implementation)
//
// WHAT THIS IS
// ------------------------------------------------------------------
// The fork's confuse effect (d_albw_lockout.cpp): a Dominion Rod ball that
// grazes a common enemy while the ALBW meter is locked "confuses" that enemy
// for 10s — it turns on the nearest OTHER enemy and attacks it (friendly fire).
//
// The fork implements this by editing EACH enemy actor (d_a_e_oc, d_a_e_dn, …)
// to query dAlbwLockout_getConfuse* at its own targeting sites. As a hook mod
// we cannot edit those actors, so — per the user's request — we make ONE shared
// seam that every enemy already routes through: the base spatial primitives
// fopAcM_searchActor{AngleY,AngleX,Distance,DistanceXZ}. Every stock enemy that
// aims at / ranges to Link does so via fopAcM_searchPlayer* (inline wrappers over
// those four). Hook the four, and when the caller is the confused host asking
// about player[0], redirect the ANSWER to the rival target. That transparently
// bends the confused enemy's entire spatial perception of "the player" onto the
// rival — aim AND distance-gated attack decisions both follow — with no per-enemy
// edits. This is DN-10 option (1): we drive the enemies' own native targeting.
//
// The trigger is a clean member hook on daCrod_c::execute (exported): reproduce
// the fork's flight-path proximity search and hand off to the confuse state.
//
// SCOPE NOTE: this ports the confuse STEERING (the visible "enemy turns on its
// ally"). The fork's friendly-fire DAMAGE crediting + ProvokedEnemy retaliation
// (enemy-vs-enemy damage registration) is a separate layer, not included here.
// ============================================

#include "global.h"
#include <os.h>

#include "d/d_com_inf_game.h"
#include "SSystem/SComponent/c_math.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_actor_iter.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_manager.h"

#define private public
#include "d/actor/d_a_crod.h"
#include "d/actor/d_a_alink.h"
#undef private

#include "albw_common.h"
#include "meter_bridge.h"
#include "confuse.h"
#include "mods/hook.hpp"

#if TARGET_PC

namespace {

// --- fork constants (d_albw_lockout.cpp) -------------------------------------
constexpr int kConfuseFrames        = 300;              // 10 seconds @ 30fps
constexpr f32 kRetargetRange2       = 2500.0f * 2500.0f;
constexpr f32 kConfuseVictimRadius  = 140.0f;           // ball flight-path graze radius

struct DomRodConfuseState {
    fpc_ProcID mHostId;
    fpc_ProcID mTargetId;
    s16        mFrames;
};

DomRodConfuseState sConfuse = {fpcM_ERROR_PROCESS_ID_e, fpcM_ERROR_PROCESS_ID_e, 0};

// Reentrancy guard: the redirect post-hooks recompute their answer by calling
// the very primitive they hook, against the rival target. The guard makes that
// inner call fall through untouched (compute against the target as asked).
bool s_inRedirect = false;

bool meterLocked() {
    return albw_meter_is_locked();
}

// --- eligibility (fork isConfuseDenylistName / isConfuseEligibleActor) --------
// Bosses / traps / leafs / specials that must never be confuse hosts or victims.
bool isConfuseDenylistName(s16 i_name) {
    switch (i_name) {
    // Dungeon / final bosses
    case fpcNm_E_FM_e:      // Fyrus
    case fpcNm_B_BQ_e:      // Diababa head
    case fpcNm_B_BH_e:      // Diababa tentacles
    case fpcNm_B_OB_e:      // Morpheel
    case fpcNm_B_DS_e:      // Stallord
    case fpcNm_B_YO_e:      // Blizzeta
    case fpcNm_B_YOI_e:     // Blizzeta ice
    case fpcNm_B_GM_e:      // Armogohma
    case fpcNm_B_DR_e:      // Argorok
    case fpcNm_B_ZANT_e:
    case fpcNm_B_ZANTM_e:
    case fpcNm_B_ZANTZ_e:
    case fpcNm_B_ZANTS_e:
    case fpcNm_B_GND_e:     // Ganondorf
    case fpcNm_B_MGN_e:     // Beast Ganon
    case fpcNm_E_HZELDA_e:  // Possessed Zelda
    case fpcNm_B_GO_e:      // Goron Golem (trap-adjacent)
    case fpcNm_B_GOS_e:
    case fpcNm_B_OH_e:
    case fpcNm_B_OH2_e:
    case fpcNm_B_DRE_e:
    // Traps / spawners / projectiles / leaf sub-actors
    case fpcNm_E_BI_e:      // Bombling (exploder special-case)
    case fpcNm_E_BI_LEAF_e:
    case fpcNm_E_DB_LEAF_e:
    case fpcNm_E_HB_LEAF_e:
    case fpcNm_E_YD_LEAF_e:
    case fpcNm_E_NEST_e:
    case fpcNm_E_BEE_e:
    case fpcNm_E_GA_e:
    case fpcNm_E_ARROW_e:
    case fpcNm_E_IS_e:      // Armos
    case fpcNm_E_YM_TAG_e:
        return true;
    default:
        return false;
    }
}

bool isConfuseEligibleActor(fopAc_ac_c* i_actor) {
    return i_actor != NULL && fopAcM_GetGroup(i_actor) == fopAc_ENEMY_e &&
           !isConfuseDenylistName(fopAcM_GetName(i_actor));
}

// --- nearest-enemy search (fork findNearestEnemy* via fopAcIt_Judge) ----------
f32 distPointToSegmentXZ2(cXyz const& i_point, cXyz const& i_segA, cXyz const& i_segB) {
    const f32 abx = i_segB.x - i_segA.x;
    const f32 abz = i_segB.z - i_segA.z;
    const f32 apx = i_point.x - i_segA.x;
    const f32 apz = i_point.z - i_segA.z;
    const f32 abLen2 = abx * abx + abz * abz;
    if (abLen2 <= 0.0001f) {
        return apx * apx + apz * apz;
    }

    f32 t = (apx * abx + apz * abz) / abLen2;
    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }

    const f32 cx = i_segA.x + abx * t - i_point.x;
    const f32 cz = i_segA.z + abz * t - i_point.z;
    return cx * cx + cz * cz;
}

struct NearestEnemySearch {
    fopAc_ac_c* mExclude;
    cXyz const* mPos;
    cXyz const* mSegA;
    cXyz const* mSegB;
    f32         mBestDist2;
    fopAc_ac_c* mBest;
    bool        mEligibleOnly;
    bool        mUseSegment;
};

void* judgeNearestEnemy(void* i_actor, void* i_data) {
    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(i_actor);
    NearestEnemySearch* search = static_cast<NearestEnemySearch*>(i_data);
    if (actor == NULL || actor == search->mExclude ||
        fopAcM_GetGroup(actor) != fopAc_ENEMY_e) {
        return NULL;
    }
    if (search->mEligibleOnly && !isConfuseEligibleActor(actor)) {
        return NULL;
    }

    const f32 dist2 =
        search->mUseSegment
            ? distPointToSegmentXZ2(actor->current.pos, *search->mSegA, *search->mSegB)
            : search->mPos->abs2(actor->current.pos);
    if (dist2 < search->mBestDist2) {
        search->mBestDist2 = dist2;
        search->mBest      = actor;
    }
    return NULL;
}

fopAc_ac_c* findNearestEnemy(cXyz const& i_pos, fopAc_ac_c* i_exclude, f32 i_maxDist2,
                             bool i_eligibleOnly) {
    NearestEnemySearch search;
    search.mExclude      = i_exclude;
    search.mPos          = &i_pos;
    search.mSegA         = NULL;
    search.mSegB         = NULL;
    search.mBestDist2    = i_maxDist2;
    search.mBest         = NULL;
    search.mEligibleOnly = i_eligibleOnly;
    search.mUseSegment   = false;
    fopAcIt_Judge(judgeNearestEnemy, &search);
    return search.mBest;
}

fopAc_ac_c* findNearestEnemyAlongSegment(cXyz const& i_segA, cXyz const& i_segB,
                                         fopAc_ac_c* i_exclude, f32 i_maxDist2,
                                         bool i_eligibleOnly) {
    NearestEnemySearch search;
    search.mExclude      = i_exclude;
    search.mPos          = NULL;
    search.mSegA         = &i_segA;
    search.mSegB         = &i_segB;
    search.mBestDist2    = i_maxDist2;
    search.mBest         = NULL;
    search.mEligibleOnly = i_eligibleOnly;
    search.mUseSegment   = true;
    fopAcIt_Judge(judgeNearestEnemy, &search);
    return search.mBest;
}

// --- state (fork clearConfuse / refreshConfuseTarget / onDomRodConfuseHit) ----
void clearConfuse() {
    sConfuse.mHostId   = fpcM_ERROR_PROCESS_ID_e;
    sConfuse.mTargetId = fpcM_ERROR_PROCESS_ID_e;
    sConfuse.mFrames   = 0;
}

void refreshConfuseTarget() {
    if (sConfuse.mFrames <= 0) {
        return;
    }

    fopAc_ac_c* host = fopAcM_SearchByID(sConfuse.mHostId);
    if (host == NULL || fopAcM_GetGroup(host) != fopAc_ENEMY_e) {
        clearConfuse();
        return;
    }

    fopAc_ac_c* currentTarget = fopAcM_SearchByID(sConfuse.mTargetId);
    if (currentTarget != NULL && currentTarget != host &&
        isConfuseEligibleActor(currentTarget) &&
        host->current.pos.abs2(currentTarget->current.pos) <= kRetargetRange2)
    {
        return;
    }

    fopAc_ac_c* target =
        findNearestEnemy(host->current.pos, host, kRetargetRange2, true);
    sConfuse.mTargetId = target != NULL ? target->id : fpcM_ERROR_PROCESS_ID_e;
}

void onDomRodConfuseHit(fopAc_ac_c* i_enemy) {
    if (!meterLocked() || !isConfuseEligibleActor(i_enemy)) {
        return;
    }
    sConfuse.mHostId = i_enemy->id;
    sConfuse.mFrames = static_cast<s16>(kConfuseFrames);
    refreshConfuseTarget();
    if (svc_log != nullptr) svc_log->info(mod_ctx, "[confuse] host set");
}

bool isConfused(const fopAc_ac_c* i_enemy) {
    return i_enemy != NULL && sConfuse.mFrames > 0 && meterLocked() &&
           i_enemy->id == sConfuse.mHostId;
}

fopAc_ac_c* getConfuseTarget(const fopAc_ac_c* i_attacker) {
    if (!isConfused(i_attacker)) {
        return NULL;
    }
    fopAc_ac_c* target = fopAcM_SearchByID(sConfuse.mTargetId);
    if (!isConfuseEligibleActor(target)) {
        return NULL;
    }
    return target;
}

// A search-primitive call is a "confuse redirect candidate" when the querying
// actor (A) is the confused host and it is asking about the player (B). Returns
// the rival target to answer against instead, or NULL to leave the call alone.
fopAc_ac_c* redirectTargetFor(const fopAc_ac_c* i_a, const fopAc_ac_c* i_b) {
    // Fast-out: these primitives are extremely hot. When no confuse is live the
    // whole check collapses to one global read (mFrames), before any call.
    if (sConfuse.mFrames <= 0) {
        return NULL;
    }
    if (s_inRedirect || i_a == NULL || i_b == NULL) {
        return NULL;
    }
    if (i_b != dComIfGp_getPlayer(0)) {
        return NULL;  // only the "aim at / range to Link" queries are bent
    }
    if (!isConfused(i_a)) {
        return NULL;
    }
    return getConfuseTarget(i_a);
}

// ============================================
// Trigger — daCrod_c::execute post-hook (fork d_a_crod.cpp flight-path search)
// While the ball is thrown/flying (param 3) and the meter is locked, graze the
// flight segment (old.pos -> current.pos) for an eligible enemy; confuse it and
// send the ball home, exactly like the fork's in-execute proximity search.
// ============================================
DEFINE_HOOK(&daCrod_c::execute, CrodExecute);

void on_crod_execute_post(ModContext*, void* args, void*, void*) {
    if (!meterLocked()) {
        return;
    }
    auto* ball = mods::arg<daCrod_c*>(args, 0);
    if (ball == NULL || fopAcM_GetParam(ball) != 3) {
        return;
    }
    fopAc_ac_c* victim = findNearestEnemyAlongSegment(
        ball->old.pos, ball->current.pos, NULL,
        kConfuseVictimRadius * kConfuseVictimRadius, true);
    if (victim != NULL) {
        onDomRodConfuseHit(victim);
        ball->setReturn();
    }
}

// ============================================
// Per-frame tick — Link execute post-hook (fork's lockout per-frame update).
// The confuse window (10s) outlives the ball, so it needs a persistent tick:
// count down, drop when the meter unlocks or the timer ends, else re-home the
// host onto the nearest surviving rival.
// ============================================
DEFINE_HOOK(&daAlink_c::execute, ConfuseTick);

void on_confuse_tick_post(ModContext*, void*, void*, void*) {
    if (sConfuse.mFrames <= 0) {
        return;
    }
    sConfuse.mFrames--;
    if (sConfuse.mFrames <= 0 || !meterLocked()) {
        clearConfuse();
    } else {
        refreshConfuseTarget();
    }
}

// ============================================
// Shared seam — the four base spatial primitives every enemy aims/ranges by.
// Post-hooks: when the caller is the confused host querying player[0], recompute
// the answer against the rival target (guarded so the recompute doesn't recurse).
// ============================================
DEFINE_HOOK(&fopAcM_searchActorAngleY, SearchAngleY);
DEFINE_HOOK(&fopAcM_searchActorAngleX, SearchAngleX);
DEFINE_HOOK(&fopAcM_searchActorDistance, SearchDistance);
DEFINE_HOOK(&fopAcM_searchActorDistanceXZ, SearchDistanceXZ);

void on_search_angle_y_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr) return;
    fopAc_ac_c* target = redirectTargetFor(mods::arg<fopAc_ac_c*>(args, 0),
                                           mods::arg<fopAc_ac_c*>(args, 1));
    if (target == NULL) return;
    s_inRedirect = true;
    *static_cast<s16*>(retval) = fopAcM_searchActorAngleY(mods::arg<fopAc_ac_c*>(args, 0), target);
    s_inRedirect = false;
}

void on_search_angle_x_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr) return;
    fopAc_ac_c* target = redirectTargetFor(mods::arg<fopAc_ac_c*>(args, 0),
                                           mods::arg<fopAc_ac_c*>(args, 1));
    if (target == NULL) return;
    s_inRedirect = true;
    *static_cast<s16*>(retval) = fopAcM_searchActorAngleX(mods::arg<fopAc_ac_c*>(args, 0), target);
    s_inRedirect = false;
}

void on_search_distance_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr) return;
    fopAc_ac_c* target = redirectTargetFor(mods::arg<fopAc_ac_c*>(args, 0),
                                           mods::arg<fopAc_ac_c*>(args, 1));
    if (target == NULL) return;
    s_inRedirect = true;
    *static_cast<f32*>(retval) = fopAcM_searchActorDistance(mods::arg<fopAc_ac_c*>(args, 0), target);
    s_inRedirect = false;
}

void on_search_distance_xz_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr) return;
    fopAc_ac_c* target = redirectTargetFor(mods::arg<fopAc_ac_c*>(args, 0),
                                           mods::arg<fopAc_ac_c*>(args, 1));
    if (target == NULL) return;
    s_inRedirect = true;
    *static_cast<f32*>(retval) = fopAcM_searchActorDistanceXZ(mods::arg<fopAc_ac_c*>(args, 0), target);
    s_inRedirect = false;
}

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace

ModResult albw_confuse_init(ModError* error) {
    if (!install(error, "CrodExecuteConfuse",
                 mods::hook_add_post<CrodExecute>(svc_hook, on_crod_execute_post)) ||
        !install(error, "ConfuseTick",
                 mods::hook_add_post<ConfuseTick>(svc_hook, on_confuse_tick_post)) ||
        !install(error, "ConfuseSearchAngleY",
                 mods::hook_add_post<SearchAngleY>(svc_hook, on_search_angle_y_post)) ||
        !install(error, "ConfuseSearchAngleX",
                 mods::hook_add_post<SearchAngleX>(svc_hook, on_search_angle_x_post)) ||
        !install(error, "ConfuseSearchDistance",
                 mods::hook_add_post<SearchDistance>(svc_hook, on_search_distance_post)) ||
        !install(error, "ConfuseSearchDistanceXZ",
                 mods::hook_add_post<SearchDistanceXZ>(svc_hook, on_search_distance_xz_post)))
    {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw dom-rod confuse (shared-seam) ready");
    return MOD_OK;
}

// Public query for other TUs (e.g. friendly-fire credit at the cc_at seam).
bool albw_confuse_is_confused(const fopAc_ac_c* i_enemy) {
    return isConfused(i_enemy);
}
fopAc_ac_c* albw_confuse_get_target(const fopAc_ac_c* i_attacker) {
    return getConfuseTarget(i_attacker);
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
