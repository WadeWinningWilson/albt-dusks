// ALBW Meter — P3 spends + lockout economy + vanilla oil/magic suppression (P4).
// Deferred (need deeper actor seams): Dom Rod confuse AI, double-claw finisher.
// External mod only; does not modify the ALBT fork tree.

#include "global.h"
#include "albw_symbols.h"

#include "SSystem/SComponent/c_cc_d.h"
#include "d/actor/d_a_arrow.h"
#include "d/actor/d_a_player.h"
#include "d/d_attention.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"

#define private public
#include "d/actor/d_a_alink.h"
#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"
#undef private

#include "d/d_meter2_info.h"
#include "d/d_meter_HIO.h"
#include "f_op/f_op_actor_mng.h"
#include "albw_game.h"
#include "albw_common.h"
#include "config_vars.h"
#include "lockout.h"
#include "meter.h"
#include "meter_bridge.h"
#include "mq_hearts.h"
#include "shield.h"
#include "shield_adapt.h"
#include "focused_arts.h"
#include "flurry_rush.h"
#include "mods/hook.hpp"

#include "d/d_item_data.h"
#include "d/d_save.h"

#include <algorithm>
#include <chrono>

bool albw_g_meter_locked = false;

namespace albw_meter_impl {

bool meter_enabled() {
    return albw_cfg_bool(g_meter_enabled, true);
}

constexpr int kBaseMax = 10900;
constexpr int kRecoverPer100ms = 136;
constexpr int kMaxDebt = -5450;
constexpr int kLockoutZTargetBaseRecoveryTicks = 70;
constexpr int kLockoutIdleBaseRecoveryTicks = 138;
constexpr int kLockoutExpandRecoveryTicks = 30;
constexpr s16 kVisualMeterBaseWidth = 32;

constexpr int kCostSword = 1817;
constexpr int kCostSidestep = 2180;
constexpr int kCostBackJump = 3633;
constexpr int kCostRoll = 3633;
constexpr int kCostArrow = 5450;
constexpr int kCostBomb = 5450;
constexpr int kCostBombArrow = 8175;
constexpr int kCostSling = 3633;
constexpr int kCostBoom = 2725;
constexpr int kCostHook = 2725;
constexpr int kCostDoubleHook = 1362;
constexpr int kCostDomRodPer100ms = 36;
constexpr int kCostSpinnerBase = 156;
constexpr int kCostHiddenSkill = 5450;

enum daAlink_CutFinishParamType {
    CUT_FINISH_PARAM_LEFT,
    CUT_FINISH_PARAM_VERTICAL,
    CUT_FINISH_PARAM_STAB,
    CUT_FINISH_PARAM_MORTAL_DRAW_A,
    CUT_FINISH_PARAM_MORTAL_DRAW_B,
    CUT_FINISH_PARAM_RIGHT,
};

int g_meter = kBaseMax;
int g_max = kBaseMax;
bool g_locked = false;
bool g_exhausted = false;
bool g_armor_depleted = false;

bool g_flag_sword = false;
bool g_flag_sidestep = false;
bool g_flag_backjump = false;
bool g_flag_roll = false;
bool g_flag_arrow = false;
bool g_flag_bomb = false;
bool g_flag_bomb_arrow = false;
bool g_flag_sling = false;
bool g_flag_boom = false;
bool g_flag_hook = false;
bool g_flag_double_hook = false;
bool g_flag_ironball = false;
bool g_flag_hidden_skill = false;
bool g_spinner_active = false;
bool g_domrod_active = false;

bool g_bow_had_arrow = false;
u8 g_bow_was_bomb = 0;
s16 g_prev_hook_mode = -1;
u8 g_prev_bomb_num = 0;
u8 g_prev_2fcf = 0;

struct OilSuppressSnap {
    s16 save_oil = 0;
    s16 save_max_oil = 0;
    s32 stolen_oil = 0;
    s32 stolen_max_oil = 0;
};

OilSuppressSnap g_oil_snap{};
bool g_armor_hit_pending = false;

std::chrono::steady_clock::time_point g_lastRecover{};
std::chrono::steady_clock::time_point g_lastSpinnerDrain{};
std::chrono::steady_clock::time_point g_lastDomRodDrain{};

bool g_player_idle = false;

dMeter2_c* g_cached_meter = nullptr;

void consume_one_shot_flags();
void push_albw_layout(dMeter2_c* meter);
void tick_continuous_and_recover();
void sync_save_oil_to_albw();
void apply_stolen_oil_to_albw();
void refresh_meter_max_from_progress();

static u16 player_save_oil() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getOil();
}

static u16 player_save_max_oil() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxOil();
}

static u8 player_save_max_magic() {
    return g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxMagic();
}

DEFINE_HOOK(&dMeter2_c::moveKantera, MoveKantera);
DEFINE_HOOK_SYMBOL(ALBT_SYM_SET_ITEM_MAGIC_COUNT, void(s16), SetItemMagicCount);
DEFINE_HOOK_SYMBOL(ALBT_SYM_FASTCREATE,
                   fopAc_ac_c*(s16, u32, const cXyz*, int, const csXyz*, const cXyz*, s8, createFunc,
                               void*, u32, u8),
                   MeterFastCreate);
DEFINE_HOOK(&dMeter2_c::alphaAnimeKantera, AlphaAnimeKantera);
DEFINE_HOOK(&dMeter2Draw_c::draw, MeterDraw);
DEFINE_HOOK(&dMeter2Draw_c::drawKanteraScreen, DrawKanteraScreen);

