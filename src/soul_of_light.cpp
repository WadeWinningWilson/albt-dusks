// Soul of Light — part of dev.albt.albw (owns wallet half on stock Dusklight).

#include "helpers/string.hpp"  // TEXT_SPAN - precede any d_save.h include (via potion.h)

#include "global.h"

#include "albw_common.h"
#include "config_vars.h"
#include "modules.h"
#include "tear_actor.hpp"  // Dusklight 2.0 custom tear actor (replaces daObjDrop spawn)

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
#include "potion.h"  // dAlbwPotion_refillSoulboundToMax (refill soulbound potion on death)
#include "soul_probe.h"
#include "albw_dusk_log.h"  // DuskLog, for the lifecycle trace
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_method.h"   // fpcMtd_Execute (play-scene tickSpawn point)
#include "f_pc/f_pc_manager.h"  // fpcM_GetProfile
#include "f_pc/f_pc_profile.h"  // process_profile_definition
#include "mods/svc/hook.hpp"

#include <cstring>

// ============================================
// NEW CODE - ALBT multiplatform
// fopAcM_fastCreate must be resolved at RUNTIME on every platform, never linked.
//
// The shipping game exports only the TARGET_PC overload carrying the two
// IF_DUSK_ARG extras (itemGiveTag / itemOriginalNo) - verified against
// dusklight.exe, which has ...@Z2IE@Z and does NOT have the 9-arg ...@Z2@Z.
// The published SDK link stub still advertises the old 9-arg import, so ANY
// link-time reference is wrong: aliasing the 9-arg name builds clean and then
// fails at load with ERROR_PROC_NOT_FOUND, and taking &fopAcM_fastCreate leaves
// an 11-arg undefined symbol the stub cannot satisfy.
//
// Resolving by name at runtime creates no import-table entry, so it sidesteps
// the stale stub on all eight platforms. Mangled names are per-ABI; the Itanium
// one was read back from clang for this signature. dlsym() takes the name
// without Mach-O's leading underscore, so one string covers ELF and Mach-O.
// ============================================
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define ALBT_FASTCREATE_SYMBOL                                                                         "?fopAcM_fastCreate@@YAPEAVfopAc_ac_c@@FIPEBUcXyz@@HPEBVcsXyz@@0CP6AHPEAX@Z2IE@Z"
#else
#include <dlfcn.h>
#define ALBT_FASTCREATE_SYMBOL "_Z17fopAcM_fastCreatesjPK4cXyziPK5csXyzS1_aPFiPvES5_jh"
#endif
// ============================================
// NEW CODE ENDS HERE
// ============================================

