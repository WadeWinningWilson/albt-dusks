// Soul of Light — part of dev.albt.albw (owns wallet half on stock Dusklight).

#include "global.h"

#include "albw_common.h"
#include "config_vars.h"
#include "modules.h"

#include "SSystem/SComponent/c_m3d.h"
#include "SSystem/SComponent/c_phase.h"
#include "Z2AudioLib/Z2AudioMgr.h"
#include "Z2AudioLib/Z2Instances.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_obj_drop.h"
#include "d/actor/d_a_player.h"
#include "d/d_bg_s.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "mods/svc/hook.hpp"

#include <cstring>

// ============================================
// NEW CODE - ALBT multiplatform
// The TARGET_PC header overload carries two IF_DUSK_ARG extras (itemGiveTag /
// itemOriginalNo), but every published SDK link stub exports the 9-arg stock
// symbol. Bind that one under a private name so the extras never enter the call:
// MSVC aliases it at link time, Itanium targets take the mangled label directly.
// clang uses an asm label VERBATIM and does not apply the platform symbol
// prefix, so Mach-O needs the extra leading underscore spelled out - ELF and
// Mach-O cannot share one string.
//
// The label is not hand-written: it was read back from clang for this exact
// signature and matched against the published stub's symbol table. Do not copy
// the variant in tools/mods/soul-of-light - that one says _Z16 for
// a 17-character name and spells out PK4cXyz/Pv where Itanium requires the S1_
// and S5_ back-references, so it binds nothing off Windows.
//
// Declared at file scope on purpose: /alternatename below encodes a global
// function (@@YA...), which is not what MSVC would emit inside a namespace.
// ============================================
// Mach-O prefixes every symbol with '_'; ELF does not.
#if defined(__APPLE__)
#define ALBT_FASTCREATE_STOCK_LABEL "__Z17fopAcM_fastCreatesjPK4cXyziPK5csXyzS1_aPFiPvES5_"
#else
#define ALBT_FASTCREATE_STOCK_LABEL "_Z17fopAcM_fastCreatesjPK4cXyziPK5csXyzS1_aPFiPvES5_"
#endif

#if defined(_MSC_VER)
fopAc_ac_c* fopAcM_fastCreate_stock(s16 i_procName, u32 i_parameters, const cXyz* i_pos,
                                    int i_roomNo, const csXyz* i_angle, const cXyz* i_scale,
                                    s8 i_argument, createFunc i_createFunc, void* i_createFuncData);
#pragma comment(linker,     "/alternatename:?fopAcM_fastCreate_stock@@YAPEAVfopAc_ac_c@@FIPEBUcXyz@@HPEBVcsXyz@@0CP6AHPEAX@Z2@Z=?fopAcM_fastCreate@@YAPEAVfopAc_ac_c@@FIPEBUcXyz@@HPEBVcsXyz@@0CP6AHPEAX@Z2@Z")
#else
extern "C++" fopAc_ac_c* fopAcM_fastCreate_stock(s16 i_procName, u32 i_parameters,
                                                 const cXyz* i_pos, int i_roomNo,
                                                 const csXyz* i_angle, const cXyz* i_scale,
                                                 s8 i_argument, createFunc i_createFunc,
                                                 void* i_createFuncData)
    asm(ALBT_FASTCREATE_STOCK_LABEL);
#endif
// ============================================
// NEW CODE ENDS HERE
// ============================================