DEFINE_HOOK(&daAlink_c::procCutNormalInit, CutNormalInit);
DEFINE_HOOK(&daAlink_c::setCutDash, CutDash);
DEFINE_HOOK(&daAlink_c::procCutFinishInit, CutFinishInit);
DEFINE_HOOK(&daAlink_c::procCutJumpInit, CutJumpInit);
DEFINE_HOOK(&daAlink_c::procCutTurnInit, CutTurnInit);
DEFINE_HOOK(&daAlink_c::procCutTurnChargeInit, CutTurnChargeInit);
DEFINE_HOOK(&daAlink_c::procCutHeadInit, CutHeadInit);
DEFINE_HOOK(&daAlink_c::procCutLargeJumpChargeInit, CutLargeJumpChargeInit);
DEFINE_HOOK(&daAlink_c::procHorseCutInit, HorseCutInit);
DEFINE_HOOK(&daAlink_c::procHorseCutTurnInit, HorseCutTurnInit);
DEFINE_HOOK(&daAlink_c::procPickPut, PickPut);
DEFINE_HOOK(&daAlink_c::procCutFinishJumpUpInit, CutFinishJumpUpInit);
DEFINE_HOOK(&daAlink_c::procSideStepInit, SideStepInit);
DEFINE_HOOK(&daAlink_c::procFrontRollInit, FrontRollInit);
DEFINE_HOOK(&daAlink_c::procSideRollInit, SideRollInit);
DEFINE_HOOK(&daAlink_c::throwBoomerang, ThrowBoomerang);
DEFINE_HOOK(&daAlink_c::makeArrow, MakeArrow);
DEFINE_HOOK(&daAlink_c::setBowReadyAnime, SetBowReadyAnime);
DEFINE_HOOK(&daAlink_c::changeArrowType, ChangeArrowType);
DEFINE_HOOK(&daAlink_c::checkUpperItemActionBow, UpperBow);
DEFINE_HOOK(&daAlink_c::checkUpperItemActionHookshot, UpperHookshot);
DEFINE_HOOK(&daAlink_c::checkUpperItemActionBoomerang, UpperBoomerang);
DEFINE_HOOK(&daAlink_c::setItemActor, SetItemActor);
DEFINE_HOOK(&daAlink_c::procIronBallThrowInit, IronBallThrowInit);
DEFINE_HOOK(&daAlink_c::procSpinnerReadyInit, SpinnerReadyInit);
// note: IronBallThrowInit also gets a pre-hook for lockout gate
DEFINE_HOOK(&daAlink_c::procSpinnerWait, SpinnerWait);
DEFINE_HOOK(&daAlink_c::procCopyRodSubjectInit, CopyRodSubjectInit);
DEFINE_HOOK(&daAlink_c::procCopyRodSubject, CopyRodSubject);
DEFINE_HOOK(&daAlink_c::procCopyRodMove, CopyRodMove);
DEFINE_HOOK(&daAlink_c::checkRestHPAnime, CheckRestHPAnime);
DEFINE_HOOK(&daAlink_c::checkHeavyStateOn, CheckHeavyStateOn);
DEFINE_HOOK(&daAlink_c::procWait, ProcWait);
DEFINE_HOOK(&daAlink_c::checkMagicArmorNoDamage, CheckMagicArmorNoDamage);
DEFINE_HOOK(&daAlink_c::setDamagePoint, SetDamagePoint);
DEFINE_HOOK(&daAlink_c::getSpinnerRideSpeedF, SpinnerRideSpeedF);

bool is_wolf() {
    daPy_py_c* player = static_cast<daPy_py_c*>(g_dComIfG_gameInfo.play.getPlayer(0));
    return player != nullptr && player->checkWolf();
}

daAlink_c* as_alink(daPy_py_c* player) {
    return static_cast<daAlink_c*>(player);
}

bool draw_ready(dMeter2Draw_c* draw) {
    return draw != nullptr && draw->mpMagicParent != nullptr && draw->mpMagicMeter != nullptr &&
           draw->mpMagicFrameL != nullptr && draw->mpMagicFrameR != nullptr &&
           draw->mpMagicBase != nullptr && draw->mpKanteraScreen != nullptr;
}

void lockout_on_begin() { albw_lockout_on_begin(); }

void lockout_on_end() { albw_lockout_on_end(); }

void refresh_lock_state(bool play_dec) {
    if (g_meter <= 0) {
        const bool was_locked = g_locked;
        g_locked = true;
        g_exhausted = true;
        if (play_dec && !was_locked) {
            lockout_on_begin();
        }
    }
    if (g_exhausted && g_meter >= kBaseMax) {
        g_exhausted = false;
    }
    if (g_locked && g_meter >= g_max) {
        g_locked = false;
        lockout_on_end();
    }
    if (g_armor_depleted && g_meter >= kBaseMax) {
        g_armor_depleted = false;
    }
    if (g_meter > g_max) {
        g_meter = g_max;
    }
    albw_g_meter_locked = g_locked;
}

bool can_nock_arrow() {
    if (g_locked) {
        return albw_lockout_can_fire_bow();
    }
    return true;
}

bool can_nock_bomb_arrow() {
    if (g_locked) {
        return albw_lockout_can_fire_bomb_arrow();
    }
    return true;
}

void flush_pending_spends(dMeter2_c* meter) {
    if (!meter_enabled()) {
        return;
    }
    if (meter != nullptr) {
        g_cached_meter = meter;
    }
    consume_one_shot_flags();
    if (g_cached_meter != nullptr) {
        push_albw_layout(g_cached_meter);
    }
}

void add_base_fraction(int numerator, int denominator) {
    if (denominator <= 0) {
        return;
    }
    g_meter += (kBaseMax * numerator) / denominator;
    if (g_meter > g_max) {
        g_meter = g_max;
    }
    refresh_lock_state(false);
}

void sub_base_fraction(int numerator, int denominator) {
    if (denominator <= 0) {
        return;
    }
    g_meter -= (kBaseMax * numerator) / denominator;
    if (g_locked && g_meter < 0) {
        g_meter = 0;
    } else if (g_meter < kMaxDebt) {
        g_meter = kMaxDebt;
    }
    refresh_lock_state(false);
}

void drain_to_lockout() {
    g_meter = 0;
    refresh_lock_state(true);
}

void drain_meter(int amount) {
    if (dFocusedArts_shouldSuppressAlbwMeterDrain() || dFlurryRush_shouldSuppressAlbwSpend()) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    g_meter -= amount;
    if (g_locked) {
        if (g_meter < 0) {
            g_meter = 0;
        }
    } else if (g_meter < kMaxDebt) {
        g_meter = kMaxDebt;
    }
    // Lockout at empty: continuous drains (spinner) must not stall passive recovery.
    if (!g_locked || g_meter > 0) {
        g_lastRecover = now;
    }
    refresh_lock_state(true);
}

void signal_drain(bool& flag) {
    flag = true;
    flush_pending_spends(nullptr);
}

bool can_sword_agility() { return !g_locked; }
bool can_hidden_skill() { return !g_locked; }
bool can_spinner() { return g_locked || g_meter > 0; }
bool can_domrod() { return g_locked || g_meter > 0; }
bool can_armor_block() { return !g_armor_depleted && g_meter > 0; }
bool can_lockout_bow() { return albw_lockout_can_fire_bow(); }
bool can_lockout_bomb_arrow() { return albw_lockout_can_fire_bomb_arrow(); }
bool can_ironball() {
    if (!g_locked) {
        return true;
    }
    return g_meter >= (g_max * 93) / 100;
}

bool lockout_z_target_recovery() {
    dAttention_c* attn = g_dComIfG_gameInfo.play.getAttention();
    return attn != nullptr && attn->Lockon();
}

int lockout_z_target_base_rate() {
    return kBaseMax / kLockoutZTargetBaseRecoveryTicks;
}

int lockout_idle_base_rate() {
    return (kBaseMax + kLockoutIdleBaseRecoveryTicks / 2) / kLockoutIdleBaseRecoveryTicks;
}

int lockout_expand_rate() {
    const int expansion = g_max - kBaseMax;
    if (expansion <= 0) {
        return lockout_idle_base_rate();
    }
    return expansion / kLockoutExpandRecoveryTicks;
}

