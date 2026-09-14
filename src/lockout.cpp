// ============================================
// ALBW meter lockout — item special-usages during lockout.
//
// STAGE 1 MERGE: the fork's d_albw_lockout.cpp is now the AUTHORITATIVE logic
// (lockout_port.inc, extracted verbatim by tools/port/port_tool.py). This TU
// supplies the small bridge deps, includes that port, and provides the WIRING —
// the hooks that reproduce the fork's per-actor call sites (d_a_nbomb, d_a_alink
// hookshot, d_a_crod, d_cc_uty) plus the mod's shared-seam confuse redirect
// (fopAcM_searchActor*). The hand-written lockout/confuse LOGIC was deleted; only
// the d_a_nbomb bomb-actor reproduction (water bomb visuals + bombling collision
// registration), which is NOT in d_albw_lockout.cpp, is kept here.
// ============================================

#include "global.h"
#include "albw_symbols.h"

#include "SSystem/SComponent/c_cc_d.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "d/actor/d_a_player.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "m_Do/m_Do_mtx.h"

#define private public
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_crod.h"
#include "d/actor/d_a_nbomb.h"
#undef private

#include "albw_common.h"
#include "focused_arts.h"
#include "lockout.h"
#include "lockout_port.h"
#include "meter_bridge.h"
#include "modules.h"
#include "shield.h"
#include "wolf_combat.h"
#include "mods/hook.hpp"

#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "d/d_bomb.h"

#include <algorithm>

#if TARGET_PC

// ============================================
// Bridge deps: the ported dAlbwLockout_* code calls these fork-named engine/meter
// helpers; route them to the mod's equivalents. (wolf-stun, shield-bash and
// focused-arts fork names are already defined by their own mod TUs.)
// ============================================
bool dMeter2_isALBWLocked() {
    return albw_meter_is_locked();
}
void dMeter2_addALBWBaseFraction(int numerator, int denominator) {
    albw_meter_add_base_fraction(numerator, denominator);
}
void dMeter2_drainALBWToLockout() {
    albw_meter_drain_to_lockout();
}
// daAlink_getAlinkActorClass is provided inline by d/actor/d_a_alink.h.

// The authoritative fork lockout logic (counters, tagged/slingshot, double-claw,
// bombling deploy/orbit, applyAttackPowerBoost, Dom-Rod confuse + provoke).
#include "lockout_port.inc"

