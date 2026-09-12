// ALBW Phase 4 — manual shield + parry/bash hooks on stock dusklight-main host.

#include "global.h"

#include "SSystem/SComponent/c_cc_d.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "SSystem/SComponent/c_phase.h"
#include "d/d_resorce.h"
#include "d/actor/d_a_player.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#define private public
#include "d/actor/d_a_alink.h"
#undef private

#include "albw_common.h"
#include "albw_dusk_log.h"
#include "lockout.h"
#include "meter_bridge.h"
#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"
#include "mods/hook.hpp"
#include "shield.h"
#include "shield_adapt.h"
#include "shield_game.h"
#include "shield_mod.h"
#include "boss_hp_hud.h"
#include "parry_master.h"
#include "sumo_test.h"
#include "d/d_save.h"

namespace {

DEFINE_HOOK(&daAlink_c::checkGuardAccept, CheckGuardAccept);
DEFINE_HOOK(&daAlink_c::checkGuardActionChange, CheckGuardActionChange);
DEFINE_HOOK(&daAlink_c::setShieldGuard, SetShieldGuard);
DEFINE_HOOK(&daAlink_c::swordSwingTrigger, SwordSwingTrigger);
DEFINE_HOOK(&daAlink_c::checkItemAction, CheckItemAction);
DEFINE_HOOK(&daAlink_c::procGuardAttackInit, ProcGuardAttackInit);
DEFINE_HOOK(&daAlink_c::procGuardSlipInit, ProcGuardSlipInit);
DEFINE_HOOK(&daAlink_c::procGuardBreakInit, ProcGuardBreakInit);
DEFINE_HOOK(&daAlink_c::execute, LinkExecute);
DEFINE_HOOK(&daAlink_c::setShieldChange, SetShieldChange);
DEFINE_HOOK(&daAlink_c::loadShieldModelDVD, LoadShieldModelDVD);
DEFINE_HOOK(&daAlink_c::setShieldModel, SetShieldModel);
DEFINE_HOOK(&daAlink_c::setShieldArcName, SetShieldArcName);
DEFINE_HOOK(&daAlink_c::checkShieldDraw, CheckShieldDraw);
DEFINE_HOOK(&daAlink_c::checkSwordDraw, CheckSwordDraw);
DEFINE_HOOK(&dMeter2_c::moveKantera, MoveKantera);
DEFINE_HOOK(&dMeter2Draw_c::draw, MeterDraw);

// Fork multi-shield quick-swap safety (docs/d-pad-reworking.md).
static const char kShieldArcCw[] = "CWShd";
static const char kShieldArcSw[] = "SWShd";
static const char kShieldArcHy[] = "HyShd";

HookAction on_set_shield_change_pre(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    // Restarting mid-reload leaves mShieldModel NULL forever (invisible + can't cycle).
    if (link->mShieldChangeWaitTimer != 0) {
        DuskLog.info("[shield] setShieldChange refused (timer={})",
                     (int)link->mShieldChangeWaitTimer);
        return HOOK_SKIP_ORIGINAL;
    }
    DuskLog.info("[shield] change begins: equip={}", (int)albw_shield_game::get_select_equip_shield());
    link->offNoResetFlg2(daPy_py_c::FLG2_UNK_8000000);
    link->mShieldChangeWaitTimer = 4;
    return HOOK_SKIP_ORIGINAL;
}

// ============================================
// NEW CODE - ALBW Port (hardened shield reload - fork d_a_alink_swindow.inc)
//
// The fork hardened loadShieldModelDVD and setShieldModel; the mod never
// carried it, and both playtest symptoms are that one gap:
//   - stock dComIfG_resDelete no-ops when the phase id != 2, so the arc stays
//     REGISTERED while freeAll() guts its heap. The next resLoad sees the stale
//     registration and the rebuilt model comes from freed data -> INVISIBLE
//     shield. Fork fix: force-unregister via dComIfG_deleteObjectResMain.
//   - a reload that never completes flips the timer 1<->2 forever, and every
//     swap path checks that timer -> "cannot continue swapping". Fork fix:
//     NULL-heap bailout zeroes the timer; the model-miss is REPORTED.
//
// loadShieldModelDVD is replaced with the fork body (member access via the
// private-public include, OS_REPORT -> svc_log). setShieldModel needs no
// wholesale copy: the fork's only change is a NULL-data guard before initModel,
// so a pre-hook checks the same lookup and skips vanilla with mShieldModel
// cleared - vanilla IS the fork's else-branch.
// ============================================
HookAction on_load_shield_model_dvd_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    auto ret = [retval](int v) {
        *static_cast<int*>(retval) = v;
        return HOOK_SKIP_ORIGINAL;
    };