int lockout_recovery_rate() {
    const int baseRate =
        lockout_z_target_recovery() ? lockout_z_target_base_rate() : lockout_idle_base_rate();
    if (g_meter < 0) {
        const int debtRate = (baseRate * 5) / 3;
        return debtRate < 1 ? 1 : debtRate;
    }
    if (g_meter < kBaseMax) {
        return baseRate;
    }
    return lockout_expand_rate();
}

bool hud_should_hide(dMeter2_c* meter) {
    if (meter == nullptr) {
        return true;
    }
    if (g_dComIfG_gameInfo.play.isPauseFlag() || g_dComIfG_gameInfo.play.isHeapLockFlag() == 6) {
        return true;
    }
    if (is_wolf()) {
        return true;
    }

    daPy_py_c* player = static_cast<daPy_py_c*>(g_dComIfG_gameInfo.play.getPlayer(0));
    if (player != nullptr) {
        if (player->getSumouMode() != 0) {
            return true;
        }
        if (player->checkCanoeSlider() &&
            (g_dComIfG_gameInfo.play.getTimerMode() == 3 ||
             g_dComIfG_gameInfo.play.getTimerMode() == 4))
        {
            return true;
        }
    }

    const u32 status = meter->mStatus;
    if ((status & 0x4000) || (status & 0x100000) || (status & 0x80000000) || (status & 8) ||
        (status & 0x10) || (status & 0x01000000) || (status & 0x20) || (status & 0x04000000) ||
        (status & 0x08000000) || (status & 0x10000000))
    {
        return true;
    }
    if ((status & 0x40) && g_dComIfG_gameInfo.play.getEvent()->checkHind(0x400)) {
        return true;
    }
    if (g_dComIfG_gameInfo.play.getOxygenShowFlag()) {
        return true;
    }
    return false;
}

void set_magic_anime_min(dMeter2Draw_c* draw) {
    if (draw->field_0x742[0] <= 0) {
        draw->mMeterAlphaRate[0] = 0.0f;
    } else {
        draw->field_0x742[0]--;
        if (draw->field_0x742[0] < 0) {
            draw->field_0x742[0] = 0;
        }
        draw->mMeterAlphaRate[0] = (draw->field_0x742[0] / 5.0f) * g_drawHIO.mParentAlpha;
    }
}

void set_magic_anime_max(dMeter2Draw_c* draw) {
    if (draw->field_0x742[0] >= 5) {
        draw->mMeterAlphaRate[0] = g_drawHIO.mParentAlpha;
    } else {
        draw->field_0x742[0]++;
        if (draw->field_0x742[0] > 5) {
            draw->field_0x742[0] = 5;
        }
        draw->mMeterAlphaRate[0] = (draw->field_0x742[0] / 5.0f) * g_drawHIO.mParentAlpha;
    }
}

void consume_one_shot_flags() {
    if (g_flag_bomb_arrow) {
        if (g_locked) {
            if (albw_lockout_can_fire_bomb_arrow()) {
                albw_lockout_on_bomb_arrow_fired();
            }
        } else {
            drain_meter(kCostBombArrow);
        }
        g_flag_bomb_arrow = false;
    } else if (g_flag_arrow) {
        if (g_locked) {
            if (albw_lockout_can_fire_bow()) {
                albw_lockout_on_arrow_fired();
            }
        } else {
            drain_meter(kCostArrow);
        }
        g_flag_arrow = false;
    } else if (g_flag_bomb) {
        if (!g_locked) {
            drain_meter(kCostBomb);
        }
        g_flag_bomb = false;
    } else if (g_flag_boom) {
        if (g_locked) {
            add_base_fraction(3, 10);  // +30% base
        } else {
            drain_meter(kCostBoom);
        }
        g_flag_boom = false;
    } else if (g_flag_sling) {
        if (!g_locked) {
            drain_meter(kCostSling);
        }
        g_flag_sling = false;
    } else if (g_flag_ironball) {
        if (!g_locked) {
            drain_meter(g_max / 2);
        }
        g_flag_ironball = false;
    } else if (g_flag_hook) {
        if (g_locked) {
            albw_lockout_on_hookshot_fired();
        } else {
            drain_meter(kCostHook);
        }
        g_flag_hook = false;
    } else if (g_flag_double_hook) {
        if (!g_locked) {
            drain_meter(kCostDoubleHook);
        }
        g_flag_double_hook = false;
    } else if (g_flag_sword) {
        drain_meter(kCostSword);
        g_flag_sword = false;
    } else if (g_flag_sidestep) {
        drain_meter(kCostSidestep);
        g_flag_sidestep = false;
    } else if (g_flag_backjump) {
        drain_meter(kCostBackJump);
        g_flag_backjump = false;
    } else if (g_flag_roll) {
        drain_meter(kCostRoll);
        g_flag_roll = false;
    } else if (g_flag_hidden_skill) {
        if (!dFocusedArts_shouldSuppressHiddenSkillAlbw()) {
            drain_meter(kCostHiddenSkill);
        }
        g_flag_hidden_skill = false;
    }
}

void tick_continuous_and_recover() {
    const auto now = std::chrono::steady_clock::now();
    if (g_dComIfG_gameInfo.play.isPauseFlag() || g_dComIfG_gameInfo.play.isHeapLockFlag() == 6) {
        g_lastRecover = now;
        g_lastSpinnerDrain = now;
        g_lastDomRodDrain = now;
        return;
    }

    consume_one_shot_flags();

    if (g_spinner_active) {
        const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastSpinnerDrain).count();
        if (ms >= 100) {
            const int rate = (g_max <= 10900) ? kCostSpinnerBase : (g_max <= 16350) ? 164 : 88;
            drain_meter(rate);
            g_lastSpinnerDrain = now;
        }
        g_spinner_active = false;
    }
    if (g_domrod_active) {
        if (!g_locked) {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                                  g_lastDomRodDrain)
                                .count();
            if (ms >= 100) {
                drain_meter(kCostDomRodPer100ms);
                g_lastDomRodDrain = now;
            }
        }
        g_domrod_active = false;
    }

    daPy_py_c* player = static_cast<daPy_py_c*>(g_dComIfG_gameInfo.play.getPlayer(0));
    if (player != nullptr && player->checkPlayerGuard()) {
        g_lastRecover = now;
        return;
    }

    int rate = g_locked ? lockout_recovery_rate() : kRecoverPer100ms;
    if (!g_locked && g_player_idle) {
        rate = (rate * 105) / 100;
    }
    g_player_idle = false;

    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastRecover).count();
    if (ms >= 100 && g_meter < g_max) {
        g_meter += rate;
        if (g_meter > g_max) {
            g_meter = g_max;
        }
        g_lastRecover = now;
        refresh_lock_state(false);
    }
}

constexpr int kHeartArmorTierBonus = 8480;
constexpr int kDungeonClearBonus = 1817;

