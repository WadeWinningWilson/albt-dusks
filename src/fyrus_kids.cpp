// ============================================
// NEW CODE - ALBW Port (Fyrus phase-3 Goron kids: B_GOS as real combatants)
//
// The refinement BRAIN (boss_refinement.cpp) and the B_GO clump side
// (fyrus_golem.cpp, "Pass 2") were ported; the B_GOS side never was. In the
// mod the shed kids are STOCK sparring Gorons: they wander (wait/walk), they
// cannot be locked on, they have no aggro, no punch, no punch collider, no
// shield-bash reaction, no stagger, no grA animations and 1000 HP that nothing
// can reach - so `dAlbwBoss_fyrusOnGolemKidsCleared()` is never reached from
// the kid side and phase 3 can only end through fyrus_golem.cpp's
// "all children gone" sweep, which nothing ever makes true.
//
// Fork source: src/d/actor/d_a_b_gos.cpp. The fork adds (fork line numbers):
//   :25-31    5 Action enum values  FIGHT/PUNCH/HEAD_BACK/TOTTER/STONE_DIE
//   :202-213  OBJ_GRA Regular-Soldier tuning constants (grA BCK ids, ranges)
//   :215-276  b_gos_albw* aggro / react helpers
//   :278-310  damage_check   - REPLACES stock's EMPTY body
//   :312-350  b_gos_albwEnsurePunchCc     (the two hand At spheres)
//   :352-360  b_gos_albwPlayPunchAnm
//   :362-502  fight / head_back / totter / punch / b_gos_albwPunchHitReact
//   :504-549  b_gos_albwTryNotifyLastKidCleared / stone_die
//   :551-614  b_gos_albwSetAttnPos / RegisterPunchAt / SetDamageCyl
//   :617-765  action()       - MODIFIED (XZ distance, 5 new switch arms, Tg
//                              bit management, kid re-aggro timer, two
//                              `boss != NULL` guards)
//   :767-813  daB_GOS_Execute - MODIFIED (attn pos, punch At registration,
//                              GrA-sized damage cylinder, punch hit react)
//   :157-159  stick()        - MODIFIED (`boss == NULL` early return)
//   :880-885  daB_GOS_Create - MODIFIED (kid HP from the refinement brain)
//
// ---- SEAM ----------------------------------------------------------------
// Everything above except the Create hunk lives inside action(),
// damage_check() and the file-static mode functions. `action` (95 actor TUs)
// and `damage_check` (53) are FILE-STATICS whose bare names are AMBIGUOUS in
// the host symbol manifest, exactly like the e_s1 / armogohma cases - they
// cannot be hooked by name. The unique symbol on this actor is
// daB_GOS_Execute, and it is the sole caller of both, so the fork chain
//     daB_GOS_Execute -> action -> {wait,walk,ball,stick,fight,punch,
//                                   head_back,totter,stone_die}
//                     -> damage_check
// is ported WHOLE (bgos_port.inc, mechanically extracted by
// tools/port/port_tool.py from tools/port/bgos.json - not one character
// retyped) and run under HOOK_SKIP_ORIGINAL. The stock damage_check is an
// EMPTY body, so the fork's version displaces nothing vanilla.
//
// daB_GOS_Create is a UNIQUE symbol, so the kid-HP hunk is a plain POST hook.
// daB_GOS_Draw / daB_GOS_Delete / daB_GOS_IsDelete are untouched by the fork.
//
// ---- FILE-STATIC WALL ACCOUNTING -----------------------------------------
//   * boss        - LOCAL COPY. Recomputed at the top of every Execute from
//                   parentActorID, so the local static holds the same value
//                   the host's would; only the ported chain reads it.
//   * l_HIO       - LOCAL COPY (ctor pulled verbatim: field_0x4 -1, mSize 1.0,
//                   mNormalSpeed 10.0). Nothing in d_a_b_gos.cpp ever writes
//                   l_HIO after construction except `l_HIO.field_0x4 = -1` in
//                   Create, which the ctor already did, and this actor
//                   registers no HIO editor node - so host copy == local copy.
//                   changelink / e_s1 precedent.
//   * j_info[]    - LOCAL COPY, declared below (fork == stock, byte-identical).
//                   port_tool cannot key an array declarator, so it is the one
//                   dependency supplied by hand; it is pure data.
//   * enum Action / enum B_GOS_RES_FILE_ID - LOCAL COPY, declared below,
//                   verbatim from the fork (e_s1_hooks.cpp precedent).
//   * data_8060560C - NOT needed: read only by Create/Delete, neither ported.
//   * anm_init, wait, walk, ball, stick, the b_gos_albw* helpers and the five
//                   new mode functions - LOCAL COPY (extract), verbatim.
//
// ---- FEATURE GATE --------------------------------------------------------
// dAlbwBossRefinement_isEnabled(). OFF -> HOOK_CONTINUE on both hooks: the
// stock originals run untouched and NOTHING extra is registered into the Ccsp
// list (the punch spheres field_0x8f8 / field_0xa30 are stock fields that
// stock B_GOS never Sets, and the ported chain only Sets them from
// b_gos_albwRegisterPunchAt, which is unreachable unless mAction ==
// ACTION_PUNCH). Provably byte-identical vanilla with the toggle off.
//
// ---- DEPENDENCY ----------------------------------------------------------
// The grA animations (punch / head-back L+R / totter) are resolved with
// dComIfG_getObjectRes("grA", ...). That arc is phase-loaded by
// fyrus_golem.cpp's daB_GO_Create POST hook while the golem window is live and
// released in its daB_GO_Delete POST hook - i.e. it is resident for exactly
// the lifetime of the kids. b_gos_albwPlayGraAnm / b_gos_albwPlayPunchAnm are
// the fork's own null-safe fallbacks if it is not.
// ============================================