namespace {

// Fork daNbomb_FLG0 bits — not named in stock header. (d_a_nbomb reproduction.)
constexpr u32 kFlgAlbwLockoutWater    = 0x40000u;
constexpr u32 kFlgAlbwLockoutBombling = 0x80000u;

bool lockout_active() {
    return albw_meter_is_enabled() && albw_meter_is_locked();
}

daAlink_c* link_actor() {
    return static_cast<daAlink_c*>(g_dComIfG_gameInfo.play.getPlayer(0));
}

// ---- d_a_nbomb reproduction (NOT in d_albw_lockout.cpp — the bomb-actor ALBW
//      edits live in d_a_nbomb.cpp, a stock giant, so they stay as hook bodies) ----
void nbomb_set_flg(daNbomb_c* bomb, u32 bit) {
    if (bomb != nullptr) {
        bomb->mStateFlg0 |= bit;
    }
}
bool nbomb_has_flg(const daNbomb_c* bomb, u32 bit) {
    return bomb != nullptr && (bomb->mStateFlg0 & bit) != 0;
}

void mod_nbomb_coHitCallback(fopAc_ac_c* i_coActorA, dCcD_GObjInf*, fopAc_ac_c* i_coActorB,
                             dCcD_GObjInf*) {
    static_cast<daNbomb_c*>(i_coActorA)->coHitCallback(i_coActorB);
}

// Motion comes from the port (dAlbwLockout_updateBomblingOrbit); this adds the
// per-frame collision registration the bomb needs while orbiting.
bool finish_bombling_orbit_frame(daNbomb_c* bomb) {
    if (!dAlbwLockout_updateBomblingOrbit(bomb)) {
        return false;
    }
    if (bomb->mpModel != nullptr) {
        mDoMtx_stack_c::transS(bomb->current.pos);
        mDoMtx_stack_c::YrotM(bomb->shape_angle.y);
        bomb->mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
    }
    bomb->mCcSph.SetC(bomb->current.pos);
    bomb->mCcSph.SetR(bomb->scale.x * 30.0f * 1.05f);
    g_dComIfG_gameInfo.play.mCcs.Set(&bomb->mCcSph);
    return true;
}

void begin_lockout_bombling_orbit(daNbomb_c* bomb) {
    if (bomb == nullptr || !lockout_active()) {
        return;
    }
    const bool insectPlayer = bomb->checkStateFlg0(daNbomb_c::FLG0_INSECT_BOMB) &&
                              bomb->checkStateFlg0(daNbomb_c::FLG0_PLAYER_MAKE);
    const bool insectParam = fopAcM_GetParam(bomb) == dBomb_c::PRM_INSECT_BOMB_PLAYER;
    if (!insectPlayer && !insectParam) {
        return;
    }
    if (dAlbwLockout_isBomblingActive() && bomb->id == sLockoutBomblingId) {
        // already the tracked bombling — re-arm collision without advancing orbit
        nbomb_set_flg(bomb, kFlgAlbwLockoutBombling);
        bomb->mCcSph.OnCoSetBit();
        bomb->mCcSph.SetCoHitCallback(mod_nbomb_coHitCallback);
        bomb->procWaitInit();
        return;
    }
    if (!dAlbwLockout_canUseBombling()) {
        return;
    }
    bomb->offStateFlg0(daNbomb_c::FLG0_UNK_2000);
    nbomb_set_flg(bomb, kFlgAlbwLockoutBombling);
    dAlbwLockout_onBomblingDeployed(bomb);  // port: spend a use + +30% base + track id
    bomb->mCcSph.OnCoSetBit();
    bomb->mCcSph.SetCoHitCallback(mod_nbomb_coHitCallback);
    bomb->procWaitInit();
}

void begin_lockout_water_bomb(daNbomb_c* bomb) {
    if (bomb == nullptr || !lockout_active()) {
        return;
    }
    if (!bomb->checkStateFlg0(daNbomb_c::FLG0_WATER_BOMB) &&
        fopAcM_GetParam(bomb) != dBomb_c::PRM_WATER_BOMB_PLAYER) {
        return;
    }
    if (nbomb_has_flg(bomb, kFlgAlbwLockoutWater)) {
        return;
    }
    nbomb_set_flg(bomb, kFlgAlbwLockoutWater);
    bomb->onStateFlg0(daNbomb_c::FLG0_WATER_BOMB);
    bomb->scale.set(5.0f, 5.0f, 5.0f);
    if (bomb->mpModel != nullptr) {
        bomb->mpModel->setBaseScale(bomb->scale);
    }
    dFocusedArts_clearOneBankCharge();
}

void apply_lockout_water_float(daNbomb_c* bomb) {
    if (!nbomb_has_flg(bomb, kFlgAlbwLockoutWater)) {
        return;
    }
    bomb->scale.set(5.0f, 5.0f, 5.0f);
    if (bomb->mpModel != nullptr) {
        bomb->mpModel->setBaseScale(bomb->scale);
    }
    bomb->gravity = 0.0f;
    bomb->maxFallSpeed = 0.0f;
    cLib_chaseF(&bomb->speed.y, 8.0f, 0.4f);
    bomb->speedF *= 0.9f;
}

// ---- confuse shared-seam redirect (mod-specific: the fork edits each enemy; the
//      mod redirects the base search primitives instead) ----
bool s_inRedirect = false;

fopAc_ac_c* redirectTargetFor(const fopAc_ac_c* i_a, const fopAc_ac_c* i_b) {
    if (sConfuse.mFrames <= 0 && sProvokedCount == 0) {
        return nullptr;  // hot-path fast-out: nothing confused or provoked
    }
    if (s_inRedirect || i_a == nullptr || i_b == nullptr) {
        return nullptr;
    }
    if (i_b != dComIfGp_getPlayer(0)) {
        return nullptr;  // only "aim at / range to Link" queries are bent
    }
    return dAlbwLockout_getRivalTarget(const_cast<fopAc_ac_c*>(i_a));
}

// ============================================
// Hook declarations
// ============================================
DEFINE_HOOK(&daAlink_c::execute, LinkExecute);
DEFINE_HOOK(cc_at_check, CcAtCheck);
DEFINE_HOOK(&daAlink_c::hookshotAtHitCallBack, HookshotHit);
DEFINE_HOOK(&daAlink_c::procHookshotFly, HookshotFly);
DEFINE_HOOK(&daNbomb_c::create, NbombCreate);
DEFINE_HOOK(&daNbomb_c::procWait, NbombWait);
DEFINE_HOOK(&daNbomb_c::procInsectMove, NbombInsectMove);
DEFINE_HOOK(&daNbomb_c::procExplodeInit, NbombExplodeInit);
DEFINE_HOOK(&daNbomb_c::execute, NbombExecute);
DEFINE_HOOK_SYMBOL(ALBT_SYM_FASTCREATE,
                   fopAc_ac_c*(s16, u32, const cXyz*, int, const csXyz*, const cXyz*, s8, createFunc,
                               void*, u32, u8),
                   FastCreate);
DEFINE_HOOK(&daCrod_c::execute, CrodExecute);
DEFINE_HOOK(&fopAcM_searchActorAngleY, SearchAngleY);
DEFINE_HOOK(&fopAcM_searchActorAngleX, SearchAngleX);
DEFINE_HOOK(&fopAcM_searchActorDistance, SearchDistance);
DEFINE_HOOK(&fopAcM_searchActorDistanceXZ, SearchDistanceXZ);

// ---- cc_at_check: slingshot tag (pre) + attack-power boost / double-claw finisher
//      / confuse friendly-fire (post). Fork call sites: d_cc_uty.cpp. ----
bool s_block_insect_bomb_create = false;

HookAction on_cc_at_check_pre(ModContext*, void* args, void*, void*) {
    if (!lockout_active()) {
        return HOOK_CONTINUE;
    }
    auto* enemy = mods::arg<fopAc_ac_c*>(args, 0);
    auto* info = mods::arg<dCcU_AtInfo*>(args, 1);
    if (enemy == nullptr || info == nullptr || info->mpCollider == nullptr) {
        return HOOK_CONTINUE;
    }
    if (fopAcM_GetGroup(enemy) == fopAc_ENEMY_e && info->mpCollider->ChkAtType(AT_TYPE_SLINGSHOT)) {
        dAlbwLockout_onSlingshotHit(enemy);  // tags the enemy for isRangedOpened queries
        // A raw pause (the port's tag) skips the enemy's whole execute — including its
        // own damage_check — so a paused enemy can't take the follow-up damage that is
        // the whole point of "opening" it. Route through the wolf-stun seam instead:
        // it freezes the enemy mid-pose AND re-registers its hurt collider each frame
        // via the draw-phase bridge, so Link's hits still resolve through cc_at_check.
        // Same shared mechanism Midna stun uses; one seam for every enemy.
        dAlbwWolfStun_applyTimed(enemy, kLockoutSlingshotDebuffFrames);
    }
    return HOOK_CONTINUE;
}

void on_cc_at_check_post(ModContext*, void* args, void*, void*) {
    auto* enemy = mods::arg<fopAc_ac_c*>(args, 0);
    auto* info = mods::arg<dCcU_AtInfo*>(args, 1);
    if (enemy == nullptr || info == nullptr || info->mAttackPower == 0 ||
        fopAcM_GetGroup(enemy) != fopAc_ENEMY_e) {
        return;
    }
    // Confuse friendly-fire: a confused enemy that damaged another enemy provokes it.
    if (albw_meter_is_locked() && info->mpActor != nullptr &&
        fopAcM_GetGroup(info->mpActor) == fopAc_ENEMY_e && dAlbwLockout_isConfused(info->mpActor)) {
        cXyz hitPos = enemy->current.pos;
        hitPos.y += 100.0f;
        g_dComIfG_gameInfo.play.getParticle()->setHitMark(3, enemy, &hitPos, NULL, NULL, 0);
        dAlbwLockout_onConfuseFriendlyFireHit(enemy, info->mpActor);
    }
    if (!lockout_active() || info->mpCollider == nullptr) {
        return;
    }
    // Double-claw finisher: fixed ATP after all remaps.
    const u16 clawPower = dAlbwLockout_getDoubleClawSlashAttackPower();
    if (clawPower != 0) {
        info->mAttackPower = clawPower;
        return;
    }
    dAlbwLockout_applyAttackPowerBoost(info->mAttackPower, info->mpCollider->GetAtType());
}

void on_link_execute_post(ModContext*, void*, void*, void*) {
    if (!albw_meter_is_enabled()) {
        return;
    }
    dAlbwLockout_update();  // ticks slingshot + Dom-Rod confuse + provoke timers
}

// ---- hookshot double-claw finisher. Fork call sites: d_a_alink_hook.inc. ----
void on_hookshot_hit_post(ModContext*, void* args, void*, void*) {
    if (!lockout_active()) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    auto* target = mods::arg<fopAc_ac_c*>(args, 2);
    if (link == nullptr || target == nullptr || link->mEquipItem != dItemNo_W_HOOKSHOT_e) {
        return;
    }
    if (dAlbwLockout_isDoubleClawFlyActive() || dAlbwLockout_isDoubleClawSlashActive()) {
        return;
    }
    if (!dAlbwLockout_shouldForceEnemyStick(target)) {
        return;
    }
    dAlbwLockout_onDoubleClawLatch(target);  // port: freeze-hold + arm fly
    link->mHookTargetAcKeep.setData(target);
}

void on_hookshot_fly_post(ModContext*, void* args, void*, void*) {
    if (!dAlbwLockout_isDoubleClawFlyActive()) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }
    fopAc_ac_c* target = dAlbwLockout_getDoubleClawTarget();
    if (target == nullptr) {
        dAlbwLockout_onDoubleClawFlyEnded();
        return;
    }
    if (link->mItemMode != 5 && link->mItemMode != 4) {
        return;
    }
    if (link->current.pos.abs(target->current.pos) < 200.0f) {
        dAlbwLockout_onDoubleClawSlashBegin();
    }
}

