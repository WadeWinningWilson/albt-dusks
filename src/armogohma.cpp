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
#include "d/d_resorce.h"
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
#define ANM_GOMA_DASH           15
#define ANM_GOMA_DEATH          16
#define ANM_GOMA_MOVE           22
#define ANM_GOMA_RETURN         23
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

// The mechanically-extracted helper block: daB_GM_HIO_c ctor + l_HIO + all
// s_gm*/kAlbw* statics + armo_anm_init + the 10 phase-3/reveal/pursuit helpers,
// verbatim from the fork (tools/port_tool.py, brace-verified).
#include "armogohma_helpers.inc"
#include "armogohma_seams.inc"

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

// fork daB_GM_Execute inserts (action() dispatch for the two added ACTION
// states + per-frame phase-3 driving). Runs as a pre-hook; the new actions are
// beyond stock's switch, so vanilla ignores them and we drive them here.
// ============================================
// Egg-spam gate (fork d_a_b_gm.cpp b_gm_move trigger replacement). Vanilla lays
// eggs when getArrowNum() <= 3 so the player can refill by killing babies; in
// ALBW arrows are never the resource (the meter is), so the count sits <= 3
// forever and eggs spawn endlessly. b_gm_move is file-static (unhookable), so
// force the arrow count > 3 across this frame's execute — the vanilla trigger
// then only fires on the legit statue-batch path (field_0x1ad5 == 2) — and
// restore it in the post-hook. Applies regardless of Boss Refinement, matching
// the fork (its refinement path keeps the HP-gated egg system separately).
// ============================================
u8   s_savedArrowNum = 0;
bool s_arrowForced   = false;

HookAction on_bgm_execute_pre(ModContext*, void* args, void*, void*) {
    auto* i_this = mods::arg<b_gm_class*>(args, 0);
    if (i_this == nullptr) {
        return HOOK_CONTINUE;
    }
    s_savedArrowNum = dComIfGs_getArrowNum();
    s_arrowForced = false;
    if (s_savedArrowNum <= 3) {
        dComIfGs_setArrowNum(4);
        s_arrowForced = true;
    }
    if (!dAlbwBossRefinement_isEnabled()) {
        return HOOK_CONTINUE;
    }
    // Reproduce damage_check's ALBW routing (trigger + drain) before vanilla runs.
    albw_armo_damage_route(i_this);
    // Drive the two added actions (vanilla's switch ignores 12/13).
    if (i_this->mAction == ACTION_PHASE3) {
        b_gm_phase3(i_this);
    } else if (i_this->mAction == ACTION_PURSUIT_TEST) {
        b_gm_pursuit_test(i_this);
    }
    return HOOK_CONTINUE;
}

void on_bgm_execute_post(ModContext*, void*, void*, void*) {
    if (s_arrowForced) {
        dComIfGs_setArrowNum(s_savedArrowNum);
        s_arrowForced = false;
    }
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

ModResult albw_armogohma_init(ModError* error) {
    if (!install(error, "BGmCreateReveal",
                 mods::hook_add_post<BGmCreate>(svc_hook, on_bgm_create_post)) ||
        !install(error, "BGmExecutePhase3",
                 mods::hook_add_pre<BGmExecute>(svc_hook, on_bgm_execute_pre)) ||
        !install(error, "BGmExecuteEggGate",
                 mods::hook_add_post<BGmExecute>(svc_hook, on_bgm_execute_post)) ||
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
