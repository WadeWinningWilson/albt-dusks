// ============================================
// NEW CODE — ALBW Port (Shadow Beast e_s1: wolf-art kill / pack-finish path)
//
// Stock e_s1 lacks the fork's wolf-art kill path, so packs revive forever and
// are unclearable with the wolf arts: stock damage_check only fail-routes on
// the >=60-power wolf hit or health<=0, its pack logic (s_last_sub/fail_id)
// never treats the 1-HP survivors of a howl as finishable, and nothing floors
// health driven to 0 outside damage_check.
//
// Fork src/d/actor/d_a_e_s1.cpp carries the fix:
//   * damage_check       — wolf-art kill path: health<=1 + pack-finish attack
//                          kills; every fail route goes through
//                          e_s1_enter_fail_wait + e_s1_try_pack_finish
//                          (fork :473-622)
//   * helpers            — e_s1_is_wolf_pack_finish_attack /
//                          e_s1_enter_fail_wait / s_pack_count_sub /
//                          s_pack_finish_sub / e_s1_try_pack_finish
//                          (fork :386-472)
//   * daE_S1_Execute     — W2c health floor + fail-route for health that hit 0
//                          outside damage_check (fork :2058-2068)
//   * daE_S1_Delete tail — W2a hang-bite latch cleanup (fork :2287-2295)
//
// Seam: damage_check is a FILE-STATIC whose bare name is ambiguous in the host
// symbol manifest (every actor TU has one — resolve() returns Ambiguous), so it
// cannot be hooked directly. The hook target is the UNIQUE daE_S1_Execute
// symbol: the fork chain Execute -> action -> damage_check is ported whole
// (e_s1_port.inc, extracted by tools/port/port_tool.py from tools/port/
// e_s1.json) and run under HOOK_SKIP_ORIGINAL. action() and every pulled mode
// function are byte-identical fork==stock (verified by diff; the only fork
// edits are gen-2 mUseEs1Arc branches, folded dead by the json substitutions —
// the field does not exist on the stock e_s1_class). The W2a Delete cleanup is
// a POST hook on daE_S1_Delete (additive tail in the fork).
//
// Feature gate: dAlbwWolfCombat_isEnabled(). OFF -> HOOK_CONTINUE everywhere:
// the stock originals run untouched, byte-identical vanilla.
//
// File-static wall accounting (fork/stock file-locals the ported chain needs):
//   * l_HIO      — LOCAL COPY (ctor defaults, emitted by the extract). The
//                  host HIO editor tunes the HOST copy only; debug-only drift,
//                  changelink precedent.
//   * l_no_fail  — LOCAL COPY + the stock daE_S1_Create derivation mirrored in
//                  a Create POST hook below (Palace of Twilight D_MN08* rooms
//                  51/9/52), so the ported brain sees the same value the stock
//                  brain computes into its own static.
//   * anm_init   — LOCAL COPY (extract), gen-2 mapping folded to the stock
//                  "E_S2" path.
//   * everything else (mode functions, path/pl checks, search subs, ke_set,
//     demo_camera, anm_se_set, body_eff_set) — LOCAL COPY, fork==stock.
// ============================================

#include "global.h"
#include <os.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "SSystem/SComponent/c_math.h"
#include "SSystem/SComponent/c_phase.h"  // cPhs_COMPLEATE_e (Create-post mirror)
#include "Z2AudioLib/Z2Instances.h"
#include "d/d_com_inf_game.h"
#include "d/d_path.h"
#include "d/d_s_play.h"
#include "d/d_cc_uty.h"
#include "d/d_cc_d.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_e_s1.h"
#include "f_op/f_op_actor_enemy.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_camera_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"

#include "albw_common.h"
#include "wolf_combat.h"
#include "mods/hook.hpp"

#if TARGET_PC

// ============================================
// NEW CODE — [WOLFHIT] confirmation probe (STRIP before release).
// Same macro as wolf_combat.cpp; the e_s1 extract logs pack-finish sweeps and
// hang-bite-refuse fallthroughs through it. Set to 0 to compile probes out.
// ============================================
#define ALBW_WOLFHIT_PROBE 1
#if ALBW_WOLFHIT_PROBE
#define ALBW_WOLFHIT_LOG(...)                                                 \
    do {                                                                      \
        if (svc_log != nullptr) {                                             \
            char wolfhitBuf_[256];                                            \
            std::snprintf(wolfhitBuf_, sizeof(wolfhitBuf_), __VA_ARGS__);     \
            svc_log->info(mod_ctx, wolfhitBuf_);                              \
        }                                                                     \
    } while (0)
#else
#define ALBW_WOLFHIT_LOG(...)                                                 \
    do {                                                                      \
    } while (0)
#endif

// ============================================
// Mirrored fork-local types/defines (d_a_e_s1.cpp — file-local, not in the
// header, so re-declared here; values verbatim from the fork = stock. The HIO
// class must precede the extract: port_tool's class/ctor name collision means
// the ctor is pulled but the class declaration is not — armogohma precedent).
// ============================================
class daE_S1_HIO_c {
public:
    daE_S1_HIO_c();
    virtual ~daE_S1_HIO_c() {}

    /* 0x04 */ s8 field_0x4;
    /* 0x08 */ f32 mBaseSize;
    /* 0x0C */ f32 mMoveSpeed;
    /* 0x10 */ u8 field_0x10[0x14 - 0x10];
    /* 0x14 */ f32 mDashSpeed;
    /* 0x18 */ f32 field_0x18;
    /* 0x1C */ s16 mFallSEWaitTime;
    /* 0x1E */ s16 mAllDeadWaitTime;
    /* 0x20 */ f32 mReactionDist;
    /* 0x24 */ f32 mReactionAngle;
    /* 0x28 */ s16 mReactionTime;
    /* 0x2A */ u8 mInvincible;
};

