/**
 * d_albw_wolf_stun.cpp — ALBW Port
 * Wolf combat system — generic pause-based stun module.
 *
 * On a Midna field attack hit against a non-twilight, non-boss enemy:
 *   dAlbwWolfStun_apply() is called from cc_at_check.
 *   fpcM_PauseEnable(enemy, 1) skips the enemy's execute() each frame
 *   while fpcEx_Execute still returns early, but draw() is unaffected —
 *   the enemy freezes mid-animation in its last pose for the stun duration.
 *
 * dAlbwWolfStun_update() is called every frame from daAlink_c::execute()
 * to decrement timers and call fpcM_PauseDisable when each expires.
 *
 * Paused enemies no longer register hurt colliders in execute(). The
 * draw-phase bridge (beforeMove / afterMove) re-registers snapshotted TG
 * shapes so follow-up Link melee (wolf bites or human sword) resolves
 * through cc_at_check().
 */

#include "wolf_combat.h"
#include "wolf_charge_hud.h"
#include "albw_game.h"
#include "albw_common.h"
#include "config_vars.h"
#include "mods/hook.hpp"
#include "d/d_cc_uty.h"
#include "d/d_meter2_draw.h"
#include "SSystem/SComponent/c_cc_d.h"
#include "SSystem/SComponent/c_cc_s.h"
#include "f_pc/f_pc_name.h"
#include "SSystem/SComponent/c_counter.h"
#include "SSystem/SComponent/c_sxyz.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "d/dolzel.h"
#include "d/actor/d_a_e_db.h"
#include "d/actor/d_a_e_gb.h"
#include "d/actor/d_a_e_gi.h"
#include "d/actor/d_a_e_gob.h"
#include "d/actor/d_a_e_kk.h"
#include "d/actor/d_a_e_nz.h"
#include "d/actor/d_a_e_oc.h"
#include "d/actor/d_a_e_rb.h"
#include "d/actor/d_a_e_rd.h"
#include "d/actor/d_a_e_rdb.h"
#include "d/actor/d_a_e_sh.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include <cstring>
#include "Z2AudioLib/Z2Creature.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

static u8 g_wolf_charge_count = 0;
static u8 g_wolf_bite_steps = 0;

