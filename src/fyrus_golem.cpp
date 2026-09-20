// ============================================
// NEW CODE - ALBW Port (Fyrus SS8/SS9 "Pass 2" - the B_GO actor side)
//
// The refinement BRAIN (boss_refinement.cpp) was ported long ago; the B_GO
// actor-side wiring it drives was stubbed ("no-op on stock host until Pass 2
// wires kids") and never built. Every reported Fyrus symptom is that stub:
//   - "inflated health": b_go_albwProxyDamage was absent, so hull hits landed
//     on B_GO's own (huge) health pool instead of being proxied into E_FM via
//     cc_at_check - the fight LOOKED like inflated HP because none of the
//     damage counted.
//   - "goes to player but doesn't move further": the golem-window branches in
//     execute/action (field_0x692 pinning, short idle timers) were absent, so
//     the vanilla sparring-Goron brain ran instead of the fork's boss loop.
//   - "phase 2 attacks aren't parry-able": e_fm_albwApplyParryableAt (AtSpl 1 +
//     SPrm bit 12 - ChkAtNoGuard rejects spl >= 12) was never applied to
//     E_FM's at/chain/effect spheres, nor to the golem slam volumes.
//
// Helper bodies below are VERBATIM from fork d_a_b_go.cpp (b_go_albw*). The
// fork's in-function inserts are re-expressed at hook seams, each noted at its
// site; hunks whose host is a file-static with a collision-prone symbol name
// (h_wait / h_attack / action) are reproduced from the daB_GO_Execute hooks
// with the same state effect, since those statics cannot be hooked by name
// safely.
//
// NOT carried: daB_GO_setDisplayModelImage writes l_HIO.mDisplayModelImage, a
// file-static HIO struct - unreachable from a mod (see the file-static port
// wall). The stub in boss_refinement.cpp stays a no-op; if the amalgam mesh
// ever fails to draw, the fix is a daB_GO_Draw hook, not a write to l_HIO.
// ============================================

#include "global.h"
#include <os.h>

#include "SSystem/SComponent/c_phase.h"
#include "SSystem/SComponent/c_math.h"
#include "d/d_com_inf_game.h"
#include "d/d_cc_uty.h"
#include "d/actor/d_a_b_go.h"
#include "d/actor/d_a_e_fm.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_mtx.h"

#include "albw_common.h"
#include "boss_refinement.h"
#include "fyrus.h"
#include "mods/hook.hpp"

#if TARGET_PC