#define ANM_ATTACK 5
#define ANM_ATTACK_02 6
#define ANM_DAMAGED 7
#define ANM_DASH_01 8
#define ANM_DASH_02 9
#define ANM_DEAD_02 10
#define ANM_DEAD_03 11
#define ANM_DEAD_04 12
#define ANM_DEADWAIT_02 13
#define ANM_DEADWAIT_03 14
#define ANM_DEADWAIT_04 15
#define ANM_DEADWAKE_02 16
#define ANM_DEADWAKE_03 17
#define ANM_DEADWAKE_04 18
#define ANM_DOWN 19
#define ANM_HANGED 20
#define ANM_HANG_DAMAGE 21
#define ANM_HANG_BRUSH 22
#define ANM_HANG_BRUSH2 23
#define ANM_HANG_WAIT 24
#define ANM_SHOUT 25
#define ANM_SHRINK 26
#define ANM_SHRINK_DOWN 27
#define ANM_STICK 28
#define ANM_WAIT_01 29
#define ANM_WAIT_02 30
#define ANM_WALK 31

enum daE_S1_ACTION {
    ACT_WAIT,
    ACT_ROOF,
    ACT_FIGHT_RUN,
    ACT_FIGHT,
    ACT_BIBIRI,
    ACT_DAMAGE,
    ACT_INVINCIBLE,  // does not exist anymore
    ACT_PATH,
    ACT_WOLFBITE,
    ACT_FAIL_WAIT,
    ACT_FAIL,
    ACT_SHOUT,
    ACT_WARP_APPEAR = 20,
};

// The verbatim fork chain (gen-2 branches folded, W1 diag stripped, rupee
// grant re-homed — every deviation is marked in-line; see tools/port/e_s1.json).
#include "e_s1_port.inc"

namespace es1_hooks {

DEFINE_HOOK_SYMBOL("daE_S1_Execute", int(e_s1_class*), Es1Execute);
DEFINE_HOOK_SYMBOL("daE_S1_Create", int(fopAc_ac_c*), Es1Create);
DEFINE_HOOK_SYMBOL("daE_S1_Delete", int(e_s1_class*), Es1Delete);

// Replace the stock daE_S1_Execute wholesale with the fork's verbatim copy
// (e_s1_port.inc). When wolf combat is off, fall through to vanilla so the mod
// is inert for players who disable it. The ported Execute is a superset of
// stock (identical body + the W2c health floor), and its action() calls the
// ported fork damage_check — the wolf-art kill / pack-finish path.
HookAction on_es1_execute_pre(ModContext*, void* args, void* retval, void*) {
    auto* i_this = mods::arg<e_s1_class*>(args, 0);
    if (i_this == nullptr || !dAlbwWolfCombat_isEnabled()) {
        return HOOK_CONTINUE;
    }
    *static_cast<int*>(retval) = albw_es1_Execute(i_this);  // the ported fork copy
    return HOOK_SKIP_ORIGINAL;
}

// Mirror the stock l_no_fail derivation into the extract's local (stock
// daE_S1_Create computes it into ITS file-static, which the ported brain cannot
// read; same inputs, same result — verbatim from stock d_a_e_s1.cpp:2091-2102).
void on_es1_create_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr || *static_cast<int*>(retval) != cPhs_COMPLEATE_e) {
        return;
    }
    fopAc_ac_c* i_this = mods::arg<fopAc_ac_c*>(args, 0);
    if (i_this == nullptr) {
        return;
    }
    l_no_fail = false;

    if ((strcmp(dComIfGp_getStartStageName(), "D_MN08") == 0 ||
         strcmp(dComIfGp_getStartStageName(), "D_MN08B") == 0 ||
         strcmp(dComIfGp_getStartStageName(), "D_MN08C") == 0))
    {
        if ((s8)fopAcM_GetRoomNo(i_this) == 51 || (s8)fopAcM_GetRoomNo(i_this) == 9 ||
            (s8)fopAcM_GetRoomNo(i_this) == 52)
        {
            l_no_fail = true;
        }
    }
}

// Fork daE_S1_Delete tail (W2a): clear the hang-bite keep if Link was latched
// on this beast — a despawn mid-hang otherwise leaves a stale latch on Link.
void on_es1_delete_post(ModContext*, void* args, void*, void*) {
    if (!dAlbwWolfCombat_isEnabled()) {
        return;  // feature off = stock (the latch only arises from wolf combat play)
    }
    auto* a_this = reinterpret_cast<fopAc_ac_c*>(mods::arg<e_s1_class*>(args, 0));
    daPy_py_c* player = daPy_getPlayerActorClass();
    if (a_this != NULL && player != NULL && player->checkWolfEnemyBiteAllOwn(a_this)) {
        static_cast<daAlink_c*>(player)->resetWolfEnemyBiteAll();
        ALBW_WOLFHIT_LOG("[WOLFHIT] e_s1 delete: resetWolfEnemyBiteAll (was latched)");
    }
}

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace es1_hooks

ModResult albw_e_s1_wolf_pack_init(ModError* error) {
    using namespace es1_hooks;
    if (!install(error, "Es1ExecuteReplace",
                 mods::hook_add_pre<Es1Execute>(svc_hook, on_es1_execute_pre)) ||
        !install(error, "Es1CreateNoFailMirror",
                 mods::hook_add_post<Es1Create>(svc_hook, on_es1_create_post)) ||
        !install(error, "Es1DeleteHangBiteCleanup",
                 mods::hook_add_post<Es1Delete>(svc_hook, on_es1_delete_post)))
    {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw e_s1 wolf pack-finish (shadow beast) ready");
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