    if (link->mShieldChangeWaitTimer != 0) {
        link->mShieldChangeWaitTimer--;

        if (link->mShieldChangeWaitTimer == 2) {
            link->mShieldModel = NULL;
            if (!dComIfG_resDelete(&link->mShieldPhaseReq, link->mShieldArcName)) {
                // resDelete no-ops when phase id != 2; force-unregister before freeAll
                dComIfG_deleteObjectResMain(link->mShieldArcName);
            }
            cPhs_Reset(&link->mShieldPhaseReq);
            if (link->mpShieldArcHeap != NULL) {
                link->mpShieldArcHeap->freeAll();
            }
            link->setShieldArcName();
        } else if (link->mShieldChangeWaitTimer == 1) {
            if (link->mpShieldArcHeap == NULL) {
                link->mShieldChangeWaitTimer = 0;
                return ret(1);
            }

            int phase_state =
                dComIfG_resLoad(&link->mShieldPhaseReq, link->mShieldArcName, link->mpShieldArcHeap);
            if (phase_state == cPhs_COMPLEATE_e) {
                link->mShieldChangeWaitTimer = 0;
                link->setShieldModel();
                // PROBE: report EVERY completed reload, not only failures - an
                // "invisible but silent" session must show whether the model
                // pointer was ever non-null here.
                DuskLog.info("[shield] reload done: arc={} model={}",
                             link->mShieldArcName != NULL ? link->mShieldArcName : "(null)",
                             (const void*)link->mShieldModel);
                if (link->mShieldModel == NULL && svc_log != nullptr) {
                    svc_log->error(mod_ctx, "loadShieldModelDVD: missing shield model after load");
                }
            } else {
                // PROBE: once per second while the phase never completes.
                static u16 sStall = 0;
                if ((++sStall % 30) == 1) {
                    DuskLog.info("[shield] reload stalled: arc={} phase={}",
                                 link->mShieldArcName != NULL ? link->mShieldArcName : "(null)",
                                 phase_state);
                }
                link->mShieldChangeWaitTimer = 2;
            }
        }
    } else {
        return ret(1);
    }

    return ret(0);
}

HookAction on_set_shield_model_pre(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    if (dComIfG_getObjectRes(link->mShieldArcName, 3) == NULL) {
        // fork setShieldModel: NULL model data -> leave the model empty rather
        // than hand initModel a null (its JUT_ASSERT is compiled out here).
        link->mShieldModel = NULL;
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, "setShieldModel: shield arc has no model data (res 3)");
        }
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction on_set_shield_arc_name_pre(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }

    // Always map body arc from equipped item — multi-shield first-bit fallthrough
    // frees the wrong arc and sticks the reload with a null model.
    const u8 equip = albw_shield_game::get_select_equip_shield();
    switch (equip) {
    case dItemNo_WOOD_SHIELD_e:
        link->mShieldArcName = kShieldArcCw;
        break;
    case dItemNo_SHIELD_e:
        link->mShieldArcName = kShieldArcSw;
        break;
    case dItemNo_HYLIA_SHIELD_e:
        link->mShieldArcName = kShieldArcHy;
        break;
    default:
        if (albw_shield_game::is_item_first_bit(dItemNo_HYLIA_SHIELD_e)) {
            link->mShieldArcName = kShieldArcHy;
        } else if (albw_shield_game::is_item_first_bit(dItemNo_SHIELD_e)) {
            link->mShieldArcName = kShieldArcSw;
        } else {
            link->mShieldArcName = kShieldArcCw;
        }
        break;
    }
    return HOOK_SKIP_ORIGINAL;
}

bool allow_npc_guard(const daAlink_c* link) {
    return !((link->mTargetedActor != nullptr && !link->checkSpecialNpc(link->mTargetedActor)) &&
             (fopAcM_GetGroup(link->mTargetedActor) == 3 ||
              fopAcM_GetGroup(link->mTargetedActor) == fopAc_NPC_e));
}