#include "global.h"
#include <os.h>
#include <cmath>
#include <cstdlib>  // abs(int) used by the fork's fight() punch-angle test

#include "SSystem/SComponent/c_math.h"
#include "SSystem/SComponent/c_phase.h"  // cPhs_COMPLEATE_e (Create-post gate)
#include "Z2AudioLib/Z2SeMgr.h"
#include "d/d_com_inf_game.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#include "d/actor/d_a_b_go.h"
#include "d/actor/d_a_b_gos.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"

#include "albw_common.h"
#include "boss_refinement.h"
#include "fyrus.h"
#include "mods/hook.hpp"

#if TARGET_PC

// ============================================
// Mirrored fork-local declarations (d_a_b_gos.cpp - file-local, not in the
// header). Values verbatim from the fork; the two enums carry the fork's five
// added Action values, the j_info table is byte-identical fork == stock.
// ============================================

// fork d_a_b_gos.cpp:20-32
enum Action {
    /* 0x00 */ ACTION_WAIT,
    /* 0x01 */ ACTION_WALK,
    /* 0x02 */ ACTION_BALL,
    /* 0x03 */ ACTION_STICK,
#if TARGET_PC
    /* 0x04 */ ACTION_FIGHT,
    /* 0x05 */ ACTION_PUNCH,
    /* 0x06 */ ACTION_HEAD_BACK,
    /* 0x07 */ ACTION_TOTTER,
    /* 0x08 */ ACTION_STONE_DIE,
#endif
};

// fork d_a_b_gos.cpp:34-42
enum B_GOS_RES_FILE_ID {
    /* BCK */
    /* 0x04 */ BCK_GRA_WAIT_AGRA_RUN_A = 4,
    /* 0x05 */ BCK_GRA_WAIT_AGRA_TO_STONE_NORMAL,
    /* 0x06 */ BCK_GRA_WAIT_AGRA_WAIT_A,

    /* BMDR */
    /* 0x09 */ BMDR_GRA_A = 9,
};

// fork d_a_b_gos.cpp:44-52 (fork == stock; stick() indexes it by mJointIndex)
static b_gos_j_info j_info[] = {
    {0x000E, 1.0f},   {0x000D, 1.0f},   {0x0F0D, 0.333f}, {0x0F0D, 0.666f}, {0x000F, 1.0f},
    {0x0003, 1.0f},   {0x0304, 0.5f},   {0x0004, 1.0f},   {0x0405, 0.5f},   {0x0005, 1.0f},
    {0x0008, 1.0f},   {0x0809, 0.5f},   {0x0009, 1.0f},   {0x090A, 0.5f},   {0x000A, 1.0f},
    {0x0010, 1.0f},   {0x1011, 0.5f},   {0x0011, 1.0f},   {0x1112, 0.5f},   {0x0012, 1.0f},
    {0x0013, 1.0f},   {0x1314, 0.5f},   {0x0014, 1.0f},   {0x1415, 0.5f},   {0x0015, 1.0f},
    {0x0310, 0.333f}, {0x0310, 0.666f}, {0x0813, 0.333f}, {0x0813, 0.666f}, {0x0D03, 0.5f},
    {0x0D08, 0.5f},
};