static const int kDungeonClearBitIdx[] = {55, 64, 78, 265, 266, 267, 268, 570};

int count_dungeon_clears() {
    int count = 0;
    for (int bitIdx : kDungeonClearBitIdx) {
        if (g_dComIfG_gameInfo.info.getEvent().isEventBit(dSv_event_flag_c::saveBitLabels[bitIdx]))
        {
            count++;
        }
    }
    return count;
}

int compute_meter_max() {
    int maxVal = kBaseMax;
    if (g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxLife() >= 50) {
        maxVal += kHeartArmorTierBonus;
    }
    if (albw_game::is_item_first_bit(dItemNo_ARMOR_e)) {
        maxVal += kHeartArmorTierBonus;
    }
    maxVal += count_dungeon_clears() * kDungeonClearBonus;
    // Master Quest stamina shop purchases (event reg 101) — shop UI is Tier B+.
    maxVal += albw_mq_meter_bonus_units();
    return maxVal;
}

void refresh_meter_max_from_progress() {
    const int newMax = compute_meter_max();
    if (newMax > g_max) {
        g_meter += newMax - g_max;
    }
    g_max = newMax;
    if (g_meter > g_max) {
        g_meter = g_max;
    }
}

void sync_save_oil_to_albw() {
    const s32 nativeMax =
        static_cast<s32>(g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxOil());
    if (nativeMax <= 0 || g_max <= 0) {
        return;
    }
    const int display = g_meter < 0 ? 0 : g_meter;
    const s32 mapped = (display * nativeMax) / g_max;
    const u16 clamped = static_cast<u16>(std::min(mapped, nativeMax));
    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setOil(clamped);
}

void apply_stolen_oil_to_albw() {
    s32 oilCount = g_oil_snap.stolen_oil;
    const s32 maxOilCount = g_oil_snap.stolen_max_oil;
    if (maxOilCount != 0) {
        const s32 cap = player_save_max_oil();
        s32 newMax = g_oil_snap.save_max_oil + maxOilCount;
        if (newMax > cap) {
            newMax = cap;
        } else if (newMax < 0) {
            newMax = 0;
        }
        oilCount += newMax - g_oil_snap.save_oil;
    }
    if (oilCount == 0) {
        return;
    }

    const s32 nativeMax = g_oil_snap.save_max_oil > 0 ? g_oil_snap.save_max_oil : 1;
    if (oilCount < 0) {
        drain_meter((static_cast<s32>(-oilCount) * g_max) / nativeMax);
        dShield_repairDurabilityFraction(1, 5);
    } else {
        g_meter += (oilCount * g_max) / nativeMax;
        if (g_meter > g_max) {
            g_meter = g_max;
        }
        refresh_lock_state(false);
    }
}

void sync_kantera_display(dMeter2_c* meter) {
    const int display = g_meter < 0 ? 0 : g_meter;
    const s32 nativeMax =
        static_cast<s32>(g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxOil());
    meter->mNowOil = (nativeMax > 0 && g_max > 0)
                         ? static_cast<s32>((f32)display / (f32)g_max * (f32)nativeMax)
                         : 0;
    meter->mMaxOil = nativeMax;
    g_dComIfG_gameInfo.play.setItemNowOil(meter->mNowOil);
}

void push_albw_layout(dMeter2_c* meter) {
    dMeter2Draw_c* draw = meter->getMeterDrawPtr();
    if (!draw_ready(draw)) {
        return;
    }
    sync_kantera_display(meter);
    const s16 barMax = (g_max > 0)
                           ? static_cast<s16>((f32)kVisualMeterBaseWidth * (f32)g_max / (f32)kBaseMax)
                           : kVisualMeterBaseWidth;
    const int display = g_meter < 0 ? 0 : g_meter;
    const s16 barCur =
        (g_max > 0) ? static_cast<s16>((f32)display / (f32)g_max * (f32)barMax) : static_cast<s16>(0);
    const f32 x_pos = g_drawHIO.mLanternMeterPosX;
    const f32 y_pos = g_drawHIO.mLanternMeterPosY;
    draw->drawKantera(meter->mMaxOil, meter->mNowOil, x_pos, y_pos);
    draw->drawMagic(barMax, barCur, x_pos, y_pos);
    draw->setAlphaMagicChange(false);
}

// --- HUD / vanilla economy suppression ---

HookAction on_move_kantera_pre(ModContext*, void*, void*, void*) {
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }

    g_oil_snap.save_oil = player_save_oil();
    g_oil_snap.save_max_oil = player_save_max_oil();
    g_oil_snap.stolen_oil = g_dComIfG_gameInfo.play.getItemOilCount();
    g_oil_snap.stolen_max_oil = g_dComIfG_gameInfo.play.getItemMaxOilCount();
    if (g_oil_snap.stolen_oil != 0) {
        g_dComIfG_gameInfo.play.clearItemOilCount();
    }
    if (g_oil_snap.stolen_max_oil != 0) {
        g_dComIfG_gameInfo.play.clearItemMaxOilCount();
    }

    // Keep save-oil aligned with ALBW so stock moveKantera skips oil tween/sfx.
    sync_save_oil_to_albw();
    return HOOK_CONTINUE;
}

void on_move_kantera_post(ModContext*, void* args, void*, void*) {
    if (!meter_enabled()) {
        return;
    }
    auto* meter = mods::arg<dMeter2_c*>(args, 0);
    if (meter == nullptr) {
        return;
    }
    g_cached_meter = meter;
    refresh_meter_max_from_progress();
    apply_stolen_oil_to_albw();
    sync_save_oil_to_albw();
    tick_continuous_and_recover();
    push_albw_layout(meter);
}

HookAction on_set_item_magic_pre(ModContext*, void* args, void*, void*) {
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }

    const s16 count = mods::arg<s16>(args, 0);
    if (count != 0) {
        const u8 nativeMax = player_save_max_magic();
        if (nativeMax > 0) {
            if (count > 0) {
                g_meter += (static_cast<s32>(count) * g_max) / nativeMax;
                if (g_meter > g_max) {
                    g_meter = g_max;
                }
                refresh_lock_state(false);
            } else {
                drain_meter((static_cast<s32>(-count) * g_max) / nativeMax);
            }
        }
    }
    return HOOK_SKIP_ORIGINAL;
}

void on_fast_create_post(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled() || retval == nullptr) {
        return;
    }
    if (*static_cast<fopAc_ac_c**>(retval) == nullptr) {
        return;
    }
    const s16 procName = mods::arg<s16>(args, 0);
    const u32 parameters = mods::arg<u32>(args, 1);
    // makeSlingStone() in d_a_arrow.h (param 0x401).
    if (procName == fpcNm_ARROW_e && parameters == 0x401) {
        signal_drain(g_flag_sling);
    }
}