namespace {

fopAc_ac_c* fast_create_drop(s16 procName, u32 parameters, const cXyz* pos, int roomNo,
                             const csXyz* angle, const cXyz* scale, s8 argument) {
    return fopAcM_fastCreate_stock(procName, parameters, pos, roomNo, angle, scale, argument,
                                   nullptr, nullptr);
}

u16 sOrbRecovery = 0;
bool sOrbPending = false;
bool sOrbSpawned = false;
bool sOrbCreateGate = false;
bool sOrbDropCreatePending = false;
int sOrbMissingFrames = 0;
int sOrbSpawnCooldown = 0;
fpc_ProcID sOrbActorId = fpcM_ERROR_PROCESS_ID_e;
char sOrbStage[32] = {};
int sOrbRoom = -1;
cXyz sOrbPos = {0.0f, 0.0f, 0.0f};
f32 sOrbRefY = 0.0f;
bool sOrbSnapshotValid = false;
bool sTearRenderFlagWasSet[3] = {};

static constexpr u32 kRecoveryDropParams = 0x0000FF00u;
static constexpr f32 kOrbFloatAboveGround = 135.0f;
static constexpr f32 kVoidPosY = -500.0f;
static constexpr f32 kMaxGroundBelowRef = 120.0f;
static constexpr f32 kFeetBelowEyes = 90.0f;

DEFINE_HOOK(&daAlink_c::procCoDeadInit, DeadInit);
DEFINE_HOOK(&daObjDrop_c::create, DropCreate);
DEFINE_HOOK(&daObjDrop_c::dropGet, DropGet);
DEFINE_HOOK(&daObjDrop_c::checkGetArea, CheckGetArea);
DEFINE_HOOK(&daObjDrop_c::checkCompleteDemo, CheckCompleteDemo);
DEFINE_HOOK(&daObjDrop_c::execute, DropExecute);
DEFINE_HOOK(&dSv_memBit_c::isTbox, IsTbox);
DEFINE_HOOK_SYMBOL("fopAc_Execute", int(void*), AcExecute);

bool orbEnabled() {
    return albw_cfg_bool(g_recovery_orb, true);
}

u16 getWallet() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupee();
}

void setWallet(u16 rupees) {
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setRupee(rupees);
}

u16 getWalletMax() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupeeMax();
}

daAlink_c* linkActor() {
    return static_cast<daAlink_c*>(g_dComIfG_gameInfo.play.getPlayerPtr(LINK_PTR));
}

daPy_py_c* playerActor() {
    return static_cast<daPy_py_c*>(g_dComIfG_gameInfo.play.getPlayer(0));
}

const char* startStageName() {
    return g_dComIfG_gameInfo.play.getStartStageName();
}

int stayRoomNo() {
    return dStage_roomControl_c::getStayNo();
}

bool isValidGroundY(f32 groundY) {
    return groundY > -G_CM3D_F_INF + 1.0f;
}

f32 resolveReferenceY(const daAlink_c* link) {
    f32 refY = link->current.pos.y;

    if (link->mLinkAcch.ChkGroundHit()) {
        const f32 groundH = link->mLinkAcch.GetGroundH();
        if (isValidGroundY(groundH)) {
            return groundH;
        }
    }

    if (refY >= kVoidPosY) {
        return refY;
    }
    if (link->eyePos.y >= kVoidPosY) {
        return link->eyePos.y - kFeetBelowEyes;
    }
    if (link->old.pos.y >= kVoidPosY) {
        return link->old.pos.y;
    }
    if (link->field_0x33c8 >= kVoidPosY) {
        return link->field_0x33c8;
    }
    return refY;
}

f32 resolveOrbFloorY(f32 x, f32 z, f32 refY) {
    cXyz probe(x, refY, z);
    dBgS_GndChk gnd;
    gnd.SetPos(&probe);
    const f32 groundY = g_dComIfG_gameInfo.play.mBgs.GroundCross(&gnd);
    if (!isValidGroundY(groundY)) {
        return refY;
    }
    if (groundY < refY - kMaxGroundBelowRef) {
        return refY;
    }
    return groundY;
}

void pushTearRenderFlags() {
    dSv_light_drop_c& drops = g_dComIfG_gameInfo.info.getPlayer().getLightDrop();
    for (int i = 0; i < 3; ++i) {
        sTearRenderFlagWasSet[i] = drops.isLightDropGetFlag(static_cast<u8>(i)) != FALSE;
        drops.onLightDropGetFlag(static_cast<u8>(i));
    }
}

void popTearRenderFlags() {
    dSv_light_drop_c& drops = g_dComIfG_gameInfo.info.getPlayer().getLightDrop();
    for (int i = 0; i < 3; ++i) {
        if (!sTearRenderFlagWasSet[i]) {
            drops.offLightDropGetFlag(static_cast<u8>(i));
        }
        sTearRenderFlagWasSet[i] = false;
    }
}