namespace {

DEFINE_HOOK_SYMBOL("daB_GO_Create", int(fopAc_ac_c*), BGoCreate);
DEFINE_HOOK_SYMBOL("daB_GO_Execute", int(b_go_class*), BGoExecute);
DEFINE_HOOK_SYMBOL("daB_GO_Delete", int(b_go_class*), BGoDelete);

// fork d_a_b_go.cpp file statics
request_of_phase_process_class s_fyrusGraPhase;
u8 s_fyrusGraLoaded;

// ---------------------------------------------------------------------------
// fork b_go_albwApplyParryableSlamAt - verbatim.
// Joint-3 sph also carries Fyrus at_sph At (CSTATUE_SWING / Atp 2 / spl 1 when
// live); OnAt only during native unk_0x660 slam frames. AtSpl 1 + bit 12 for
// Link parry (ChkAtNoGuard rejects spl >= 12 - same fix as E_FM
// e_fm_albwApplyParryableAt).
// ---------------------------------------------------------------------------
void b_go_albwApplyParryableSlamAt(dCcD_GObjInf* i_at) {
    i_at->SetAtSpl((dCcG_At_Spl)1);
    i_at->OnAtSPrmBit(12);
    i_at->OnAtVsPlayerBit();
}

// fork b_go_albwEnsureBodyCc - verbatim.
void b_go_albwEnsureBodyCc(b_go_class* i_this) {
    if (i_this->mStts.GetWeightUc() == 0xFA) {
        return;
    }

    static dCcD_SrcSph body_sph_src = {
        {
            {0x0, {{AT_TYPE_CSTATUE_SWING, 0x2, 0x1d}, {0xd8fbfdff, 0x3}, 0x79}},
            {dCcD_SE_NONE, 0x0, 0xe, 0x0, 0x0},
            {dCcD_SE_NONE, 0x5, 0x0, 0x0, 0x2},
            {0x0},
        },
        {
            {{0.0f, 0.0f, 0.0f}, 40.0f},
        },
    };
    static dCcD_SrcCyl body_cyl_src = {
        {
            {0x0, {{0x0, 0x0, 0x0}, {0xd8fbfdff, 0x3}, 0x79}},
            {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x0},
            {dCcD_SE_NONE, 0x5, 0x0, 0x0, 0x2},
            {0x0},
        },
        {
            {
                {0.0f, 0.0f, 0.0f},
                150.0f,
                400.0f,
            },
        },
    };

    i_this->mStts.Init(0xFA, 0, i_this);
    i_this->field_0x8e8.Set(body_sph_src);
    i_this->field_0x8e8.SetStts(&i_this->mStts);
    i_this->field_0x8e8.OffAtSetBit();
    i_this->field_0xa20.Set(body_sph_src);
    i_this->field_0xa20.SetStts(&i_this->mStts);
    i_this->field_0xa20.OffAtSetBit();
    i_this->field_0xb58.Set(body_cyl_src);
    i_this->field_0xb58.SetStts(&i_this->mStts);
}

// fork b_go_albwRegisterFyrusHull - verbatim.
void b_go_albwRegisterFyrusHull(b_go_class* i_this) {
    J3DModel* model = i_this->mpMorf->getModel();
    if (model == NULL) {
        return;
    }

    cXyz pos;
    cXyz offset(0.0f, 0.0f, 0.0f);

    MTXCopy(model->getAnmMtx(3), *calc_mtx);
    MtxPosition(&offset, &pos);
    i_this->field_0x8e8.SetC(pos);
    i_this->field_0x8e8.SetR(i_this->unk_0x660 != 0 ? 300.0f : 170.0f);
    dComIfG_Ccsp()->Set(&i_this->field_0x8e8);
    i_this->eyePos = pos;
    i_this->attention_info.position = pos;
    i_this->attention_info.position.y += 30.0f;

    MTXCopy(model->getAnmMtx(0xE), *calc_mtx);
    MtxPosition(&offset, &pos);
    i_this->field_0xa20.SetC(pos);
    i_this->field_0xa20.SetR(i_this->unk_0x660 != 0 ? 220.0f : 120.0f);
    dComIfG_Ccsp()->Set(&i_this->field_0xa20);

    i_this->field_0xb58.SetC(i_this->current.pos);
    i_this->field_0xb58.SetR(i_this->unk_0x660 != 0 ? 220.0f : 150.0f);
    i_this->field_0xb58.SetH(400.0f);
    dComIfG_Ccsp()->Set(&i_this->field_0xb58);
}

// fork b_go_albwApplySlamAt - verbatim.
void b_go_albwApplySlamAt(b_go_class* i_this) {
    if (i_this->unk_0x660 != 0) {
        // Same volumes are Tg+Co for hull chips. Co shoves Link off the At
        // sphere; Fyrus's own at_sph has Co off. Drop Co and At the column too.
        i_this->field_0x8e8.OnAtSetBit();
        i_this->field_0x8e8.OffCoSetBit();
        b_go_albwApplyParryableSlamAt(&i_this->field_0x8e8);
        i_this->field_0xa20.OnAtSetBit();
        i_this->field_0xa20.OffCoSetBit();
        b_go_albwApplyParryableSlamAt(&i_this->field_0xa20);
        i_this->field_0xb58.SetAtType(AT_TYPE_CSTATUE_SWING);
        i_this->field_0xb58.SetAtAtp(2);
        i_this->field_0xb58.OnAtSetBit();
        i_this->field_0xb58.OffCoSetBit();
        b_go_albwApplyParryableSlamAt(&i_this->field_0xb58);
    } else {
        i_this->field_0x8e8.OffAtSetBit();
        i_this->field_0x8e8.OnCoSetBit();
        i_this->field_0xa20.OffAtSetBit();
        i_this->field_0xa20.OnCoSetBit();
        i_this->field_0xb58.OffAtSetBit();
        i_this->field_0xb58.OnCoSetBit();
    }
}

// fork b_go_albwProxyDamage - verbatim.
void b_go_albwProxyDamage(b_go_class* i_this) {
    fopAc_ac_c* fmActor = fopAcM_SearchByName(fpcNm_E_FM_e);
    if (fmActor == NULL || !fopAcM_IsActor(fmActor)) {
        return;
    }
    e_fm_class* fm = (e_fm_class*)fmActor;
    // Native leftover unk_0x690 - Golem i-frames. Do not gate on E_FM's
    // core timer (opening arrow would eat hull swings).
    if (i_this->unk_0x690 != 0) {
        i_this->field_0x8e8.ClrTgHit();
        i_this->field_0xa20.ClrTgHit();
        i_this->field_0xb58.ClrTgHit();
        return;
    }

    cCcD_Obj* hit = NULL;
    if (i_this->field_0x8e8.ChkTgHit()) {
        hit = i_this->field_0x8e8.GetTgHitObj();
    } else if (i_this->field_0xa20.ChkTgHit()) {
        hit = i_this->field_0xa20.GetTgHitObj();
    } else if (i_this->field_0xb58.ChkTgHit()) {
        hit = i_this->field_0xb58.GetTgHitObj();
    }

    i_this->field_0x8e8.ClrTgHit();
    i_this->field_0xa20.ClrTgHit();
    i_this->field_0xb58.ClrTgHit();

    if (hit == NULL) {
        return;
    }

    fm->mAtInfo.mpCollider = hit;
    cc_at_check(fm, &fm->mAtInfo);
    i_this->unk_0x690 = 6;
}

// fork b_go_albwShieldStaggerCheck - verbatim except anm_init, a file-static
// whose one-line body (getObjectRes "B_go" + setAnm) is inlined here.
void b_go_albwShieldStaggerCheck(b_go_class* i_this) {
    if (i_this->mActionID != ACT_ATTACK || i_this->unk_0x660 == 0 || i_this->unk_0x690 != 0) {
        return;
    }

    if (!i_this->field_0x8e8.ChkAtShieldHit() && !i_this->field_0xa20.ChkAtShieldHit() &&
        !i_this->field_0xb58.ChkAtShieldHit())
    {
        return;
    }

    i_this->unk_0x660 = 0;
    i_this->unk_0x690 = 6;
    i_this->mActionID = ACT_WAIT;
    i_this->mMode = 0;
    i_this->mTimers[0] = (s16)(cM_rndF(60.0f) + 100.0f);
    // fork calls the file-static anm_init(i_this, ANM_DAMAGE_01, 3.0f, 0, 1.0f);
    // its body, verbatim (d_a_b_go.cpp:37):
    J3DAnmTransform* bck = (J3DAnmTransform*)dComIfG_getObjectRes("B_go", ANM_DAMAGE_01);
    i_this->mpMorf->setAnm(bck, 0, 3.0f, 1.0f, 0.0f, -1.0f);
    i_this->mAnmID = ANM_DAMAGE_01;
    dComIfGp_getVibration().StartShock(VIBMODE_S_POWER8, 0x1F, cXyz(0.0f, 1.0f, 0.0f));
}

// ---------------------------------------------------------------------------
// Hooks.
// ---------------------------------------------------------------------------

// fork daB_GO_Create insert (stock :230): while the golem window is live, also
// phase-load the grA arc alongside B_go, and hold create until both complete.
void on_bgo_create_post(ModContext*, void* args, void* retval, void*) {
    auto* i_this = mods::arg<fopAc_ac_c*>(args, 0);
    if (i_this == nullptr || retval == nullptr) {
        return;
    }
    if (!dAlbwBoss_fyrusGolemWindowIsLive()) {
        return;
    }
    const int gra_state = dComIfG_resLoad(&s_fyrusGraPhase, "grA");
    if (gra_state == cPhs_COMPLEATE_e) {
        s_fyrusGraLoaded = 1;
    }
    if (*static_cast<int*>(retval) == cPhs_COMPLEATE_e && gra_state != cPhs_COMPLEATE_e) {
        *static_cast<int*>(retval) = gra_state;
    }
}

// fork daB_GO_Execute inserts (stock :176/:179), pre-action: pin the sparring
// brain's mode/timer while the boss owns this golem. The fork also guards
// action()'s own field_0x692 rewrite (stock :125) - action is a file-static
// with a collision-prone name, so that guard is reproduced here by keeping
// mTimers[1] nonzero in BOTH golem states (the rewrite only runs at timer 0;
// during the live window the fork's own =100 pin does the same thing).
HookAction on_bgo_execute_pre(ModContext*, void* args, void*, void*) {
    auto* i_this = mods::arg<b_go_class*>(args, 0);
    if (i_this == nullptr) {
        return HOOK_CONTINUE;
    }
    if (dAlbwBoss_fyrusGolemWindowIsLive() && dAlbwBoss_fyrusIsOurGolem(fopAcM_GetID(i_this))) {
        i_this->field_0x692 = 2;
        i_this->mTimers[1] = 100;
    } else if (dAlbwBoss_fyrusGolemKidsLoose() &&
               dAlbwBoss_fyrusIsOurGolem(fopAcM_GetID(i_this)))
    {
        i_this->field_0x692 = 1;
        if (i_this->mTimers[1] == 0) {
            i_this->mTimers[1] = 2;  // stand-in for the fork's action() guard
        }
    }
    return HOOK_CONTINUE;
}

void on_bgo_execute_post(ModContext*, void* args, void*, void*) {
    auto* i_this = mods::arg<b_go_class*>(args, 0);
    if (i_this == nullptr) {
        return;
    }
    const bool ours = dAlbwBoss_fyrusIsOurGolem(fopAcM_GetID(i_this));

    if (dAlbwBoss_fyrusGolemWindowIsLive()) {
        if (ours) {
            // fork h_wait insert (stock :52): 90% shorter idle - ~10-16f vs
            // vanilla ~100-160f. h_wait is a collision-prone file-static, so
            // the clamp runs here, the same frame the timer is set.
            if (i_this->mActionID == ACT_WAIT && i_this->mTimers[0] > 16) {
                i_this->mTimers[0] = (s16)(cM_rndF(6.0f) + 10.0f);
            }
            // fork h_attack insert (stock :105): outside the slam frames the
            // swing is NOT live - clear unk_0x660 so ApplySlamAt drops the At.
            if (i_this->mActionID == ACT_ATTACK && i_this->mMode == 1) {
                const int anm_frame = (int)i_this->mpMorf->getFrame();
                if (anm_frame < 25 || anm_frame > 33) {
                    i_this->unk_0x660 = 0;
                }
            }
        }

        // fork daB_GO_Execute insert (stock :187) - verbatim block.
        fopAcM_OnStatus(i_this, 0);
        i_this->attention_info.flags = fopAc_AttnFlag_BATTLE_e;
        b_go_albwEnsureBodyCc(i_this);
        i_this->mStts.Move();
        b_go_albwApplySlamAt(i_this);
        b_go_albwRegisterFyrusHull(i_this);
        b_go_albwShieldStaggerCheck(i_this);
        b_go_albwProxyDamage(i_this);
    } else if (dAlbwBoss_fyrusGolemKidsLoose() && ours) {
        // fork: freeze in place while the kids are loose...
        i_this->field_0x692 = 1;
        i_this->mActionID = ACT_WAIT;
        i_this->speedF = 0.0f;
        i_this->speed.x = 0.0f;
        i_this->speed.z = 0.0f;
        // ...and despawn once every shed kid is gone.
        bool anyKid = false;
        for (int i = 0; i < GORON_CHILD_MAX; i++) {
            const fpc_ProcID childId = (fpc_ProcID)i_this->mGoronChildIDs[i];
            if (childId != fpcM_ERROR_PROCESS_ID_e && fopAcM_SearchByID(childId) != NULL) {
                anyKid = true;
                break;
            }
        }
        if (!anyKid) {
            dAlbwBoss_fyrusOnGolemKidsCleared();
            fopAcM_delete(i_this);
        }
    }
}

// fork daB_GO_Delete inserts (stock :195/:196).
void on_bgo_delete_post(ModContext*, void* args, void*, void*) {
    auto* i_this = mods::arg<b_go_class*>(args, 0);
    if (i_this == nullptr) {
        return;
    }
    const bool ours = dAlbwBoss_fyrusIsOurGolem(fopAcM_GetID(i_this));
    if (s_fyrusGraLoaded && ours) {
        dComIfG_resDelete(&s_fyrusGraPhase, "grA");
        s_fyrusGraLoaded = 0;
    }
    if (ours) {
        dAlbwBoss_fyrusClearGolemActor();
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

ModResult albw_fyrus_golem_init(ModError* error) {
    if (!install(error, "BGoCreateGra",
                 mods::hook_add_post<BGoCreate>(svc_hook, on_bgo_create_post)) ||
        !install(error, "BGoExecutePre",
                 mods::hook_add_pre<BGoExecute>(svc_hook, on_bgo_execute_pre)) ||
        !install(error, "BGoExecuteGolem",
                 mods::hook_add_post<BGoExecute>(svc_hook, on_bgo_execute_post)) ||
        !install(error, "BGoDeleteGolem",
                 mods::hook_add_post<BGoDelete>(svc_hook, on_bgo_delete_post)))
    {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw fyrus golem (B_GO pass 2) hooks ready");
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