void on_alpha_kantera_post(ModContext*, void* args, void*, void*) {
    if (!meter_enabled()) {
        return;
    }
    auto* meter = mods::arg<dMeter2_c*>(args, 0);
    if (meter == nullptr) {
        return;
    }
    dMeter2Draw_c* draw = meter->getMeterDrawPtr();
    if (!draw_ready(draw)) {
        return;
    }
    draw->setAlphaKanteraAnimeMin();
    draw->setAlphaKanteraChange(true);
    if (hud_should_hide(meter)) {
        set_magic_anime_min(draw);
    } else {
        set_magic_anime_max(draw);
    }
    draw->setAlphaMagicChange(true);
}

HookAction on_draw_kantera_screen_pre(ModContext*, void* args, void*, void*) {
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }
    if (mods::arg<u8>(args, 1) == 1) {
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void on_meter_draw_post(ModContext*, void* args, void*, void*) {
    if (!meter_enabled()) {
        return;
    }
    auto* draw = mods::arg<dMeter2Draw_c*>(args, 0);
    if (!draw_ready(draw) || is_wolf()) {
        return;
    }
    if (g_dComIfG_gameInfo.play.isPauseFlag() || g_dComIfG_gameInfo.play.isHeapLockFlag() == 6) {
        return;
    }
    draw->drawKanteraScreen(0);
}

HookAction gate_sword_pre(ModContext*, void* args, void* retval, void*);

HookAction gate_cut_finish_pre(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }
    const int cutType = mods::arg<int>(args, 1);
    if (cutType == CUT_FINISH_PARAM_MORTAL_DRAW_A || cutType == CUT_FINISH_PARAM_MORTAL_DRAW_B) {
        if (dAlbw_isHiddenSkillReworkEnabled()) {
            if (!can_hidden_skill()) {
                if (retval != nullptr) {
                    *static_cast<int*>(retval) = 1;
                }
                return HOOK_SKIP_ORIGINAL;
            }
            signal_drain(g_flag_hidden_skill);
            dFocusedArts_onPlayerHiddenSkillUse();
        }
        return HOOK_CONTINUE;
    }
    return gate_sword_pre(nullptr, args, retval, nullptr);
}

HookAction gate_cut_turn_pre(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link != nullptr && dAlbw_isHiddenSkillReworkEnabled() && link->checkCutLargeTurnState()) {
        if (!can_hidden_skill()) {
            if (retval != nullptr) {
                *static_cast<int*>(retval) = 1;
            }
            return HOOK_SKIP_ORIGINAL;
        }
        signal_drain(g_flag_hidden_skill);
        dFocusedArts_onPlayerHiddenSkillUse();
        dFocusedArts_onHiddenSkillProcStarted(daPy_py_c::CUT_TYPE_TWIRL);
        return HOOK_CONTINUE;
    }
    return gate_sword_pre(nullptr, args, retval, nullptr);
}

HookAction gate_cut_turn_charge_pre(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    // Fork gate sits after the dash-anime early return in procCutTurnChargeInit.
    if (link->mComboCutCount == 0 && link->checkDashAnime()) {
        return HOOK_CONTINUE;
    }
    if (dAlbw_isHiddenSkillReworkEnabled() && link->checkCutLargeTurnState()) {
        if (!can_hidden_skill()) {
            if (retval != nullptr) {
                *static_cast<int*>(retval) = 1;
            }
            return HOOK_SKIP_ORIGINAL;
        }
        return HOOK_CONTINUE;
    }
    return gate_sword_pre(nullptr, args, retval, nullptr);
}

HookAction gate_cut_head_pre(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled() || !dAlbw_isHiddenSkillReworkEnabled()) {
        return HOOK_CONTINUE;
    }
    if (!can_hidden_skill()) {
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 0;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    signal_drain(g_flag_hidden_skill);
    return HOOK_CONTINUE;
}

HookAction gate_cut_large_jump_charge_pre(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled() || !dAlbw_isHiddenSkillReworkEnabled()) {
        return HOOK_CONTINUE;
    }
    if (!can_hidden_skill()) {
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 1;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    signal_drain(g_flag_hidden_skill);
    return HOOK_CONTINUE;
}

// --- Sword / agility ---

HookAction gate_sword_pre(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }
    if (!can_sword_agility()) {
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 1;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    signal_drain(g_flag_sword);
    return HOOK_CONTINUE;
}

HookAction gate_sword_void_pre(ModContext*, void*, void*, void*) {
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }
    if (!can_sword_agility()) {
        return HOOK_SKIP_ORIGINAL;
    }
    signal_drain(g_flag_sword);
    return HOOK_CONTINUE;
}

void soft_sword_post(ModContext*, void*, void*, void*) {
    if (!meter_enabled()) {
        return;
    }
    if (can_sword_agility()) {
        signal_drain(g_flag_sword);
    }
}

HookAction on_side_step_pre(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }
    const int dir = mods::arg<int>(args, 1);
    if (dir == daAlink_c::DIR_BACKWARD) {
        if (!can_sword_agility()) {
            *static_cast<int*>(retval) = 1;
            return HOOK_SKIP_ORIGINAL;
        }
        signal_drain(g_flag_backjump);
    } else {
        if (!can_sword_agility()) {
            *static_cast<int*>(retval) = 1;
            return HOOK_SKIP_ORIGINAL;
        }
        signal_drain(g_flag_sidestep);
    }
    return HOOK_CONTINUE;
}

HookAction on_front_roll_pre(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    const bool dive = link->mProcID == daAlink_c::PROC_DIVE_JUMP;
    if (!dive && !link->checkWolf() && link->mAttention != nullptr &&
        link->mAttention->LockonTruth())
    {
        if (!can_sword_agility()) {
            *static_cast<int*>(retval) = 1;
            return HOOK_SKIP_ORIGINAL;
        }
        signal_drain(g_flag_roll);
    }
    return HOOK_CONTINUE;
}

HookAction on_side_roll_pre(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    if (!link->checkWolf() && link->mAttention != nullptr && link->mAttention->LockonTruth()) {
        if (!can_sword_agility()) {
            *static_cast<int*>(retval) = 1;
            return HOOK_SKIP_ORIGINAL;
        }
        signal_drain(g_flag_sidestep);
    }
    return HOOK_CONTINUE;
}

// --- Items ---

static s16 g_make_arrow_count_backup = 0;
static bool g_make_arrow_count_patched = false;
static bool g_throw_boomerang_pending = false;