// ---- nbomb: bombling orbit + water bomb (d_a_nbomb reproduction). ----
HookAction on_fast_create_pre(ModContext*, void* args, void* retval, void*) {
    if (!s_block_insect_bomb_create || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    if (mods::arg<s16>(args, 0) == fpcNm_NBOMB_e && mods::arg<u32>(args, 1) == 10) {
        *static_cast<fopAc_ac_c**>(retval) = nullptr;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void on_nbomb_create_post(ModContext*, void* args, void*, void*) {
    auto* bomb = mods::arg<daNbomb_c*>(args, 0);
    if (bomb == nullptr || !lockout_active()) {
        return;
    }
    begin_lockout_bombling_orbit(bomb);
    begin_lockout_water_bomb(bomb);
}

HookAction on_nbomb_wait_pre(ModContext*, void* args, void* retval, void*) {
    auto* bomb = mods::arg<daNbomb_c*>(args, 0);
    if (bomb == nullptr) {
        return HOOK_CONTINUE;
    }
    if (finish_bombling_orbit_frame(bomb)) {
        if (retval != nullptr) {
            *static_cast<BOOL*>(retval) = TRUE;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    apply_lockout_water_float(bomb);
    return HOOK_CONTINUE;
}

HookAction on_nbomb_insect_move_pre(ModContext*, void* args, void* retval, void*) {
    auto* bomb = mods::arg<daNbomb_c*>(args, 0);
    if (bomb == nullptr) {
        return HOOK_CONTINUE;
    }
    if (!nbomb_has_flg(bomb, kFlgAlbwLockoutBombling)) {
        if (lockout_active() && bomb->checkStateFlg0(daNbomb_c::FLG0_INSECT_BOMB) &&
            bomb->checkStateFlg0(daNbomb_c::FLG0_PLAYER_MAKE)) {
            begin_lockout_bombling_orbit(bomb);
            if (finish_bombling_orbit_frame(bomb)) {
                if (retval != nullptr) {
                    *static_cast<BOOL*>(retval) = TRUE;
                }
                return HOOK_SKIP_ORIGINAL;
            }
        }
        return HOOK_CONTINUE;
    }
    if (finish_bombling_orbit_frame(bomb)) {
        if (retval != nullptr) {
            *static_cast<BOOL*>(retval) = TRUE;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void on_nbomb_explode_init_post(ModContext*, void* args, void*, void*) {
    auto* bomb = mods::arg<daNbomb_c*>(args, 0);
    if (bomb == nullptr || !nbomb_has_flg(bomb, kFlgAlbwLockoutWater)) {
        return;
    }
    daAlink_c* player = link_actor();
    if (player == nullptr) {
        return;
    }
    bomb->scale.x = player->getBombEffScale() * 5.0f;
    bomb->scale.y = bomb->scale.x;
    bomb->scale.z = bomb->scale.x;
    bomb->mCcSph.SetAtAtp(0);
    bomb->mCcSph.SetR(player->getBombAtR() * 5.0f);
    albw_meter_restore_to_full();
}

void on_nbomb_execute_post(ModContext*, void* args, void*, void*) {
    auto* bomb = mods::arg<daNbomb_c*>(args, 0);
    if (bomb == nullptr || !nbomb_has_flg(bomb, kFlgAlbwLockoutWater)) {
        return;
    }
    bomb->scale.set(5.0f, 5.0f, 5.0f);
    if (bomb->mpModel != nullptr) {
        bomb->mpModel->setBaseScale(bomb->scale);
    }
}

// ---- Dom-Rod confuse trigger (fork d_a_crod.cpp flight-path graze). ----
void on_crod_execute_post(ModContext*, void* args, void*, void*) {
    if (!albw_meter_is_locked()) {
        return;
    }
    auto* ball = mods::arg<daCrod_c*>(args, 0);
    if (ball == nullptr || fopAcM_GetParam(ball) != 3) {
        return;
    }
    fopAc_ac_c* victim =
        dAlbwLockout_searchDomRodConfuseVictim(ball->old.pos, ball->current.pos, 140.0f);
    if (victim != nullptr) {
        dAlbwLockout_onDomRodConfuseHit(victim);
        ball->setReturn();
    }
}

// ---- confuse aim redirect (shared search-primitive seam). ----
void on_search_angle_y_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr) return;
    fopAc_ac_c* target =
        redirectTargetFor(mods::arg<fopAc_ac_c*>(args, 0), mods::arg<fopAc_ac_c*>(args, 1));
    if (target == nullptr) return;
    s_inRedirect = true;
    *static_cast<s16*>(retval) = fopAcM_searchActorAngleY(mods::arg<fopAc_ac_c*>(args, 0), target);
    s_inRedirect = false;
}
void on_search_angle_x_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr) return;
    fopAc_ac_c* target =
        redirectTargetFor(mods::arg<fopAc_ac_c*>(args, 0), mods::arg<fopAc_ac_c*>(args, 1));
    if (target == nullptr) return;
    s_inRedirect = true;
    *static_cast<s16*>(retval) = fopAcM_searchActorAngleX(mods::arg<fopAc_ac_c*>(args, 0), target);
    s_inRedirect = false;
}
void on_search_distance_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr) return;
    fopAc_ac_c* target =
        redirectTargetFor(mods::arg<fopAc_ac_c*>(args, 0), mods::arg<fopAc_ac_c*>(args, 1));
    if (target == nullptr) return;
    s_inRedirect = true;
    *static_cast<f32*>(retval) = fopAcM_searchActorDistance(mods::arg<fopAc_ac_c*>(args, 0), target);
    s_inRedirect = false;
}
void on_search_distance_xz_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr) return;
    fopAc_ac_c* target =
        redirectTargetFor(mods::arg<fopAc_ac_c*>(args, 0), mods::arg<fopAc_ac_c*>(args, 1));
    if (target == nullptr) return;
    s_inRedirect = true;
    *static_cast<f32*>(retval) =
        fopAcM_searchActorDistanceXZ(mods::arg<fopAc_ac_c*>(args, 0), target);
    s_inRedirect = false;
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

// ============================================
// Public interface bridges (callers use the mod-named albw_lockout_* / the confuse
// queries). These forward to the ported fork logic.
// ============================================
void albw_lockout_on_begin() { dAlbwLockout_onBegin(); }
void albw_lockout_on_end() { dAlbwLockout_onEnd(); }
bool albw_lockout_can_fire_bow() { return dAlbwLockout_canFireBow(); }
bool albw_lockout_can_fire_bomb_arrow() { return dAlbwLockout_canFireBombArrow(); }
bool albw_lockout_can_use_bombling() { return dAlbwLockout_canUseBombling(); }
bool albw_lockout_can_use_double_hookshot() { return dAlbwLockout_canUseDoubleHookshot(); }
void albw_lockout_on_arrow_fired() { dAlbwLockout_onArrowFired(); }
void albw_lockout_on_bomb_arrow_fired() { dAlbwLockout_onBombArrowFired(); }
void albw_lockout_on_hookshot_fired() { dAlbwLockout_onHookshotFired(); }
void albw_lockout_on_bombling_deployed(fopAc_ac_c* bombling) {
    dAlbwLockout_onBomblingDeployed(bombling);
}
bool albw_lockout_is_bombling_active() { return dAlbwLockout_isBomblingActive(); }
void albw_lockout_set_block_insect_bomb_create(bool block) { s_block_insect_bomb_create = block; }

// Confuse queries (kept for other TUs that read confuse state).
bool albw_confuse_is_confused(const fopAc_ac_c* enemy) {
    return dAlbwLockout_isConfused(const_cast<fopAc_ac_c*>(enemy));
}
fopAc_ac_c* albw_confuse_get_target(const fopAc_ac_c* attacker) {
    return dAlbwLockout_getConfuseTarget(const_cast<fopAc_ac_c*>(attacker));
}

ModResult albw_lockout_init(ModError* error) {
    if (!install(error, "CcAtCheckPre", mods::hook_add_pre<CcAtCheck>(svc_hook, on_cc_at_check_pre)) ||
        !install(error, "CcAtCheckPost", mods::hook_add_post<CcAtCheck>(svc_hook, on_cc_at_check_post)) ||
        !install(error, "LinkExecute", mods::hook_add_post<LinkExecute>(svc_hook, on_link_execute_post)) ||
        !install(error, "HookshotHit", mods::hook_add_post<HookshotHit>(svc_hook, on_hookshot_hit_post)) ||
        !install(error, "HookshotFly", mods::hook_add_post<HookshotFly>(svc_hook, on_hookshot_fly_post)) ||
        !install(error, "FastCreatePre", mods::hook_add_pre<FastCreate>(svc_hook, on_fast_create_pre)) ||
        !install(error, "NbombCreatePost", mods::hook_add_post<NbombCreate>(svc_hook, on_nbomb_create_post)) ||
        !install(error, "NbombWaitPre", mods::hook_add_pre<NbombWait>(svc_hook, on_nbomb_wait_pre)) ||
        !install(error, "NbombInsectMovePre", mods::hook_add_pre<NbombInsectMove>(svc_hook, on_nbomb_insect_move_pre)) ||
        !install(error, "NbombExplodeInitPost", mods::hook_add_post<NbombExplodeInit>(svc_hook, on_nbomb_explode_init_post)) ||
        !install(error, "NbombExecutePost", mods::hook_add_post<NbombExecute>(svc_hook, on_nbomb_execute_post)) ||
        !install(error, "CrodExecuteConfuse", mods::hook_add_post<CrodExecute>(svc_hook, on_crod_execute_post)) ||
        !install(error, "ConfuseSearchAngleY", mods::hook_add_post<SearchAngleY>(svc_hook, on_search_angle_y_post)) ||
        !install(error, "ConfuseSearchAngleX", mods::hook_add_post<SearchAngleX>(svc_hook, on_search_angle_x_post)) ||
        !install(error, "ConfuseSearchDistance", mods::hook_add_post<SearchDistance>(svc_hook, on_search_distance_post)) ||
        !install(error, "ConfuseSearchDistanceXZ", mods::hook_add_post<SearchDistanceXZ>(svc_hook, on_search_distance_xz_post))) {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw lockout + confuse (fork-ported logic) ready");
    return MOD_OK;
}

ModResult albw_lockout_shutdown(ModError*) {
    return MOD_OK;
}

#endif  // TARGET_PC