void destroySpawnedOrbActor() {
    if (sOrbActorId == fpcM_ERROR_PROCESS_ID_e) {
        return;
    }
    if (fopAc_ac_c* actor = fopAcM_SearchByID(sOrbActorId)) {
        fopAcM_delete(actor);
    }
    sOrbActorId = fpcM_ERROR_PROCESS_ID_e;
}

void resetOrbState(bool deleteActor) {
    if (deleteActor) {
        destroySpawnedOrbActor();
    } else {
        sOrbActorId = fpcM_ERROR_PROCESS_ID_e;
    }
    popTearRenderFlags();
    sOrbPending = false;
    sOrbSpawned = false;
    sOrbRecovery = 0;
    sOrbDropCreatePending = false;
    sOrbMissingFrames = 0;
    sOrbSpawnCooldown = 0;
    sOrbStage[0] = '\0';
    sOrbRoom = -1;
}

void clearLinkTearCollectEffect() {
    if (daPy_py_c* py = playerActor()) {
        py->offNoResetFlg3(daPy_py_c::FLG3_UNK_200000);
    }
    if (daAlink_c* alink = linkActor()) {
        alink->field_0x346c = -1.0f;
    }
}

bool isRecoveryOrbDrop(const fopAc_ac_c* dropActor) {
    if (!dropActor || sOrbRecovery == 0) {
        return false;
    }
    return fopAcM_GetID(dropActor) == sOrbActorId;
}

bool allowOrbDropCreate() {
    if (!orbEnabled()) {
        return false;
    }
    return sOrbCreateGate || sOrbDropCreatePending;
}

void finishRecoveryDropSetup(daObjDrop_c* drop) {
    if (!drop || sOrbActorId != fopAcM_GetID(drop)) {
        return;
    }

    const f32 floorY = resolveOrbFloorY(sOrbPos.x, sOrbPos.z, sOrbRefY);
    cXyz pos(sOrbPos.x, floorY + kOrbFloatAboveGround, sOrbPos.z);

    drop->current.pos = pos;
    drop->home.pos = pos;
    drop->home.pos.y += 75.0f;
    drop->speedF = 0.0f;
    drop->setMode(daObjDrop_c::MODE_WAIT_e);
    drop->mModeAction = 2;
    drop->mModeTimer = 0;

    drop->removeBodyEffect();
    drop->createBodyEffect();
    drop->mSound.startSound(Z2SE_OBJ_LIGHTDROP_APPEAR, 0, -1);
}

void onOrbDropCreateAborted() {
    if (!sOrbDropCreatePending) {
        return;
    }
    sOrbDropCreatePending = false;
    sOrbSpawned = false;
    sOrbActorId = fpcM_ERROR_PROCESS_ID_e;
    sOrbSpawnCooldown = 60;
}

