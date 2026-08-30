#include "flurry_rush.h"

#include "albw_common.h"
#include "albw_game.h"
#include "config_vars.h"
#include "focused_arts.h"
#include "sim_time_scale.h"

#include "d/d_attention.h"
#include "d/d_com_inf_game.h"
#define private public
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_e_oc.h"
#undef private
#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"

#include <chrono>
#include <cstdio>

namespace {

constexpr float kFlurryStartGateRealSeconds = 2.0f;
constexpr float kFlurrySimTimeScale = 0.1f;

enum FlurryMode { FlurryMode_None = 0, FlurryMode_Melee = 1 };

struct State {
    bool active = false;
    FlurryMode mode = FlurryMode_None;
    fopAc_ac_c* target = nullptr;
    fpc_ProcID targetId = fpcM_ERROR_PROCESS_ID_e;
    dFlurryRushSwordProfile profile = dFlurryRushProfile_Unknown;
    std::chrono::steady_clock::time_point startDeadline{};
    bool startGateArmed = false;
    bool hasStartedAttack = false;
    int hitCount = 0;
    bool pendingPerfectDodge = false;
    dFlurryPerfectDodgeKind pendingKind = dFlurryPerfectDodge_SideStep;
};

State s_state;

bool flurry_enabled() {
    return albw_cfg_bool(g_focused_arts, false) && albw_cfg_bool(g_flurry_rush, false);
}

dFlurryMeleeTelegraphAxis queryOcTelegraph(fopAc_ac_c* actor) {
    if (actor == nullptr || fopAcM_GetName(actor) != fpcNm_E_OC_e) {
        return dFlurryTelegraph_None;
    }

    auto* oc = static_cast<daE_OC_c*>(actor);
    constexpr int kOcActionAttack = 4;
    constexpr f32 kTelegraphEndFrame = 14.0f;
    constexpr f32 kVerticalStartFrame = 8.0f;
    constexpr f32 kHorizontalStartFrame = 6.0f;

    if (oc->getActionMode() != kOcActionAttack || oc->mpMorf == nullptr) {
        return dFlurryTelegraph_None;
    }
    if (oc->mOcState != 1 && oc->mOcState != 2) {
        return dFlurryTelegraph_None;
    }

    const f32 frame = oc->mpMorf->getFrame();
    if (frame < kTelegraphEndFrame) {
        if (oc->mOcState == 1 && frame >= kVerticalStartFrame) {
            return dFlurryTelegraph_Vertical;
        }
        if (oc->mOcState == 2 && frame >= kHorizontalStartFrame) {
            return dFlurryTelegraph_Horizontal;
        }
    }
    return dFlurryTelegraph_None;
}

dFlurryRushSwordProfile swordProfileFromEquip() {
    const u8 sword = albw_game::select_equip_sword();
    if (sword == dItemNo_WOOD_STICK_e) {
        return dFlurryRushProfile_Wood;
    }
    if (sword == dItemNo_SWORD_e) {
        return dFlurryRushProfile_Ordon;
    }
    if (sword == dItemNo_MASTER_SWORD_e) {
        return dFlurryRushProfile_Master;
    }
    if (sword == dItemNo_LIGHT_SWORD_e) {
        return dFlurryRushProfile_Light;
    }
    return dFlurryRushProfile_Unknown;
}

dFlurryRushProfile profileTable(dFlurryRushSwordProfile profile) {
    switch (profile) {
    case dFlurryRushProfile_Wood:
        return {0, 0, 10};
    case dFlurryRushProfile_Ordon:
        return {2, 2, 7};
    case dFlurryRushProfile_Master:
    case dFlurryRushProfile_Light:
        return {3, 3, 5};
    default:
        return {0, 0, 0};
    }
}

void refreshTarget() {
    if (s_state.target != nullptr && fopAcM_IsActor(s_state.target)) {
        return;
    }
    s_state.target = fopAcM_SearchByID(s_state.targetId);
}

bool beginMelee(fopAc_ac_c* target) {
    if (!flurry_enabled() || s_state.active || target == nullptr) {
        return false;
    }

    s_state.active = true;
    s_state.mode = FlurryMode_Melee;
    s_state.target = target;
    s_state.targetId = fopAcM_GetID(target);
    s_state.profile = swordProfileFromEquip();
    s_state.hitCount = 0;
    s_state.hasStartedAttack = false;
    s_state.startGateArmed = false;
    s_state.pendingPerfectDodge = false;
    albw::set_sim_time_scale(kFlurrySimTimeScale);
    return true;
}

void armStartGate() {
    if (!s_state.active || s_state.hasStartedAttack) {
        return;
    }
    s_state.startGateArmed = true;
    s_state.startDeadline =
        std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<float>(kFlurryStartGateRealSeconds));
}

}  // namespace

bool dFlurryRush_shouldSuppressAlbwSpend() {
    return s_state.active;
}

bool dFlurryRush_isEnabled() {
    return flurry_enabled();
}

bool dFlurryRush_isActive() {
    return s_state.active;
}

