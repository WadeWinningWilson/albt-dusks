// ============================================
// NEW CODE - ALBW Port (Fyrus refinement phases 1/3 - the E_FM behavior layer)
//
// The brain (boss_refinement.cpp) and the transition guards (fyrus.cpp) were
// ported; this TU carries the fork's E_FM-side BEHAVIOR, which was not:
//
//   e_fm_albwPickGolemStunAnm / GolemHold  d_a_e_fm.cpp:481/:494  (phase 2 park)
//   e_fm_albwTexReadyForFireStart          :530   (hollow attack-start gate -
//         vanilla gates mode 0 on the FM tex frame, which never reads 0 while
//         hollow shows PUTOUT_WAIT, deadlocking every attack in phase 3)
//   e_fm_albwRestoreFightTex               :537
//   e_fm_albwTryResumeFight                :549   (phase 3 re-engage)
//   e_fm_albwQueueCommit / Resolve         :595   (SS10 one-frame deferred
//         attack commit -> dAlbwBoss_fyrusOnAttackCommit(perfect-parry))
//   e_fm_albwAblazeVulnStun                :625   (phase 1 vuln-hit stun)
//   attack/breath speed-ups                :1176/:1280/:1394/:3486
//   action() pre-dispatch golem block      :3190  (park/unpark + per-frame
//         TryResumeFight + SyncFireVulnState + TryFloorDown)
//   damage_check refinement core-hit tail  :2727  (ablaze stun entry, hollow
//         DAMAGE_RUN, and skipping vanilla's 10-chip A_DOWN health snap)
//
// Helper bodies are verbatim. In-function inserts land at hook seams:
// e_fm_n_fight / e_fm_f_fight / e_fm_fire are uniquely named symbols (hooked
// like fyrus.cpp already hooks their siblings); action / damage_check /
// effect_set are collision-prone file-statics, so their inserts run from the
// daE_FM_Execute pre-hook (which precedes vanilla action() -> damage_check())
// and post-hook (for the attack-FX re-speed), each equivalence noted.
//
// The fork's two added enum values are mirrored by declaration order, the same
// way fyrus.cpp mirrors kActionNormal: ACTION_ANM_PREVIEW=13,
// ACTION_ALBW_GOLEM_HOLD=14, ACTION_ALBW_ABLAZE_STUN=15 (stock's enum ends at
// ACTION_END=12, so vanilla's switch ignores 14/15 - which is exactly how the
// fork parks the boss). ACTION_ANM_PREVIEW itself and the look-pass cluster
// are NOT ported: the fork marks them "TEMP: orphan BCK look-pass (revert
// after pick)" - dev scaffolding for choosing the stun anims, whose pick
// (kStunAnm) is baked into PickGolemStunAnm below.
// ============================================

#include "global.h"
#include <os.h>

#include "SSystem/SComponent/c_math.h"
#include "d/d_com_inf_game.h"
#include "d/actor/d_a_e_fm.h"
#include "f_op/f_op_actor_mng.h"
#include "Z2AudioLib/Z2Instances.h"

#include "albw_common.h"
#include "albw_game.h"
#include "boss_refinement.h"
#include "fyrus.h"
#include "shield.h"
#include "albw_dusk_log.h"
#include "mods/hook.hpp"

#if TARGET_PC

