/**
 * Boss Refinement (Layer A) — port of fork d_albw_boss.h.
 * Gate: albw config `boss_refinement` (not dusk game.bossRefinement).
 */

#pragma once

#include "d/d_cc_d.h"
#include "f_op/f_op_actor.h"
#include "f_pc/f_pc_base.h"
#include "SSystem/SComponent/c_xyz.h"

struct dCcU_AtInfo;

bool dAlbwBoss_armogohmaShouldSuppressVanillaArrowDamage(fopAc_ac_c* i_boss,
                                                         dCcU_AtInfo* i_atInfo);

static constexpr int kAlbwBossWarpStageCount = 9;

enum AlbwBossArenaId {
    ALBW_BOSS_ARENA_INVALID = -1,
    ALBW_BOSS_ARENA_FOREST = 0,
    ALBW_BOSS_ARENA_GORON_MINES,
    ALBW_BOSS_ARENA_LAKEBED,
    ALBW_BOSS_ARENA_ARBITER,
    ALBW_BOSS_ARENA_SNOWPEAK,
    ALBW_BOSS_ARENA_TEMPLE_OF_TIME,
    ALBW_BOSS_ARENA_CITY_IN_SKY,
    ALBW_BOSS_ARENA_PALACE_OF_TWilight,
    ALBW_BOSS_ARENA_LAKEBED_WARP = 8,
};

bool dAlbwBossRefinement_isEnabled();

bool dAlbwBossRefinement_playerHasBossSword();
bool dAlbwBossRefinement_colliderCountsAsMasterSword(dCcD_GObjInf* i_collider);

AlbwBossArenaId dAlbwBoss_stageNameToArenaId(const char* i_stageName);

void dAlbwBoss_requestWarpBootstrap(const char* i_stageName);
void dAlbwBoss_applyPendingStageBootstrap();
bool dAlbwBoss_tryApplyActorBootstrap(s16 i_procName, fopAc_ac_c* i_actor);

class b_gm_class;

void dAlbwBoss_armogohmaResetFightState();
void dAlbwBoss_armogohmaEnsureInitialized(fopAc_ac_c* i_boss);
void dAlbwBoss_armogohmaOnBowCoreHit(fopAc_ac_c* i_boss);
bool dAlbwBoss_armogohmaIsOnCeiling(fopAc_ac_c* i_boss);
bool dAlbwBoss_armogohmaTakeCeilingDropPending();
void dAlbwBoss_armogohmaFillDisplayHp(fopAc_ac_c* i_boss, s16* o_current, s16* o_max);

struct dAlbwBoss_ArmogohmaBarState {
    bool visible;
    u8 phase;
    f32 fillRatio;
    s16 current;
    s16 max;
};

bool dAlbwBoss_armogohmaQueryHealthBar(dAlbwBoss_ArmogohmaBarState* o_state);
bool dAlbwBoss_diababaQueryHealthBar(int* o_current, int* o_max);

static constexpr int kAlbwDiababaBombReceiveDamage = 30;

void dAlbwBoss_diababaResetFightState();
void dAlbwBoss_diababaUpdatePhase(fopAc_ac_c* i_boss);
bool dAlbwBoss_diababaIsLatePhase();

bool dAlbwBoss_zantQueryHealthBar(int* o_current, int* o_max);
void dAlbwBoss_zantResetFightState();

bool dAlbwBoss_fyrusQueryHealthBar(int* o_current, int* o_max);

void dAlbwBoss_fyrusResetFightState();
void dAlbwBoss_fyrusUpdateGolemWindow(fopAc_ac_c* i_fm);
bool dAlbwBoss_fyrusGolemWindowIsLive();
bool dAlbwBoss_fyrusGolemKidsLoose();
bool dAlbwBoss_fyrusIsOurGolem(fpc_ProcID i_id);
bool dAlbwBoss_fyrusTryGolemLookPos(cXyz** o_pos);
void dAlbwBoss_fyrusTryFloorDown(fopAc_ac_c* i_fm);
void dAlbwBoss_fyrusClearGolemActor();
void dAlbwBoss_fyrusOnGolemKidsCleared();
bool dAlbwBoss_fyrusTakeResumeFightPending();
bool dAlbwBoss_fyrusStayHollow();
s16 dAlbwBoss_fyrusCreateHp();
s16 dAlbwBoss_fyrusKidCreateHp();

class e_fm_class;
bool dAlbwBoss_fyrusAblazePhase();
bool dAlbwBoss_fyrusHollowPhase();
bool dAlbwBoss_fyrusAblazeVulnOpen();
void dAlbwBoss_fyrusSyncFireVulnState(e_fm_class* i_fm);
void dAlbwBoss_fyrusOnAttackCommit(bool i_perfectParry);
void dAlbwBoss_fyrusOnAblazeVulnDamaged();
bool dAlbwBoss_fyrusShouldChipAblazeDamage();
void dAlbwBoss_fyrusApplyChipDamage(e_fm_class* i_fm, int i_hpBefore);

