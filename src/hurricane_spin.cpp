// ============================================
// Hurricane Spin overlay - part of dev.albt.albw.
// Port of the fork's Focused-Arts Great-Spin finisher (d_a_alink_hurricane.inc) +
// its 3-layer spin SE (dusk/hurricane_test.cpp), extracted with tools/port/port_tool.py
// (hurricane.json / hurricane_se.json).
//
// The fork implemented this as new daAlink_c procs (PROC_CUT_GS_HURRICANE/_TIRED). The
// stock exe has no proc-table slot for those, so we OVERLAY: keep Link in the existing
// great-spin proc (PROC_CUT_TURN), skip that proc's per-frame logic via the meter's
// procCutTurn hook, and run the hurricane update ourselves for the duration, then exit
// to a wait proc. Every daAlink_c member/method the port touches is public in stock, so
// the fork's daAlink_c methods adapt to free functions taking `daAlink_c* link`.
// ============================================

#include "hurricane_spin.h"

#include "global.h"
#include "albw_common.h"
#include "focused_arts.h"
#include "modules.h"
#include "mods/svc/hook.hpp"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"

#include "JSystem/JAudio2/JAISe.h"
#include "JSystem/JAudio2/JASTrack.h"
#include "JSystem/JAudio2/JASChannel.h"
#include "JSystem/JAudio2/JASDSPChannel.h"
#include "JSystem/JAudio2/JASOscillator.h"
#include "Z2AudioLib/Z2LinkMgr.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "Z2AudioLib/Z2SoundObject.h"