HookAction on_make_arrow_pre(ModContext*, void* args, void*, void*) {
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }

    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_SKIP_ORIGINAL;
    }

    // Fork makeArrow() @ TARGET_PC — lockout gate, not save arrows.
    if (!can_nock_arrow()) {
        link->mItemVar0.field_0x3018 = 0;
        return HOOK_SKIP_ORIGINAL;
    }

    if (link->mEquipItem == dItemNo_BOMB_ARROW_e && can_nock_bomb_arrow()) {
        link->field_0x301e = 1;
    } else if (link->field_0x301e == 1 && link->mEquipItem == dItemNo_BOMB_ARROW_e &&
               !can_nock_bomb_arrow())
    {
        link->field_0x301e = 0;
    }

    // Stock host still gates on dComIfGs_getArrowNum(); satisfy without linking makeArrow.
    const s16 arrows =
        static_cast<s16>(g_dComIfG_gameInfo.info.getPlayer().getItemRecord().getArrowNum());
    if (arrows == 0) {
        g_make_arrow_count_backup = arrows;
        g_dComIfG_gameInfo.info.getPlayer().getItemRecord().setArrowNum(1);
        g_make_arrow_count_patched = true;
    }

    return HOOK_CONTINUE;
}

void on_make_arrow_post(ModContext*, void*, void*, void*) {
    if (g_make_arrow_count_patched) {
        g_dComIfG_gameInfo.info.getPlayer().getItemRecord().setArrowNum(
            static_cast<u8>(g_make_arrow_count_backup));
        g_make_arrow_count_patched = false;
    }
}

void on_set_bow_ready_post(ModContext*, void* args, void*, void*) {
    if (!meter_enabled()) {
        return;
    }

    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link->checkBowAnime()) {
        return;
    }

    if (link->mEquipItem == dItemNo_BOMB_ARROW_e) {
        link->field_0x301e = can_nock_bomb_arrow() ? 1 : 0;
    }
}

HookAction on_change_arrow_type_pre(ModContext*, void* args, void*, void*) {
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }

    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }

    // Fork changeArrowType() bomb-arrow branch uses canALBWBombArrow(), not save bombs.
    if (link->mEquipItem == dItemNo_BOMB_ARROW_e && link->field_0x301e == 0 &&
        !can_nock_bomb_arrow())
    {
        return HOOK_SKIP_ORIGINAL;
    }

    return HOOK_CONTINUE;
}

HookAction on_throw_boom_pre(ModContext*, void* args, void*, void*) {
    g_throw_boomerang_pending = false;
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    fopAc_ac_c* item = link->mItemAcKeep.getActor();
    g_throw_boomerang_pending =
        item != nullptr && fopAcM_GetName(item) == fpcNm_BOOMERANG_e;
    return HOOK_CONTINUE;
}

void on_throw_boom_post(ModContext*, void*, void*, void*) {
    if (!meter_enabled() || !g_throw_boomerang_pending) {
        return;
    }
    g_throw_boomerang_pending = false;
    signal_drain(g_flag_boom);
}

HookAction on_bow_pre(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    g_bow_had_arrow = false;
    g_bow_was_bomb = 0;
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    fopAc_ac_c* item = link->mItemAcKeep.getActor();
    if (item != nullptr && fopAcM_GetName(item) == fpcNm_ARROW_e) {
        g_bow_had_arrow = true;
        auto* arrow = static_cast<daArrow_c*>(item);
        g_bow_was_bomb = arrow->checkBombArrow() ? 1 : 0;
    }
    return HOOK_CONTINUE;
}

void on_bow_post(ModContext*, void* args, void*, void*) {
    if (!meter_enabled()) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }

    // Lockout shot gate: strip nocked arrow when session shots are spent.
    if (g_locked) {
        const bool deny_ba =
            link->mEquipItem == dItemNo_BOMB_ARROW_e && !can_lockout_bomb_arrow();
        const bool deny_bow =
            (link->mEquipItem == dItemNo_BOW_e || link->mEquipItem == dItemNo_LIGHT_ARROW_e ||
             link->mEquipItem == dItemNo_HAWK_ARROW_e || link->mEquipItem == dItemNo_ARROW_LV1_e ||
             link->mEquipItem == dItemNo_ARROW_LV2_e || link->mEquipItem == dItemNo_ARROW_LV3_e) &&
            !can_lockout_bow();
        if (deny_ba || deny_bow) {
            fopAc_ac_c* held = link->mItemAcKeep.getActor();
            if (held != nullptr && fopAcM_GetName(held) == fpcNm_ARROW_e) {
                fopAcM_delete(held);
                link->mItemAcKeep.clearData();
            }
        }
    }

    if (!g_bow_had_arrow) {
        return;
    }
    // Shoot clears the keep (fork d_a_alink_bow.inc fire path).
    if (link->mItemAcKeep.getActor() == nullptr) {
        if (g_bow_was_bomb) {
            signal_drain(g_flag_bomb_arrow);
        } else {
            signal_drain(g_flag_arrow);
        }
    }
}

HookAction on_hook_pre(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    g_prev_hook_mode = (link != nullptr) ? link->mItemMode : -1;
    return HOOK_CONTINUE;
}

void on_hook_post(ModContext*, void* args, void*, void*) {
    if (!meter_enabled()) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }
    // Ready(1) → fire(2) matches fork drain site.
    if (g_prev_hook_mode == 1 && link->mItemMode == 2) {
        if (link->mEquipItem == dItemNo_W_HOOKSHOT_e && g_locked &&
            !albw_lockout_can_use_double_hookshot())
        {
            link->mItemMode = 1;
            return;
        }
        if (link->mEquipItem == dItemNo_HOOKSHOT_e) {
            signal_drain(g_flag_hook);
        } else if (link->mEquipItem == dItemNo_W_HOOKSHOT_e) {
            signal_drain(g_flag_double_hook);
        }
    }
}

HookAction on_pick_put_pre(ModContext*, void* args, void*, void*) {
    albw_lockout_set_block_insect_bomb_create(false);
    if (!meter_enabled()) {
        return HOOK_CONTINUE;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    g_prev_2fcf = link->field_0x2fcf;

    if (link->mProcVar2.field_0x300c != 0) {
        daPy_frameCtrl_c* framectrl = &link->mUnderFrameCtrl[0];
        if (!link->checkAnmEnd(framectrl) &&
            framectrl->getFrame() >= link->mpHIO->mItem.mPickUp.m.mPlaceAnm.mCancelFrame &&
            framectrl->checkPass(9.0f) && g_locked && !albw_lockout_can_use_bombling())
        {
            albw_lockout_set_block_insect_bomb_create(true);
        }
    }
    return HOOK_CONTINUE;
}

void on_pick_put_post(ModContext*, void* args, void*, void*) {
    albw_lockout_set_block_insect_bomb_create(false);
    if (!meter_enabled()) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }
    if (link->field_0x2fcf > g_prev_2fcf && !g_locked) {
        signal_drain(g_flag_bomb);
    }
}

HookAction on_set_item_actor_pre(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    g_prev_bomb_num = (link != nullptr) ? link->mActiveBombNum : 0;
    return HOOK_CONTINUE;
}

void on_set_item_actor_post(ModContext*, void* args, void*, void*) {
    if (!meter_enabled()) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }
    if (link->mActiveBombNum > g_prev_bomb_num) {
        signal_drain(g_flag_bomb);
    }
}