namespace {

using FastCreateFn = fopAc_ac_c* (*)(s16, u32, const cXyz*, int, const csXyz*, const cXyz*, s8,
                                     createFunc, void*, u32, u8);
FastCreateFn g_fastCreate = nullptr;

bool resolve_fast_create() {
    if (g_fastCreate != nullptr) {
        return true;
    }
#if defined(_WIN32)
    HMODULE exe = GetModuleHandleW(nullptr);
    if (exe == nullptr) {
        return false;
    }
    g_fastCreate =
        reinterpret_cast<FastCreateFn>(GetProcAddress(exe, ALBT_FASTCREATE_SYMBOL));
#else
    g_fastCreate = reinterpret_cast<FastCreateFn>(dlsym(RTLD_DEFAULT, ALBT_FASTCREATE_SYMBOL));
#endif
    return g_fastCreate != nullptr;
}

fopAc_ac_c* fast_create_drop(s16 procName, u32 parameters, const cXyz* pos, int roomNo,
                             const csXyz* angle, const cXyz* scale, s8 argument) {
    if (g_fastCreate == nullptr) {
        return nullptr;
    }
    return g_fastCreate(procName, parameters, pos, roomNo, angle, scale, argument, nullptr, nullptr,
                        0, 0xFF);
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
// Set ONLY by pushTearRenderFlags. popTearRenderFlags must not touch a single
// save bit unless this says a push actually happened - see the block comment
// on popTearRenderFlags for the save damage that caused.
bool sTearRenderFlagsPushed = false;

#if ALBW_SOUL_PROBE
// One line, one shape, at every lifecycle point - so a bug report can be
// diffed against a working run instead of read prose-style. vessel= is the
// save state this feature writes (dSv_light_drop_c::mLightDropGetFlag) and
// tears= its companion counts: a cleared flag with a full count of 16 is the
// one detectable signature of the damage this feature used to do.
void soulTrace(const char* ev) {
    dSv_light_drop_c& d = g_dComIfG_gameInfo.info.getPlayer().getLightDrop();
    DuskLog.info("[soul] {} pending={} spawned={} recovery={} actorId={} room={} "
                 "snap={} pushed={} vessel={}{}{} tears={}/{}/{}",
                 ev, sOrbPending ? 1 : 0, sOrbSpawned ? 1 : 0, sOrbRecovery,
                 (unsigned)sOrbActorId, sOrbRoom, sOrbSnapshotValid ? 1 : 0,
                 sTearRenderFlagsPushed ? 1 : 0,
                 d.isLightDropGetFlag(0) ? 1 : 0, d.isLightDropGetFlag(1) ? 1 : 0,
                 d.isLightDropGetFlag(2) ? 1 : 0, (int)d.getLightDropNum(0),
                 (int)d.getLightDropNum(1), (int)d.getLightDropNum(2));
}
#define ALBW_SOUL_TRACE(ev) soulTrace(ev)
#else
#define ALBW_SOUL_TRACE(ev) ((void)0)
#endif

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
// fork calls dALBWDeathRupees_tickSpawn() at the END of dScnPly_Execute (d_s_play.cpp:855) -
// after all processes execute, before the draw pass. dScnPly_Execute is static/unhookable, so
// we post-hook the method dispatcher fpcMtd_Execute and fire only for the PLAY_SCENE process:
// that runs tickSpawn at the exact fork frame point, so completing the async tear load
// (entryResourceManager slot 2) never races mid-actor-execute.
// Typed hook: fpcMtd_Execute is header-declared (f_pc/f_pc_method.h:16), so the
// compiler mangles it per target rather than depending on a bare name being in
// the host's symbol manifest. See albw_symbols.h.
DEFINE_HOOK(&fpcMtd_Execute, MtdExecute);

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
    sTearRenderFlagsPushed = true;
}

// ============================================
// SAVE-DATA FIX - this function was destroying Vessel of Light possession.
//
// THE BUG. pushTearRenderFlags() has NO CALLER anywhere in the mod, so
// sTearRenderFlagWasSet was never anything but all-false. The old body then
// read `if (!sTearRenderFlagWasSet[i]) offLightDropGetFlag(i)` - which with an
// all-false array is an UNCONDITIONAL clear of all three flags, every time.
// resetOrbState() calls this on orb pickup, on death and on shutdown.
//
// mLightDropGetFlag is not a render flag. It is SAVE state (d_save.h:451) and
// it means "the player has the Vessel of Light for this region":
//   d_item.cpp:1760-1768   FARON/ELDIN/LANAYRU_VESSEL possession
//   d_a_obj_drop.cpp:131,:304  gates the tear-drop actor get/complete logic
//   d_a_e_ym.cpp:2808      Twilight bug behaviour
//   d_meter2*.cpp          the vessel HUD
// So every death was silently revoking the player's vessels and desyncing the
// drop actor that owns the tear-collect and death-complete sequence.
//
// THE FIX. A pop may only undo a push. Without the latch this function has no
// saved state to restore and therefore nothing legitimate to do.
//
// NOT AUTO-REPAIRED, deliberately, and for the same reason as the
// saveBitLabels migration: a cleared vessel flag is indistinguishable from a
// player who has not reached that region yet, so "repairing" it would GRANT
// progress rather than restore it. A save damaged by an earlier build needs
// the flag re-earned, or a deliberate, separately-reviewed repair keyed on the
// one detectable inconsistency (getLightDropNum(i) == 16 with the flag off).
// ============================================
void popTearRenderFlags() {
    if (!sTearRenderFlagsPushed) {
        return;  // nothing was pushed - touching a save bit here is the bug
    }
    dSv_light_drop_c& drops = g_dComIfG_gameInfo.info.getPlayer().getLightDrop();
    for (int i = 0; i < 3; ++i) {
        if (!sTearRenderFlagWasSet[i]) {
            drops.offLightDropGetFlag(static_cast<u8>(i));
        }
        sTearRenderFlagWasSet[i] = false;
    }
    sTearRenderFlagsPushed = false;
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
    ALBW_SOUL_TRACE("reset_enter");
    if (deleteActor) {
        albw_tear_actor_despawn();  // remove any live custom tear (new death / cleanup)
    }
    sOrbActorId = fpcM_ERROR_PROCESS_ID_e;
    popTearRenderFlags();
    sOrbPending = false;
    sOrbSpawned = false;
    sOrbRecovery = 0;
    sOrbDropCreatePending = false;
    sOrbMissingFrames = 0;
    sOrbSpawnCooldown = 0;
    sOrbStage[0] = '\0';
    sOrbRoom = -1;
    ALBW_SOUL_TRACE("reset_exit");
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

    // Kick off the supplemental tear-FX archive load (Pscene011). The tear body
    // effect only renders once it is resident; it is polled each frame below.
    albw_tear_ensure_scene_res();

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

// ============================================
// NEW CODE - ALBW Port (Dusklight 2.0 custom tear actor)
// Spawns the custom "altear" actor (tear_actor.cpp) at the death spot instead of
// the vanilla daObjDrop_c. The actor owns float / proximity-pickup / recovery /
// sounds, and tear_glow.cpp draws its visible glow via GfxService — so this drops
// the daObjDrop hook web and the fragile Pscene011 supplemental-archive path.
// ============================================
void trySpawnOrbInRoom(const char* stageName, int roomNo) {
    if (!orbEnabled() || !sOrbPending || sOrbRecovery == 0 || !stageName) {
        return;
    }
    if (roomNo != sOrbRoom || strcmp(stageName, sOrbStage) != 0) {
        return;
    }

    // Collected: stop respawning and clear the pending state.
    if (albw_tear_actor_was_collected()) {
        resetOrbState(false);
        return;
    }

    if (sOrbSpawnCooldown > 0) {
        sOrbSpawnCooldown--;
        return;
    }

    // Still alive? nothing to do. It stays until collected or the stage unloads.
    if (sOrbSpawned) {
        if (albw_tear_actor_is_active()) {
            sOrbMissingFrames = 0;
            return;
        }
        // Gone (stage reload while away) — respawn after a short grace.
        if (++sOrbMissingFrames < 30) {
            return;
        }
        sOrbMissingFrames = 0;
        sOrbSpawned = false;
    }

    // Float above the resolved floor, matching the fork's tear placement.
    const f32 floorY = resolveOrbFloorY(sOrbPos.x, sOrbPos.z, sOrbRefY);
    cXyz spawnPos(sOrbPos.x, floorY + kOrbFloatAboveGround, sOrbPos.z);

    ActorId id = albw_tear_actor_spawn(spawnPos, static_cast<s8>(roomNo), sOrbRecovery);
    if (id == 0) {
        sOrbSpawnCooldown = 30;
        svc_log->warn(mod_ctx, "soul of light: tear actor spawn failed");
        return;
    }
    sOrbSpawned = true;
    ALBW_SOUL_TRACE("spawned");
}

void tickSpawn() {
    if (!orbEnabled() || !sOrbPending || sOrbRecovery == 0) {
        return;
    }
    // fork dALBWDeathRupees_tickSpawn (d_albw_death_rupee.cpp:341) - poll the async tear
    // load to completion. Runs at the play-scene execute point (see on_mtd_execute_post), so
    // the swap + entryResourceManager(slot 2) land after all execution, before draw.
    albw_tear_ensure_scene_res();

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
    // ============================================
    // NEW CODE - ALBW Port (Soulbound Red Potion refill on death)
    // Fork d_gameover.cpp:490 tops the soulbound bottle back to its current max
    // charges on the death/respawn path. procCoDeadInit is the .dusk's death seam
    // it already hooks. refillSoulboundToMax self-gates (no-op without the potion),
    // so this is independent of the Soul-of-Light orb feature gated below.
    // ============================================
    ALBW_SOUL_TRACE("death");
    dAlbwPotion_refillSoulboundToMax();

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
    ALBW_SOUL_TRACE("pending_armed");
    sOrbSpawned = false;
    sOrbSnapshotValid = false;
    // NOTE: the fork kicks the tear load here (onLinkDeathBegin), but doing the same in the
    // mod (mDoDvdThd_toMainRam_c::create at the moment of death) corrupts unrelated scene
    // particles' texture image pointers -> GXLoadTexObj crash (fault 0x41900148). The play-scene
    // poll (on_mtd_execute_post -> tickSpawn) starts the load safely during gameplay instead.
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
    ALBW_SOUL_TRACE("collect_pre");
    grantOrbRecovery();
    drop->mSetCollectDrop = false;
    ALBW_SOUL_TRACE("collect_post");
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
    // Poll the supplemental tear archive to completion (async DVD read), then the
    // recreated body emitters resolve their FX through slot 2 and become visible.
    albw_tear_ensure_scene_res();
    // fork d_a_obj_drop.cpp:674 gates the body-effect keep-alive on mModeAction < 3
    // (the idle/wait states); recreating during the collect demo fights the line FX.
    if (drop->mModeAction < 3) {
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
    }
    clearLinkTearCollectEffect();
}

void on_mtd_execute_post(ModContext*, void* args, void*, void*) {
    // Fire tickSpawn only after the PLAY_SCENE process's execute (dScnPly_Execute) - the
    // exact site the fork uses. fpcMtd_Execute runs for every process each frame; filter
    // by profile name so this is once per frame, after all execution and before draw.
    void* process = mods::arg<void*>(args, 1);
    if (process == NULL) {
        return;
    }
    process_profile_definition* prof = fpcM_GetProfile(process);
    if (prof == NULL || prof->name != fpcNm_PLAY_SCENE_e) {
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
    if (!resolve_fast_create()) {
        svc_log->error(mod_ctx, "fopAcM_fastCreate not found in host binary");
        return MOD_ERROR;
    }
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
    if (mods::hook::add_post<MtdExecute>(on_mtd_execute_post) != MOD_OK) {
        svc_log->error(mod_ctx, "failed to hook fpcMtd_Execute (soul of light play-scene tick)");
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_soul_of_light_shutdown(ModError*) {
    resetOrbState(true);
    return MOD_OK;
}
