// ============================================
// NEW CODE - ALBW Port (Armogohma refined fight — boss actor side, d_a_b_gm.cpp)
//
// The refinement brain (dAlbwBoss_armogohma*) is in boss_refinement.cpp; the
// boss ACTOR side was unported. Ported here via the proven Fyrus/Diababa
// pattern: symbol hooks on daB_GM_Create/Execute/Delete/Draw, the file-static
// helper block extracted MECHANICALLY by tools/port_tool.py (no hand-typing),
// and the modified-function inserts reproduced at the Execute seam.
//
// Reveal model: the phase-3 "revealed" model is the loose B_gm_37.bmd, bundled
// in this mod's res/ and loaded via the host resource service + the engine's
// own dRes_info_c::loaderBasicBmd (the same BMDV finish the game uses). This is
// the boundary shim that replaces the fork's custom_assets::try_load_uncached —
// no custom-asset manager needed; the capability is a host feature.
// ============================================

#include "global.h"
#include <os.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#include "d/d_resorce.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_obj_ystone.h"
#include "f_op/f_op_camera_mng.h"
#include "f_op/f_op_msg_mng.h"
#include "c/c_damagereaction.h"
#include "modules.h"
#include "d/d_cc_uty.h"
#include "SSystem/SComponent/c_cc_d.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "SSystem/SComponent/c_math.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "Z2AudioLib/Z2Instances.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#define private public
#include "d/actor/d_a_b_gm.h"
#include "d/actor/d_a_e_gm.h"
#undef private

#include "albw_common.h"
#include "albw_symbols.h"
#include "boss_refinement.h"
#include "armogohma.h"
#include "mods/hook.hpp"
#include "mods/svc/resource.h"

#if TARGET_PC

// ============================================
// Mirrored fork-local types/defines (d_a_b_gm.cpp — file-local, not in the
// header, so re-declared here; values verbatim).
// ============================================
#define ANM_EYE_TEST            6
#define ANM_GM_BEAM             7
#define ANM_GOMA_ATTACK_01      8
#define ANM_GOMA_ATTACK_A       9
#define ANM_GOMA_ATTACK_B       10
#define ANM_GOMA_ATTACK_C       11
#define ANM_GOMA_DAMAGE_01      12
#define ANM_GOMA_DAMAGE_02      13
#define ANM_GOMA_DAMAGE_WAIT    14
#define ANM_GOMA_DASH           15
#define ANM_GOMA_DEATH          16
#define ANM_GOMA_FALL_LOOP      17
#define ANM_GOMA_LANDING        18
#define ANM_GOMA_LANDING_DAMAGE 19
#define ANM_GOMA_LANDING_WAIT   20
#define ANM_GOMA_LAY_EGGS       21
#define ANM_GOMA_MOVE           22
#define ANM_GOMA_RETURN         23
#define ANM_GOMA_ROOF_DAMAGE    24
#define ANM_GOMA_SLOW_MOVE      25
#define ANM_GOMA_STEP_L         26
#define ANM_GOMA_STEP_R         27
#define ANM_GOMA_UP             28
#define ANM_GOMA_UP_02          29
#define ANM_GOMA_WAIT           30

enum { P3_DASH = 0, P3_VULN = 1, P3_LASER = 2, P3_INTRO = 3, P3_LIEDOWN = 4 };
enum { ARMO_PURSUIT_DASH = 0, ARMO_PURSUIT_CRAWL = 1, ARMO_PURSUIT_EYETEST = 2 };
enum daB_GM_ACTION_albw {
    ACTION_WAIT,
    ACTION_MOVE,
    ACTION_BEAM = 5,
    ACTION_KOGOMA,
    ACTION_DAMAGE = 10,
    ACTION_DROP,
    ACTION_PURSUIT_TEST = 12,
    ACTION_PHASE3 = 13,
};



IMPORT_SERVICE(ResourceService, svc_resource);

namespace {

// daB_GM_HIO_c: plain field class, file-local in the fork. Re-declared verbatim
// (the tool skips it due to a class/constructor name collision — see the note
// in the commit; the constructor + l_HIO instance come from the .inc below).
class daB_GM_HIO_c {
public:
    daB_GM_HIO_c();
    virtual ~daB_GM_HIO_c() {}