namespace {

DEFINE_HOOK_SYMBOL("daE_FM_Execute", int(e_fm_class*), FmExecutePhases);
DEFINE_HOOK_SYMBOL("e_fm_n_fight", void(e_fm_class*), FmNFight);
DEFINE_HOOK_SYMBOL("e_fm_f_fight", void(e_fm_class*), FmFFight);
DEFINE_HOOK_SYMBOL("e_fm_fire", void(e_fm_class*), FmFire);

// Mirrors of the fork's TU-local enums (declaration order; see header note).
constexpr s16 kActionNormalF     = 0;
constexpr s16 kActionFightRun    = 1;
constexpr s16 kActionFFight      = 3;
constexpr s16 kActionFire        = 6;
constexpr s16 kActionDown        = 9;
constexpr s16 kActionADown       = 10;
constexpr s16 kActionStart       = 11;
constexpr s16 kActionEnd         = 12;
constexpr s16 kActionGolemHold   = 14;  // fork ACTION_ALBW_GOLEM_HOLD
constexpr s16 kActionAblazeStun  = 15;  // fork ACTION_ALBW_ABLAZE_STUN

constexpr u8 kTexFm         = 0;  // TEXANM_FM
constexpr u8 kTexAttack     = 1;  // TEXANM_ATTACK
constexpr u8 kTexPutOut     = 2;  // TEXANM_PUTOUT
constexpr u8 kTexPutOutWait = 3;  // TEXANM_PUTOUT_WAIT

constexpr int kBckAnimal       = 0x09;  // BCK_FM_ANIMAL
constexpr int kBckAttack       = 0x0B;  // BCK_FM_ATTACK
constexpr int kBckAttack02     = 0x0C;  // BCK_FM_ATTACK02
constexpr int kBckChance       = 0x0F;  // BCK_FM_CHANCE
constexpr int kBckChanceDamage = 0x10;  // BCK_FM_CHANCEDAMAGE
constexpr int kBckDamageL      = 0x12;  // BCK_FM_DAMAGE_L
constexpr int kBckDamageR      = 0x13;  // BCK_FM_DAMAGE_R

// Whip/blast +50%, breath +15% / lead-in 70% shorter (fork :369-384).
constexpr f32 kAttackAnimSpeed  = 1.5f;
constexpr f32 kBreathAnimSpeed  = 1.15f;
constexpr f32 kBreathLeadInMorf = 3.0f;

f32 attackAnimSpeed() { return dAlbwBossRefinement_isEnabled() ? kAttackAnimSpeed : 1.0f; }
f32 breathAnimSpeed() { return dAlbwBossRefinement_isEnabled() ? kBreathAnimSpeed : 1.0f; }
f32 breathLeadInMorf() { return dAlbwBossRefinement_isEnabled() ? kBreathLeadInMorf : 10.0f; }

// fork anm_init (d_a_e_fm.cpp:344), a file-static; one-line body inlined.
void fm_anm_init(e_fm_class* i_this, int i_anm, f32 i_morf, u8 i_mode, f32 i_speed) {
    i_this->mpFmModelMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("E_fm", i_anm), i_mode,
                                  i_morf, i_speed, 0.0f, -1.0f, NULL);
    i_this->mAnm = i_anm;
}

// ---------------------------------------------------------------------------
// fork helpers - verbatim.
// ---------------------------------------------------------------------------

int s_golemStunAnm = -1;  // fork s_albwFmGolemStunAnm

int e_fm_albwPickGolemStunAnm() {
    static const int kStunAnm[] = {kBckChance, kBckDamageL, kBckDamageR};
    int idx = (int)cM_rndF(3.0f);
    if (idx > 2) {
        idx = 2;
    }
    if (s_golemStunAnm >= 0 && kStunAnm[idx] == s_golemStunAnm) {
        idx = (idx + 1) % 3;
    }
    s_golemStunAnm = kStunAnm[idx];
    return s_golemStunAnm;
}

void e_fm_albwGolemHold(e_fm_class* i_this) {
    i_this->speedF = 0.0f;
    i_this->field_0x790 = 0;
    i_this->field_0x1829 = 0;
    i_this->field_0x770 = 0;
    i_this->mDoCreateBa = FALSE;

    if (i_this->field_0x792 != 0) {
        i_this->field_0x792 = 0;
    }

    switch (i_this->mMode) {
    case 0:
        i_this->mPlayTexAnmNo = kTexPutOut;
        i_this->mpFmBrk[kTexPutOut]->setFrame(0.0f);
        i_this->mpFmBtk[kTexPutOut]->setFrame(0.0f);
        fm_anm_init(i_this, e_fm_albwPickGolemStunAnm(), 5.0f, 0, 1.0f);
        i_this->mMode = 1;
        break;
    case 1:
        if (i_this->mPlayTexAnmNo == kTexPutOut) {
            mDoExt_brkAnm* putoutBrk = i_this->mpFmBrk[kTexPutOut];
            if (putoutBrk->getFrame() >= putoutBrk->getEndFrame() - 2.0f) {
                i_this->mPlayTexAnmNo = kTexPutOutWait;
                i_this->mpFmBrk[kTexPutOutWait]->setFrame(0.0f);
                i_this->mpFmBtk[kTexPutOutWait]->setFrame(0.0f);
            }
        }
        if (i_this->mpFmModelMorf->isStop()) {
            fm_anm_init(i_this, e_fm_albwPickGolemStunAnm(), 5.0f, 0, 1.0f);
        }
        break;
    }
}