HookAction on_ironball_pre(ModContext*, void*, void* retval, void*) {
    if (!can_ironball()) {
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 0;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void on_ironball_post(ModContext*, void*, void* retval, void*) {
    if (retval != nullptr && *static_cast<int*>(retval) != 0) {
        signal_drain(g_flag_ironball);
    }
}

void on_spinner_speed_post(ModContext*, void*, void* retval, void*) {
    if (!g_locked || retval == nullptr) {
        return;
    }
    *static_cast<f32*>(retval) *= 1.15f;
}

HookAction on_spinner_ready_pre(ModContext*, void*, void* retval, void*) {
    if (!can_spinner()) {
        *static_cast<int*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void on_spinner_wait_post(ModContext*, void*, void*, void*) { g_spinner_active = true; }

HookAction on_copyrod_subject_init_pre(ModContext*, void*, void* retval, void*) {
    if (!can_domrod()) {
        *static_cast<int*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void on_copyrod_tick_post(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link->getCopyRodControllActor() == nullptr) {
        return;
    }
    if (g_locked) {
        return;  // free control during lockout
    }
    if (g_meter <= 0) {
        link->returnCopyRod();
        return;
    }
    g_domrod_active = true;
}

// --- Exhaustion / armor ---

void on_rest_hp_post(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled() || retval == nullptr || *static_cast<BOOL*>(retval)) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link->checkWolf() || !g_exhausted) {
        return;
    }
    if (!link->checkPlayerGuard() &&
        (link->checkNoUpperAnime() || link->checkHorseTiredAnime()) &&
        link->mTargetedActor == nullptr && !link->checkWindSpeedOnAngle() &&
        !link->checkPlayerDemoMode())
    {
        *static_cast<BOOL*>(retval) = TRUE;
    }
}

void on_heavy_state_post(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled() || retval == nullptr || *static_cast<BOOL*>(retval)) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link->checkWolf() || !g_exhausted || link->checkEventRun()) {
        return;
    }
    *static_cast<BOOL*>(retval) = TRUE;
}

void on_proc_wait_post(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link != nullptr && !link->checkWolf()) {
        g_player_idle = true;
    }
}

void on_armor_no_dmg_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr || !*static_cast<BOOL*>(retval)) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || !link->checkMagicArmorWearAbility()) {
        return;
    }
    // ALBW: armor only blocks while meter can pay; also need rupees for stock drain path.
    if (!can_armor_block() ||
        g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupee() < 500) {
        *static_cast<BOOL*>(retval) = FALSE;
    }
}

HookAction on_damage_point_pre(ModContext*, void* args, void*, void*) {
    g_armor_hit_pending = false;
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || !link->checkMagicArmorWearAbility()) {
        return HOOK_CONTINUE;
    }
    // Match stock NORMAL armor absorb; meter still > 0 so the body can block this hit.
    if (can_armor_block() &&
        g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupee() >= 500 &&
        !link->checkMagicArmorHeavy()) {
        g_armor_hit_pending = true;
    }
    return HOOK_CONTINUE;
}

void on_damage_point_post(ModContext*, void*, void*, void*) {
    if (!g_armor_hit_pending) {
        return;
    }
    g_armor_hit_pending = false;
    g_meter = 0;
    g_armor_depleted = true;
    g_lastRecover = std::chrono::steady_clock::now();
    refresh_lock_state(true);
}

bool install(ModError* error, const char* name, ModResult r) {
    if (r != MOD_OK) {
        svc_log->error(mod_ctx, name);
        mods::set_error(error, MOD_ERROR, name);
        return false;
    }
    return true;
}


// ============================================
// NEW CODE - ALBT multiplatform
// Fork-named recovery-rate accessors for the ported wardrobe module
// (fork d_meter2.cpp:384 / :388). Both read this meter's live rates.
// ============================================
int albw_meter_normal_recovery_rate() { return kRecoverPer100ms; }
int albw_meter_lockout_recovery_rate() { return lockout_recovery_rate(); }

}  // namespace albw_meter_impl

bool albw_meter_is_enabled() {
    return albw_cfg_bool(g_meter_enabled, true);
}

bool albw_meter_is_locked() {
    return albw_g_meter_locked;
}

void albw_meter_drain_to_lockout() {
    albw_meter_impl::drain_to_lockout();
}

void albw_meter_add_base_fraction(int numerator, int denominator) {
    albw_meter_impl::add_base_fraction(numerator, denominator);
}

void albw_meter_sub_base_fraction(int numerator, int denominator) {
    albw_meter_impl::sub_base_fraction(numerator, denominator);
}

void albw_meter_drain_amount(int amount) {
    albw_meter_impl::drain_meter(amount);
}

void albw_meter_fill_on_death() {
    if (!albw_meter_is_enabled()) {
        return;
    }
    albw_meter_impl::g_meter = albw_meter_impl::g_max;
    albw_meter_impl::g_locked = false;
}

void albw_meter_restore_to_full() {
    if (!albw_meter_is_enabled()) {
        return;
    }
    albw_meter_impl::g_meter = albw_meter_impl::g_max;
    albw_meter_impl::g_locked = false;
    albw_meter_impl::g_exhausted = false;
    albw_lockout_on_end();
}

int albw_meter_get_value() {
    return albw_meter_impl::g_meter;
}

int albw_meter_get_max() {
    return albw_meter_impl::g_max;
}