void dAlbwBoss_diababaOnPoisonSprayBegin();
void dAlbwBoss_diababaOnPoisonDamage(int i_damageToLink);
bool dAlbwBoss_diababaHitShouldSiphon(fopAc_ac_c* i_bq);
void dAlbwBoss_diababaOnSideHeadDamage();

void dAlbwBoss_diababaSetRetaliationPoison(bool i_active);
bool dAlbwBoss_diababaIsRetaliationPoison();
void dAlbwBoss_diababaSetPendingHangAfterAppear(bool i_pending);
bool dAlbwBoss_diababaTakePendingHangAfterAppear();
bool dAlbwBoss_diababaTakeChipLookMAlternate();

void dAlbwBoss_armogohmaOnRodHit(fopAc_ac_c* i_boss, s8 i_hitCount);
bool dAlbwBoss_armogohmaPhase3Damage(fopAc_ac_c* i_boss, int i_rawPower);
bool dAlbwBoss_armogohmaTryBeginEggPhase(b_gm_class* i_boss);

static constexpr int kAlbwMorpheelRingBombs = 12;
static constexpr f32 kAlbwMorpheelBubbleScale = 2.4f;
static constexpr f32 kAlbwMorpheelBubbleYOfs = -90.0f;
static constexpr f32 kAlbwMorpheelBubbleRadius = 195.0f;
static constexpr f32 kAlbwMorpheelLungeSpeed = 55.0f;
static constexpr f32 kAlbwMorpheelTentacleRootR = 160.0f;
static constexpr s16 kAlbwMorpheelGrabHoldFrames = 240;
static constexpr s16 kAlbwMorpheelGrabChipInterval = 60;
static constexpr int kAlbwMorpheelGrabChipDamage = 1;
static constexpr int kAlbwMorpheelTentacleCount = 8;
static constexpr s16 kAlbwMorpheelGrabGapMin = 60;
static constexpr s16 kAlbwMorpheelGrabGapMax = 120;
static constexpr f32 kAlbwMorpheelTentacleStrikeLength = 135.0f;
static constexpr u32 kAlbwMorpheelRingBombParam = 0xA1B00000u;

enum AlbwMorpheelRefPhase {
    ALBW_MORPHEEL_REF_OFF = 0,
    ALBW_MORPHEEL_REF_RING,
    ALBW_MORPHEEL_REF_CLAW,
    ALBW_MORPHEEL_REF_EXPOSED,
    ALBW_MORPHEEL_REF_HANDOFF,
};

void dAlbwBoss_morpheelResetFightState();
void dAlbwBoss_morpheelEnsureInit(fopAc_ac_c* i_boss);
bool dAlbwBoss_morpheelIsActive();
bool dAlbwBoss_morpheelFightIsLive();
void dAlbwBoss_morpheelMarkBombsSpawned();
void dAlbwBoss_morpheelSetFightLive(bool i_live);
AlbwMorpheelRefPhase dAlbwBoss_morpheelGetPhase();
bool dAlbwBoss_morpheelBubbleUp();
bool dAlbwBoss_morpheelEyeHookAllowed();
bool dAlbwBoss_morpheelEyeDamageAllowed();
void dAlbwBoss_morpheelOnEyeHooked();
void dAlbwBoss_morpheelOnEyeDepleted();
void dAlbwBoss_morpheelTick(fopAc_ac_c* i_boss);
void dAlbwBoss_morpheelTickBubbleLoad();
bool dAlbwBoss_morpheelDrawChuBubble(fopAc_ac_c* i_boss);
f32 dAlbwBoss_morpheelBubbleRadius();
bool dAlbwBoss_morpheelTryGetEyePos(cXyz* o_pos);
bool dAlbwBoss_morpheelTryRootTentacle(int i_slot, cXyz* o_pos, s16* o_homeYaw);
bool dAlbwBoss_morpheelSnapRingBomb(fopAc_ac_c* i_fish);
bool dAlbwBoss_morpheelIsRingBombParam(u32 i_param);
int dAlbwBoss_morpheelRingBombSlot(u32 i_param);
void dAlbwBoss_morpheelRequestTentacleGrab();
bool dAlbwBoss_morpheelConsumeTentacleGrab(int i_slot);
bool dAlbwBoss_morpheelTentacleGrabBusy();
void dAlbwBoss_morpheelNotifyTentacleStrikeMiss();
void dAlbwBoss_morpheelNotifyTentacleGrabCaught();
void dAlbwBoss_morpheelNotifyTentacleGrabHoldDone();
void dAlbwBoss_morpheelNotifyTentacleGrabDone();

bool dAlbwBoss_isArmogohmaWarpBootstrap();
void dAlbwBoss_onArmogohmaVictory();
void dAlbwBoss_onStageLoad();