bool e_fm_albwTexReadyForFireStart(e_fm_class* i_this) {
    if (dAlbwBoss_fyrusStayHollow() && i_this->field_0x792 == 0) {
        return (int)i_this->mpFmBtk[kTexPutOutWait]->getFrame() == 0;
    }
    return (int)i_this->mpFmBtk[kTexFm]->getFrame() == 0;
}

void e_fm_albwRestoreFightTex(e_fm_class* i_this) {
    if (dAlbwBoss_fyrusStayHollow()) {
        i_this->mPlayTexAnmNo = kTexPutOutWait;
        i_this->mpFmBrk[kTexPutOutWait]->setFrame(0.0f);
        i_this->mpFmBtk[kTexPutOutWait]->setFrame(0.0f);
    } else {
        i_this->mPlayTexAnmNo = kTexFm;
        i_this->mpFmBrk[kTexFm]->setFrame(0.0f);
        i_this->mpFmBtk[kTexFm]->setFrame(0.0f);
    }
}

bool e_fm_albwTryResumeFight(e_fm_class* i_this) {
    if (!dAlbwBossRefinement_isEnabled() || i_this->health <= 0) {
        return false;
    }
    if (!dAlbwBoss_fyrusHollowPhase()) {
        return false;
    }
    if (i_this->mAction == kActionEnd || i_this->mAction == kActionDown ||
        i_this->mAction == kActionADown || i_this->mAction == kActionStart ||
        i_this->mAction == kActionAblazeStun)
    {
        return false;
    }

    fopAc_ac_c* player = g_dComIfG_gameInfo.play.getPlayer(0);
    if (player == NULL || fopAcM_otherBgCheck(i_this, player)) {
        return false;
    }

    if (i_this->mAction == kActionGolemHold) {
        i_this->mAction = kActionNormalF;
        i_this->mMode = 0;
        i_this->mPlayTexAnmNo = kTexPutOutWait;
        i_this->mpFmBrk[kTexPutOutWait]->setFrame(0.0f);
        i_this->mpFmBtk[kTexPutOutWait]->setFrame(0.0f);
    }

    const bool passive = i_this->mAction == kActionNormalF ||
                         (i_this->mAction == kActionFFight && i_this->mMode == 0) ||
                         (i_this->mAction == kActionFire && i_this->mMode == 0);
    if (!passive) {
        return false;
    }

    i_this->mAction = kActionFightRun;
    i_this->mMode = 0;
    i_this->mTimers[0] = 0;
    i_this->mTimers[2] = 0;
    dAlbwBoss_fyrusSyncFireVulnState(i_this);
    e_fm_albwRestoreFightTex(i_this);
    return true;
}

// SS10: one-frame deferred commit resolve (shield perfect latched in dShield).
u8 s_commitPending = 0;

void e_fm_albwQueueCommit(u8 i_kind) {
    if (!dAlbwBoss_fyrusAblazePhase() || dAlbwBoss_fyrusAblazeVulnOpen()) {
        return;
    }
    s_commitPending = i_kind;
}

void e_fm_albwResolvePendingCommit() {
    if (s_commitPending == 0) {
        return;
    }
    const bool perfect = dShield_takeFyrusAttackPerfectParry();
    dAlbwBoss_fyrusOnAttackCommit(perfect);
    s_commitPending = 0;
}