bool grantOrbRecovery() {
    if (sOrbRecovery == 0) {
        return false;
    }

    const u16 wallet = getWallet();
    const u16 max = getWalletMax();
    u16 next = wallet + sOrbRecovery;
    if (next > max) {
        next = max;
    }
    setWallet(next);

    if (Z2AudioMgr* audio = Z2GetAudioMgr()) {
        audio->seStart(Z2SE_SY_LIGHT_DROP_GET, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }
    clearLinkTearCollectEffect();
    resetOrbState(false);
    return true;
}

void trySpawnOrbInRoom(const char* stageName, int roomNo) {
    if (!orbEnabled() || !sOrbPending || sOrbRecovery == 0 || !stageName) {
        return;
    }
    if (roomNo != sOrbRoom || strcmp(stageName, sOrbStage) != 0) {
        return;
    }

    if (sOrbSpawnCooldown > 0) {
        sOrbSpawnCooldown--;
        return;
    }

    if (sOrbSpawned) {
        if (sOrbDropCreatePending) {
            return;
        }
        if (sOrbActorId != fpcM_ERROR_PROCESS_ID_e && fopAcM_SearchByID(sOrbActorId)) {
            sOrbMissingFrames = 0;
            return;
        }
        if (++sOrbMissingFrames < 30) {
            return;
        }
        sOrbMissingFrames = 0;
        sOrbSpawned = false;
        sOrbActorId = fpcM_ERROR_PROCESS_ID_e;
    }

    static const csXyz kAngle(0, 0, 0);
    static const cXyz kScale(1.35f, 1.35f, 1.35f);

    pushTearRenderFlags();
    sOrbDropCreatePending = true;
    sOrbCreateGate = true;
    fopAc_ac_c* actor = fast_create_drop(fpcNm_Obj_Drop_e, kRecoveryDropParams, &sOrbPos, roomNo,
                                         &kAngle, &kScale, -1);
    sOrbCreateGate = false;
    if (!actor) {
        sOrbDropCreatePending = false;
        sOrbSpawnCooldown = 30;
        svc_log->warn(mod_ctx, "soul of light: tear create failed");
        return;
    }

    sOrbActorId = fopAcM_GetID(actor);
    sOrbSpawned = true;
}

void tickSpawn() {
    if (!orbEnabled() || !sOrbPending || sOrbRecovery == 0) {
        return;
    }
    const char* stage = startStageName();
    if (!stage || stage[0] == '\0') {
        return;
    }
    trySpawnOrbInRoom(stage, stayRoomNo());
}

HookAction on_dead_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == NULL) {
        sOrbSnapshotValid = false;
        return HOOK_CONTINUE;
    }
    sOrbPos.x = link->current.pos.x;
    sOrbPos.z = link->current.pos.z;
    sOrbRefY = resolveReferenceY(link);
    sOrbPos.y = sOrbRefY;
    sOrbRoom = fopAcM_GetRoomNo(link);
    sOrbSnapshotValid = true;
    return HOOK_CONTINUE;
}

void on_dead_post(ModContext*, void*, void*, void*) {
    if (!orbEnabled()) {
        return;
    }

    const bool hadSnapshot = sOrbSnapshotValid;
    const cXyz savedPos = sOrbPos;
    const f32 savedRefY = sOrbRefY;
    const int savedRoom = sOrbRoom;

    resetOrbState(true);

    if (hadSnapshot) {
        sOrbPos = savedPos;
        sOrbRefY = savedRefY;
        sOrbRoom = savedRoom;
        sOrbSnapshotValid = true;
    }

    const u16 wallet = getWallet();
    if (wallet == 0) {
        return;
    }

    const u16 lost = static_cast<u16>((wallet + 1) / 2);
    const u16 kept = wallet - lost;
    setWallet(kept);

    sOrbRecovery = static_cast<u16>(lost / 2);
    if (sOrbRecovery == 0) {
        return;
    }

    if (!sOrbSnapshotValid) {
        daAlink_c* link = linkActor();
        if (link == NULL) {
            sOrbRecovery = 0;
            return;
        }
        sOrbPos.x = link->current.pos.x;
        sOrbPos.z = link->current.pos.z;
        sOrbRefY = resolveReferenceY(link);
        sOrbPos.y = sOrbRefY;
        sOrbRoom = fopAcM_GetRoomNo(link);
    }

    const char* stage = startStageName();
    if (stage) {
        strncpy(sOrbStage, stage, sizeof(sOrbStage) - 1);
        sOrbStage[sizeof(sOrbStage) - 1] = '\0';
    }

    sOrbPending = true;
    sOrbSpawned = false;
    sOrbSnapshotValid = false;
}

HookAction on_is_tbox_pre(ModContext*, void*, void* retval, void*) {
    if (!allowOrbDropCreate() || retval == NULL) {
        return HOOK_CONTINUE;
    }
    *static_cast<BOOL*>(retval) = FALSE;
    return HOOK_SKIP_ORIGINAL;
}

void on_create_post(ModContext*, void* args, void* retval, void*) {
    daObjDrop_c* drop = mods::arg<daObjDrop_c*>(args, 0);
    if (retval == NULL) {
        return;
    }
    const int status = *static_cast<int*>(retval);
    if (status == cPhs_ERROR_e) {
        onOrbDropCreateAborted();
        return;
    }
    if (status != cPhs_COMPLEATE_e || !sOrbDropCreatePending || drop == NULL) {
        return;
    }
    sOrbDropCreatePending = false;
    sOrbActorId = fopAcM_GetID(drop);
    finishRecoveryDropSetup(drop);
}

