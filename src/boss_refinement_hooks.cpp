// Boss Refinement Layer A — sword gates + Armogohma bookends (Pass 1).

#include "boss_refinement_hooks.h"

#include "albw_common.h"
#include "albw_game.h"
#include "boss_refinement.h"
#include "config_vars.h"

#include "SSystem/SComponent/c_math.h"
#include "Z2AudioLib/Z2Creature.h"
#include "d/actor/d_a_player.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_particle.h"
#include "d/d_particle_name.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "mods/hook.hpp"

#define private public
#include "d/actor/d_a_b_mgn.h"
#include "d/actor/d_a_b_zant.h"
#include "d/actor/d_a_obj_bosswarp.h"
#undef private

namespace {

DEFINE_HOOK(&daB_ZANT_c::checkBigDamage, ZantCheckBigDamage);
DEFINE_HOOK(&daB_ZANT_c::checkDamageType, ZantCheckDamageType);
DEFINE_HOOK(&daB_MGN_c::damage_check, MgnDamageCheck);
DEFINE_HOOK(&daPy_py_c::checkMasterSwordEquip, CheckMasterSwordEquip);
DEFINE_HOOK(&daObjBossWarp_c::demoProc, BossWarpDemoProc);
DEFINE_HOOK_SYMBOL("fopAc_Execute", int(void*), AcExecute);

fpc_ProcID s_gmSeenId = fpcM_ERROR_PROCESS_ID_e;
bool s_gmBootstrapTried = false;

HookAction on_zant_check_big_damage_pre(ModContext*, void* args, void* retval, void*) {
    if (!dAlbwBossRefinement_isEnabled()) {
        return HOOK_CONTINUE;
    }
    auto* zant = mods::arg<daB_ZANT_c*>(args, 0);
    if (zant == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }

    daPy_py_c* player = albw_game::link_player();
    BOOL taken_big_dmg = false;

    if (dAlbwBossRefinement_colliderCountsAsMasterSword(
            static_cast<dCcD_GObjInf*>(zant->mAtInfo.mpCollider)))
    {
        if (zant->mAtInfo.mpCollider->GetAtAtp() >= 4) {
            taken_big_dmg = true;
        } else if (player != nullptr && player->getSwordAtUpTime() != 0) {
            taken_big_dmg = true;
        } else if (player != nullptr && player->getCutCount() >= 4) {
            taken_big_dmg = true;
        }
    }

    *static_cast<bool*>(retval) = taken_big_dmg != FALSE;
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_zant_check_damage_type_pre(ModContext*, void* args, void* retval, void*) {
    if (!dAlbwBossRefinement_isEnabled()) {
        return HOOK_CONTINUE;
    }
    auto* zant = mods::arg<daB_ZANT_c*>(args, 0);
    if (zant == nullptr || retval == nullptr || zant->mAtInfo.mpCollider == nullptr) {
        return HOOK_CONTINUE;
    }

    int type = daB_ZANT_c::DMGTYPE_MISC;
    if (zant->mAtInfo.mpCollider->ChkAtType(AT_TYPE_HOOKSHOT) &&
        zant->mFightPhase == daB_ZANT_c::PHASE_OI)
    {
        type = daB_ZANT_c::DMGTYPE_HOOK_OI;
    } else if (zant->mAtInfo.mpCollider->ChkAtType(AT_TYPE_HOOKSHOT) ||
               zant->mAtInfo.mpCollider->ChkAtType(AT_TYPE_SPINNER) ||
               zant->mAtInfo.mpCollider->ChkAtType(AT_TYPE_ARROW) ||
               zant->mAtInfo.mpCollider->ChkAtType(AT_TYPE_SHIELD_ATTACK))
    {
        type = daB_ZANT_c::DMGTYPE_OBJ;
    } else if (zant->mAtInfo.mpCollider->ChkAtType(AT_TYPE_BOOMERANG) ||
               zant->mAtInfo.mpCollider->ChkAtType(AT_TYPE_40))
    {
        type = daB_ZANT_c::DMGTYPE_BOOMERANG;
    } else if (dAlbwBossRefinement_colliderCountsAsMasterSword(
                   static_cast<dCcD_GObjInf*>(zant->mAtInfo.mpCollider)))
    {
        type = daB_ZANT_c::DMGTYPE_SWORD;
    }

    *static_cast<int*>(retval) = type;
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_check_master_sword_equip_pre(ModContext*, void*, void* retval, void*) {
    // GND fight gates call checkMasterSwordEquip mid-static; remap while B_GND lives.
    if (!dAlbwBossRefinement_isEnabled() || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    if (!albw_game::sword_get()) {
        return HOOK_CONTINUE;
    }
    fopAc_ac_c* gnd = fopAcM_SearchByName(fpcNm_B_GND_e);
    if (gnd == nullptr || !fopAcM_IsActor(gnd)) {
        return HOOK_CONTINUE;
    }
    *static_cast<BOOL*>(retval) = TRUE;
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_mgn_damage_check_pre(ModContext*, void* args, void*, void*) {
    auto* self = mods::arg<daB_MGN_c*>(args, 0);
    if (self == nullptr) {
        return HOOK_CONTINUE;
    }
    if (!dAlbwBossRefinement_isEnabled()) {
        return HOOK_CONTINUE;
    }

    daPy_py_c* player = albw_game::link_player();

    if (self->field_0xafd == 0 && self->field_0xaff >= 4) {
        for (int i = 0; i < 15; i++) {
            self->mBodyCcSph[i].SetTgType(self->mTgType & 0xFFBFDFDF);
        }
    } else {
        for (int i = 0; i < 15; i++) {
            self->mBodyCcSph[i].SetTgType(self->mTgType);
        }
    }

    if (self->field_0xafd == 0) {
        if (self->field_0xaff >= 4) {
            self->field_0x20f4[0].SetTgType(0);
            self->field_0x20f4[1].SetTgType(0);
        } else {
            self->field_0x20f4[0].SetTgType(0x402020);
            self->field_0x20f4[1].SetTgType(0x402020);
        }
        self->field_0x20f4[0].OffTgNoHitMark();
        self->field_0x20f4[1].OffTgNoHitMark();
    } else {
        self->field_0x20f4[0].SetTgType(0xDC000000);
        self->field_0x20f4[1].SetTgType(0xDC000000);
        self->field_0x20f4[0].OnTgNoHitMark();
        self->field_0x20f4[1].OnTgNoHitMark();
    }

    self->mCcStts.Move();

    if (self->mDamageInvulnerabilityTimer == 0) {
        self->mAtInfo.mpCollider = NULL;

        cXyz sp24;
        if (self->field_0x20f4[0].ChkTgHit()) {
            self->mAtInfo.mpCollider = self->field_0x20f4[0].GetTgHitObj();
            sp24 = *self->field_0x20f4[0].GetTgHitPosP();
        }
        if (self->field_0x20f4[1].ChkTgHit()) {
            self->mAtInfo.mpCollider = self->field_0x20f4[1].GetTgHitObj();
            sp24 = *self->field_0x20f4[1].GetTgHitPosP();
        }

        if (self->mAtInfo.mpCollider != NULL) {
            if (self->mAtInfo.mpCollider->ChkAtType(0xD8000000)) {
                self->mDamageInvulnerabilityTimer = 20;
            } else {
                self->mDamageInvulnerabilityTimer = 10;
            }
            if (self->mAtInfo.mAttackPower <= 1) {
                // Vanilla: KREG_S(8)+10; KREG is 0 on stock PC builds.
                self->mDamageInvulnerabilityTimer = 10;
            }

            s16 prev_hp = self->health;
            cc_at_check(self, &self->mAtInfo);

            if (self->field_0xafd == 0) {
                self->mSound.startCreatureVoice(Z2SE_EN_MGN_V_KOROBU, -1);
                self->mHeadHitEffTimer = 100;

                JPABaseEmitter* emitter =
                    albw_game::particle_get_emitter(self->mHeadLightEmitterID);
                if (emitter != nullptr) {
                    emitter->deleteAllParticle();
                }

                if (!albw_game::is_one_zone_switch(5, fopAcM_GetRoomNo(self))) {
                    albw_game::on_one_zone_switch(5, fopAcM_GetRoomNo(self));
                }

                self->field_0xb07 = 0;
                self->health = prev_hp;
                self->mAtSph.OffAtSetBit();

                if (cM_rnd() < 0.5f) {
                    self->setActionMode(daB_MGN_c::ACTION_DOWN_e, 0);
                } else {
                    self->setActionMode(daB_MGN_c::ACTION_DOWN_e, 1);
                }
            } else {
                if (self->mAtInfo.mpCollider->GetAtAtp() >= 1) {
                    sp24 = self->getNearHitPos(&sp24);

                    if (self->mAtInfo.mHitStatus == 0) {
                        albw_game::set_hit_mark(1, self, &sp24, NULL, NULL, 0);
                    } else {
                        albw_game::set_hit_mark(3, self, &sp24, NULL, NULL, 0);
                    }

                    if (player != nullptr) {
                        csXyz effrot(0, cLib_targetAngleY(&sp24, &player->current.pos), 0);
                        albw_game::particle_set(dPa_RM(ID_ZI_S_MGN_BODYCOREHIT_A), &sp24,
                                                &self->tevStr, &effrot, nullptr);
                    }
                }

                if (self->field_0xb08 == 0) {
                    self->field_0xaa0 = 100;
                    self->field_0xb08 = 1;
                }

                if (player != nullptr && player->getCutCount() >= 3 && self->field_0xaa0 < 20) {
                    self->field_0xaa0 = 20;
                }

                if (!albw_game::is_one_zone_switch(6, fopAcM_GetRoomNo(self))) {
                    albw_game::on_one_zone_switch(6, fopAcM_GetRoomNo(self));
                }

                if (player != nullptr &&
                    self->mAtInfo.mpCollider->ChkAtType(AT_TYPE_WOLF_ATTACK) &&
                    player->getCutType() != daPy_py_c::CUT_TYPE_WOLF_B_LEFT &&
                    player->getCutType() != daPy_py_c::CUT_TYPE_WOLF_B_RIGHT &&
                    player->onWolfEnemyHangBite(self))
                {
                    self->setActionMode(daB_MGN_c::ACTION_DOWN_BITE_DAMAGE_e, 0);
                    self->field_0x20f4[0].ClrTgHit();
                    self->field_0x20f4[1].ClrTgHit();
                    return HOOK_SKIP_ORIGINAL;
                }

                u8 var_r29 = 0;
                if (dAlbwBossRefinement_colliderCountsAsMasterSword(
                        static_cast<dCcD_GObjInf*>(self->mAtInfo.mpCollider)))
                {
                    if (self->mAtInfo.mpCollider->GetAtAtp() >= 4) {
                        if (player != nullptr && player->getSwordAtUpTime() != 0) {
                            var_r29 = 4;
                        } else {
                            var_r29 = 2;
                        }
                    } else if (player != nullptr && player->getSwordAtUpTime() != 0) {
                        var_r29 = 2;
                    } else if (player != nullptr && player->getCutCount() >= 4) {
                        var_r29 = 2;
                    } else if (static_cast<dCcD_GObjInf*>(self->mAtInfo.mpCollider)->GetAtSpl() ==
                               1)
                    {
                        self->field_0xb07++;
                        if (self->field_0xb07 >= 2) {
                            var_r29 = 2;
                        }
                    }
                } else if (self->mAtInfo.mpCollider->GetAtAtp() >= 4 ||
                           static_cast<dCcD_GObjInf*>(self->mAtInfo.mpCollider)->GetAtSpl() == 1)
                {
                    var_r29 = 1;
                }

                if (self->field_0xaff == 3 && var_r29 != 0) {
                    var_r29 = 1;
                }

                self->field_0xb06 = var_r29;
                self->setActionMode(daB_MGN_c::ACTION_DOWN_DAMAGE_e, 0);
            }

            self->field_0x20f4[0].ClrTgHit();
            self->field_0x20f4[1].ClrTgHit();
        }
    }

    return HOOK_SKIP_ORIGINAL;
}

void on_ac_execute_post(ModContext*, void* args, void*, void*) {
    auto* actor = static_cast<fopAc_ac_c*>(mods::arg<void*>(args, 0));
    if (actor == nullptr || !fopAcM_IsActor(actor)) {
        return;
    }
    if (fopAcM_GetName(actor) != fpcNm_B_GM_e) {
        return;
    }

    const fpc_ProcID id = fopAcM_GetID(actor);
    if (id != s_gmSeenId) {
        s_gmSeenId = id;
        s_gmBootstrapTried = false;
        dAlbwBoss_armogohmaResetFightState();
    }

    dAlbwBoss_armogohmaEnsureInitialized(actor);

    if (!s_gmBootstrapTried) {
        s_gmBootstrapTried = true;
        dAlbwBoss_tryApplyActorBootstrap(fpcNm_B_GM_e, actor);
    }
}

// Fork: SCENE_CHG advance in demoProc calls requestWarpBootstrap before scene change.
HookAction on_boss_warp_demo_proc_pre(ModContext*, void* args, void*, void*) {
    if (!dAlbwBossRefinement_isEnabled()) {
        return HOOK_CONTINUE;
    }
    auto* warp = mods::arg<daObjBossWarp_c*>(args, 0);
    if (warp == nullptr) {
        return HOOK_CONTINUE;
    }

    static DUSK_CONST char* action_table[15] = {
        "WAIT",         "APPEAR",       "DISAPPEAR",    "SCENE_CHG",    "STONE_FALL",
        "STONE_MIDNA",  "WALK_TARGET1", "APPEAR_END",   "STONE_DELETE", "STONE_PUTAWAY",
        "WCHECK",       "SETPOS",       "SCALING",      "STONE_SCALE",  "HEART_MOVE",
    };

    if (albw_game::evt_is_addvance(warp->mStaffId)) {
        const int act_idx =
            albw_game::evt_my_act_idx(warp->mStaffId, action_table, 15, 0, 0);
        if (act_idx == 3) {
            dAlbwBoss_requestWarpBootstrap(albw_game::stage_name());
        }
    }
    return HOOK_CONTINUE;
}

bool install(const char* what, ModResult result, ModError* error) {
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, what);
        if (error != nullptr) {
            error->code = MOD_ERROR;
        }
        return false;
    }
    return true;
}

}  // namespace

void albw_boss_refinement_on_stage_load() {
    dAlbwBoss_onStageLoad();
    s_gmSeenId = fpcM_ERROR_PROCESS_ID_e;
    s_gmBootstrapTried = false;
}

ModResult albw_boss_refinement_init(ModError* error) {
    if (!install("boss_refinement: ZantCheckBigDamage",
                 mods::hook_add_pre<ZantCheckBigDamage>(svc_hook, on_zant_check_big_damage_pre),
                 error) ||
        !install("boss_refinement: ZantCheckDamageType",
                 mods::hook_add_pre<ZantCheckDamageType>(svc_hook, on_zant_check_damage_type_pre),
                 error) ||
        !install("boss_refinement: MgnDamageCheck",
                 mods::hook_add_pre<MgnDamageCheck>(svc_hook, on_mgn_damage_check_pre), error) ||
        !install("boss_refinement: CheckMasterSwordEquip",
                 mods::hook_add_pre<CheckMasterSwordEquip>(svc_hook,
                                                           on_check_master_sword_equip_pre),
                 error) ||
        !install("boss_refinement: BossWarpDemoProc",
                 mods::hook_add_pre<BossWarpDemoProc>(svc_hook, on_boss_warp_demo_proc_pre),
                 error) ||
        !install("boss_refinement: AcExecute",
                 mods::hook_add_post<AcExecute>(svc_hook, on_ac_execute_post), error))
    {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "boss_refinement hooks ready");
    return MOD_OK;
}

ModResult albw_boss_refinement_shutdown(ModError*) {
    return MOD_OK;
}