namespace {

// ============================================
// audio:: aram-exclusion hooks are fork aurora-backend additions (dusk/audio/DuskDsp.hpp):
// the backend consults them during wave eviction. Stock's backend does not, so these are
// no-op stubs - the wave-loop force below still works; only the anti-eviction optimization
// is absent (a very long spin could, in theory, drop the loop; nothing worse).
// ============================================
namespace audio {
void excludeHurricaneSpinWaveAram(u32) {}
bool isHurricaneSpinWaveExcluded(u32) { return false; }
void registerHurricaneSpinWaveAram(u32) {}
void clearHurricaneSpinWaveRegistration() {}
void setHurricaneSpinSeLoopActive(bool) {}
}  // namespace audio

// fork hurricane_test.cpp SE-preset flag bits.
enum {
    HURRICANE_SE_ONE_SHOT = 1 << 0,
    HURRICANE_SE_NO_LOOP_PRIMARY = 1 << 1,
    HURRICANE_SE_NO_LOOP_SECONDARY = 1 << 2,
    HURRICANE_SE_NO_LOOP_TERTIARY = 1 << 3,
};

// Forward decls so the verbatim SE .inc (initHurricaneSpinSe calls maintain...) links.
void initHurricaneSpinSe(daAlink_c* link);
void maintainHurricaneSpinSe(daAlink_c* link);
void stopHurricaneSpinSe(daAlink_c* link);

// The full 3-layer spin SE (Zant + tornado + spinner, pitch/volume, forced wave loop),
// verbatim from the fork via port_tool.py - see tools/port/hurricane_se.json.
#include "hurricane_se_port.inc"

// ============================================
// Hurricane proc bodies - adapted from d_a_alink_hurricane.inc: daAlink_c methods -> free
// functions on `link`, and commonProcInit(PROC_CUT_GS_HURRICANE) -> the overlay (see below).
// ============================================

static constexpr int l_hurricaneDurationFrames = 300;
static constexpr int l_hurricaneTiredLockFrames = 60;
static constexpr f32 l_hurricaneCruiseSpeed = 15.0f;
static constexpr f32 l_hurricaneAnimRateScale = 3.5f;
static f32 l_hurricaneFrozenPoseFrame = 0.0f;

void hurricaneInitFrozenSpin(daAlink_c* i_link, daAlink_c::daAlink_ANM anmID,
                             const daAlinkHIO_anm_c* anm_data, f32 start_frame, int param_direction) {
    l_hurricaneFrozenPoseFrame = start_frame;
    i_link->setSingleAnime(anmID, 0.0f, start_frame, anm_data->mEndFrame, anm_data->mInterpolation);
    i_link->mUnderFrameCtrl[0].setRate(0.0f);
    i_link->mUnderFrameCtrl[0].setFrame(start_frame);
    i_link->mUnderFrameCtrl[0].offEndFlg();
    i_link->getNowAnmPackUnder(daAlink_c::UNDER_0)->setFrame(start_frame);

    const f32 attackSpan = i_link->field_0x3488 - i_link->field_0x3484;
    if (attackSpan > 0.0f) {
        const f32 rotPerFrame = 65536.0f * anm_data->mSpeed * l_hurricaneAnimRateScale / attackSpan;
        i_link->mProcVar2.field_0x300c = (s16)(param_direction == 1 ? -rotPerFrame : rotPerFrame);
    } else {
        i_link->mProcVar2.field_0x300c = 0;
    }
}

void hurricaneUpdateFrozenSpin(daAlink_c* i_link) {
    daPy_frameCtrl_c* frameCtrl = &i_link->mUnderFrameCtrl[0];
    frameCtrl->setRate(0.0f);
    frameCtrl->setFrame(l_hurricaneFrozenPoseFrame);
    frameCtrl->offEndFlg();
    i_link->getNowAnmPackUnder(daAlink_c::UNDER_0)->setFrame(l_hurricaneFrozenPoseFrame);
    i_link->shape_angle.y += i_link->mProcVar2.field_0x300c;
}

void hurricaneApplyMovement(daAlink_c* i_link) {
    f32 targetSpeed = l_hurricaneCruiseSpeed;
    if (i_link->checkInputOnR()) {
        cLib_addCalcAngleS(&i_link->current.angle.y, i_link->mMoveAngle, 4, 0x3000, 0x400);
        const f32 stickSpeed = i_link->mMaxSpeed * i_link->mMoveValue;
        if (stickSpeed > targetSpeed) {
            targetSpeed = stickSpeed;
        }
    }
    i_link->mNormalSpeed = targetSpeed;
    i_link->onModeFlg(1);
    i_link->mSpeedModifier = 0.0f;
}

// ============================================
// Overlay state machine (mod-side; replaces the fork's PROC_CUT_GS_HURRICANE state).
// ============================================
enum HurricanePhase { HP_NONE = 0, HP_SPIN, HP_TIRED };
HurricanePhase s_phase = HP_NONE;
int s_frames = 0;

// fork procCutGsHurricaneInit (minus the FA/test gate the caller already checked, and with
// commonProcInit(PROC_CUT_GS_HURRICANE) -> PROC_CUT_TURN so mProcID stays valid).
void hurricane_begin(daAlink_c* link, int param_direction) {
    const daAlinkHIO_cutTurn_c1* cutData = &link->mpHIO->mCut.mCutTurn.m;
    const daAlinkHIO_anm_c* anm_data;
    daAlink_c::daAlink_ANM anmID;
    f32 start_frame;

    link->commonProcInit(daAlink_c::PROC_CUT_TURN);
    link->resetCombo(TRUE);

    if (param_direction == 1) {
        anmID = daAlink_c::ANM_CUT_TURN_RIGHT;
        anm_data = &cutData->mRightTurnAnm;
        start_frame = cutData->mRightTurnInputStartFrame;
        link->field_0x3484 = cutData->mRightAttackStartFrame;
        link->field_0x3488 = cutData->mRightAttackEndFrame;
        link->mProcVar1.field_0x300a = 6;
        link->mProcVar4.field_0x3010 = 1;
        link->setCutType(daPy_py_c::CUT_TYPE_LARGE_TURN_RIGHT);
    } else {
        anmID = daAlink_c::ANM_CUT_TURN_LEFT;
        anm_data = &cutData->mLeftTurnAnm;
        start_frame = cutData->mLeftTurnInputStartFrame;
        link->field_0x3484 = cutData->mLeftAttackStartFrame;
        link->field_0x3488 = cutData->mLeftAttackEndFrame;
        link->mProcVar1.field_0x300a = 8;
        link->mProcVar4.field_0x3010 = 0;
        link->setCutType(daPy_py_c::CUT_TYPE_LARGE_TURN_LEFT);
    }

    link->field_0x3478 = cutData->mLargeAttackRadius;
    link->field_0x348c = cutData->mLargeAttackAccel;
    link->field_0x347c = link->field_0x3478;
    link->field_0x3480 = anm_data->mCancelFrame;
    link->field_0x3180 = 0;
    link->current.angle.y = link->shape_angle.y;
    link->field_0x2fe4 = link->shape_angle.y;
    link->field_0x2f98 = 0;
    link->mMaxSpeed = link->mpHIO->mItem.mSpinner.m.mRideSpeed;
    link->mNormalSpeed = l_hurricaneCruiseSpeed;
    link->mProcVar2.field_0x300c = 0;
    link->mProcVar5.field_0x3012 = param_direction;
    link->mLeftHandIndex = 100;
    link->onModeFlg(1);
    link->mSpeedModifier = 0.0f;

    hurricaneInitFrozenSpin(link, anmID, anm_data, link->field_0x3484, param_direction);

    if (!link->checkWoodSwordEquip()) {
        link->simpleAnmPlay(link->m_nSwordBtk);
    }

    link->setCutWaterDropEffect();
    link->initCutTurnAt(link->field_0x347c, 4);
    link->onNoResetFlg1(daAlink_c::FLG1_UNK_10000000);
    dComIfGp_setPlayerStatus0(0, 0x8000);
    link->onResetFlg0(daAlink_c::RFLG0_UNK_2);
    initHurricaneSpinSe(link);
    link->setCutTurnEffect();

    s_phase = HP_SPIN;
    s_frames = l_hurricaneDurationFrames;
}

// fork procCutGsHurricane per-frame (returns false to keep spinning, true when the spin
// duration ends and the tired phase should start).
bool hurricane_spin_frame(daAlink_c* link) {
    if (link->checkGroundSpecialMode()) {
        return false;
    }
    link->setShapeAngleToAtnActor(0);
    link->onNoResetFlg1(daAlink_c::FLG1_UNK_10000000);
    link->field_0x2f99 = 4;
    link->onEndResetFlg0(daAlink_c::ERFLG0_UNK_8000000);

    if (!link->checkWoodSwordEquip()) {
        link->simpleAnmPlay(link->m_nSwordBtk);
    }

    hurricaneUpdateFrozenSpin(link);
    maintainHurricaneSpinSe(link);
    hurricaneApplyMovement(link);
    link->field_0x2fe4 = link->shape_angle.y;

    link->mAtSph.ResetAtHit();
    link->onResetFlg0(daAlink_c::RFLG0_UNK_2);
    link->field_0x347c = link->field_0x3478;
    link->mAtSph.SetR(link->field_0x347c);

    s_frames--;
    if (s_frames <= 0) {
        stopHurricaneSpinSe(link);
        link->mAtSph.OffAtSetBit();
        link->field_0x2fd0 = 0;
        return true;
    }
    return false;
}

// fork procCutGsHurricaneTiredInit (commonProcInitNotSameProc(PROC_CUT_GS_HURRICANE_TIRED)
// dropped - we keep the overlay proc and just play the tired anim).
void hurricane_tired_begin(daAlink_c* link) {
    link->mNormalSpeed = 0.0f;
    s_frames = l_hurricaneTiredLockFrames;
    link->setSingleAnimeBase(daAlink_c::ANM_WAIT_TO_TIRED);
    s_phase = HP_TIRED;
}

// fork procCutGsHurricaneTired per-frame (returns true when finished -> exit overlay).
bool hurricane_tired_frame(daAlink_c* link) {
    daPy_frameCtrl_c* frameCtrl_p = link->mUnderFrameCtrl;
    if (link->checkGroundSpecialMode()) {
        return false;
    }
    link->mNormalSpeed = 0.0f;

    if (s_frames > 0) {
        s_frames--;
        if (link->checkAnmEnd(frameCtrl_p)) {
            link->setSingleAnimeBase(daAlink_c::ANM_WAIT_TIRED);
        } else if (frameCtrl_p->getAttribute() == 2) {
            link->setTiredVoice(frameCtrl_p);
        }
        return false;
    }
    return true;  // done -> caller exits to wait proc
}

}  // namespace