fopAc_ac_c* dFlurryRush_getTargetActor() {
    if (!s_state.active) {
        return nullptr;
    }
    refreshTarget();
    return s_state.target;
}

bool dFlurryRush_isTargetActor(fopAc_ac_c* actor) {
    if (!s_state.active || actor == nullptr) {
        return false;
    }
    refreshTarget();
    return actor == s_state.target;
}

void dFlurryRush_end(dFlurryRushEndReason) {
    if (!s_state.active) {
        return;
    }
    s_state = State{};
    albw::set_sim_time_scale(1.0f);
}

bool dFlurryRush_tryPerfectDodge(dFlurryPerfectDodgeKind kind) {
    if (!flurry_enabled() || s_state.active) {
        return false;
    }

    daPy_py_c* player = albw_game::link_player();
    if (player == nullptr || player->checkWolf() ||
        albw_game::select_equip_sword() == dItemNo_NONE_e || !player->checkAttentionLock())
    {
        return false;
    }

    dAttention_c* attn = albw_game::attention();
    fopAc_ac_c* target = attn != nullptr ? attn->LockonTarget(0) : nullptr;
    if (target == nullptr) {
        return false;
    }

    const dFlurryMeleeTelegraphAxis telegraph = queryOcTelegraph(target);
    const bool axisMatch =
        (kind == dFlurryPerfectDodge_SideStep && telegraph == dFlurryTelegraph_Vertical) ||
        (kind == dFlurryPerfectDodge_BackJump && telegraph == dFlurryTelegraph_Horizontal);
    if (!axisMatch) {
        return false;
    }

    const dFlurryRushProfile profile = profileTable(swordProfileFromEquip());
    if (profile.maxHits <= 0 ||
        !dFocusedArts_canPerfectDodgeSpend(profile.spendGate, profile.barCost))
    {
        return false;
    }

    if (!beginMelee(target)) {
        return false;
    }
    if (!dFocusedArts_onPerfectDodgeSpend(profile.spendGate, profile.barCost)) {
        dFlurryRush_end(dFlurryRushEnd_Interrupt);
        return false;
    }

    s_state.pendingPerfectDodge = true;
    s_state.pendingKind = kind;
    return true;
}

bool dFlurryRush_tryEnterFromDodge() {
    if (!s_state.active || s_state.mode != FlurryMode_Melee || !s_state.pendingPerfectDodge) {
        return false;
    }
    s_state.pendingPerfectDodge = false;
    armStartGate();
    return true;
}

void dFlurryRush_onAttackStarted() {
    if (!s_state.active) {
        return;
    }
    s_state.hasStartedAttack = true;
    s_state.startGateArmed = false;
}

void dFlurryRush_onHitLanded(fopAc_ac_c* enemy) {
    if (!s_state.active || !dFlurryRush_isTargetActor(enemy)) {
        return;
    }
    s_state.hitCount++;
    const dFlurryRushProfile profile = profileTable(s_state.profile);
    if (profile.maxHits > 0 && s_state.hitCount >= profile.maxHits) {
        dFlurryRush_end(dFlurryRushEnd_HitCap);
    }
}

void dFlurryRush_update() {
    if (!flurry_enabled()) {
        if (s_state.active) {
            dFlurryRush_end(dFlurryRushEnd_Interrupt);
        }
        return;
    }
    if (!s_state.active) {
        return;
    }

    refreshTarget();
    if (s_state.target == nullptr) {
        dFlurryRush_end(dFlurryRushEnd_TargetLost);
        return;
    }

    dAttention_c* attn = albw_game::attention();
    if (attn != nullptr) {
        attn->keepLock(30);
    }

    if (!s_state.hasStartedAttack) {
        daPy_py_c* player = albw_game::link_player();
        daAlink_c* link = static_cast<daAlink_c*>(player);
        if (player == nullptr || !player->checkAttentionLock()) {
            dFlurryRush_end(dFlurryRushEnd_Interrupt);
            return;
        }

        if (link != nullptr && s_state.mode == FlurryMode_Melee) {
            const u16 linkProc = link->mProcID;
            if (linkProc == daAlink_c::PROC_SIDESTEP || linkProc == daAlink_c::PROC_BACK_JUMP) {
                if (link->swordSwingTrigger()) {
                    link->onNoResetFlg2(daPy_py_c::FLG2_COMBO_RESERB);
                }
            }
        }

        if (s_state.startGateArmed && std::chrono::steady_clock::now() >= s_state.startDeadline) {
            dFlurryRush_end(dFlurryRushEnd_StartGateExpired);
        }
    } else {
        const dFlurryRushProfile profile = profileTable(s_state.profile);
        if (profile.maxHits > 0 && s_state.hitCount >= profile.maxHits) {
            dFlurryRush_end(dFlurryRushEnd_HitCap);
        }
    }
}

ModResult albw_flurry_init(ModError* error) {
    if (albw_flurry_hooks_init(error) != MOD_OK) {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_flurry_shutdown(ModError*) {
    dFlurryRush_end(dFlurryRushEnd_Interrupt);
    return MOD_OK;
}

void albw_flurry_tick() {
    dFlurryRush_update();
}
