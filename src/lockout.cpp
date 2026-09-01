// ALBW meter lockout perks — ported from d_albw_lockout.cpp (fork donor).
// Dom Rod confuse AI deferred (requires per-enemy actor hooks on stock host).

#include "global.h"
#include "albw_symbols.h"

#include "SSystem/SComponent/c_cc_d.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "m_Do/m_Do_mtx.h"

#define private public
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_nbomb.h"
#undef private

#include "albw_common.h"
#include "focused_arts.h"
#include "lockout.h"
#include "meter_bridge.h"
#include "mods/hook.hpp"

#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "d/d_bomb.h"

#include <algorithm>

namespace {

constexpr u8 kBowShotsMax = 3;
constexpr u8 kBombArrowShotsMax = 2;
constexpr u8 kBomblingUsesMax = 2;
constexpr int kSlingshotDebuffFrames = 120;
constexpr int kTaggedMax = 16;
constexpr f32 kBomblingOrbitRadius = 200.0f;
constexpr s16 kBomblingOrbitStep = 0x433;
constexpr u16 kDoubleClawSlashPower = 30;
// Fork daNbomb_FLG0 bits — not named in stock header.
constexpr u32 kFlgAlbwLockoutWater = 0x40000u;
constexpr u32 kFlgAlbwLockoutBombling = 0x80000u;

struct TaggedEnemy {
    fpc_ProcID mId;
    s16 mFrames;
    bool mPauseActive;
};

u8 g_bow_shots = kBowShotsMax;
u8 g_bomb_arrow_shots = kBombArrowShotsMax;
u8 g_bombling_uses = kBomblingUsesMax;

fpc_ProcID g_bombling_id = fpcM_ERROR_PROCESS_ID_e;
s16 g_bombling_orbit_angle = 0;
bool g_block_insect_bomb_create = false;

bool g_double_claw_fly = false;
bool g_double_claw_slash = false;
fpc_ProcID g_double_claw_target_id = fpcM_ERROR_PROCESS_ID_e;

TaggedEnemy g_tagged[kTaggedMax];
int g_tagged_count = 0;

bool lockout_active() {
    return albw_meter_is_enabled() && albw_meter_is_locked();
}

daAlink_c* link_actor() {
    return static_cast<daAlink_c*>(g_dComIfG_gameInfo.play.getPlayer(0));
}

void on_nbomb_flg(daNbomb_c* bomb, u32 bit) {
    if (bomb != nullptr) {
        bomb->mStateFlg0 |= bit;
    }
}

bool nbomb_has_flg(const daNbomb_c* bomb, u32 bit) {
    return bomb != nullptr && (bomb->mStateFlg0 & bit) != 0;
}

TaggedEnemy* find_tagged(fpc_ProcID id) {
    for (int i = 0; i < g_tagged_count; ++i) {
        if (g_tagged[i].mId == id) {
            return &g_tagged[i];
        }
    }
    return nullptr;
}

void clear_tagged_at(int index) {
    TaggedEnemy& entry = g_tagged[index];
    if (entry.mPauseActive) {
        base_process_class* proc = fpcM_SearchByID(entry.mId);
        if (proc != nullptr) {
            fpcM_PauseDisable(proc, 1);
        }
    }
    g_tagged_count--;
    for (int i = index; i < g_tagged_count; ++i) {
        g_tagged[i] = g_tagged[i + 1];
    }
}

void clear_all_tagged() {
    while (g_tagged_count > 0) {
        clear_tagged_at(0);
    }
}

void enemy_hold(fopAc_ac_c* enemy) {
    if (enemy != nullptr) {
        fpcM_PauseEnable(enemy, 1);
    }
}

void enemy_thaw(fopAc_ac_c* enemy) {
    if (enemy == nullptr) {
        return;
    }
    TaggedEnemy* entry = find_tagged(enemy->id);
    if (entry != nullptr && entry->mPauseActive) {
        base_process_class* proc = fpcM_SearchByID(enemy->id);
        if (proc != nullptr) {
            fpcM_PauseDisable(proc, 1);
        }
        entry->mPauseActive = false;
    }
    base_process_class* proc = fpcM_SearchByID(enemy->id);
    if (proc != nullptr) {
        fpcM_PauseDisable(proc, 1);
    }
}

void clear_double_claw(bool thaw) {
    if (thaw && g_double_claw_target_id != fpcM_ERROR_PROCESS_ID_e) {
        enemy_thaw(fopAcM_SearchByID(g_double_claw_target_id));
    }
    g_double_claw_fly = false;
    g_double_claw_slash = false;
    g_double_claw_target_id = fpcM_ERROR_PROCESS_ID_e;
}

bool is_double_claw_equip() {
    daAlink_c* link = link_actor();
    return link != nullptr && link->mEquipItem == dItemNo_W_HOOKSHOT_e;
}

void tag_slingshot_enemy(fopAc_ac_c* enemy) {
    if (!lockout_active() || enemy == nullptr || fopAcM_GetGroup(enemy) != fopAc_ENEMY_e) {
        return;
    }

    TaggedEnemy* existing = find_tagged(enemy->id);
    if (existing != nullptr) {
        existing->mFrames = static_cast<s16>(kSlingshotDebuffFrames);
        return;
    }
    if (g_tagged_count >= kTaggedMax) {
        return;
    }

    TaggedEnemy& entry = g_tagged[g_tagged_count++];
    entry.mId = enemy->id;
    entry.mFrames = static_cast<s16>(kSlingshotDebuffFrames);
    entry.mPauseActive = false;
    fpcM_PauseEnable(enemy, 1);
    entry.mPauseActive = true;
}

void apply_attack_power_boost(u16& power, u32 at_type) {
    if (power == 0 || !lockout_active()) {
        return;
    }
    if (at_type == AT_TYPE_ARROW || at_type == AT_TYPE_BOMB || at_type == AT_TYPE_SPINNER) {
        power = static_cast<u16>((power * 3) / 2);
    } else if (at_type == AT_TYPE_IRON_BALL) {
        power = static_cast<u16>(std::min<u32>((static_cast<u32>(power) * 5) / 2, 0xFFFFu));
    }
}

bool update_bombling_orbit(daNbomb_c* bomb) {
    if (bomb == nullptr || !lockout_active()) {
        return false;
    }
    if (!nbomb_has_flg(bomb, kFlgAlbwLockoutBombling) && bomb->id != g_bombling_id) {
        return false;
    }
    if (g_bombling_id != fpcM_ERROR_PROCESS_ID_e && bomb->id != g_bombling_id) {
        return false;
    }

    daAlink_c* player = link_actor();
    if (player == nullptr) {
        return true;
    }

    g_bombling_orbit_angle += kBomblingOrbitStep;
    const f32 radius = kBomblingOrbitRadius;
    bomb->current.pos.x = player->current.pos.x + radius * cM_ssin(g_bombling_orbit_angle);
    bomb->current.pos.z = player->current.pos.z + radius * cM_scos(g_bombling_orbit_angle);
    bomb->current.pos.y = player->current.pos.y + 40.0f;
    bomb->old.pos = bomb->current.pos;
    bomb->speedF = 0.0f;
    bomb->speed = cXyz::Zero;
    bomb->gravity = 0.0f;
    bomb->shape_angle.y = static_cast<s16>(g_bombling_orbit_angle + 0x4000);
    bomb->current.angle.y = bomb->shape_angle.y;
    return true;
}

bool finish_bombling_orbit_frame(daNbomb_c* bomb) {
    if (!update_bombling_orbit(bomb)) {
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

static void mod_nbomb_coHitCallback(fopAc_ac_c* i_coActorA, dCcD_GObjInf* i_coObjInfA,
                                    fopAc_ac_c* i_coActorB, dCcD_GObjInf* i_coObjInfB) {
    (void)i_coObjInfA;
    (void)i_coObjInfB;
    static_cast<daNbomb_c*>(i_coActorA)->coHitCallback(i_coActorB);
}

void begin_lockout_bombling_orbit(daNbomb_c* bomb) {
    if (bomb == nullptr || !lockout_active()) {
        return;
    }
    const bool insectPlayer =
        bomb->checkStateFlg0(daNbomb_c::FLG0_INSECT_BOMB) &&
        bomb->checkStateFlg0(daNbomb_c::FLG0_PLAYER_MAKE);
    const bool insectParam = fopAcM_GetParam(bomb) == dBomb_c::PRM_INSECT_BOMB_PLAYER;
    if (!insectPlayer && !insectParam) {
        return;
    }
    // Already tracking this actor as the lockout bombling.
    if (g_bombling_id != fpcM_ERROR_PROCESS_ID_e && bomb->id == g_bombling_id) {
        on_nbomb_flg(bomb, kFlgAlbwLockoutBombling);
        bomb->mCcSph.OnCoSetBit();
        bomb->mCcSph.SetCoHitCallback(mod_nbomb_coHitCallback);
        bomb->procWaitInit();
        return;
    }
    if (!albw_lockout_can_use_bombling()) {
        return;
    }

    // Stock create may have already entered insect-crawl or explode — force wait orbit.
    bomb->offStateFlg0(daNbomb_c::FLG0_UNK_2000);
    on_nbomb_flg(bomb, kFlgAlbwLockoutBombling);
    albw_lockout_on_bombling_deployed(bomb);
    bomb->mCcSph.OnCoSetBit();
    bomb->mCcSph.SetCoHitCallback(mod_nbomb_coHitCallback);
    bomb->procWaitInit();
}

void begin_lockout_water_bomb(daNbomb_c* bomb) {
    if (bomb == nullptr || !lockout_active()) {
        return;
    }
    if (!bomb->checkStateFlg0(daNbomb_c::FLG0_WATER_BOMB) &&
        fopAcM_GetParam(bomb) != dBomb_c::PRM_WATER_BOMB_PLAYER)
    {
        return;
    }
    if (nbomb_has_flg(bomb, kFlgAlbwLockoutWater)) {
        return;
    }

    on_nbomb_flg(bomb, kFlgAlbwLockoutWater);
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

void lockout_update() {
    if (g_tagged_count > 0) {
        int write = 0;
        for (int i = 0; i < g_tagged_count; ++i) {
            g_tagged[i].mFrames--;
            if (g_tagged[i].mFrames > 0) {
                g_tagged[write++] = g_tagged[i];
            } else if (g_tagged[i].mPauseActive) {
                base_process_class* proc = fpcM_SearchByID(g_tagged[i].mId);
                if (proc != nullptr) {
                    fpcM_PauseDisable(proc, 1);
                }
            }
        }
        g_tagged_count = write;
    }

    if (g_bombling_id != fpcM_ERROR_PROCESS_ID_e && fopAcM_SearchByID(g_bombling_id) == nullptr) {
        g_bombling_id = fpcM_ERROR_PROCESS_ID_e;
    }

    if (g_double_claw_slash) {
        g_double_claw_slash = false;
        clear_double_claw(true);
    }
}

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

HookAction on_cc_at_check_pre(ModContext*, void* args, void*, void*) {
    if (!lockout_active()) {
        return HOOK_CONTINUE;
    }
    auto* enemy = mods::arg<fopAc_ac_c*>(args, 0);
    auto* info = mods::arg<dCcU_AtInfo*>(args, 1);
    if (enemy == nullptr || info == nullptr || info->mpCollider == nullptr) {
        return HOOK_CONTINUE;
    }
    if (fopAcM_GetGroup(enemy) == fopAc_ENEMY_e &&
        info->mpCollider->ChkAtType(AT_TYPE_SLINGSHOT))
    {
        tag_slingshot_enemy(enemy);
    }
    return HOOK_CONTINUE;
}

void on_cc_at_check_post(ModContext*, void* args, void*, void*) {
    if (!lockout_active()) {
        return;
    }
    auto* enemy = mods::arg<fopAc_ac_c*>(args, 0);
    auto* info = mods::arg<dCcU_AtInfo*>(args, 1);
    if (enemy == nullptr || info == nullptr || info->mpCollider == nullptr ||
        info->mAttackPower == 0 || fopAcM_GetGroup(enemy) != fopAc_ENEMY_e)
    {
        return;
    }

    if (g_double_claw_slash) {
        info->mAttackPower = kDoubleClawSlashPower;
        return;
    }

    apply_attack_power_boost(info->mAttackPower, info->mpCollider->GetAtType());
}

void on_link_execute_post(ModContext*, void*, void*, void*) {
    if (!albw_meter_is_enabled()) {
        return;
    }
    lockout_update();
}

void on_hookshot_hit_post(ModContext*, void* args, void*, void*) {
    if (!lockout_active() || !is_double_claw_equip()) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    auto* target = mods::arg<fopAc_ac_c*>(args, 2);
    if (link == nullptr || target == nullptr || g_double_claw_fly || g_double_claw_slash) {
        return;
    }
    if (fopAcM_GetGroup(target) != fopAc_ENEMY_e) {
        return;
    }

    clear_double_claw(true);
    g_double_claw_target_id = target->id;
    g_double_claw_fly = true;
    enemy_hold(target);
    link->mHookTargetAcKeep.setData(target);
}

void on_hookshot_fly_post(ModContext*, void* args, void*, void*) {
    if (!g_double_claw_fly) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }
    fopAc_ac_c* target = fopAcM_SearchByID(g_double_claw_target_id);
    if (target == nullptr) {
        clear_double_claw(false);
        return;
    }
    if (link->mItemMode != 5 && link->mItemMode != 4) {
        return;
    }
    if (link->current.pos.abs(target->current.pos) < 200.0f) {
        g_double_claw_fly = false;
        g_double_claw_slash = true;
    }
}

HookAction on_fast_create_pre(ModContext*, void* args, void* retval, void*) {
    if (!g_block_insect_bomb_create || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    const s16 procName = mods::arg<s16>(args, 0);
    const u32 parameters = mods::arg<u32>(args, 1);
    if (procName == fpcNm_NBOMB_e && parameters == 10) {
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
    // Fork create() PRM_INSECT_BOMB_PLAYER / PRM_WATER_BOMB_PLAYER lockout branches.
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

    // Water float runs inside stock wait after our pre — patch gravity/scale then continue.
    apply_lockout_water_float(bomb);
    return HOOK_CONTINUE;
}

HookAction on_nbomb_insect_move_pre(ModContext*, void* args, void* retval, void*) {
    auto* bomb = mods::arg<daNbomb_c*>(args, 0);
    if (bomb == nullptr || !nbomb_has_flg(bomb, kFlgAlbwLockoutBombling)) {
        // Stock create may have entered insect crawl before our create-post ran on a
        // prior frame; if lockout + insect player, hijack into orbit.
        if (bomb != nullptr && lockout_active() &&
            bomb->checkStateFlg0(daNbomb_c::FLG0_INSECT_BOMB) &&
            bomb->checkStateFlg0(daNbomb_c::FLG0_PLAYER_MAKE) &&
            (g_bombling_id == bomb->id || g_bombling_id == fpcM_ERROR_PROCESS_ID_e))
        {
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
    // Giant harmless blast that fully restores the ALBW meter (fork procExplodeInit).
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

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace

void albw_lockout_on_begin() {
    g_bow_shots = kBowShotsMax;
    g_bomb_arrow_shots = kBombArrowShotsMax;
    g_bombling_uses = kBomblingUsesMax;
    g_bombling_id = fpcM_ERROR_PROCESS_ID_e;
    g_bombling_orbit_angle = 0;
    clear_double_claw(true);
    clear_all_tagged();
}

void albw_lockout_on_end() {
    albw_lockout_on_begin();
}

bool albw_lockout_can_fire_bow() {
    return lockout_active() && g_bow_shots > 0;
}

bool albw_lockout_can_fire_bomb_arrow() {
    return lockout_active() && g_bomb_arrow_shots > 0;
}

bool albw_lockout_can_use_bombling() {
    if (!lockout_active()) {
        return true;
    }
    if (g_bombling_uses == 0) {
        return false;
    }
    if (g_bombling_id != fpcM_ERROR_PROCESS_ID_e && fopAcM_SearchByID(g_bombling_id) != nullptr) {
        return false;
    }
    return true;
}

bool albw_lockout_can_use_double_hookshot() {
    return lockout_active();
}

void albw_lockout_on_arrow_fired() {
    if (!lockout_active() || g_bow_shots == 0) {
        return;
    }
    g_bow_shots--;
}

void albw_lockout_on_bomb_arrow_fired() {
    if (!lockout_active() || g_bomb_arrow_shots == 0) {
        return;
    }
    g_bomb_arrow_shots--;
}

void albw_lockout_on_hookshot_fired() {
    if (!lockout_active()) {
        return;
    }
    albw_meter_drain_to_lockout();
}

void albw_lockout_set_block_insect_bomb_create(bool block) {
    g_block_insect_bomb_create = block;
}

void albw_lockout_on_bombling_deployed(fopAc_ac_c* bombling) {
    if (!lockout_active() || bombling == nullptr || g_bombling_uses == 0) {
        return;
    }
    g_bombling_uses--;
    g_bombling_id = bombling->id;
    g_bombling_orbit_angle = 0;
    albw_meter_add_base_fraction(3, 10);
}

bool albw_lockout_is_bombling_active() {
    if (g_bombling_id == fpcM_ERROR_PROCESS_ID_e || !lockout_active()) {
        return false;
    }
    return fopAcM_SearchByID(g_bombling_id) != nullptr;
}

ModResult albw_lockout_init(ModError* error) {
    if (!install(error, "CcAtCheckPre", mods::hook_add_pre<CcAtCheck>(svc_hook, on_cc_at_check_pre)) ||
        !install(error, "CcAtCheckPost",
                 mods::hook_add_post<CcAtCheck>(svc_hook, on_cc_at_check_post)) ||
        !install(error, "LinkExecute",
                 mods::hook_add_post<LinkExecute>(svc_hook, on_link_execute_post)) ||
        !install(error, "HookshotHit",
                 mods::hook_add_post<HookshotHit>(svc_hook, on_hookshot_hit_post)) ||
        !install(error, "HookshotFly",
                 mods::hook_add_post<HookshotFly>(svc_hook, on_hookshot_fly_post)) ||
        !install(error, "FastCreatePre",
                 mods::hook_add_pre<FastCreate>(svc_hook, on_fast_create_pre)) ||
        !install(error, "NbombCreatePost",
                 mods::hook_add_post<NbombCreate>(svc_hook, on_nbomb_create_post)) ||
        !install(error, "NbombWaitPre",
                 mods::hook_add_pre<NbombWait>(svc_hook, on_nbomb_wait_pre)) ||
        !install(error, "NbombInsectMovePre",
                 mods::hook_add_pre<NbombInsectMove>(svc_hook, on_nbomb_insect_move_pre)) ||
        !install(error, "NbombExplodeInitPost",
                 mods::hook_add_post<NbombExplodeInit>(svc_hook, on_nbomb_explode_init_post)) ||
        !install(error, "NbombExecutePost",
                 mods::hook_add_post<NbombExecute>(svc_hook, on_nbomb_execute_post)))
    {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw lockout hooks ready");
    return MOD_OK;
}

ModResult albw_lockout_shutdown(ModError*) {
    return MOD_OK;
}