ModResult albw_meter_init(ModError* error) {
    using namespace albw_meter_impl;
    g_meter = kBaseMax;
    g_max = kBaseMax;
    g_locked = false;
    g_exhausted = false;
    g_armor_depleted = false;
    g_lastRecover = std::chrono::steady_clock::now();
    g_lastSpinnerDrain = g_lastRecover;
    g_lastDomRodDrain = g_lastRecover;

    refresh_meter_max_from_progress();

    if (!install(error, "MoveKanteraPre",
                 mods::hook_add_pre<MoveKantera>(svc_hook, on_move_kantera_pre)) ||
        !install(error, "MoveKantera",
                 mods::hook_add_post<MoveKantera>(svc_hook, on_move_kantera_post)) ||
        !install(error, "SetItemMagicCount",
                 mods::hook_add_pre<SetItemMagicCount>(svc_hook, on_set_item_magic_pre)) ||
        !install(error, "MeterFastCreate",
                 mods::hook_add_post<MeterFastCreate>(svc_hook, on_fast_create_post)) ||
        !install(error, "AlphaAnimeKantera",
                 mods::hook_add_post<AlphaAnimeKantera>(svc_hook, on_alpha_kantera_post)) ||
        !install(error, "DrawKanteraScreen",
                 mods::hook_add_pre<DrawKanteraScreen>(svc_hook, on_draw_kantera_screen_pre)) ||
        !install(error, "MeterDraw", mods::hook_add_post<MeterDraw>(svc_hook, on_meter_draw_post)) ||
        !install(error, "CutNormalInit",
                 mods::hook_add_pre<CutNormalInit>(svc_hook, gate_sword_pre)) ||
        !install(error, "CutDash", mods::hook_add_pre<CutDash>(svc_hook, gate_sword_void_pre)) ||
        !install(error, "CutFinishInit",
                 mods::hook_add_pre<CutFinishInit>(svc_hook, gate_cut_finish_pre)) ||
        !install(error, "CutJumpInit", mods::hook_add_pre<CutJumpInit>(svc_hook, gate_sword_pre)) ||
        !install(error, "CutTurnInit",
                 mods::hook_add_pre<CutTurnInit>(svc_hook, gate_cut_turn_pre)) ||
        !install(error, "CutTurnChargeInit",
                 mods::hook_add_pre<CutTurnChargeInit>(svc_hook, gate_cut_turn_charge_pre)) ||
        !install(error, "CutHeadInit",
                 mods::hook_add_pre<CutHeadInit>(svc_hook, gate_cut_head_pre)) ||
        !install(error, "CutLargeJumpChargeInit",
                 mods::hook_add_pre<CutLargeJumpChargeInit>(svc_hook,
                                                             gate_cut_large_jump_charge_pre)) ||
        !install(error, "HorseCutInit",
                 mods::hook_add_pre<HorseCutInit>(svc_hook, gate_sword_pre)) ||
        !install(error, "HorseCutTurnInit",
                 mods::hook_add_pre<HorseCutTurnInit>(svc_hook, gate_sword_pre)) ||
        !install(error, "PickPutPre", mods::hook_add_pre<PickPut>(svc_hook, on_pick_put_pre)) ||
        !install(error, "PickPutPost", mods::hook_add_post<PickPut>(svc_hook, on_pick_put_post)) ||
        !install(error, "CutFinishJumpUpInit",
                 mods::hook_add_post<CutFinishJumpUpInit>(svc_hook, soft_sword_post)) ||
        !install(error, "SideStepInit",
                 mods::hook_add_pre<SideStepInit>(svc_hook, on_side_step_pre)) ||
        !install(error, "FrontRollInit",
                 mods::hook_add_pre<FrontRollInit>(svc_hook, on_front_roll_pre)) ||
        !install(error, "SideRollInit",
                 mods::hook_add_pre<SideRollInit>(svc_hook, on_side_roll_pre)) ||
        !install(error, "ThrowBoomerangPre",
                 mods::hook_add_pre<ThrowBoomerang>(svc_hook, on_throw_boom_pre)) ||
        !install(error, "ThrowBoomerang",
                 mods::hook_add_post<ThrowBoomerang>(svc_hook, on_throw_boom_post)) ||
        !install(error, "MakeArrow",
                 mods::hook_add_pre<MakeArrow>(svc_hook, on_make_arrow_pre)) ||
        !install(error, "MakeArrowPost",
                 mods::hook_add_post<MakeArrow>(svc_hook, on_make_arrow_post)) ||
        !install(error, "SetBowReadyAnime",
                 mods::hook_add_post<SetBowReadyAnime>(svc_hook, on_set_bow_ready_post)) ||
        !install(error, "ChangeArrowType",
                 mods::hook_add_pre<ChangeArrowType>(svc_hook, on_change_arrow_type_pre)) ||
        !install(error, "UpperBowPre", mods::hook_add_pre<UpperBow>(svc_hook, on_bow_pre)) ||
        !install(error, "UpperBowPost", mods::hook_add_post<UpperBow>(svc_hook, on_bow_post)) ||
        !install(error, "UpperHookPre", mods::hook_add_pre<UpperHookshot>(svc_hook, on_hook_pre)) ||
        !install(error, "UpperHookPost",
                 mods::hook_add_post<UpperHookshot>(svc_hook, on_hook_post)) ||
        !install(error, "SetItemActorPre",
                 mods::hook_add_pre<SetItemActor>(svc_hook, on_set_item_actor_pre)) ||
        !install(error, "SetItemActorPost",
                 mods::hook_add_post<SetItemActor>(svc_hook, on_set_item_actor_post)) ||
        !install(error, "IronBallThrowInitPre",
                 mods::hook_add_pre<IronBallThrowInit>(svc_hook, on_ironball_pre)) ||
        !install(error, "IronBallThrowInitPost",
                 mods::hook_add_post<IronBallThrowInit>(svc_hook, on_ironball_post)) ||
        !install(error, "SpinnerRideSpeedF",
                 mods::hook_add_post<SpinnerRideSpeedF>(svc_hook, on_spinner_speed_post)) ||
        !install(error, "SpinnerReadyInit",
                 mods::hook_add_pre<SpinnerReadyInit>(svc_hook, on_spinner_ready_pre)) ||
        !install(error, "SpinnerWait",
                 mods::hook_add_post<SpinnerWait>(svc_hook, on_spinner_wait_post)) ||
        !install(error, "CopyRodSubjectInit",
                 mods::hook_add_pre<CopyRodSubjectInit>(svc_hook, on_copyrod_subject_init_pre)) ||
        !install(error, "CopyRodSubject",
                 mods::hook_add_post<CopyRodSubject>(svc_hook, on_copyrod_tick_post)) ||
        !install(error, "CopyRodMove",
                 mods::hook_add_post<CopyRodMove>(svc_hook, on_copyrod_tick_post)) ||
        !install(error, "CheckRestHPAnime",
                 mods::hook_add_post<CheckRestHPAnime>(svc_hook, on_rest_hp_post)) ||
        !install(error, "CheckHeavyStateOn",
                 mods::hook_add_post<CheckHeavyStateOn>(svc_hook, on_heavy_state_post)) ||
        !install(error, "ProcWait", mods::hook_add_post<ProcWait>(svc_hook, on_proc_wait_post)) ||
        !install(error, "CheckMagicArmorNoDamage",
                 mods::hook_add_post<CheckMagicArmorNoDamage>(svc_hook, on_armor_no_dmg_post)) ||
        !install(error, "SetDamagePointPre",
                 mods::hook_add_pre<SetDamagePoint>(svc_hook, on_damage_point_pre)) ||
        !install(error, "SetDamagePointPost",
                 mods::hook_add_post<SetDamagePoint>(svc_hook, on_damage_point_post)))
    {
        return MOD_ERROR;
    }

    svc_log->info(mod_ctx, "albw meter hooks ready (P3 + oil suppression + lockout perks)");
    return MOD_OK;
}

ModResult albw_meter_shutdown(ModError*) {
    return MOD_OK;
}

void albw_meter_update() {
    using namespace albw_meter_impl;
    if (!meter_enabled()) {
        return;
    }
    dMeter2_c* meter = g_meter2_info.getMeterClass();
    if (meter == nullptr) {
        return;
    }
    g_cached_meter = meter;
    tick_continuous_and_recover();
    push_albw_layout(meter);
}