HookAction on_drop_get_pre(ModContext*, void* args, void*, void*) {
    daObjDrop_c* drop = mods::arg<daObjDrop_c*>(args, 0);
    if (!isRecoveryOrbDrop(drop)) {
        return HOOK_CONTINUE;
    }
    grantOrbRecovery();
    drop->mSetCollectDrop = false;
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_check_get_area_pre(ModContext*, void* args, void* retval, void*) {
    daObjDrop_c* drop = mods::arg<daObjDrop_c*>(args, 0);
    if (!isRecoveryOrbDrop(drop) || retval == NULL) {
        return HOOK_CONTINUE;
    }
    BOOL inRange = FALSE;
    if (daPy_py_c* player = playerActor()) {
        inRange = drop->current.pos.abs(player->current.pos) < 250.0f ? TRUE : FALSE;
    }
    *static_cast<BOOL*>(retval) = inRange;
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_check_complete_pre(ModContext*, void* args, void*, void*) {
    daObjDrop_c* drop = mods::arg<daObjDrop_c*>(args, 0);
    if (!isRecoveryOrbDrop(drop)) {
        return HOOK_CONTINUE;
    }
    grantOrbRecovery();
    drop->mSetCollectDrop = false;
    return HOOK_SKIP_ORIGINAL;
}

void on_drop_execute_post(ModContext*, void* args, void*, void*) {
    daObjDrop_c* drop = mods::arg<daObjDrop_c*>(args, 0);
    if (!isRecoveryOrbDrop(drop)) {
        return;
    }
    bool needBody = false;
    for (int i = 0; i < 6; i++) {
        if (drop->mpBodyEffEmtrs[i] == NULL) {
            needBody = true;
            break;
        }
    }
    if (needBody) {
        drop->removeBodyEffect();
        drop->createBodyEffect();
    }
    clearLinkTearCollectEffect();
}

void on_execute_post(ModContext*, void* args, void*, void*) {
    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(mods::arg<void*>(args, 0));
    if (actor == NULL || fopAcM_GetName(actor) != fpcNm_ALINK_e) {
        return;
    }
    tickSpawn();
}

}  // namespace

ModResult albw_soul_of_light_build_panel(UiElementHandle panel, ModError*) {
    return albw_ui_add_toggle(
        panel, "Soul of Light",
        "On death, lose half your rupees (round up) and spawn a Tear of Light at the death "
        "spot. Pickup returns half of what was lost. Do not load dev.albt.soul_of_light "
        "alongside this bundle — that standalone observes wallet loss instead of halving.",
        g_recovery_orb);
}

ModResult albw_soul_of_light_init(ModError*) {
    if (mods::hook::add_pre<DeadInit>(on_dead_pre) != MOD_OK ||
        mods::hook::add_post<DeadInit>(on_dead_post) != MOD_OK)
    {
        svc_log->error(mod_ctx, "failed to hook procCoDeadInit");
        return MOD_ERROR;
    }
    if (mods::hook::add_pre<IsTbox>(on_is_tbox_pre) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook isTbox");
        return MOD_ERROR;
    }
    if (mods::hook::add_post<DropCreate>(on_create_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook daObjDrop_c::create");
        return MOD_ERROR;
    }
    if (mods::hook::add_pre<DropGet>(on_drop_get_pre) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook dropGet");
        return MOD_ERROR;
    }
    if (mods::hook::add_pre<CheckGetArea>(on_check_get_area_pre) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook checkGetArea");
        return MOD_ERROR;
    }
    if (mods::hook::add_pre<CheckCompleteDemo>(on_check_complete_pre) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook checkCompleteDemo");
        return MOD_ERROR;
    }
    if (mods::hook::add_post<DropExecute>(on_drop_execute_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook daObjDrop_c::execute");
        return MOD_ERROR;
    }
    if (mods::hook::add_post<AcExecute>(on_execute_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook fopAc_Execute (soul of light)");
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_soul_of_light_shutdown(ModError*) {
    resetOrbState(true);
    return MOD_OK;
}