void e_fm_albwAblazeVulnStun(e_fm_class* i_this) {
    i_this->speedF = 0.0f;
    i_this->field_0x790 = 0;
    i_this->field_0x1829 = 0;

    switch (i_this->mMode) {
    case 0:
        fm_anm_init(i_this, e_fm_albwPickGolemStunAnm(), 5.0f, 0, 1.0f);
        i_this->mMode = 1;
        break;
    case 1:
        if (i_this->mpFmModelMorf->isStop()) {
            dAlbwBoss_fyrusSyncFireVulnState(i_this);
            e_fm_albwRestoreFightTex(i_this);
            i_this->mAction = kActionFightRun;
            i_this->mMode = 0;
            i_this->mTimers[0] = 0;
            i_this->mTimers[2] = 0;
            i_this->mDamageInvulnerabilityTimer = 10;
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// Execute-seam wiring.
// ---------------------------------------------------------------------------

s16 s_prev790 = 0;  // for the effect_set FX re-speed (tickCrossed to 2)

// ============================================
// PROBE (multi-hypothesis, strip after the no-attack report is rooted): one
// line per CHANGE of (action,mode) plus gate verdicts, so a session shows the
// exact state machine Fyrus actually walked.
// ============================================
s16 s_probeAction = -99;
s16 s_probeMode   = -99;
void probeState(e_fm_class* fm, const char* where) {
    if (fm->mAction == s_probeAction && fm->mMode == s_probeMode) {
        return;
    }
    s_probeAction = fm->mAction;
    s_probeMode   = fm->mMode;
    DuskLog.info("[fyrus] {} action={} mode={} hp={} ablaze={} vulnOpen={} hollow={} texAnm={}",
                 where, (int)fm->mAction, (int)fm->mMode, (int)fm->health,
                 dAlbwBoss_fyrusAblazePhase(), dAlbwBoss_fyrusAblazeVulnOpen(),
                 dAlbwBoss_fyrusHollowPhase(), (int)fm->mPlayTexAnmNo);
}

// fork action() pre-dispatch block (:3190-3207) + damage_check tail (:2661) -
// runs BEFORE vanilla Execute -> action() -> damage_check(), same order as
// the fork (its block sits at action() entry, before the switch).
HookAction on_fm_execute_phases_pre(ModContext*, void* args, void*, void*) {
    auto* fm = mods::arg<e_fm_class*>(args, 0);
    if (fm == nullptr || !dAlbwBossRefinement_isEnabled()) {
        return HOOK_CONTINUE;
    }
    // ============================================
    // TEMP DIAG — FYRUS-FREEZE. probeState() only logs on action/mode CHANGE, so a
    // freeze (constant state) is invisible. Log the full state UNCONDITIONALLY every
    // ~30 frames so a stuck phase-1 Fyrus reveals which action/mode/anim he is parked
    // in, whether the attack morf is frozen (isStop), and whether a phase predicate
    // mis-fired. Parse tag: "FYRUS-TICK". STRIP before release.
    // ============================================
    {
        static u16 s_fzTick = 0;
        if ((s_fzTick++ % 30) == 0) {
            DuskLog.info(
                "[FYRUS-TICK] action={} mode={} anm={} morfFrame={} isStop={} hp={} "
                "golemWin={} ablaze={} vulnOpen={} hollow={}",
                (int)fm->mAction, (int)fm->mMode, (int)fm->mAnm,
                (int)fm->mpFmModelMorf->getFrame(), (int)fm->mpFmModelMorf->isStop(),
                (int)fm->health, dAlbwBoss_fyrusGolemWindowIsLive(),
                dAlbwBoss_fyrusAblazePhase(), dAlbwBoss_fyrusAblazeVulnOpen(),
                dAlbwBoss_fyrusHollowPhase());
        }
    }
    s_prev790 = fm->field_0x790;

    // fork damage_check:2661 - resolve the previous frame's queued commit.
    e_fm_albwResolvePendingCommit();

    // fork action():3190 - park/unpark under the golem window. (UpdateGolemWindow
    // itself is called by fyrus.cpp's earlier pre-hook on this same seam.)
    if (dAlbwBoss_fyrusGolemWindowIsLive()) {
        if (fm->mAction != kActionGolemHold && fm->mAction != kActionEnd) {
            fm->mAction = kActionGolemHold;
            fm->mMode = 0;
        }
    } else if (fm->mAction == kActionGolemHold) {
        fm->mAction = kActionNormalF;
        fm->mMode = 0;
        fm->mPlayTexAnmNo = kTexPutOutWait;
        fm->mpFmBrk[kTexPutOutWait]->setFrame(0.0f);
        fm->mpFmBtk[kTexPutOutWait]->setFrame(0.0f);
    }
    if (dAlbwBoss_fyrusHollowPhase()) {
        e_fm_albwTryResumeFight(fm);
    }
    dAlbwBoss_fyrusSyncFireVulnState(fm);
    dAlbwBoss_fyrusTryFloorDown(fm);

    // fork action() switch dispatch for the two added actions - stock's switch
    // has no case 14/15 (its enum ends at 12), so it no-ops and the per-frame
    // bodies run here instead, same frame, same order (dispatch precedes the
    // vanilla arms only for these two states, which vanilla never enters).
    if (fm->mAction == kActionGolemHold) {
        e_fm_albwGolemHold(fm);
    } else if (fm->mAction == kActionAblazeStun) {
        e_fm_albwAblazeVulnStun(fm);
    }
    probeState(fm, "exec");

    // fork damage_check:2720-2752 - the refinement core-hit tail beyond the
    // claims fyrus.cpp already makes (which handle ablaze-vuln damage + chip
    // and then clear the hit). What was missing: the ablaze STUN entry, the
    // hollow DAMAGE_RUN, and skipping vanilla's 10-chip A_DOWN health snap.
    if (fm->mCoreSph.ChkTgHit()) {
        fm->mAtInfo.mpCollider = fm->mCoreSph.GetTgHitObj();
        if (dAlbwBoss_fyrusAblazePhase() && dAlbwBoss_fyrusAblazeVulnOpen()) {
            cc_at_check(fm, &fm->mAtInfo);
            dAlbwBoss_fyrusOnAblazeVulnDamaged();
            dAlbwBoss_fyrusSyncFireVulnState(fm);
            fm->mPlayTexAnmNo = kTexFm;
            fm->mpFmBrk[kTexFm]->setFrame(0.0f);
            fm->mpFmBtk[kTexFm]->setFrame(0.0f);
            fm->mAction = kActionAblazeStun;
            fm->mMode = 0;
            fm->mSound.startCreatureVoice(Z2SE_EN_FM_V_CHANCEDAMAGE, -1);
            albw_game::on_event_bit(dSv_event_flag_c::saveBitLabels[254]);
        } else if (dAlbwBoss_fyrusShouldChipAblazeDamage()) {
            const int hpBefore = fm->health;
            cc_at_check(fm, &fm->mAtInfo);
            dAlbwBoss_fyrusApplyChipDamage(fm, hpBefore);
        } else if (dAlbwBoss_fyrusHollowPhase()) {
            cc_at_check(fm, &fm->mAtInfo);
            fm->field_0x804++;
            albw_game::on_event_bit(dSv_event_flag_c::saveBitLabels[254]);
            if (fm->mAction != kActionGolemHold && fm->health > 0) {
                fm->mAction = 4;  // ACTION_DAMAGE_RUN
                fm_anm_init(fm, kBckChanceDamage, 3.0f, 0, 1.0f);
                fm->mSound.startCreatureVoice(Z2SE_EN_FM_V_CHANCEDAMAGE, -1);
                fm->mMode = 0;
            }
        } else if (fm->mAction != kActionGolemHold && fm->health > 0) {
            cc_at_check(fm, &fm->mAtInfo);
            fm->mAction = 4;  // ACTION_DAMAGE_RUN
            fm_anm_init(fm, kBckChanceDamage, 3.0f, 0, 1.0f);
            fm->mSound.startCreatureVoice(Z2SE_EN_FM_V_CHANCEDAMAGE, -1);
            fm->mMode = 0;
            albw_game::on_event_bit(dSv_event_flag_c::saveBitLabels[254]);
        }
        // Claimed either way: vanilla's 10-chip A_DOWN counter (health snap to
        // 50) must never see a refinement hit - the fork's whole outer branch
        // replaces it.
        fm->mCoreSph.ClrTgHit();
    }
    return HOOK_CONTINUE;
}

// fork effect_set:3486 - the ATTACK blast FX morfs get the same +50% when
// field_0x790 crosses to 2. effect_set is a collision-prone file-static;
// vanilla's own setAnm at 1.0 ran this frame (frame 0), so re-speeding here is
// the same result one call later in the same frame.
void on_fm_execute_phases_post(ModContext*, void* args, void*, void*) {
    auto* fm = mods::arg<e_fm_class*>(args, 0);
    if (fm == nullptr || !dAlbwBossRefinement_isEnabled()) {
        return;
    }
    if (s_prev790 < 2 && fm->field_0x790 >= 2) {
        static const int ef_bck[] = {7, 8};
        const f32 fx_spd = attackAnimSpeed();
        for (int i = 0; i < 2; i++) {
            fm->mpAttackEfModelMorf[i]->setAnm(
                (J3DAnmTransform*)dComIfG_getObjectRes("E_fm", ef_bck[i]), 0, 1.0f, fx_spd, 0.0f,
                -1.0f, NULL);
            fm->mpAttackEfModelMorf[i]->setFrame(0.0f);
            fm->mpAttackEfBtk[i]->setFrame(0.0f);
        }
    }
}

// ---------------------------------------------------------------------------
// n_fight: whip speed-up (:1176) + commits 1/2 at frames 35/53 (:1200/:1219).
// checkFrame compares prev/cur play positions and is stateless, so re-asking
// it post-call answers the same as the fork's in-body call.
// ---------------------------------------------------------------------------
s16 s_nfPreMode = -1;

HookAction on_fm_n_fight_pre(ModContext*, void* args, void*, void*) {
    auto* fm = mods::arg<e_fm_class*>(args, 0);
    s_nfPreMode = (fm != nullptr) ? fm->mMode : -1;
    return HOOK_CONTINUE;
}

void on_fm_n_fight_post(ModContext*, void* args, void*, void*) {
    auto* fm = mods::arg<e_fm_class*>(args, 0);
    if (fm == nullptr || !dAlbwBossRefinement_isEnabled()) {
        return;
    }
    // fork :1176 - the whip anim started this call at 1.0; restart at 1.5
    // (same frame 0, same morf - identical to the fork's init-time speed).
    if (s_nfPreMode == 0 && fm->mMode == 1 && fm->mAnm == kBckAttack02) {
        fm_anm_init(fm, kBckAttack02, 10.0f, 0, attackAnimSpeed());
    }
    if (fm->mMode == 1 && fm->mAnm == kBckAttack02) {
        if (fm->mpFmModelMorf->checkFrame(35.0f)) {
            e_fm_albwQueueCommit(1);
        }
        if (fm->mpFmModelMorf->checkFrame(53.0f)) {
            e_fm_albwQueueCommit(2);
        }
    }
}

// ---------------------------------------------------------------------------
// f_fight: mode-0 gate + blast speed-up (:1275-1293), commit 3 (:1302).
// The hollow gate is load-bearing: vanilla arms mode 0 only when the FM tex
// frame reads 0, which never happens while hollow shows PUTOUT_WAIT - phase 3
// would deadlock with no attacks. When the fork's gate passes and vanilla's
// would not (or the speed differs), run the fork's mode-0 arm and skip.
// ---------------------------------------------------------------------------
HookAction on_fm_f_fight_pre(ModContext*, void* args, void*, void*) {
    auto* fm = mods::arg<e_fm_class*>(args, 0);
    if (fm == nullptr || !dAlbwBossRefinement_isEnabled() || fm->mMode != 0) {
        return HOOK_CONTINUE;
    }
    if (!e_fm_albwTexReadyForFireStart(fm)) {
        // PROBE: once per second while stuck at this gate - the "won't attack"
        // hypothesis that phase 1's FM tex frame never reads 0.
        static u16 sGateStuck = 0;
        if ((++sGateStuck % 30) == 1) {
            DuskLog.info("[fyrus] f_fight gate WAIT: texAnm={} fmBtkFrame={} powFrame={} hollow={}",
                         (int)fm->mPlayTexAnmNo, (int)fm->mpFmBtk[kTexFm]->getFrame(),
                         (int)fm->mpFmBtk[kTexPutOutWait]->getFrame(),
                         dAlbwBoss_fyrusStayHollow());
        }
        return HOOK_CONTINUE;
    }
    DuskLog.info("[fyrus] f_fight ARM (blast) spd={}", attackAnimSpeed());
    // fork f_fight mode-0 arm, verbatim (:1280-1293; unlike n_fight/fire the
    // stock fn body does NOT set field_0x7c0).
    const f32 atk_spd = attackAnimSpeed();
    fm_anm_init(fm, kBckAttack, 10.0f, 0, atk_spd);
    fm->mpFmBrk[kTexAttack]->setPlaySpeed(atk_spd);
    fm->mpFmBtk[kTexAttack]->setPlaySpeed(atk_spd);
    fm->mSound.startCreatureVoice(Z2SE_EN_FM_V_ATTACK_TAME, -1);
    fm->mSound.startCreatureSound(Z2SE_EN_FM_ATTACK_TAME, 0, -1);
    fm->mPlayTexAnmNo = kTexAttack;
    fm->mpFmBrk[kTexAttack]->setFrame(0.0f);
    fm->mpFmBtk[kTexAttack]->setFrame(0.0f);
    fm->mMode = 1;
    return HOOK_SKIP_ORIGINAL;
}

void on_fm_f_fight_post(ModContext*, void* args, void*, void*) {
    auto* fm = mods::arg<e_fm_class*>(args, 0);
    if (fm == nullptr || !dAlbwBossRefinement_isEnabled()) {
        return;
    }
    // fork :1302 - blast commit when field_0x790 flips to 1.
    if (s_prev790 == 0 && fm->field_0x790 == 1) {
        e_fm_albwQueueCommit(3);
    }
}

// ---------------------------------------------------------------------------
// fire: mode-0 gate + breath speed-up (:1387-1400), commit 4 (:1412).
// ---------------------------------------------------------------------------
s8 s_firePre1829 = 0;

HookAction on_fm_fire_pre(ModContext*, void* args, void*, void*) {
    auto* fm = mods::arg<e_fm_class*>(args, 0);
    if (fm == nullptr) {
        return HOOK_CONTINUE;
    }
    s_firePre1829 = fm->field_0x1829;
    if (!dAlbwBossRefinement_isEnabled() || fm->mMode != 0) {
        return HOOK_CONTINUE;
    }
    if (!e_fm_albwTexReadyForFireStart(fm)) {
        static u16 sFireStuck = 0;
        if ((++sFireStuck % 30) == 1) {
            DuskLog.info("[fyrus] fire gate WAIT: texAnm={} fmBtkFrame={}",
                         (int)fm->mPlayTexAnmNo, (int)fm->mpFmBtk[kTexFm]->getFrame());
        }
        return HOOK_CONTINUE;
    }
    DuskLog.info("[fyrus] fire ARM (breath) spd={}", breathAnimSpeed());
    // fork fire mode-0 arm, verbatim (:1391-1400).
    fm->field_0x7c0 = 1;
    fm->field_0x1830 = 0.0f;
    fm_anm_init(fm, kBckAnimal, breathLeadInMorf(), 0, breathAnimSpeed());
    fm->mMode = 1;
    fm->mSound.startCreatureVoice(Z2SE_EN_FM_V_GAOO_SHORT, -1);
    return HOOK_SKIP_ORIGINAL;
}

void on_fm_fire_post(ModContext*, void* args, void*, void*) {
    auto* fm = mods::arg<e_fm_class*>(args, 0);
    if (fm == nullptr || !dAlbwBossRefinement_isEnabled()) {
        return;
    }
    // fork :1412 - breath commit the frame the breath window opens
    // (field_0x1829 flips with field_0x1828 == 2; frame-exact via the flip,
    // avoiding the file-static l_HIO frame constant).
    if (s_firePre1829 == 0 && fm->field_0x1829 == 1 && fm->field_0x1828 == 2) {
        e_fm_albwQueueCommit(4);
    }
}

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}

}  // namespace

ModResult albw_fyrus_phases_init(ModError* error) {
    if (!install(error, "FmExecutePhasesPre",
                 mods::hook_add_pre<FmExecutePhases>(svc_hook, on_fm_execute_phases_pre)) ||
        !install(error, "FmExecutePhasesPost",
                 mods::hook_add_post<FmExecutePhases>(svc_hook, on_fm_execute_phases_post)) ||
        !install(error, "FmNFightPre", mods::hook_add_pre<FmNFight>(svc_hook, on_fm_n_fight_pre)) ||
        !install(error, "FmNFightPost",
                 mods::hook_add_post<FmNFight>(svc_hook, on_fm_n_fight_post)) ||
        !install(error, "FmFFightPre", mods::hook_add_pre<FmFFight>(svc_hook, on_fm_f_fight_pre)) ||
        !install(error, "FmFFightPost",
                 mods::hook_add_post<FmFFight>(svc_hook, on_fm_f_fight_post)) ||
        !install(error, "FmFirePre", mods::hook_add_pre<FmFire>(svc_hook, on_fm_fire_pre)) ||
        !install(error, "FmFirePost", mods::hook_add_post<FmFire>(svc_hook, on_fm_fire_post)))
    {
        return MOD_ERROR;
    }
    svc_log->info(mod_ctx, "albw fyrus phase 1/3 behavior ready");
    return MOD_OK;
}

#endif  // TARGET_PC

// ============================================
// NEW CODE ENDS HERE
// ============================================
