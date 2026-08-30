// Stick cycle lock-on — part of dev.albt.albw.

#include "albw_common.h"
#include "config_vars.h"
#include "modules.h"

#include "d/d_attention.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_controller_pad.h"
#include "mods/svc/hook.hpp"

#include <cmath>

namespace {

int s_stickCycleCooldown = 0;
f32 s_prevStickX = 0.0f;

DEFINE_HOOK(&dAttention_c::Run, AttentionRun);
DEFINE_HOOK(&dCamera_c::lockonCamera, LockonCamera);

bool enabled() {
    return albw_cfg_bool(g_stick_cycle, true);
}

bool isLiveActor(fopAc_ac_c* actor) {
    if (actor == NULL || !fopAcM_IsActor(actor)) {
        return false;
    }
    const fpc_ProcID procId = fpcM_GetID(actor);
    return procId != fpcM_ERROR_PROCESS_ID_e && fpcM_IsExecuting(procId);
}

bool isBattleEnemy(fopAc_ac_c* actor) {
    return isLiveActor(actor) && fopAcM_GetGroup(actor) == fopAc_ENEMY_e &&
           (actor->attention_info.flags & fopAc_AttnFlag_BATTLE_e) != 0;
}

bool isPlaySceneActive() {
    return fpcM_SearchByName(fpcNm_PLAY_SCENE_e) != NULL;
}

void tryStickCycleBattleLockon(dAttention_c* attn) {
    if (!enabled()) {
        return;
    }
    if (!isPlaySceneActive() || g_dComIfG_gameInfo.play.getEvent()->runCheck() ||
        g_dComIfG_gameInfo.play.isPauseFlag())
    {
        s_prevStickX = 0.0f;
        return;
    }
    if (attn->mAttnStatus != dAttention_c::EState_LOCK || !attn->LockonTruth()) {
        s_prevStickX = 0.0f;
        return;
    }
    if (s_stickCycleCooldown > 0) {
        s_stickCycleCooldown--;
        return;
    }

    const f32 stickX = mDoCPd_c::getSubStickX(attn->mPadNo);
    const f32 threshold = 0.45f;
    if (std::fabs(stickX) < threshold) {
        s_prevStickX = stickX;
        return;
    }
    if (std::fabs(s_prevStickX) >= threshold) {
        s_prevStickX = stickX;
        return;
    }

    const int direction = stickX > 0.0f ? 1 : -1;
    s_prevStickX = stickX;

    int battleIndices[8];
    int battleCount = 0;
    for (int i = 0; i < attn->mLockonCount && battleCount < 8; i++) {
        if (isBattleEnemy(attn->mLockOnList[i].getActor())) {
            battleIndices[battleCount++] = i;
        }
    }
    if (battleCount <= 1) {
        return;
    }

    int curBattleIdx = -1;
    for (int i = 0; i < battleCount; i++) {
        if (battleIndices[i] == attn->mLockOnOffset) {
            curBattleIdx = i;
            break;
        }
    }
    if (curBattleIdx < 0) {
        attn->mLockOnOffset = battleIndices[direction > 0 ? 0 : battleCount - 1];
    } else {
        curBattleIdx += direction;
        if (curBattleIdx < 0) {
            curBattleIdx = battleCount - 1;
        } else if (curBattleIdx >= battleCount) {
            curBattleIdx = 0;
        }
        attn->mLockOnOffset = battleIndices[curBattleIdx];
    }

    fopAc_ac_c* target = attn->LockonTarget(0);
    if (!isBattleEnemy(target)) {
        return;
    }

    attn->mLockTargetID = attn->LockonTargetPId(0);
    attn->field_0x32e = 15;
    attn->setFlag(0x8);
    s_stickCycleCooldown = 10;
}

void on_attention_run_post(ModContext*, void* args, void*, void*) {
    dAttention_c* attn = mods::arg<dAttention_c*>(args, 0);
    if (attn != nullptr && attn->mAttnStatus == dAttention_c::EState_LOCK) {
        tryStickCycleBattleLockon(attn);
    }
}

HookAction on_lockon_camera_pre(ModContext*, void* args, void*, void*) {
    if (!enabled()) {
        return HOOK_CONTINUE;
    }
    dCamera_c* cam = mods::arg<dCamera_c*>(args, 0);
    if (cam == nullptr) {
        return HOOK_CONTINUE;
    }
    dAttention_c* attn = g_dComIfG_gameInfo.play.getAttention();
    if (attn == nullptr || !attn->Lockon()) {
        return HOOK_CONTINUE;
    }
    cam->mPadInfo.mCStick.mLastPosX = 0.0f;
    cam->mPadInfo.mCStick.mLastPosY = 0.0f;
    return HOOK_CONTINUE;
}

}  // namespace

ModResult albw_stick_cycle_build_panel(UiElementHandle panel, ModError*) {
    return albw_ui_add_toggle(
        panel, "Stick Cycle Lock-on",
        "While Z-targeting, flick the right stick left or right to cycle nearby battle "
        "enemies. Lock-on camera orbit from that stick is suppressed.",
        g_stick_cycle);
}

ModResult albw_stick_cycle_init(ModError*) {
    if (mods::hook::add_post<AttentionRun>(on_attention_run_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook dAttention_c::Run");
        return MOD_ERROR;
    }
    if (mods::hook::add_pre<LockonCamera>(on_lockon_camera_pre) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook dCamera_c::lockonCamera");
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_stick_cycle_shutdown(ModError*) {
    s_stickCycleCooldown = 0;
    s_prevStickX = 0.0f;
    return MOD_OK;
}