    /* 0x04 */ s8 field_0x4;
    /* 0x08 */ f32 model_size;
    /* 0x0C */ f32 check_size;
    /* 0x10 */ f32 dash_speed;
    /* 0x14 */ f32 dash_anm_speed;
    /* 0x18 */ f32 move_speed;
    /* 0x1C */ f32 move_anm_speed;
    /* 0x20 */ f32 wait_anm_speed;
    /* 0x24 */ f32 range;
    /* 0x28 */ s16 smoke_prim_R;
    /* 0x2A */ s16 smoke_prim_G;
    /* 0x2C */ s16 smoke_prim_B;
    /* 0x2E */ s16 smoke_env_R;
    /* 0x30 */ s16 smoke_env_G;
    /* 0x32 */ s16 smoke_env_B;
    /* 0x34 */ s16 smoke_alpha;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ f32 smoke_blend;
    /* 0x3C */ f32 bend_degree_1;
    /* 0x40 */ f32 bend_degree_2;
    /* 0x44 */ f32 bend_degree_3;
    /* 0x48 */ u8 foot_pos_check;
    /* 0x49 */ u8 eye_check;
};

// ============================================
// Reveal-model shim: bundled B_gm_37.bmd -> J3DModelData, per-fight (the boss
// respawns, so the fork loads uncached; we reload/reparse each Create and free
// on Delete). loaderBasicBmd fixes pointers IN PLACE, so the buffer stays alive
// as long as the parsed model does.
// ============================================
ResourceBuffer s_revealBuf = RESOURCE_BUFFER_INIT;
J3DModelData*  s_revealData = nullptr;

J3DModelData* albw_armo_load_reveal() {
    if (s_revealData != nullptr) {
        return s_revealData;
    }
    if (svc_resource == nullptr) {
        return nullptr;
    }
    if (svc_resource->load(mod_ctx, "armogohma/B_gm_37.bmd", &s_revealBuf) != MOD_OK ||
        s_revealBuf.data == nullptr)
    {
        if (svc_log != nullptr) svc_log->error(mod_ctx, "armogohma: reveal B_gm_37.bmd load failed");
        return nullptr;
    }
    s_revealData = dRes_info_c::loaderBasicBmd('BMDV', s_revealBuf.data);
    return s_revealData;
}

void albw_armo_free_reveal() {
    // Model data is derived from the buffer; drop the parsed handle and release
    // the backing buffer so the next fight reparses fresh.
    s_revealData = nullptr;
    if (svc_resource != nullptr && s_revealBuf.data != nullptr) {
        svc_resource->free(mod_ctx, &s_revealBuf);
    }
    s_revealBuf = ResourceBuffer RESOURCE_BUFFER_INIT;
}

// ============================================
// WHOLE-FUNCTION port (tools/port/armogohma.json, whole_funcs). The Execute path
// is reproduced VERBATIM from the fork with D_ALBW_ARMO_REVEAL kept and the diags
// zeroed — daB_GM_Execute + action + damage_check + b_gm_move/beam/kogoma/drop +
// demo_camera + their fork-local deps (statics, l_HIO ctor, phase-3/reveal
// helpers, b_gm_wait/b_gm_damage). We hook the stock daB_GM_Execute with
// HOOK_SKIP_ORIGINAL and run this copy, so the reveal fight's eye vulnerability,
// eyelid/eye animation, contact damage and egg gate are the fork's exact code —
// not a paraphrase. armogohma_manual.inc adds the pieces the tool cannot close
// over (the HIO class is in this TU above; nodeCallBack + reveal anchor + the
// reveal-morf shim follow the port so they see its statics).
// ============================================
// fork file-static arrays the port tool cannot emit (2D/aggregate initialisers),
// verbatim from d_a_b_gm.cpp — referenced by b_gm_move / daB_GM_Execute below.
static cXyz target_pos[] = {
    cXyz(-1350.0f, 0.0f, -1350.0f),
    cXyz(-1350.0f, 0.0f, 1350.0f),
    cXyz(1350.0f, 0.0f, -1350.0f),
    cXyz(1350.0f, 0.0f, 1350.0f),
};
static cXyz top_pos_data[] = {
    cXyz(260.0f, 0.0f, 0.0f),
    cXyz(280.0f, 0.0f, 0.0f),
    cXyz(300.0f, 0.0f, 0.0f),
    cXyz(280.0f, 0.0f, 0.0f),
};
static int top_j[] = {
    0x1B, 0x1F, 0x23, 0x27, 0x2B, 0x2F, 0x33, 0x37, 0x3C, 0x41,
};

// Bridge: the fork's daB_GM_Execute calls dAlbwEnemyRupees_tryGrantFightVictory
// on the boss's death; route it to the mod's enemy-rupees victory grant (deduped).
static void dAlbwEnemyRupees_tryGrantFightVictory(s16 profName) {
    albw_enemy_rupees_grant_fight_victory(profName);
}

// ============================================
// Runtime-resolved bridges for the two actor-create calls in the fork's
// demo_camera (death cutscene): fopAcM_create (9-arg) + fopAcM_createItemForBoss.
// These engine functions are NOT link-importable in the mod — the host resolves
// them by symbol at runtime (the same pattern boss_refinement uses). We macro-
// rename the port's calls onto these bridges across the #include below so they
// don't clash with the non-importable global declarations.
// ============================================
using ArmoCreate9Fn =
    fpc_ProcID (*)(s16, u32, const cXyz*, int, const csXyz*, const cXyz*, s8, u32, u8);
using ArmoCreateItemBossFn =
    fpc_ProcID (*)(const cXyz*, int, int, const csXyz*, const cXyz*, f32, f32, int, const char*);

fpc_ProcID armo_fopAcM_create(s16 procName, u32 params, const cXyz* pos, int roomNo,
                              const csXyz* angle, const cXyz* scale, s8 p6, u32 p7 = 0,
                              u8 p8 = 0xFF) {
    static ArmoCreate9Fn fn = nullptr;
    if (fn == nullptr && svc_hook != nullptr) {
        void* addr = nullptr;
        if (svc_hook->resolve(mod_ctx, ALBT_SYM_FOPACM_CREATE, &addr, nullptr) == MOD_OK)
            fn = reinterpret_cast<ArmoCreate9Fn>(addr);
    }
    return fn != nullptr ? fn(procName, params, pos, roomNo, angle, scale, p6, p7, p8)
                         : fpcM_ERROR_PROCESS_ID_e;
}

fpc_ProcID armo_fopAcM_createItemForBoss(const cXyz* pos, int itemNo, int roomNo,
                                         const csXyz* angle, const cXyz* scale, f32 p5, f32 p6,
                                         int p7, const char* p8 = nullptr) {
    static ArmoCreateItemBossFn fn = nullptr;
    if (fn == nullptr && svc_hook != nullptr) {
        void* addr = nullptr;
        if (svc_hook->resolve(mod_ctx, ALBT_SYM_FOPACM_CREATE_ITEM_FOR_BOSS, &addr, nullptr) == MOD_OK)
            fn = reinterpret_cast<ArmoCreateItemBossFn>(addr);
    }
    return fn != nullptr ? fn(pos, itemNo, roomNo, angle, scale, p5, p6, p7, p8)
                         : fpcM_ERROR_PROCESS_ID_e;
}

// The reveal fight is the whole point of this TU, so its feature gate is ON
// (the port keeps every `#if … D_ALBW_ARMO_REVEAL` block). The remaining diag
// gates are defined 0 inside the generated .inc. The two create calls are
// macro-renamed onto the runtime bridges above (token-exact: fopAcM_createItem*
// stays untouched).
#define D_ALBW_ARMO_REVEAL 1
#define fopAcM_create armo_fopAcM_create
#define fopAcM_createItemForBoss armo_fopAcM_createItemForBoss
#include "armogohma_port.inc"
#undef fopAcM_create
#undef fopAcM_createItemForBoss
#include "armogohma_manual.inc"

DEFINE_HOOK_SYMBOL("daB_GM_Create", int(fopAc_ac_c*), BGmCreate);
DEFINE_HOOK_SYMBOL("daB_GM_Execute", int(b_gm_class*), BGmExecute);
DEFINE_HOOK_SYMBOL("daB_GM_Delete", int(b_gm_class*), BGmDelete);

// fork daB_GM_Create tail: reset phase-3 state + prepare the reveal model.
void on_bgm_create_post(ModContext*, void* args, void*, void*) {
    if (!dAlbwBossRefinement_isEnabled()) return;
    s_gmPhase3Active = false;
    s_gmPhase3EyeOpen = false;
    s_gmPhase3HitCount = 0;
    s_gmRevealActive = false;
    s_gmRevealMorf = nullptr;
    dAlbwBoss_armogohmaResetFightState();
    auto* a_this = mods::arg<b_gm_class*>(args, 0);
    if (a_this != nullptr) {
        albw_armo_build_reveal_morf(a_this);  // parse+wrap the bundled reveal BMD this fight
    }
}

// Replace the stock daB_GM_Execute wholesale with the fork's verbatim copy
// (armogohma_port.inc). When Boss Refinement is off, fall through to vanilla so
// the mod is inert for players who disable it. The fork's Execute is a superset
// of stock (vanilla ceiling/wall fight + the ALBW reveal/phase-3 path), so the
// vanilla fight is preserved and the reveal fight is exact.
HookAction on_bgm_execute_pre(ModContext*, void* args, void* retval, void*) {
    auto* i_this = mods::arg<b_gm_class*>(args, 0);
    if (i_this == nullptr || !dAlbwBossRefinement_isEnabled()) {
        return HOOK_CONTINUE;
    }
    *static_cast<int*>(retval) = daB_GM_Execute(i_this);  // the ported fork copy
    return HOOK_SKIP_ORIGINAL;
}

void on_bgm_delete_post(ModContext*, void* args, void*, void*) {
    albw_armo_free_reveal();
    s_gmRevealMorf = nullptr;
    s_gmRevealActive = false;
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

// ============================================
// Shutdown: return the bundled reveal BMD buffer to the host.
//
// DONOR REFERENCE (d_a_b_gm.cpp:3086-3110): the fork loads the reveal model
// PER-FIGHT with dusk::custom_assets::try_load_uncached() from inside
// useHeapInit, so the model data lives on the boss's solid heap and is freed
// with the actor - the fork never holds a process-lifetime allocation and so
// has nothing to free at exit.
//
// The mod cannot use that loader (the BMD ships inside the .dusk bundle), so the
// bridge loads it through ResourceService into a host-owned buffer held in a
// file static. albw_armo_free_reveal() already runs on the boss's Delete hook -
// the donor-equivalent moment - but quitting the app while the fight is resident
// never reaches Delete, and the host then reports
//   "[dev.albt.albw] reclaimed 1 resource buffer(s) that were never freed".
// Freeing here is the same receiver-boundary translation already applied to the
// Deku Leaf bundle (deku_leaf.cpp:693) and to tear_glow.wgsl (tear_glow.cpp:244).
//
// Safe to run unconditionally: s_revealData is only ever wrapped by
// s_gmRevealMorf, which lives on the boss's solid heap and is never drawn again
// after mod_shutdown (the game loop has already exited - m_Do_main.cpp:358-361).
// ============================================
ModResult albw_armogohma_shutdown(ModError*) {
    albw_armo_free_reveal();
    return MOD_OK;
}

ModResult albw_armogohma_init(ModError* error) {
    if (!install(error, "BGmCreateReveal",
                 mods::hook_add_post<BGmCreate>(svc_hook, on_bgm_create_post)) ||
        !install(error, "BGmExecuteReplace",
                 mods::hook_add_pre<BGmExecute>(svc_hook, on_bgm_execute_pre)) ||
        !install(error, "BGmDeleteReveal",
                 mods::hook_add_post<BGmDelete>(svc_hook, on_bgm_delete_post)))
    {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw armogohma refined fight (actor side) ready");
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