HookAction on_check_guard_accept_pre(ModContext*, void* args, void* retval, void*) {
    if (!albw_manual_shield_enabled()) {
        return HOOK_CONTINUE;
    }

    auto* link = mods::arg<daAlink_c*>(args, 0);
    const BOOL ok = link != nullptr && albw_manual_shield_button(link) &&
                    link->checkShieldGet() && !link->checkFmChainGrabAnime() &&
                    !link->checkNotBattleStage();
    *static_cast<BOOL*>(retval) = ok;
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_check_guard_action_change_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }

    if (albw_manual_shield_enabled()) {
        const BOOL ok = albw_manual_shield_button(link) && !link->checkIronBallWaitAnime() &&
                        !link->checkGrabAnime() && !link->checkCopyRodControllAnime() &&
                        allow_npc_guard(link);
        *static_cast<BOOL*>(retval) = ok;
        return HOOK_SKIP_ORIGINAL;
    }

    return HOOK_CONTINUE;
}

void on_set_shield_guard_post(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }

    if (albw_manual_shield_enabled() && !albw_manual_shield_button(link)) {
        link->offNoResetFlg2(daPy_py_c::FLG2_UNK_8000000);
    }

    if (albw_shield_parry_enabled()) {
        dShield_updateGuardTracking(link);
    }
}

HookAction on_sword_swing_trigger_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link != nullptr && albw_manual_shield_blocks_sword(link)) {
        *static_cast<BOOL*>(retval) = FALSE;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction on_check_item_action_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link->checkWolf() || !albw_shield_features_active()) {
        return HOOK_CONTINUE;
    }

    if (albw_manual_shield_attack_trigger(link) && link->checkGuardActionChange() &&
        !link->checkUpperReadyThrowAnime() && !link->checkModeFlg(0x70C52) &&
        link->checkShieldGet() && !link->checkNotBattleStage() &&
        (link->mLinkAcch.ChkGroundHit() || link->checkMagneBootsOn()))
    {
        *static_cast<BOOL*>(retval) = link->procGuardAttackInit();
        return HOOK_SKIP_ORIGINAL;
    }

    if (albw_shield_parry_enabled() && link->checkGuardActionChange() &&
        !link->checkUpperReadyThrowAnime() && !link->checkModeFlg(0x70C52) &&
        link->checkShieldGet() && !link->checkNotBattleStage())
    {
        // Vanilla R-bash path — disabled while parry/bash economy is on.
        return HOOK_CONTINUE;
    }

    return HOOK_CONTINUE;
}

HookAction on_proc_guard_attack_init_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || !albw_shield_parry_enabled()) {
        return HOOK_CONTINUE;
    }

    if (link->mDemo.getDemoMode() == daPy_demo_c::DEMO_GUARD_ATTACK_e &&
        link->mProcID == daAlink_c::PROC_GUARD_ATTACK)
    {
        return HOOK_CONTINUE;
    }

    if (link->mDemo.getDemoMode() != daPy_demo_c::DEMO_GUARD_ATTACK_e &&
        !albw_manual_shield_attack_trigger(link))
    {
        *static_cast<int*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }

    if (link->mDemo.getDemoMode() != daPy_demo_c::DEMO_GUARD_ATTACK_e &&
        !dShield_tryBeginGuardAttack())
    {
        *static_cast<int*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }

    return HOOK_CONTINUE;
}

HookAction on_proc_guard_slip_init_pre(ModContext*, void* args, void* retval, void*) {
    if (!albw_shield_parry_enabled()) {
        return HOOK_CONTINUE;
    }

    auto* link = mods::arg<daAlink_c*>(args, 0);
    const int at_spl = mods::arg<int>(args, 1);
    auto* objinf = mods::arg<dCcD_GObjInf*>(args, 2);
    fopAc_ac_c* attacker = objinf != nullptr ? objinf->GetTgHitAc() : nullptr;

    if (link != nullptr && dShield_onShieldHit(link, at_spl, attacker)) {
        *static_cast<int*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }

    if (link != nullptr && dShield_onFailedGuardBreakBlock(link, at_spl, attacker)) {
        *static_cast<int*>(retval) = 1;
        return HOOK_SKIP_ORIGINAL;
    }

    dShield_onFailedGuardBlock(attacker);
    if (link != nullptr) {
        // Fork passes GetAtAtp() (attack power), not AtSpl (special type).
        const int atp = objinf != nullptr ? static_cast<int>(objinf->GetAtAtp()) : 0;
        dParryMaster_onFailedBlock(link, atp);
    }
    return HOOK_CONTINUE;
}