namespace {

// Stock dusklight-main lacks fork-only enemy helpers; use layout-stable shims.
constexpr ptrdiff_t kOcSoundOff = 0x5c4;
constexpr ptrdiff_t kShSoundOff = 0x5bc;
constexpr ptrdiff_t kGiCryTimerOff = 0x690;
constexpr ptrdiff_t kKkActionModeOff = 0x660;
constexpr ptrdiff_t kKkField679Off = 0x679;

static Z2Creature* mod_getEnemySound(fopAc_ac_c* i_enemy, s16 i_name) {
    if (i_enemy == NULL) {
        return NULL;
    }
    switch (i_name) {
    case fpcNm_E_OC_e:
        return reinterpret_cast<Z2Creature*>(reinterpret_cast<char*>(i_enemy) + kOcSoundOff);
    case fpcNm_E_SH_e:
        return reinterpret_cast<Z2Creature*>(reinterpret_cast<char*>(i_enemy) + kShSoundOff);
    default:
        return NULL;
    }
}

static bool mod_daE_GI_isScreamOwner(fopAc_ac_c* i_enemy) {
    const int cryTimer =
        *reinterpret_cast<const int*>(reinterpret_cast<const char*>(i_enemy) + kGiCryTimerOff);
    return cryTimer > 0;
}

static bool mod_kkFreezeUnsafe(const fopAc_ac_c* i_enemy) {
    const char* base = reinterpret_cast<const char*>(i_enemy);
    const s32 actionMode = *reinterpret_cast<const s32*>(base + kKkActionModeOff);
    const u8 mode679 = *reinterpret_cast<const u8*>(base + kKkField679Off);
    return actionMode == 10 || mode679 == 1 || mode679 == 2;
}

// RelWithDebInfo strips OS_REPORT (DEBUG=0). Write playtest traces to a file instead.
void wolfStun_debugLog(const char* fmt, ...) {
    if (!dAlbwWolfCombat_isEnabled()) {
        return;
    }

    static bool sResetDone = false;

    char path[512];
    path[0] = '\0';
    const char* user = getenv("USERPROFILE");
    if (user != NULL && user[0] != '\0') {
        snprintf(path, sizeof(path), "%s/Documents/dusklight/albw_wolf_stun_debug.txt", user);
    } else {
        strncpy(path, "albw_wolf_stun_debug.txt", sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }

    FILE* fp = fopen(path, sResetDone ? "a" : "w");
    if (fp == NULL) {
        fp = fopen("albw_wolf_stun_debug.txt", sResetDone ? "a" : "w");
    }
    if (fp == NULL) {
        return;
    }

    if (!sResetDone) {
        sResetDone = true;
        fprintf(fp, "--- ALBW Wolf Link stun / collision bridge debug ---\n");
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    fclose(fp);
}

struct WolfStunEntry {
    fpc_ProcID  mId;
    s16         mTimer;
    s8          mColliderCount;
    // Hold until dAlbwWolfStun_thaw (lockout double-claw). Timer is not decremented.
    bool        mHoldUntilThaw;
    cCcD_Obj*   mColliders[8];
    // Execute-phase cc_at_check already resolved the opening field-attack hit;
    // skip draw-phase bridge on that frame to avoid a same-frame double deduct.
    int         mSkipBridgeFrame;
};

constexpr int WOLF_STUN_MAX = 16;

static WolfStunEntry sStunList[WOLF_STUN_MAX];
static int           sStunCount = 0;

void wolfStun_debugLogHeartbeat(cCcS* i_ccs) {
    static int sLastHeartbeatFrame = -1000;

    const int frame = g_Counter.mCounter0;
    if (frame - sLastHeartbeatFrame < 60) {
        return;
    }
    sLastHeartbeatFrame = frame;

    for (int i = 0; i < sStunCount; i++) {
        fopAc_ac_c* enemy =
            static_cast<fopAc_ac_c*>(fpcM_SearchByID(sStunList[i].mId));
        const s16 name = enemy != NULL ? fopAcM_GetName(enemy) : -1;
        const s16 hp = enemy != NULL ? enemy->health : -1;
        int tgSetCount = 0;
        for (int c = 0; c < sStunList[i].mColliderCount; c++) {
            cCcD_Obj* obj = sStunList[i].mColliders[c];
            if (obj != NULL && obj->ChkTgSet()) {
                tgSetCount++;
            }
        }

        wolfStun_debugLog(
            "f=%06d evt=heartbeat id=%u name=%d coll=%d tgSet=%d hp=%d ccAt=%d ccTg=%d ccObj=%d\n",
            frame, sStunList[i].mId, name, sStunList[i].mColliderCount, tgSetCount, hp,
            i_ccs != NULL ? i_ccs->mObjAtCount : -1, i_ccs != NULL ? i_ccs->mObjTgCount : -1,
            i_ccs != NULL ? i_ccs->mObjCount : -1);
    }
}

void wolfStun_debugLogNoColliders(fpc_ProcID i_id) {
    static fpc_ProcID sLastId = fpcM_ERROR_PROCESS_ID_e;
    static int sLastFrame = -1000;

    const int frame = g_Counter.mCounter0;
    if (i_id == sLastId && frame - sLastFrame < 30) {
        return;
    }
    sLastId = i_id;
    sLastFrame = frame;

    wolfStun_debugLog("f=%06d evt=no-colliders id=%u mpObjCount=%d\n", frame, i_id,
                      albw_game::cc_system()->mObjCount);
}

WolfStunEntry* findEntry(fpc_ProcID i_id) {
    for (int i = 0; i < sStunCount; i++) {
        if (sStunList[i].mId == i_id) {
            return &sStunList[i];
        }
    }
    return NULL;
}

bool sttsBelongsToActor(cCcD_Stts* i_stts, fpc_ProcID i_id, fopAc_ac_c* i_enemy) {
    if (i_stts == NULL) {
        return false;
    }

    if (i_stts->GetApid() == i_id) {
        return true;
    }

    fopAc_ac_c* ac = i_stts->GetAc();
    return ac != NULL && ac == i_enemy;
}

bool colliderRegistered(cCcS* i_ccs, cCcD_Obj* i_obj) {
    for (int i = 0; i < i_ccs->mObjTgCount; i++) {
        if (i_ccs->mpObjTg[i] == i_obj) {
            return true;
        }
    }
    return false;
}

void storeColliders(WolfStunEntry* i_entry, cCcD_Obj* const* i_objs, int i_count) {
    i_entry->mColliderCount = 0;

    const int maxStore = static_cast<int>(ARRAY_SIZEU(i_entry->mColliders));
    for (int i = 0; i < i_count && i_entry->mColliderCount < maxStore; i++) {
        cCcD_Obj* obj = i_objs[i];
        if (obj != NULL) {
            i_entry->mColliders[i_entry->mColliderCount++] = obj;
        }
    }
}

void captureCollidersFromCcS(cCcS* i_ccs, fopAc_ac_c* i_enemy, WolfStunEntry* i_entry) {
    const fpc_ProcID id = fopAcM_GetID(i_enemy);
    cCcD_Obj* found[ARRAY_SIZEU(i_entry->mColliders)] = {};
    int foundCount = 0;

    for (int i = 0; i < i_ccs->mObjCount; i++) {
        cCcD_Obj* obj = i_ccs->mpObj[i];
        if (obj == NULL || !obj->ChkTgSet()) {
            continue;
        }

        if (!sttsBelongsToActor(obj->GetStts(), id, i_enemy)) {
            continue;
        }

        if (foundCount >= static_cast<int>(ARRAY_SIZEU(found))) {
            break;
        }

        found[foundCount++] = obj;
    }

    if (foundCount > 0) {
        storeColliders(i_entry, found, foundCount);
        wolfStun_debugLog("f=%06d evt=capture-mpObj id=%u count=%d mpObj=%d\n",
                          g_Counter.mCounter0, id, foundCount, i_ccs->mObjCount);
    }
}

void removeStunEntry(int i_index);

void restoreStunColliderFlags(WolfStunEntry* i_entry) {
    for (int c = 0; c < i_entry->mColliderCount; c++) {
        cCcD_Obj* obj = i_entry->mColliders[c];
        if (obj == NULL) {
            continue;
        }

        dCcD_GObjInf* gobj = dCcD_GetGObjInf(obj);
        if (gobj != NULL) {
            gobj->OnTgNoConHit();
        }
    }
}

void prepareStunnedEnemyForBridge(fopAc_ac_c* i_enemy, WolfStunEntry* i_entry) {
    if (i_entry->mColliderCount <= 0) {
        return;
    }

    const s16 name = fopAcM_GetName(i_enemy);
    if (name == fpcNm_E_DB_e) {
        e_db_class* db = reinterpret_cast<e_db_class*>(i_enemy);
        db->ccSph.SetC(i_enemy->eyePos);
    } else if (name == fpcNm_E_GB_e) {
        e_gb_class* gb = reinterpret_cast<e_gb_class*>(i_enemy);
        gb->headSph.SetC(i_enemy->eyePos);
    } else if (name == fpcNm_E_RB_e) {
        e_rb_class* rb = reinterpret_cast<e_rb_class*>(i_enemy);
        rb->ccSph.SetC(i_enemy->eyePos);
    }

    // ============================================
    // GENERIC UN-PARK (root fix, playtest-diagnosed). The standard TP
    // i-frame idiom parks hurt colliders 10k-200k units off-body while a
    // damage timer runs (E_RD, E_FS, E_BA, E_BU, E_BS, E_BI, ...). The
    // freeze always fires inside the damage frame that STARTS that timer,
    // and the pause keeps it alive — so the snapshot is parked forever and
    // the engine's broadphase never pairs Link's sword with it (confirmed:
    // Bulblins/Puppets took zero bridge hits while Tektite, which never
    // parks, took all of them). Any captured collider found implausibly
    // far from its frozen owner snaps back to the body. Idempotent, runs
    // only on frozen enemies, covers every current and future actor —
    // the per-actor cases above remain as exact-position overrides.
    // GetCoCP() is the real center for Sph/Cyl shapes (the vast majority
    // of enemy TGs); for other shapes the base returns a dummy — no-op.
    // ============================================
    constexpr f32 kParkDistance = 2000.0f;
    for (int c = 0; c < i_entry->mColliderCount; c++) {
        cCcD_Obj* obj = i_entry->mColliders[c];
        if (obj == NULL) {
            continue;
        }
        cCcD_ShapeAttr* shape = obj->GetShapeAttr();
        if (shape == NULL) {
            continue;
        }
        cXyz& center = shape->GetCoCP();
        const f32 dx = center.x - i_enemy->current.pos.x;
        const f32 dy = center.y - i_enemy->current.pos.y;
        const f32 dz = center.z - i_enemy->current.pos.z;
        if (dx * dx + dy * dy + dz * dz > kParkDistance * kParkDistance) {
            center = i_enemy->eyePos;
            wolfStun_debugLog("f=%06d evt=unpark id=%u name=%d coll=%d\n",
                              g_Counter.mCounter0, i_entry->mId, name, c);
        }
    }

    cCcD_Stts* stts = i_entry->mColliders[0]->GetStts();
    if (stts != NULL) {
        static_cast<dCcD_Stts*>(stts)->Move();
    }

    static fpc_ProcID sLastPrepLogId = fpcM_ERROR_PROCESS_ID_e;
    static int sLastPrepLogFrame = -1000;
    const int frame = g_Counter.mCounter0;
    const bool logPrep = i_entry->mId != sLastPrepLogId || frame - sLastPrepLogFrame >= 60;

    for (int c = 0; c < i_entry->mColliderCount; c++) {
        cCcD_Obj* obj = i_entry->mColliders[c];
        if (obj == NULL) {
            continue;
        }

        dCcD_GObjInf* gobj = dCcD_GetGObjInf(obj);
        if (gobj == NULL) {
            continue;
        }

        if (logPrep && c == 0) {
            wolfStun_debugLog(
                "f=%06d evt=bridge-prep id=%u name=%d hadNoConHit=%d\n", frame, i_entry->mId, name,
                gobj->ChkTgNoConHit() ? 1 : 0);
            sLastPrepLogId = i_entry->mId;
            sLastPrepLogFrame = frame;
        }

        gobj->OffTgNoConHit();
    }
}

// ============================================
// Post-thaw kill watchdog (alpha cleanup, playtest edge case). A bridge
// kill drops health to 0 and grants rupees, but most actors only process
// death inside their own damage flow — which their (paused-then-stale)
// i-frame timer gates shut on thaw — so they could keep running at 0 HP
// and be "killed again". After a lethal bridge hit, give the actor a
// grace window to die naturally on thaw; if it is still alive-at-0HP
// when the grace expires, despawn it (rupees were already granted; the
// paired ring-evict fix in d_albw_enemy_rupee kills the double payout).
// ============================================
struct PostThawKillEntry {
    fpc_ProcID mId;
    s16 mFrames;
};
constexpr int kPostThawKillMax = 8;
constexpr s16 kPostThawGraceFrames = 30;
PostThawKillEntry sPostThawKills[kPostThawKillMax];
int sPostThawKillCount = 0;

void armPostThawKill(fpc_ProcID i_id) {
    for (int i = 0; i < sPostThawKillCount; i++) {
        if (sPostThawKills[i].mId == i_id) {
            return;
        }
    }
    if (sPostThawKillCount >= kPostThawKillMax) {
        return;
    }
    sPostThawKills[sPostThawKillCount].mId = i_id;
    sPostThawKills[sPostThawKillCount].mFrames = kPostThawGraceFrames;
    sPostThawKillCount++;
}

void tickPostThawKills() {
    for (int i = 0; i < sPostThawKillCount; i++) {
        fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(fpcM_SearchByID(sPostThawKills[i].mId));
        bool done = false;

        if (actor == NULL) {
            done = true;  // died/deleted naturally
        } else if (--sPostThawKills[i].mFrames <= 0) {
            if (actor->health <= 0) {
                wolfStun_debugLog("f=%06d evt=zombie-despawn id=%u\n", g_Counter.mCounter0,
                                  sPostThawKills[i].mId);
                fopAcM_delete(actor);
            }
            done = true;
        }

        if (done) {
            sPostThawKills[i] = sPostThawKills[sPostThawKillCount - 1];
            sPostThawKillCount--;
            i--;
        }
    }
}

void removeStunEntry(int i_index) {
    restoreStunColliderFlags(&sStunList[i_index]);
    base_process_class* proc = fpcM_SearchByID(sStunList[i_index].mId);
    if (proc != NULL) {
        fpcM_PauseDisable(proc, 1);
    }

    for (int j = i_index; j < sStunCount - 1; j++) {
        sStunList[j] = sStunList[j + 1];
    }
    sStunCount--;
}

constexpr u32 BRIDGE_LINK_MELEE_AT_TYPES =
    AT_TYPE_WOLF_ATTACK | AT_TYPE_WOLF_CUT_TURN | AT_TYPE_NORMAL_SWORD |
    AT_TYPE_MASTER_SWORD | AT_TYPE_MIDNA_LOCK;

// Link-owned projectile actors (alpha cleanup: "every Link attack must be
// viable" vs frozen enemies). Enemy projectiles use different actor names
// (e.g. E_ARROW), so the name whitelist keeps enemy fire from bridging.
constexpr u32 BRIDGE_LINK_RANGED_AT_TYPES = AT_TYPE_ARROW | AT_TYPE_BOMB | AT_TYPE_BOOMERANG;

constexpr u32 BRIDGE_WOLF_BITE_AT_TYPES = AT_TYPE_WOLF_ATTACK | AT_TYPE_WOLF_CUT_TURN;

bool isBridgeLinkMeleeAt(cCcD_Obj* i_atObj) {
    if (i_atObj == NULL) {
        return false;
    }

    fopAc_ac_c* atAc = i_atObj->GetAc();

    if (i_atObj->ChkAtType(BRIDGE_LINK_RANGED_AT_TYPES) != 0 && atAc != NULL) {
        const s16 atName = fopAcM_GetName(atAc);
        if (atName == fpcNm_ARROW_e || atName == fpcNm_NBOMB_e ||
            atName == fpcNm_BOOMERANG_e) {
            return true;
        }
    }

    if (i_atObj->ChkAtType(BRIDGE_LINK_MELEE_AT_TYPES) == 0) {
        return false;
    }

    fopAc_ac_c* link = albw_game::link_player();
    if (atAc == NULL) {
        return link != NULL;
    }

    return atAc == link || fopAcM_GetName(atAc) == fpcNm_ALINK_e;
}

Z2Creature* getBridgeEnemySound(fopAc_ac_c* i_enemy) {
    if (i_enemy == NULL) {
        return NULL;
    }

    const s16 name = fopAcM_GetName(i_enemy);
    switch (name) {
    case fpcNm_E_OC_e:
    case fpcNm_E_SH_e:
        return mod_getEnemySound(i_enemy, name);
    default:
        return NULL;
    }
}

void prepareBridgeAtInfo(fopAc_ac_c* i_enemy, dCcU_AtInfo* i_atInfo) {
    i_atInfo->mpSound = getBridgeEnemySound(i_enemy);
    i_atInfo->mPowerType = 1;
}

// Paused enemies fail ProcAtTgHitmark's ChkNoneActorPerfTblId() gate during Move(),
// so spawn slash/spark particles here after cc_at_check resolves damage.
void spawnBridgeHitMark(fopAc_ac_c* i_enemy, cCcD_Obj* i_atObj, cCcD_Obj* i_tgObj,
                        dCcU_AtInfo* i_atInfo) {
    if (i_atInfo->mAttackPower == 0 || i_atObj == NULL || i_tgObj == NULL) {
        return;
    }

    if (i_atObj->ChkAtType(BRIDGE_WOLF_BITE_AT_TYPES)) {
        return;
    }

    dCcD_GObjInf* atGObj = dCcD_GetGObjInf(i_atObj);
    dCcD_GObjInf* tgGObj = dCcD_GetGObjInf(i_tgObj);
    if (atGObj == NULL || tgGObj == NULL) {
        return;
    }

    if (atGObj->ChkAtNoHitMark() || tgGObj->ChkTgNoHitMark()) {
        return;
    }

    // ============================================
    // Alpha cleanup (user directive): the engine's TgSpl-based hitmark
    // suppression assumed a LIVE enemy would play its own hit reaction —
    // a frozen enemy cannot, so suppressing here meant sword hits landed
    // in total silence (playtest: Bulblin/Puppet/Tektite). Every damaging
    // bridge hit now spawns a hitmark; only explicit NoHitMark flags and
    // the wolf-bite branch (which has its own feedback) still skip.
    // ============================================

    cXyz* hitPosP = tgGObj->GetTgHitPosP();
    if (hitPosP == NULL) {
        return;
    }

    csXyz angle;
    angle.x = 0;
    angle.y = i_atInfo->mHitDirection.y;
    angle.z = 0;

    if (tgGObj->GetTgHitMark() == 5 || tgGObj->GetTgHitMark() == 8) {
        albw_game::set_hit_mark(2, i_enemy, hitPosP, &angle, NULL, atGObj->GetAtType());
        wolfStun_debugLog("f=%06d evt=bridge-vfx id=%u kind=hitmark2\n", g_Counter.mCounter0,
                          i_enemy->id);
        return;
    }

    u16 hitmark = tgGObj->GetTgHitMark() == 3 ? 3 : atGObj->GetAtHitMark();
    if ((hitmark == 0 && tgGObj->GetTgHitMark() != 8) ||
        (hitmark == 4 && tgGObj->GetTgHitMark() == 4))
    {
        return;
    }

    if ((hitmark != 1 && hitmark != 3) || atGObj->GetAtAtp() != 0) {
        albw_game::set_hit_mark(hitmark, i_enemy, hitPosP, &angle, NULL, atGObj->GetAtType());
        wolfStun_debugLog("f=%06d evt=bridge-vfx id=%u kind=hitmark%u\n", g_Counter.mCounter0,
                          i_enemy->id, hitmark);
    }
}

void dispatchBridgeHitVfx(fopAc_ac_c* i_enemy, cCcD_Obj* i_atObj, cCcD_Obj* i_tgObj,
                          dCcU_AtInfo* i_atInfo) {
    if (i_atInfo->mAttackPower == 0) {
        return;
    }

    if (fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e &&
        i_atObj->ChkAtType(BRIDGE_WOLF_BITE_AT_TYPES))
    {
        daAlink_c* link = static_cast<daAlink_c*>(albw_game::link_player());
        if (link != NULL) {
            link->setWolfBiteDamage(static_cast<fopEn_enemy_c*>(i_enemy));
            wolfStun_debugLog("f=%06d evt=bridge-vfx id=%u kind=wolf-bite\n", g_Counter.mCounter0,
                              i_enemy->id);
        }
        return;
    }

    spawnBridgeHitMark(i_enemy, i_atObj, i_tgObj, i_atInfo);
}

void dispatchLethalDeathReaction(fopAc_ac_c* i_enemy) {
    if (i_enemy == NULL || i_enemy->health > 0) {
        return;
    }

    const s16 name = fopAcM_GetName(i_enemy);

    if (name == fpcNm_E_OC_e) {
        daE_OC_c* oc = static_cast<daE_OC_c*>(i_enemy);
        oc->setActionMode(6, 0);
        oc->offTgSph();
        return;
    }

    if (fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e) {
        static_cast<fopEn_enemy_c*>(i_enemy)->onWolfBiteDamage();
    }
}

void processBridgeHit(fopAc_ac_c* i_enemy, cCcD_Obj* i_atObj, cCcD_Obj* i_tgObj) {
    if (!isBridgeLinkMeleeAt(i_atObj)) {
        wolfStun_debugLog("f=%06d evt=bridge-reject id=%u atType=0x%x ac=%p wolf=%d\n",
                          g_Counter.mCounter0, i_enemy->id,
                          i_atObj != NULL ? i_atObj->GetAtType() : 0,
                          i_atObj != NULL ? i_atObj->GetAc() : NULL,
                          albw_game::is_wolf_form() ? 1 : 0);
        return;
    }

    dCcU_AtInfo atInfo = {};
    atInfo.mpCollider = i_atObj;
    prepareBridgeAtInfo(i_enemy, &atInfo);
    cc_at_check(i_enemy, &atInfo);

    // ============================================
    // King Bulblin deferred defeat (user design): his defeat is knockdown-
    // count-driven and his own damage flow is paused while frozen, so the
    // only frozen-hit risk is health reaching 0 (an undefined state for a
    // count-defeated boss). Clamp health at 1 while frozen — hits still
    // land with full visual feedback (bridge hit sparks below), but the
    // finishing blow only counts once he is moving again.
    // ============================================
    if (fopAcM_GetName(i_enemy) == fpcNm_E_RDB_e && i_enemy->health < 1) {
        i_enemy->health = 1;
    }

    wolfStun_debugLog("f=%06d evt=bridge-hit id=%u dmg=%u hp=%d mpActor=%p sound=%p wolf=%d\n",
                      g_Counter.mCounter0, i_enemy->id, atInfo.mAttackPower, i_enemy->health,
                      atInfo.mpActor, atInfo.mpSound, albw_game::is_wolf_form() ? 1 : 0);

    dispatchBridgeHitVfx(i_enemy, i_atObj, i_tgObj, &atInfo);

    if (i_enemy->health <= 0) {
        dispatchLethalDeathReaction(i_enemy);
    }
}

} // anonymous namespace

// ============================================
// NEW CODE — ALBW Port
// ============================================

bool dAlbwWolfCombat_isEnabled() {
    return albw_cfg_bool(g_wolf_combat, false);
}

u8 albw_wolf_get_charge_count() {
    return g_wolf_charge_count;
}

u8 albw_wolf_get_max_charges() {
    return albw_game::is_event_bit(dSv_event_flag_c::F_0814) ? 3 : 2;
}

void albw_wolf_spend_charge(u8 amount) {
    if (amount >= g_wolf_charge_count) {
        g_wolf_charge_count = 0;
    } else {
        g_wolf_charge_count -= amount;
    }
}

// ============================================
// NEW CODE — ALBW Port (Wolf Arts — howl shop-unlock)
// The Wolf Howl art is unlocked by a 100-rupee rental-shop purchase (per-save event bit 713).
// Mirrors the Focused-Arts-tier shop-row pattern.  The howl MOVE (built next) reads
// dAlbwWolfArts_isHowlUnlocked() to gate itself; a dev test toggle will bypass this during
// development.  STORY GATE: the shop row should also require Eldin/Kakariko Twilight cleared --
// left as a TODO until that milestone's exact save/event flag is pinned (see
// docs/wolf-combat-layers-research.md §R0).
// ============================================
namespace {
constexpr int kWolfHowlUnlockedBit = 713;  // per-save event bit (714/715 reserved: punch/giant)
constexpr int kWolfHowlShopPrice   = 100;
}  // namespace

bool dAlbwWolfArts_isHowlUnlocked() {
    return albw_game::is_event_bit(dSv_event_flag_c::saveBitLabels[kWolfHowlUnlockedBit]);
}

void dAlbwWolfArts_unlockHowl() {
    albw_game::on_event_bit(dSv_event_flag_c::saveBitLabels[kWolfHowlUnlockedBit]);
}

bool dAlbwWolfArts_shouldShowHowlShopRow() {
    // Show only while Wolf Combat is on and the howl isn't already unlocked.
    // STORY GATE (user-pinned 2026-07-15): requires the FIRST twilight
    // (Faron/Ordon, DarkClearLV bit 0) cleared.
    return dAlbwWolfCombat_isEnabled() && !dAlbwWolfArts_isHowlUnlocked() &&
           albw_game::is_dark_clear_lv(0);
}

int dAlbwWolfArts_getHowlShopPrice() {
    return kWolfHowlShopPrice;
}

const char* dAlbwWolfArts_getHowlShopName() {
    return "Wolf Howl";
}

const char* dAlbwWolfArts_getHowlShopDesc() {
    return "Don't tell anyone but...last night I dreamed I was visited by a giant squirrel "
           "and bird. They came close...and then I awoke to this scroll in my lap. Here, "
           "you take it.";
}

bool dAlbwWolfArts_tryPurchaseHowl() {
    if (dAlbwWolfArts_isHowlUnlocked()) {
        return false;
    }
    dAlbwWolfArts_unlockHowl();
    return true;
}

// ============================================
// NEW CODE — ALBW Port (Wolf Arts — Midna Arm shop-unlock, save event bit 714)
// Same pattern as the howl above.  STORY GATE: should also require Lanayru Twilight cleared --
// TODO until that milestone's flag is pinned (docs/wolf-combat-layers-research.md §R0).
// ============================================
namespace {
constexpr int kMidnaArmUnlockedBit = 714;  // per-save event bit (715 reserved: giant)
constexpr int kMidnaArmShopPrice   = 100;
}  // namespace

bool dAlbwWolfArts_isArmUnlocked() {
    return albw_game::is_event_bit(dSv_event_flag_c::saveBitLabels[kMidnaArmUnlockedBit]);
}

void dAlbwWolfArts_unlockArm() {
    albw_game::on_event_bit(dSv_event_flag_c::saveBitLabels[kMidnaArmUnlockedBit]);
}

bool dAlbwWolfArts_shouldShowArmShopRow() {
    // Show only while Wolf Combat is on and the arm isn't already unlocked.
    // STORY GATE (user-pinned 2026-07-15, supersedes the old Lanayru TODO):
    // requires the Eldin twilight (DarkClearLV bit 1) cleared.
    return dAlbwWolfCombat_isEnabled() && !dAlbwWolfArts_isArmUnlocked() &&
           albw_game::is_dark_clear_lv(1);
}

int dAlbwWolfArts_getArmShopPrice() {
    return kMidnaArmShopPrice;
}

const char* dAlbwWolfArts_getArmShopName() {
    return "Midna's Grasp";
}

const char* dAlbwWolfArts_getArmShopDesc() {
    return "It can't be real, it can't be...I h-had one of those dreams again. This time, "
           "I swear to you, a snake slithered and loomed over me. It told me to gift a "
           "blessing to a being of Twilight...please take this.";
}

bool dAlbwWolfArts_tryPurchaseArm() {
    if (dAlbwWolfArts_isArmUnlocked()) {
        return false;
    }
    dAlbwWolfArts_unlockArm();
    return true;
}

// ============================================
// NEW CODE — ALBW Port (Wolf Charge — 3rd pip shop unlock, F_0814)
// Story gate: Master Sword obtain cutscene (F_0264).
// ============================================
namespace {
constexpr int kWolfChargeShopPrice = 100;
}

bool dAlbwWolfArts_isChargeUpgradeUnlocked() {
    return albw_game::is_event_bit(dSv_event_flag_c::F_0814);
}

void dAlbwWolfArts_unlockChargeUpgrade() {
    albw_game::on_event_bit(dSv_event_flag_c::F_0814);
}

bool dAlbwWolfArts_shouldShowChargeShopRow() {
    return dAlbwWolfCombat_isEnabled() && !dAlbwWolfArts_isChargeUpgradeUnlocked() &&
           albw_game::is_event_bit(dSv_event_flag_c::F_0264);
}

int dAlbwWolfArts_getChargeShopPrice() {
    return kWolfChargeShopPrice;
}

const char* dAlbwWolfArts_getChargeShopName() {
    return "Wolf Charge";
}

const char* dAlbwWolfArts_getChargeShopDesc() {
    return "A blessing a blessing- I'm getting ahead of them this time! Here a former "
           "butcher of Old Kakariko gave me this dried meat as a gift a ha ha. A wolf "
           "they said, ha give this to a wolf if you see one!";
}

bool dAlbwWolfArts_tryPurchaseChargeUpgrade() {
    if (dAlbwWolfArts_isChargeUpgradeUnlocked()) {
        return false;
    }
    dAlbwWolfArts_unlockChargeUpgrade();
    return true;
}

u8 dAlbwWolfArts_getMaxCharges() {
    return dAlbwWolfArts_isChargeUpgradeUnlocked() ? 3 : 2;
}

// ============================================
// NEW CODE — ALBW Port (Midna Arm art — hair-reach visual bridge)
// The arm actor publishes its reach target; daAlink_c::setNeckAngle re-applies it into
// FLG1_MIDNA_HAIR_ATN_POS / mMidnaHairAtnPos after its own per-frame clear, so daMidna_c's hair
// reaches toward the strike point regardless of actor execute order.  Session-only.
// ============================================
namespace {
bool s_midnaArmReachActive   = false;
bool s_midnaArmReachStriking = false;
cXyz s_midnaArmReachPos(0.0f, 0.0f, 0.0f);
}  // namespace

void dAlbwMidnaArm_setReachPos(const cXyz& i_pos, bool i_striking) {
    s_midnaArmReachActive   = true;
    s_midnaArmReachStriking = i_striking;
    s_midnaArmReachPos      = i_pos;
}

void dAlbwMidnaArm_clearReachPos() {
    s_midnaArmReachActive   = false;
    s_midnaArmReachStriking = false;
}

bool dAlbwMidnaArm_getReachPos(cXyz* o_pos) {
    if (s_midnaArmReachActive && o_pos != NULL) {
        *o_pos = s_midnaArmReachPos;
    }
    return s_midnaArmReachActive;
}

bool dAlbwMidnaArm_isReachStriking() {
    return s_midnaArmReachStriking;
}

// ============================================
// BUG FIX (alpha cleanup): the list originally had fpcNm_E_MD_e commented
// "Shadow Beast (twilight messenger)" — but E_MD is the SUIT OF ARMOR.
// The REAL shadow beast is E_S1 (d_a_e_s1.h: "Shadow Beast" — resurrection
// howl, pack-finish, warp-appear, chest-mash). NOT E_SH, which is the
// STALHOUND (d_a_e_sh.h) — a normal night enemy that must stay freezable
// (its per-actor frozen-collider bridge exists for exactly that). Result
// of the old entry: shadow beasts fell to the non-twilight branch (0.25x
// + 300f freeze) instead of 0.70x-no-freeze, and the Suit of Armor was
// wrongly freeze-immune. Both consult sites (damage split + stun dispatch
// in cc_at_check) call this classifier, so this one entry fixes damage
// AND freeze. Keep the per-case comments — their earlier removal is how
// the mislabel survived review.
// ============================================
bool dAlbwWolfStun_isTwilightEnemy(s16 i_name) {
    switch (i_name) {
    case fpcNm_E_S1_e:       // Shadow Beast (twilit messenger)
    case fpcNm_E_YD_e:       // Twilight Deku Baba
    case fpcNm_E_YH_e:       // Twilight Hebi Baba
    case fpcNm_E_YD_LEAF_e:  // Twilight Deku Baba - Leaf
    case fpcNm_E_YMB_e:      // Twilight Insect Boss
    case fpcNm_E_YK_e:       // Twilight Keese
    case fpcNm_E_YR_e:       // Twilight Kargarok (standard solo dive-attacker)
    case fpcNm_E_YG_e:       // Twilight Vermin
    // ============================================
    // Coverage-campaign adds (verified vs dark-render + dark-vanish marker
    // sweep): twilight enemies take the 0.70x damage boost and NEVER freeze.
    // E_YC is the strongest add — it runs a shared scripted demo with its
    // E_RDY rider over Lake Hylia; freezing the carrier desynced the pair.
    // ============================================
    case fpcNm_E_YM_e:       // Shadow Insect (Vessel-of-Light bugs)
    case fpcNm_E_YC_e:       // Twilit Carrier Kargarok (bulblin-rider carrier)
    case fpcNm_E_RDY_e:      // Shadow Bulblin
        return true;
    default:
        return false;
    }
}

bool dAlbwWolfStun_isStunned(fopAc_ac_c* i_enemy) {
    if (i_enemy == NULL) {
        return false;
    }
    return findEntry(i_enemy->id) != NULL;
}

// ============================================
// Shared charge tally (alpha cleanup). Extracted from the inline block in
// cc_at_check (d_cc_uty.cpp) so enemies whose bite damage never flows
// through cc_at_check — hang-bite grabs and chest-mash internal health
// decrements — can award charge too. One source of truth for the tally,
// the cap, and the low-HP heal.
//
// Units are FIFTEENTHS of a charge (user-tuned economy):
//   normal bite      = 3/15  (so 5 bites = 1 charge, unchanged feel)
//   chest/mash hit   = 1/15  (15 mash hits = 1 charge)
// Fractions from both sources share one accumulator (mWolfBiteCount) and
// carry across a completed charge — EXCEPT when the completion lands on
// the charge cap (2, or 3 after Wolf Charge purchase), where the leftover
// fraction is dropped by design.
// ============================================
static u8 g_wolf_bite_steps_local = 0;

constexpr int kWolfChargeStepsPerCharge = 15;
constexpr int kWolfChargeBiteSteps = 3;
constexpr int kWolfChargeMashSteps = 1;

void addWolfChargeSteps(int i_steps) {
    if (!dAlbwWolfCombat_isEnabled() || !albw_game::is_wolf_form()) {
        return;
    }

    const u8 maxCharges = albw_wolf_get_max_charges();
    g_wolf_bite_steps = static_cast<u8>(g_wolf_bite_steps + i_steps);
    if (g_wolf_bite_steps >= kWolfChargeStepsPerCharge) {
        if (g_wolf_charge_count < maxCharges) {
            g_wolf_charge_count++;
            dAlbwWolfChargeHud_notify();
        }
        if (g_wolf_charge_count >= maxCharges) {
            g_wolf_bite_steps = 0;
        } else {
            g_wolf_bite_steps = static_cast<u8>(g_wolf_bite_steps - kWolfChargeStepsPerCharge);
        }
        const u16 curHP = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getLife();
        const u16 maxHP = albw_game::max_life_gauge();
        if (curHP * 2 <= maxHP) {
            g_dComIfG_gameInfo.play.setItemLifeCount(1.0f, 0);
        }
    }
}

void dAlbwWolfCombat_onBiteConnect() {
    addWolfChargeSteps(kWolfChargeBiteSteps);
}

void dAlbwWolfCombat_onChestMashHit() {
    addWolfChargeSteps(kWolfChargeMashSteps);
}

void dAlbwWolfCombat_fillCharges() {
    if (!dAlbwWolfCombat_isEnabled()) {
        return;
    }

    const u8 maxCharges = albw_wolf_get_max_charges();
    if (g_wolf_charge_count >= maxCharges && g_wolf_bite_steps == 0) {
        return;
    }

    g_wolf_charge_count = maxCharges;
    g_wolf_bite_steps = 0;
    dAlbwWolfChargeHud_notify();
}

void dAlbwWolfStun_syncColliders(fopAc_ac_c* i_enemy, cCcD_Obj* const* i_objs, int i_count) {
    // Bridge snapshots are needed for any active freeze (wolf combat OR lockout claw).
    if (i_enemy == NULL || i_objs == NULL || i_count <= 0) {
        return;
    }

    WolfStunEntry* entry = findEntry(i_enemy->id);
    if (entry == NULL) {
        return;
    }

    storeColliders(entry, i_objs, i_count);
    wolfStun_debugLog("f=%06d evt=sync-colliders id=%u count=%d\n", g_Counter.mCounter0,
                      i_enemy->id, entry->mColliderCount);
}

void dAlbwWolfStun_captureAfterExecute() {
    if (sStunCount == 0) {
        return;
    }

    cCcS* ccs = &g_dComIfG_gameInfo.play.mCcs;

    for (int i = 0; i < sStunCount; i++) {
        if (sStunList[i].mColliderCount > 0) {
            continue;
        }

        fopAc_ac_c* enemy =
            static_cast<fopAc_ac_c*>(fpcM_SearchByID(sStunList[i].mId));
        if (enemy == NULL) {
            continue;
        }

        const int before = sStunList[i].mColliderCount;
        captureCollidersFromCcS(ccs, enemy, &sStunList[i]);
        if (sStunList[i].mColliderCount > before) {
            wolfStun_debugLog("f=%06d evt=capture-after-exec id=%u count=%d\n",
                              g_Counter.mCounter0, sStunList[i].mId, sStunList[i].mColliderCount);
        }
    }
}

// ============================================
// Central freeze gate (coverage campaign). ALL freeze eligibility beyond the
// cc_at_check dispatch lives here so every caller (shared path + bespoke
// actor hooks like E_FK) inherits it, and so sweeping rule changes stay a
// one-place edit.
//
// Name exclusions (design): E_VT Death Sword (Zant-class invisibility/phase
// scripting) and E_PZ Phantom Zant (demo modes + illusion puzzle).
//
// State guards (per-actor audit): skip the freeze when the actor is in a
// state that pausing execute() would corrupt. Freeze simply doesn't apply;
// the damage/knockback of the triggering hit still lands normally.
// ============================================
static bool isFreezeExcludedName(s16 i_name) {
    return i_name == fpcNm_E_VT_e || i_name == fpcNm_E_PZ_e;
}

static bool isFreezeUnsafeState(fopAc_ac_c* i_enemy, s16 i_name) {
    switch (i_name) {
    case fpcNm_E_RD_e:
        // Mounted/joust bulblin: frozen rider desyncs from the boar.
        return reinterpret_cast<e_rd_class*>(i_enemy)->ride_mode != 0;
    case fpcNm_E_GI_e:
        // Active screamer owns a process-global (m_cry_gi) + camera force-
        // lock that only release in execute(); freezing it stalls both.
        return mod_daE_GI_isScreamOwner(i_enemy);
    case fpcNm_E_NZ_e:
        // Latched Ghoul Rat: freezing strands its stick-slot bit and a
        // floating invisible collider. ACTION_STICK == 3.
        return reinterpret_cast<e_nz_class*>(i_enemy)->mAction == 3;
    case fpcNm_E_KK_e:
        // Chilfos: shatter/death, thrown-lance, ironball-carry states.
        return mod_kkFreezeUnsafe(i_enemy);
    case fpcNm_E_GOB_e: {
        // Dangoro: never yank him out of ball roll (5), Link-grab (8), the
        // lava jump (9), or a demo-camera sequence — the same state list the
        // ALBW bash-knockdown uses.
        const e_gob_class* gob = reinterpret_cast<const e_gob_class*>(i_enemy);
        return gob->mAction == 5 || gob->mAction == 8 || gob->mAction == 9 ||
               gob->mDemoCamMode != 0;
    }
    case fpcNm_E_RDB_e: {
        // King Bulblin: his defeat fires on the knockdown COUNT while
        // health > 0 — freeze's exact firing condition — so freezing the
        // defeat-crossing knockdown would stall the defeat demo. Skip the
        // freeze while he is knocked down / defeated (mAction 6/7) and on
        // the final pre-defeat knockdown count (threshold-1). Paired with
        // the bridge-side health clamp so frozen hits can't finish him.
        const e_rdb_class* rdb = reinterpret_cast<const e_rdb_class*>(i_enemy);
        const bool castle = strcmp(albw_game::stage_name(), "D_MN09") == 0;
        const s8 lastKnockdown = castle ? 5 : 3;
        return rdb->mAction == 6 || rdb->mAction == 7 ||
               rdb->field_0xfcc >= lastKnockdown;
    }
    default:
        return false;
    }
}

static bool tryApplyStun(fopAc_ac_c* i_enemy, s16 i_timer, bool i_holdUntilThaw) {
    if (i_enemy == NULL || fopAcM_GetGroup(i_enemy) != fopAc_ENEMY_e) {
        return false;
    }

    const s16 applyName = fopAcM_GetName(i_enemy);
    // Lockout claw hold mirrors Midna freeze: twilight types stay unfrozen.
    if ((i_holdUntilThaw && dAlbwWolfStun_isTwilightEnemy(applyName)) ||
        isFreezeExcludedName(applyName) || isFreezeUnsafeState(i_enemy, applyName))
    {
        wolfStun_debugLog("f=%06d evt=apply-skip id=%u name=%d\n", g_Counter.mCounter0,
                          i_enemy->id, applyName);
        return false;
    }

    const fpc_ProcID id = i_enemy->id;

    WolfStunEntry* existing = findEntry(id);
    if (existing != NULL) {
        if (i_holdUntilThaw) {
            existing->mHoldUntilThaw = true;
            existing->mTimer = i_timer;
        } else if (!existing->mHoldUntilThaw) {
            existing->mTimer = i_timer;
        }
        if (existing->mColliderCount == 0) {
            captureCollidersFromCcS(albw_game::cc_system(), i_enemy, existing);
        }
        wolfStun_debugLog("f=%06d evt=stun-refresh id=%u coll=%d name=%d hold=%d\n",
                          g_Counter.mCounter0, id, existing->mColliderCount, applyName,
                          existing->mHoldUntilThaw ? 1 : 0);
        return true;
    }

    if (sStunCount >= WOLF_STUN_MAX) {
        wolfStun_debugLog("f=%06d evt=stun-list-full id=%u\n", g_Counter.mCounter0, id);
        return false;
    }

    fpcM_PauseEnable(i_enemy, 1);
    sStunList[sStunCount].mId = id;
    sStunList[sStunCount].mTimer = i_timer;
    sStunList[sStunCount].mHoldUntilThaw = i_holdUntilThaw;
    sStunList[sStunCount].mColliderCount = 0;
    sStunList[sStunCount].mSkipBridgeFrame = g_Counter.mCounter0;
    for (int c = 0; c < static_cast<int>(ARRAY_SIZEU(sStunList[sStunCount].mColliders)); c++) {
        sStunList[sStunCount].mColliders[c] = NULL;
    }
    sStunCount++;

    WolfStunEntry* entry = &sStunList[sStunCount - 1];
    captureCollidersFromCcS(albw_game::cc_system(), i_enemy, entry);
    wolfStun_debugLog(
        "f=%06d evt=stun-apply id=%u active=%d coll=%d name=%d hold=%d mpObj=%d skipBridge=%d\n",
        g_Counter.mCounter0, id, sStunCount, entry->mColliderCount, applyName,
        i_holdUntilThaw ? 1 : 0, albw_game::cc_system()->mObjCount, entry->mSkipBridgeFrame);
    if (entry->mColliderCount == 0) {
        wolfStun_debugLog("f=%06d evt=stun-apply-no-colliders id=%u (expect sync on cc_set)\n",
                          g_Counter.mCounter0, id);
    }
    return true;
}

void dAlbwWolfStun_apply(fopAc_ac_c* i_enemy) {
    if (!dAlbwWolfCombat_isEnabled()) {
        return;
    }
    tryApplyStun(i_enemy, static_cast<s16>(WOLF_STUN_FRAMES), false);
}

void dAlbwWolfStun_applyHold(fopAc_ac_c* i_enemy) {
    // Hold timer is a large sentinel; update never expires hold entries.
    tryApplyStun(i_enemy, 0x7FFF, true);
}

void dAlbwWolfStun_thaw(fopAc_ac_c* i_enemy) {
    if (i_enemy == NULL) {
        return;
    }

    for (int i = 0; i < sStunCount; i++) {
        if (sStunList[i].mId == i_enemy->id) {
            wolfStun_debugLog("f=%06d evt=stun-thaw id=%u\n", g_Counter.mCounter0, i_enemy->id);
            removeStunEntry(i);
            return;
        }
    }
}

void dAlbwWolfStun_update() {
    // Post-thaw zombie watchdog runs regardless of stun-list state.
    tickPostThawKills();

    // Process the list whenever anyone is frozen — including lockout claw holds
    // while Wolf Link Combat is off. Do NOT wipe the list when the setting is off.
    int writeIdx = 0;
    for (int i = 0; i < sStunCount; i++) {
        if (sStunList[i].mHoldUntilThaw) {
            sStunList[writeIdx++] = sStunList[i];
            continue;
        }

        sStunList[i].mTimer--;
        if (sStunList[i].mTimer > 0) {
            sStunList[writeIdx++] = sStunList[i];
        } else {
            restoreStunColliderFlags(&sStunList[i]);
            base_process_class* proc = fpcM_SearchByID(sStunList[i].mId);
            if (proc != NULL) {
                fpcM_PauseDisable(proc, 1);
                wolfStun_debugLog("f=%06d evt=stun-expire id=%u\n", g_Counter.mCounter0,
                                  sStunList[i].mId);
            }
        }
    }
    sStunCount = writeIdx;
}

void dAlbwWolfStun_beforeMove() {
    if (sStunCount == 0) {
        return;
    }

    cCcS* ccs = &g_dComIfG_gameInfo.play.mCcs;

    for (int i = 0; i < sStunCount; i++) {
        fopAc_ac_c* enemy =
            static_cast<fopAc_ac_c*>(fpcM_SearchByID(sStunList[i].mId));
        if (enemy == NULL) {
            removeStunEntry(i);
            i--;
            continue;
        }

        if (sStunList[i].mColliderCount == 0) {
            captureCollidersFromCcS(ccs, enemy, &sStunList[i]);
            if (sStunList[i].mColliderCount == 0) {
                wolfStun_debugLogNoColliders(sStunList[i].mId);
            }
        }

        prepareStunnedEnemyForBridge(enemy, &sStunList[i]);

        for (int c = 0; c < sStunList[i].mColliderCount; c++) {
            cCcD_Obj* obj = sStunList[i].mColliders[c];
            if (obj == NULL) {
                continue;
            }

            if (!obj->ChkTgSet()) {
                obj->OnTgSetBit();
            }

            if (!colliderRegistered(ccs, obj)) {
                ccs->Set(obj);
            }
        }
    }

    wolfStun_debugLogHeartbeat(ccs);
}

void dAlbwWolfStun_afterMove() {
    if (sStunCount == 0) {
        return;
    }

    for (int i = 0; i < sStunCount; i++) {
        fopAc_ac_c* enemy =
            static_cast<fopAc_ac_c*>(fpcM_SearchByID(sStunList[i].mId));
        if (enemy == NULL) {
            removeStunEntry(i);
            i--;
            continue;
        }

        if (sStunList[i].mSkipBridgeFrame == g_Counter.mCounter0) {
            wolfStun_debugLog("f=%06d evt=bridge-skip-same-frame id=%u\n", g_Counter.mCounter0,
                              enemy->id);
            continue;
        }

        for (int c = 0; c < sStunList[i].mColliderCount; c++) {
            cCcD_Obj* tgObj = sStunList[i].mColliders[c];
            if (tgObj == NULL) {
                continue;
            }

            dCcD_GObjInf* tgGObj = dCcD_GetGObjInf(tgObj);
            if (tgGObj == NULL || !tgGObj->ChkTgHit()) {
                continue;
            }

            cCcD_Obj* atObj = tgGObj->GetTgHitObj();
            if (atObj == NULL) {
                wolfStun_debugLog("f=%06d evt=tg-hit-no-at id=%u tgGObj=%p\n",
                                  g_Counter.mCounter0, enemy->id, tgGObj);
                continue;
            }

            processBridgeHit(enemy, atObj, tgObj);

            if (enemy->health <= 0) {
                // Arm the zombie watchdog BEFORE unpausing: the actor
                // either dies naturally in the grace window or despawns.
                armPostThawKill(sStunList[i].mId);
                removeStunEntry(i);
                i--;
                break;
            }
        }
    }
}

// ============================================
// NEW CODE ENDS HERE
// ============================================

namespace wolf_hooks {

DEFINE_HOOK(cc_at_check, CcAtCheck);
DEFINE_HOOK(&daAlink_c::execute, LinkExecute);
DEFINE_HOOK(&dCcS::Move, CcMove);
DEFINE_HOOK(&dMeter2Draw_c::draw, MeterDraw);

HookAction on_cc_at_check_pre(ModContext*, void* args, void*, void*) {
    if (!dAlbwWolfCombat_isEnabled()) {
        return HOOK_CONTINUE;
    }
    auto* enemy = mods::arg<fopAc_ac_c*>(args, 0);
    auto* info = mods::arg<dCcU_AtInfo*>(args, 1);
    if (enemy == nullptr || info == nullptr || info->mpCollider == nullptr ||
        info->mAttackPower <= 0 || fopAcM_GetGroup(enemy) != fopAc_ENEMY_e ||
        fopAcM_GetName(enemy) == fpcNm_B_ZANT_e || !albw_game::is_wolf_form())
    {
        return HOOK_CONTINUE;
    }

    if (info->mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK)) {
        if (dAlbwWolfStun_isTwilightEnemy(fopAcM_GetName(enemy))) {
            info->mAttackPower = (info->mAttackPower * 7) / 10;
            if (info->mAttackPower < 1) {
                info->mAttackPower = 1;
            }
        } else {
            info->mAttackPower = (info->mAttackPower * 25) / 100;
            if (info->mAttackPower < 1) {
                info->mAttackPower = 1;
            }
        }
    }
    return HOOK_CONTINUE;
}

void on_cc_at_check_post(ModContext*, void* args, void*, void*) {
    if (!dAlbwWolfCombat_isEnabled()) {
        return;
    }
    auto* enemy = mods::arg<fopAc_ac_c*>(args, 0);
    auto* info = mods::arg<dCcU_AtInfo*>(args, 1);
    if (enemy == nullptr || info == nullptr || info->mpCollider == nullptr ||
        fopAcM_GetGroup(enemy) != fopAc_ENEMY_e || fopAcM_GetName(enemy) == fpcNm_B_ZANT_e ||
        !albw_game::is_wolf_form())
    {
        return;
    }

    // fork d_cc_uty.cpp:732-741 - special D-pad ARTS never build charges: the
    // combat-howl AOE rides Link's own collider (AT_TYPE_WOLF_CUT_TURN, owner
    // ALINK) so it would otherwise pass this guard; exclude it via the
    // combat-howl flag (Link can't bite mid-howl, so the flag exactly
    // identifies howl-AOE hits). The Midna-arm art is a separate non-ALINK
    // actor and is excluded automatically (mHitType stays generic).
    if (info->mHitType == HIT_TYPE_LINK_NORMAL_ATTACK &&
        !info->mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK) &&
        !albw_wolf_combat_howl_active() && info->mAttackPower > 0)
    {
        dAlbwWolfCombat_onBiteConnect();
    }

    // fork d_cc_uty.cpp:505-515 - fixed hit powers for the two arts
    // (user-tuned 2026-07-15: 100 each).
    if (info->mpActor != NULL && fopAcM_GetName(info->mpActor) == 0x31A /* ALBW_MIDNA_ARM */) {
        info->mAttackPower = 100;
    } else if (info->mHitType == HIT_TYPE_LINK_NORMAL_ATTACK &&
               info->mpCollider->ChkAtType(AT_TYPE_WOLF_CUT_TURN) &&
               albw_wolf_combat_howl_active())
    {
        info->mAttackPower = 100;
    }

    if (info->mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK) && info->mAttackPower > 0 &&
        enemy->health > 0 && !dAlbwWolfStun_isTwilightEnemy(fopAcM_GetName(enemy)))
    {
        dAlbwWolfStun_apply(enemy);
    }
}