// The verbatim fork chain (every body unmodified; see the port_tool report in
// tools/port/bgos.json for the single declarative substitution).
#include "bgos_port.inc"

namespace bgos_hooks {

DEFINE_HOOK_SYMBOL("daB_GOS_Execute", int(b_gos_class*), BGosExecute);
DEFINE_HOOK_SYMBOL("daB_GOS_Create", int(fopAc_ac_c*), BGosCreate);

// ============================================
// Replace the stock daB_GOS_Execute wholesale with the fork's verbatim copy
// (bgos_port.inc). With boss refinement off, fall through to vanilla so the
// actor is byte-identical stock for players who disable it.
//
// The ported Execute is a superset of stock: with every dAlbwBoss_fyrus*
// predicate false it walks exactly stock's path (see MANIFEST.md "toggle-on,
// no golem" for the three fork-verbatim deltas that remain in that case - the
// two `boss != NULL` guards the fork added to action(), and damage_check's
// unconditional mCcStts.Move() / mCyl.ClrTgHit(), all of which are the fork's
// own behavior and none of which stock relies on).
// ============================================
HookAction on_bgos_execute_pre(ModContext*, void* args, void* retval, void*) {
    auto* i_this = mods::arg<b_gos_class*>(args, 0);
    if (i_this == nullptr || retval == nullptr || !dAlbwBossRefinement_isEnabled()) {
        return HOOK_CONTINUE;
    }
    *static_cast<int*>(retval) = albw_bgos_Execute(i_this);  // the ported fork copy
    return HOOK_SKIP_ORIGINAL;
}

// ============================================
// fork daB_GOS_Create insert (fork d_a_b_gos.cpp:880-885), verbatim:
//
//     a_this->health = 1000;
//     a_this->field_0x560 = 1000;
// #if TARGET_PC
//     if (dAlbwBoss_fyrusGolemWindowIsLive()) {
//         a_this->health = dAlbwBoss_fyrusKidCreateHp();
//         a_this->field_0x560 = a_this->health;
//     }
// #endif
//
// This is the consumer dAlbwBoss_fyrusKidCreateHp() (boss_refinement.cpp:955,
// kAlbwFmKidHp = 30 at :754) never had. The condition is the fork's own and
// already subsumes the feature gate: dAlbwBoss_fyrusGolemWindowIsLive()
// (boss_refinement.cpp:873) is `dAlbwBossRefinement_isEnabled() &&
// s_fyrusGolemPhase == 1`, so with the toggle off no kid HP is ever rewritten.
//
// Seam note: the fork writes health inside Create's `phase_state ==
// cPhs_COMPLEATE_e` block, before Create's tail (mAcch.Set, mSound.init, the
// collider init and the one inline daB_GOS_Execute call). This POST hook runs
// after that tail. Nothing in it reads health: the inline Execute reaches
// damage_check, which early-returns on `!dAlbwBoss_fyrusGolemKidsLoose()` -
// false here, because the kids are CREATED during the clump window (phase 1)
// and only shed at phase 2. Same observable result, one call later.
// ============================================
void on_bgos_create_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr || *static_cast<int*>(retval) != cPhs_COMPLEATE_e) {
        return;
    }
    auto* a_this = mods::arg<fopAc_ac_c*>(args, 0);
    if (a_this == nullptr) {
        return;
    }
    if (dAlbwBoss_fyrusGolemWindowIsLive()) {
        a_this->health = dAlbwBoss_fyrusKidCreateHp();
        a_this->field_0x560 = a_this->health;
    }
}

// A boss hook that fails to resolve must not abort mod_initialize - the same
// doctrine fyrus.cpp states for the E_FM hooks. Losing the Goron-kid combat
// layer costs one refinement feature; returning MOD_ERROR here would unload the
// WHOLE mod, which is far worse than a stock-behaving kid.
void try_install(const char* what, ModResult result) {
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, what);
        svc_log->error(mod_ctx, "fyrus kids: hook above did NOT install - the "
                                "B_GOS combat layer is inactive this run");
    }
}

}  // namespace bgos_hooks

ModResult albw_fyrus_kids_init(ModError*) {
    using namespace bgos_hooks;
    try_install("BGosExecuteReplace",
                mods::hook_add_pre<BGosExecute>(svc_hook, on_bgos_execute_pre));
    try_install("BGosCreateKidHp",
                mods::hook_add_post<BGosCreate>(svc_hook, on_bgos_create_post));
    svc_log->info(mod_ctx, "albw fyrus goron kids (B_GOS combat) ready");
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