// ============================================
// Public overlay API (driven by the meter's procCutTurn hooks).
// ============================================
bool albw_hurricane_is_active() { return s_phase != HP_NONE; }

bool albw_hurricane_try_begin(daAlink_c* link) {
    if (link == nullptr) {
        return false;
    }
    if (s_phase != HP_NONE) {
        return true;  // already running - caller should keep skipping the stock proc
    }
    if (!dFocusedArts_isEnabled()) {
        return false;
    }
    // fork d_a_alink.cpp:13086 - only fire the hurricane when FA is on its final spend
    // charge. tryArm both decides and spends the FA finisher (onPlayerHiddenSkillUse);
    // consume clears the armed flag the fork's procCutGsHurricaneInit would have cleared.
    if (!dFocusedArts_tryArmGsHurricaneFinisher()) {
        return false;
    }
    dFocusedArts_consumeGsHurricaneFinisherArmed();
    hurricane_begin(link, link->getCutTurnDirection());
    return true;
}

bool albw_hurricane_tick(daAlink_c* link) {
    if (link == nullptr || s_phase == HP_NONE) {
        return false;
    }
    if (s_phase == HP_SPIN) {
        if (hurricane_spin_frame(link)) {
            hurricane_tired_begin(link);
        }
        return true;
    }
    // HP_TIRED
    if (hurricane_tired_frame(link)) {
        s_phase = HP_NONE;
        link->procWaitInit();
        return false;
    }
    return true;
}

// ============================================
// Per-frame overlay: while the hurricane owns Link's proc (mProcID == PROC_CUT_TURN),
// run the hurricane update and skip the stock great-spin proc. The BEGIN trigger lives
// in the meter's procCutTurnInit gate (gate_cut_turn_pre -> albw_hurricane_try_begin).
// ============================================
namespace {
DEFINE_HOOK(&daAlink_c::procCutTurn, HurricaneCutTurn);

HookAction on_cut_turn_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link != nullptr && albw_hurricane_is_active()) {
        albw_hurricane_tick(link);
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 1;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}
}  // namespace

ModResult albw_hurricane_init(ModError* error) {
    if (mods::hook::add_pre<HurricaneCutTurn>(on_cut_turn_pre) != MOD_OK) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, "failed to hook daAlink_c::procCutTurn (hurricane)");
        }
        mods::set_error(error, MOD_ERROR, "hurricane procCutTurn");
        return MOD_ERROR;
    }
    return MOD_OK;
}