void on_link_execute_post(ModContext*, void*, void*, void*) {
    if (dAlbwWolfCombat_isEnabled()) {
        dAlbwWolfStun_update();
        dAlbwWolfStun_captureAfterExecute();
    } else {
        dAlbwWolfStun_update();
    }
}

HookAction on_cc_move_pre(ModContext*, void*, void*, void*) {
    dAlbwWolfStun_beforeMove();
    return HOOK_CONTINUE;
}

void on_cc_move_post(ModContext*, void*, void*, void*) { dAlbwWolfStun_afterMove(); }

void on_meter_draw_post(ModContext*, void*, void*, void*) { dAlbwWolfChargeHud_draw(); }

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace wolf_hooks

ModResult albw_wolf_combat_init(ModError* error) {
    using namespace wolf_hooks;
    if (!install(error, "WolfCcAtPre",
                 mods::hook_add_pre<CcAtCheck>(svc_hook, on_cc_at_check_pre)) ||
        !install(error, "WolfCcAtPost",
                 mods::hook_add_post<CcAtCheck>(svc_hook, on_cc_at_check_post)) ||
        !install(error, "WolfLinkExecute",
                 mods::hook_add_post<LinkExecute>(svc_hook, on_link_execute_post)) ||
        !install(error, "WolfCcMovePre",
                 mods::hook_add_pre<CcMove>(svc_hook, on_cc_move_pre)) ||
        !install(error, "WolfCcMovePost",
                 mods::hook_add_post<CcMove>(svc_hook, on_cc_move_post)) ||
        !install(error, "WolfChargeHud",
                 mods::hook_add_post<MeterDraw>(svc_hook, on_meter_draw_post)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_wolf_combat_shutdown(ModError*) {
    return MOD_OK;
}