HookAction on_proc_guard_break_init_pre(ModContext*, void* args, void* retval, void*) {
    if (!albw_shield_parry_enabled()) {
        return HOOK_CONTINUE;
    }

    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }

    dCcD_GObjInf* shield_hit = nullptr;
    for (int i = 0; i < 3; ++i) {
        if (link->mTgCyls[i].ChkTgShieldHit()) {
            shield_hit = &link->mTgCyls[i];
            break;
        }
    }
    if (shield_hit == nullptr) {
        return HOOK_CONTINUE;
    }

    dCcD_GObjInf* tg_gobj = shield_hit->GetTgHitGObj();
    const int at_spl = tg_gobj != nullptr ? tg_gobj->GetAtSpl() : static_cast<int>(link->mCcStts.GetAtSpl());
    fopAc_ac_c* attacker = shield_hit->GetTgHitAc();

    if (dShield_shouldDeferGuardBreak(at_spl, attacker)) {
        *static_cast<int*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }

    return HOOK_CONTINUE;
}

void on_link_execute_post(ModContext*, void* args, void*, void*) {
    if (!albw_shield_parry_enabled()) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link != nullptr) {
        dShield_pollGuardAttackHit(link);
    }
}

void on_move_kantera_post(ModContext*, void* args, void*, void*) {
    albw_shield_game::set_meter_class(mods::arg<dMeter2_c*>(args, 0));
}

void on_meter_draw_post(ModContext*, void*, void*, void*) {
    // fork d_meter2_draw.cpp:1193 - the whole aux-HUD block is gated on
    // "not paused, heap-lock != 6"; without it the boss bar (and bash charges)
    // draw over menus - the reported bar-in-menus.
    if (dComIfGp_isPauseFlag() || g_dComIfG_gameInfo.play.isHeapLockFlag() == 6) {
        return;
    }
    if (albw_shield_parry_enabled()) {
        dShield_drawBashCharges();
    }
    albw_boss_hp_hud_draw();
}

// ============================================
// NEW CODE - ALBW Port (SumoTest weapon-draw gates - fork d_a_alink.cpp)
//
// The mod already had the CONSUMER (dAlbwSumoTest_showWeapons, sumo_test.cpp,
// documented in sumo_test.h as "for checkSwordDraw/checkShieldDraw") but the
// two fork-modified draw gates were never wired - the found-by-diff gap. Stock
// suppresses the sword/shield whenever the sumo body flag (FLG2_UNK_80000) is
// set, so on the SumoTest outfit both weapons go INVISIBLE. The fork drops just
// the sumo bit from the suppression mask when dAlbwSumoTest_showWeapons(), and
// checkShieldDraw also gains an mShieldModel != NULL guard. Bodies verbatim.
// ============================================
HookAction on_check_shield_draw_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    daPy_py_c::daPy_FLG2 drawSuppress = daPy_py_c::FLG2_UNK_4080000;
    if (dAlbwSumoTest_showWeapons()) {
        drawSuppress = daPy_py_c::FLG2_UNK_4000000;
    }
    const bool modelOk = link->mShieldModel != NULL;
    const bool gotOk = daPy_py_c::checkShieldGet();
    const bool timerOk = link->mShieldChangeWaitTimer == 0;
    const bool suppressed = link->checkNoResetFlg2(drawSuppress);
    const bool wolfOk = !link->checkWolf() || !dComIfGs_isEventBit(dSv_event_flag_c::M_068);
    const bool v = modelOk && gotOk && timerOk && !suppressed && wolfOk;
    // PROBE (temporary): when the shield would be hidden, log WHICH condition
    // failed — quick-swap leaves it invisible while the menu-equip does not, and
    // every static path matches the fork, so this pins the actual failing gate.
    if (!v) {
        static u16 sDrawProbe = 0;
        if ((++sDrawProbe % 20) == 1) {
            DuskLog.info("[shield] draw hidden: model={} got={} timer={} suppressed={} "
                         "flg2=0x{:x} equip={}",
                         (int)modelOk, (int)gotOk, (int)link->mShieldChangeWaitTimer,
                         (int)suppressed, (unsigned)link->mNoResetFlg2,
                         (int)albw_shield_game::get_select_equip_shield());
        }
    }
    *static_cast<bool*>(retval) = v;
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_check_sword_draw_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    daPy_py_c::daPy_FLG2 drawSuppress = daPy_py_c::FLG2_UNK_2080000;
    if (dAlbwSumoTest_showWeapons()) {
        drawSuppress = daPy_py_c::FLG2_UNK_2000000;
    }
    const bool v = ((daPy_py_c::checkSwordGet() && link->mSwordChangeWaitTimer == 0) &&
                    !link->checkNoResetFlg2(drawSuppress)) &&
                   (!link->checkWolf() || !dComIfGs_isEventBit(dSv_event_flag_c::M_068));
    *static_cast<bool*>(retval) = v;
    return HOOK_SKIP_ORIGINAL;
}

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace

void albw_shield_on_block_while_bombling() {
    // Fork: dAlbwLockout_onBlockWhileBomblingActive — only +1 while bombling
    // orbits. Ungated add here stacked on every parry's sBashCharges++ → +2.
    if (!albw_shield_parry_enabled() || !albw_lockout_is_bombling_active()) {
        return;
    }
    dShield_addBashCharge(1);
}

void albw_shield_add_bash_charge(u8 amount) {
    dShield_addBashCharge(amount);
}

ModResult albw_shield_init(ModError* error) {
    dShield_resetSession();

    if (!install(error, "CheckGuardAccept",
                 mods::hook_add_pre<CheckGuardAccept>(svc_hook, on_check_guard_accept_pre)) ||
        !install(error, "CheckGuardActionChange",
                 mods::hook_add_pre<CheckGuardActionChange>(svc_hook,
                                                            on_check_guard_action_change_pre)) ||
        !install(error, "SetShieldGuard",
                 mods::hook_add_post<SetShieldGuard>(svc_hook, on_set_shield_guard_post)) ||
        !install(error, "SwordSwingTrigger",
                 mods::hook_add_pre<SwordSwingTrigger>(svc_hook, on_sword_swing_trigger_pre)) ||
        !install(error, "CheckItemAction",
                 mods::hook_add_pre<CheckItemAction>(svc_hook, on_check_item_action_pre)) ||
        !install(error, "ProcGuardAttackInit",
                 mods::hook_add_pre<ProcGuardAttackInit>(svc_hook, on_proc_guard_attack_init_pre)) ||
        !install(error, "ProcGuardSlipInit",
                 mods::hook_add_pre<ProcGuardSlipInit>(svc_hook, on_proc_guard_slip_init_pre)) ||
        !install(error, "ProcGuardBreakInit",
                 mods::hook_add_pre<ProcGuardBreakInit>(svc_hook, on_proc_guard_break_init_pre)) ||
        !install(error, "LinkExecuteShield",
                 mods::hook_add_post<LinkExecute>(svc_hook, on_link_execute_post)) ||
        !install(error, "SetShieldChangeSafe",
                 mods::hook_add_pre<SetShieldChange>(svc_hook, on_set_shield_change_pre)) ||
        !install(error, "SetShieldArcNameEquip",
                 mods::hook_add_pre<SetShieldArcName>(svc_hook, on_set_shield_arc_name_pre)) ||
        !install(error, "LoadShieldModelDVDHardened",
                 mods::hook_add_pre<LoadShieldModelDVD>(svc_hook, on_load_shield_model_dvd_pre)) ||
        !install(error, "SetShieldModelNullGuard",
                 mods::hook_add_pre<SetShieldModel>(svc_hook, on_set_shield_model_pre)) ||
        !install(error, "MoveKanteraShield",
                 mods::hook_add_post<MoveKantera>(svc_hook, on_move_kantera_post)) ||
        !install(error, "CheckShieldDrawSumo",
                 mods::hook_add_pre<CheckShieldDraw>(svc_hook, on_check_shield_draw_pre)) ||
        !install(error, "CheckSwordDrawSumo",
                 mods::hook_add_pre<CheckSwordDraw>(svc_hook, on_check_sword_draw_pre)) ||
        !install(error, "MeterDrawShield",
                 mods::hook_add_post<MeterDraw>(svc_hook, on_meter_draw_post)))
    {
        return MOD_ERROR;
    }

    svc_log->info(mod_ctx, "albw shield P4 hooks ready");
    return MOD_OK;
}

ModResult albw_shield_shutdown(ModError*) {
    mods::hook_uninstall<SetShieldChange>(svc_hook);
    mods::hook_uninstall<SetShieldArcName>(svc_hook);
    return MOD_OK;
}
