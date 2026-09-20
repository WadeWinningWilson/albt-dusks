// ALBW Meter — P3 spends + lockout economy + vanilla oil/magic suppression (P4).
// Deferred (need deeper actor seams): Dom Rod confuse AI, double-claw finisher.
// External mod only; does not modify the ALBT fork tree.

#include "global.h"
#include "albw_symbols.h"

#include "SSystem/SComponent/c_cc_d.h"
#include "d/actor/d_a_arrow.h"
#include "d/actor/d_a_player.h"
#include "d/d_attention.h"
#include "d/d_cc_d.h"  // dCcD_GObjInf (ALBW Magic Armor tg-hit resolve)
#include "albw_fork_compat.h"  // dItemNo_DEITY_ARMOR_e (ALBW Magic Armor deity arm)
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
#include "hurricane_spin.h"
#include "flurry_rush.h"
#include "hidden_skill_charge.h"
#include "mods/hook.hpp"

#include "d/d_item_data.h"
#include "d/d_save.h"

#include <algorithm>
#include <chrono>
#include <unordered_set>
#include <vector>

bool albw_g_meter_locked = false;

namespace albw_meter_impl {

bool meter_enabled() {
    return albw_cfg_bool(g_meter_enabled, true);
}

// ============================================
// NEW CODE - ALBW Magic Armor exposure batch
// Mod-local read of the SAME config var albw_dusk_compat.h maps to
// dusk::MagicArmorMode::ALBW - one toggle, two idioms (ported fork files read
// the compat enum; mod-authored meter code reads the var directly).
// ============================================
bool albw_magic_armor_on() {
    return albw_cfg_bool(g_albw_magic_armor, false);
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
// ============================================
// NEW CODE — ALBW Port (Deku Leaf glide)
// Continuous drain, modelled on the Spinner (fork d_meter2.cpp:2306). Rate is
// faithful to WW: 0.75 magic/sec off a 32-point bar = 2.34%/sec, which on the
// base pool (kBaseMax) is ~25 units per 100ms tick -> ~43s of glide at base tier.
// The up-front charge mirrors WW's initial -1 magic (1/32 of the base bar).
// ============================================
constexpr int kCostDekuLeafPer100ms = (kBaseMax * 234) / 100000;
constexpr int kCostDekuLeafStart = kBaseMax / 32;

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
// (g_armor_depleted retired - wallet-only Magic Armor: "depleted" is now
// derived from the wallet, see albw_armor_is_depleted below.)

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
bool g_deku_leaf_active = false;

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
// (g_armor_hit_pending removed - the ALBW Magic Armor batch replaced the old
// ungated absorb latch with the toggle-gated s_albw_dmg_* set further down.)

std::chrono::steady_clock::time_point g_lastRecover{};
std::chrono::steady_clock::time_point g_lastSpinnerDrain{};
std::chrono::steady_clock::time_point g_lastDomRodDrain{};
std::chrono::steady_clock::time_point g_lastDekuLeafDrain{};

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
// fork d_a_alink_cut.inc:2344-2357 — Ending Blow gate + hidden-skill drain + FA spend.
DEFINE_HOOK(&daAlink_c::procCutDownInit, CutDownInit);
DEFINE_HOOK(&daAlink_c::procCutLargeJumpChargeInit, CutLargeJumpChargeInit);
DEFINE_HOOK(&daAlink_c::procCutLargeJumpInit, CutLargeJumpInit);
DEFINE_HOOK(&daAlink_c::procCutLargeJumpCharge, CutLargeJumpCharge);
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
// ALBW Magic Armor: attack-side encounter registration seam (the fork's own
// site - fork d_a_alink_cut.inc:338 setSwordHitVibration).
DEFINE_HOOK(&daAlink_c::setSwordHitVibration, SetSwordHitVibration);
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
bool can_lockout_bow() { return albw_lockout_can_fire_bow(); }
bool can_lockout_bomb_arrow() { return albw_lockout_can_fire_bomb_arrow(); }
bool can_ironball() {
    if (!g_locked) {
        return true;
    }
    return g_meter >= (g_max * 93) / 100;
}

// ============================================
// NEW CODE - ALBW Magic Armor exposure batch (armor economy state)
// fork d_meter2.cpp:172-188 (tracker statics) and :690-733 (armor hit /
// can-block / encounter registration), ported over this meter's own state.
// External linkage inside albw_meter_impl (a NAMED namespace, NOT anonymous -
// the albw_fork_compat.cpp dMeter2_* bridges link against these; same pattern
// as albw_meter_normal_recovery_rate below).
// ============================================
std::unordered_set<fpc_ProcID> g_armorEncounterIDs;
std::unordered_set<fpc_ProcID> g_armorTaintedIDs;
bool g_armorRewardBlocked = false;
int g_armorEncounterKillCount = 0;
s16 g_armorLastRoomNo = -1;

// ============================================
// WALLET-ONLY MAGIC ARMOR (user-arbitrated design, supersedes the fork)
// The fork spends TWO resources: one absorbed hit zeroes the ALBW meter and
// latches a "depleted" flag that only clears when the meter refills to full,
// while rupees separately gate whether the armor is powered at all. The user's
// design: rupees are the ONLY resource - the armor drains exactly when the
// wallet hits 0, with no meter term and no recharge cooldown.
//
// So "depleted" is redefined as "wallet empty". Every downstream consumer of
// dMeter2_isALBWArmorDepleted() (clothes_pipeline P2a depower/repower edges +
// albwArmorDesiredBrkOn, changelink_port Brk seed, the checkMagicArmorHeavy
// ALBW override, wardrobe's drained stack rate) stays literally unchanged and
// becomes correct for free: the armor grays, goes heavy, and stacks as drained
// exactly at 0 rupees.
//
// Retired with the meter term: g_armor_depleted (the latch, its clear-at-full
// block, its init reset), can_armor_block(), the fork's dMeter2_onALBWArmorHit
// equivalent, and the damage seam's depower latch - which also drops the fork
// rule "an enemy body hit depowers even when unblocked" (an unblocked hit costs
// no rupees, so under wallet-only nothing drains). Cost stays -500 per BLOCKED
// hit (clamped at 0), deity -2500 while >5000: blocks per wallet = rupees/500.
// Side effect (an improvement): the armor no longer depends on the ALBW meter
// at all, so the previous meter-off deviation is gone.
// ============================================
bool albw_armor_is_depleted() { return dComIfGs_getRupee() == 0; }

bool albw_armor_can_block() { return dComIfGs_getRupee() >= 1; }

// fork d_meter2.cpp:708 (dMeter2_onArmorEncounterHit) verbatim over mod state:
// registration is unconditional so the tracker knows who is in the encounter;
// taint is set on any hit that reached Link (see the fork's :810-813 note -
// blocked hits taint too).
void albw_armor_encounter_hit(fpc_ProcID actorID, bool dealtHPDamage) {
    if (actorID == 0) return;
    g_armorEncounterIDs.insert(actorID);
    if (dealtHPDamage) {
        g_armorTaintedIDs.insert(actorID);
        g_armorRewardBlocked = true;
    }
}

// fork d_meter2.cpp:726 (dMeter2_onArmorAttackHit) verbatim over mod state -
// offense-side registration never taints.
void albw_armor_attack_hit(fpc_ProcID actorID) {
    if (actorID == 0) return;
    g_armorEncounterIDs.insert(actorID);
}

// fork d_meter2.cpp:2417-2453 verbatim over mod state: per-frame scan; a room
// change flushes without reward (stale IDs must never pay out); +300 through
// the normal rupee queue when every encounter actor is gone, at least one kill
// happened, and no taint was registered.
void armor_encounter_scan() {
    const s16 curRoom = static_cast<s16>(dComIfGp_roomControl_getStayNo());
    if (g_armorLastRoomNo != -1 && curRoom != g_armorLastRoomNo) {
        g_armorEncounterIDs.clear();
        g_armorTaintedIDs.clear();
        g_armorRewardBlocked = false;
        g_armorEncounterKillCount = 0;
    }
    g_armorLastRoomNo = curRoom;

    if (!g_armorEncounterIDs.empty()) {
        std::vector<fpc_ProcID> killed;
        for (fpc_ProcID id : g_armorEncounterIDs) {
            if (fopAcM_SearchByID(id) == NULL) {
                killed.push_back(id);
            }
        }
        for (fpc_ProcID id : killed) {
            g_armorEncounterIDs.erase(id);
            g_armorTaintedIDs.erase(id);
            g_armorEncounterKillCount++;
        }
        if (g_armorEncounterIDs.empty() && g_armorEncounterKillCount > 0) {
            if (!g_armorRewardBlocked) {
                // All encounter enemies killed with no taint - reward (fork
                // d_meter2.cpp:2448; the normal rupee queue, house style per
                // enemy_rupees.cpp - dComIfGp_* free fns are not mod-linkable).
                g_dComIfG_gameInfo.play.setItemRupeeCount(300);
            }
            g_armorRewardBlocked = false;
            g_armorEncounterKillCount = 0;
        }
    }
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
        g_lastDekuLeafDrain = now;
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
    // Deku Leaf glide: continuous drain, fixed rate (not tier-scaled), mirroring
    // the fork's d_meter2.cpp:2306 leaf drain. Set every frame by the glide proc.
    if (g_deku_leaf_active) {
        if (!g_locked) {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now - g_lastDekuLeafDrain)
                                .count();
            if (ms >= 100) {
                drain_meter(kCostDekuLeafPer100ms);
                g_lastDekuLeafDrain = now;
            }
        }
        g_deku_leaf_active = false;
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
    // ============================================
    // NEW CODE - ALBW Magic Armor exposure batch (clean-encounter scan)
    // fork d_meter2.cpp:2417-2453 runs inside moveKantera's ALBW block; this
    // hook is the mod's moveKantera frame slot (a TIME, not just a place -
    // the fork's own cadence). Gated on the armor toggle only, ahead of the
    // meter_enabled gate: the +300 economy belongs to the armor feature, and
    // both registration sites are already toggle- and armor-gated.
    // ============================================
    if (albw_magic_armor_on()) {
        armor_encounter_scan();
    }
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
            // Fork procCutFinishInit: fire the Mortal Draw special finisher with
            // its real cut type (self-gates on isSpecialFinisherSpendActive).
            dFocusedArts_onHiddenSkillProcStarted(cutType == CUT_FINISH_PARAM_MORTAL_DRAW_A
                                                      ? daPy_py_c::CUT_TYPE_MORTAL_DRAW_A
                                                      : daPy_py_c::CUT_TYPE_MORTAL_DRAW_B);
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
        // GS T1 finisher = Hurricane Spin (fork d_a_alink.cpp:13086 /
        // d_a_alink_cut.inc:2318). When FA is on its final spend charge, run the
        // hurricane overlay INSTEAD of the normal great-spin (try_begin spends the FA
        // finisher itself).
        //
        // NO ALBW DRAIN ON THIS BRANCH — the fork's hurricane costs zero meter. Both
        // fork call sites reach procCutGsHurricaneInit *instead of* procCutTurnInit
        // (d_a_alink_cut.inc:2316-2324, d_a_alink.cpp:13083-13091), so the
        // dMeter2_onALBWHiddenSkill() at d_a_alink_cut.inc:1934 never runs for it, and
        // procCutGsHurricaneInit itself (d_a_alink_hurricane.inc:120-256) contains no
        // meter call at all. The signal_drain that used to sit here was mod-authored;
        // with the old stale-phase early return in albw_hurricane_try_begin it fired
        // once PER FRAME out of procCutTurnMove's release loop and emptied the bar.
        if (albw_hurricane_try_begin(link)) {
            if (retval != nullptr) {
                *static_cast<int*>(retval) = 1;
            }
            return HOOK_SKIP_ORIGINAL;
        }
        signal_drain(g_flag_hidden_skill);
        dFocusedArts_onPlayerHiddenSkillUse();
        // NOTE: Great Spin (large-turn) is NOT an onHiddenSkillProcStarted site in
        // the fork — its finisher is the GS hurricane (tryArmGsHurricaneFinisher,
        // Group C, deferred). Back Slice's TWIRL trigger lives in the aerial-twirl
        // proc (procCutFinishJumpUpInit), reproduced in soft_sword_post below.
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
    // Fork condition is `checkCutLargeTurnState() || dFocusedArts_isOnFinalSpendCharge()`
    // (d_a_alink_cut.inc:2110-2111): the final spend charge takes the hidden-skill gate
    // even when the large-turn state has not latched, so the GS hurricane finisher can
    // still be charged. The second clause was missing, which sent that case down
    // gate_sword_pre and charged it a sword drain instead.
    if (dAlbw_isHiddenSkillReworkEnabled() &&
        (link->checkCutLargeTurnState() || dFocusedArts_isOnFinalSpendCharge()))
    {
        // Great Spin drains 1/2 base at release (procCutTurnInit), not during charge.
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

// Fork procCutHeadInit head, d_a_alink_cut.inc:2567-2599 + :2643. Three donor steps
// were absent here: the re-entry early return (:2574-2576), the parry helm-splitter
// credit gate (:2583-2590), and — the FA-relevant one — dShield_chargeHelmSplitterMeterOnce()
// (:2592) + dFocusedArts_onPlayerHiddenSkillUse() (:2593). Without that last call Helm
// Split never advanced the FA spend sequence, so a bank opened by another skill could
// not be drained by Helm Splits. The mod-authored signal_drain(g_flag_hidden_skill)
// stood in for the donor's dShield_chargeHelmSplitterMeterOnce() (DN-10-S substitution);
// the mod already ports that function (shield.cpp:1620), so call it.
HookAction gate_cut_head_pre(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled() || !dAlbw_isHiddenSkillReworkEnabled()) {
        return HOOK_CONTINUE;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    // fork :2574-2576 — already in the helm-split proc: do nothing, return 1.
    if (link->mProcID == daAlink_c::PROC_CUT_HEAD ||
        link->mProcID == daAlink_c::PROC_CUT_HEAD_LAND)
    {
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 1;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    if (!can_hidden_skill()) {
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 0;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    // fork :2583-2590 — parry helm-punish credit; the scarecrow has no punish window.
    fopAc_ac_c* target = link->mTargetedActor;
    const bool isKakashi =
        target != NULL && fopAcM_GetName(target) == fpcNm_NPC_KAKASHI_e;
    if (dShield_isParryCombatEnabled() && !isKakashi && !dShield_tryBeginHelmSplitter()) {
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 0;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    dShield_chargeHelmSplitterMeterOnce();  // fork :2592 (donor's own ALBW drain)
    dFocusedArts_onPlayerHiddenSkillUse();  // fork :2593 — the missing spend site
    // Fork procCutHeadInit: fire the Helm Split special finisher (max bash
    // charges). Self-gates on isSpecialFinisherSpendActive.
    dFocusedArts_onHiddenSkillProcStarted(daPy_py_c::CUT_TYPE_HEAD_JUMP);
    return HOOK_CONTINUE;
}

// Fork procCutJumpInit, d_a_alink_cut.inc:1772-1813. The mod pointed this proc at the
// bare gate_sword_pre, which reproduces :1781-1786 but drops :1811 —
// dFocusedArts_onPlayerHiddenSkillUse(). That is the single most-taken FA spend site in
// normal play (every jump slash), so its absence is the main reason a full bank could
// not be spent down: with it missing, only Mortal Draw / Great Spin / aerial twirl /
// Jump Strike advance the sequence.
HookAction gate_cut_jump_pre(ModContext*, void* args, void* retval, void*) {
    const HookAction action = gate_sword_pre(nullptr, args, retval, nullptr);
    if (action == HOOK_SKIP_ORIGINAL) {
        return action;  // fork :1782 `return 1` — the proc never reaches :1811
    }
    dFocusedArts_onPlayerHiddenSkillUse();
    return action;
}

// Fork procCutDownInit (Ending Blow), d_a_alink_cut.inc:2344-2357. Not hooked at all in
// the mod: the hidden-skill gate, the hidden-skill drain and the FA spend were ALL
// absent, so Ending Blow was free and never advanced the spend sequence.
HookAction gate_cut_down_pre(ModContext*, void* args, void* retval, void*) {
    if (!meter_enabled() || !dAlbw_isHiddenSkillReworkEnabled()) {
        return HOOK_CONTINUE;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    // fork :2345-2348 — demo re-entry guard runs before the ALBW gate.
    if (link != nullptr && link->mDemo.getDemoMode() == daPy_demo_c::DEMO_CUT_DOWN_e &&
        (link->mProcID == daAlink_c::PROC_CUT_DOWN ||
         link->mProcID == daAlink_c::PROC_CUT_DOWN_LAND))
    {
        return HOOK_CONTINUE;
    }
    if (!can_hidden_skill()) {
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 1;
        }
        return HOOK_SKIP_ORIGINAL;
    }
    signal_drain(g_flag_hidden_skill);   // fork :2354 dMeter2_onALBWHiddenSkill()
    dFocusedArts_onPlayerHiddenSkillUse();  // fork :2355
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

void soft_sword_post(ModContext*, void* args, void*, void*) {
    if (!meter_enabled()) {
        return;
    }
    if (can_sword_agility()) {
        signal_drain(g_flag_sword);
    }
    // Fork procCutFinishJumpUpInit (aerial twirl = Back Slice): fire the Back
    // Slice special finisher with its real cut type. Self-gates on
    // isSpecialFinisherSpendActive (drain/spend bookkeeping handled elsewhere).
    if (dAlbw_isHiddenSkillReworkEnabled()) {
        auto* link = mods::arg<daAlink_c*>(args, 0);
        if (link != nullptr && link->checkCutBackState()) {
            dFocusedArts_onPlayerHiddenSkillUse();
            dFocusedArts_onHiddenSkillProcStarted(daPy_py_c::CUT_TYPE_TWIRL);
        }
    }
}

// Fork procCutLargeJumpCharge: once the charge animation completes, mark the
// Jump Strike charge "ready" so the release is allowed to become a Jump Strike.
void on_cut_large_jump_charge_post(ModContext*, void* args, void*, void*) {
    if (!dAlbw_isHiddenSkillReworkEnabled()) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link != nullptr && link->checkAnmEnd(&link->mUnderFrameCtrl[0])) {
        dAlbw_setJumpStrikeChargeReady();
    }
}

// Fork procCutLargeJumpInit (Jump Strike): the charge gate + the special-finisher
// trigger, reproduced at the top of the proc. Deny an under-charged release
// (consume the ready flag; if absent, cancel and skip the proc). On the allowed
// path, fire the FA hooks (onPlayerHiddenSkillUse + the Jump Strike finisher,
// both self-gating). Matches the fork's procCutLargeJumpInit head.
HookAction gate_cut_large_jump_init_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    if (dAlbw_isHiddenSkillReworkEnabled() &&
        link->mDemo.getDemoMode() != daPy_demo_c::DEMO_CUT_LARGE_JUMP_e)
    {
        if (!dAlbw_tryConsumeJumpStrikeChargeReady()) {
            if (link->mProcID == daAlink_c::PROC_CUT_LARGE_JUMP_CHARGE ||
                link->mProcID == daAlink_c::PROC_CUT_TURN_MOVE)
            {
                link->cancelCutCharge();
            }
            if (retval != nullptr) {
                *static_cast<int*>(retval) = 0;
            }
            return HOOK_SKIP_ORIGINAL;
        }
    }
    // Allowed path (rework charge consumed, or rework off): the fork body calls
    // these before commonProcInit; they self-gate on FA enable / spend state.
    dFocusedArts_onPlayerHiddenSkillUse();
    dFocusedArts_onHiddenSkillProcStarted(daPy_py_c::CUT_TYPE_LARGE_JUMP_INIT);
    return HOOK_CONTINUE;
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

// Slingshot ammo bypass (fork checkUpperItemActionBow @ TARGET_PC: the pachinko
// fires makeSlingStone() on dMeter2_canALBWSling() [== true], never gating on
// getPachinkoNum() seeds, and consumes meter via the fire flag instead of a seed
// decrement). Stock still gates on seeds, so we fake one seed across the call and
// undo any decrement afterward — the meter cost rides the existing g_flag_sling.
static u8 g_pachinko_num_backup = 0;
static s16 g_pachinko_count_backup = 0;
static bool g_pachinko_patched = false;

// ============================================
// NEW CODE - ALBW Port (ammo decoupling: bombs, bomblings, arrows)
//
// The fork does not "suppress ammo when the meter is spent" - it DELETES the
// ammo decrement from the source on PC. Three sites, all #if !TARGET_PC:
//   fork d_a_alink.cpp:15789-15802    setItemActor, bomb create
//                                     (stock d_a_alink.cpp:14295)
//   fork d_a_alink_grab.inc:1384-1401 procPickPut, bombling
//                                     (stock d_a_alink_grab.inc:1362)
//   fork d_a_alink_bow.inc:428-455    bow fire: the bomb-arrow bag decrement
//                                     AND dComIfGp_setItemArrowNumCount(-1)
//                                     (stock d_a_alink_bow.inc:337-341)
// The fork's slingshot seed deletion (fork d_a_alink_bow.inc:370-383) is already
// reproduced above; this is the same receiver translation for the other three.
//
// A plugin cannot delete a line inside a stock body, so the pending count deltas
// are snapshotted before the call and restored after it - identical outcome, and
// exactly the technique the pachinko block above already ships.
//
// dComIfGp_addSelectItemNum routes bomb types to setItemBombNumCount(slot, -1)
// (stock d_com_inf_game.cpp:2298-2306), which ACCUMULATES into
// mItemBombNumCount[slot] (stock d_com_inf_game.cpp:97-105); the arrow path
// accumulates into mItemArrowNumCount (stock d_com_inf_game.h:642). Both are
// therefore restored by clear() + re-add of the saved delta.
//
// NOT cancelled, deliberately: daAlink_c::deleteArrow() keeps its decrement in
// the fork too (fork d_a_alink_bow.inc:193-204 == stock :128-150) - a held bomb
// arrow force-detonated by damage still costs the bag.
// ============================================
static s16 g_bomb_count_backup[dSv_player_item_c::BOMB_BAG_MAX] = {0, 0, 0};
static s16 g_arrow_count_backup = 0;
static bool g_ammo_snapshot_held = false;

void snapshot_ammo_counts() {
    for (int i = 0; i < dSv_player_item_c::BOMB_BAG_MAX; i++) {
        g_bomb_count_backup[i] =
            g_dComIfG_gameInfo.play.getItemBombNumCount(static_cast<u8>(i));
    }
    g_arrow_count_backup = g_dComIfG_gameInfo.play.getItemArrowNumCount();
    g_ammo_snapshot_held = true;
}

// Undo by ADDING BACK THE DELTA, never clear()+set().
//
// These are save-backed item counts, so the restore must not pass through a
// zero state: clear() followed by set() leaves the count at 0 in between, and
// anything that interrupts the pair (a crash, a save written from another
// thread, a future early return added above) would strand the player at zero
// bombs or arrows. setItemBombNumCount / setItemArrowNumCount ACCUMULATE
// (stock d_com_inf_game.cpp:97-105, d_com_inf_game.h:642), so cancelling the
// stock decrement is one additive write of the exact difference - the count is
// never observably wrong, and a no-op costs nothing.
//
// This writes to save data only to put back what the stock body just took; the
// net change across the hook pair is zero and no save bit or format is touched.
// With the feature off the hooks never snapshot, so vanilla decrements run
// untouched and a player who disables the mod keeps exactly the ammo the game
// gave them.
void restore_ammo_counts() {
    if (!g_ammo_snapshot_held) {
        return;
    }
    g_ammo_snapshot_held = false;
    for (int i = 0; i < dSv_player_item_c::BOMB_BAG_MAX; i++) {
        const u8 slot = static_cast<u8>(i);
        const s16 delta = static_cast<s16>(
            g_bomb_count_backup[i] - g_dComIfG_gameInfo.play.getItemBombNumCount(slot));
        if (delta != 0) {
            g_dComIfG_gameInfo.play.setItemBombNumCount(slot, delta);
        }
    }
    const s16 arrowDelta = static_cast<s16>(
        g_arrow_count_backup - g_dComIfG_gameInfo.play.getItemArrowNumCount());
    if (arrowDelta != 0) {
        g_dComIfG_gameInfo.play.setItemArrowNumCount(arrowDelta);
    }
}
// ============================================
// NEW CODE ENDS HERE
// ============================================

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
    // Fork deletes the bombling bag decrement on PC (fork d_a_alink_grab.inc:1384-1401
    // vs stock d_a_alink_grab.inc:1362).
    snapshot_ammo_counts();
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
    g_pachinko_patched = false;
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }
    fopAc_ac_c* item = link->mItemAcKeep.getActor();
    if (item != nullptr && fopAcM_GetName(item) == fpcNm_ARROW_e) {
        g_bow_had_arrow = true;
        auto* arrow = static_cast<daArrow_c*>(item);
        g_bow_was_bomb = arrow->checkBombArrow() ? 1 : 0;
    }
    // Fork bow fire deletes BOTH ammo writes on PC (fork d_a_alink_bow.inc:428-455
    // vs stock :337-341): the bomb-arrow bag decrement and setItemArrowNumCount(-1).
    if (meter_enabled()) {
        snapshot_ammo_counts();
    }
    // Slingshot fires off the meter system, not seed ammo (fork bypass). Fake a
    // seed so the stock getPachinkoNum() gate lets makeSlingStone() run.
    if (meter_enabled() && link->mEquipItem == dItemNo_PACHINKO_e) {
        auto& rec = g_dComIfG_gameInfo.info.getPlayer().getItemRecord();
        if (rec.getPachinkoNum() == 0) {
            g_pachinko_num_backup = rec.getPachinkoNum();
            g_pachinko_count_backup = g_dComIfG_gameInfo.play.getItemPachinkoNumCount();
            rec.setPachinkoNum(1);
            g_pachinko_patched = true;
        }
    }
    return HOOK_CONTINUE;
}

void on_bow_post(ModContext*, void* args, void*, void*) {
    if (!meter_enabled()) {
        return;
    }
    // Undo the stock arrow / bomb-arrow ammo writes (fork deletes them: :428-455).
    restore_ammo_counts();
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        return;
    }

    // Undo the faked seed: restore the save count and cancel any seed decrement the
    // stock fire path applied (the fork consumes meter via g_flag_sling, not seeds).
    if (g_pachinko_patched) {
        auto& rec = g_dComIfG_gameInfo.info.getPlayer().getItemRecord();
        rec.setPachinkoNum(g_pachinko_num_backup);
        const s16 now = g_dComIfG_gameInfo.play.getItemPachinkoNumCount();
        g_dComIfG_gameInfo.play.setItemPachinkoNumCount(
            static_cast<s16>(g_pachinko_count_backup - now));
        g_pachinko_patched = false;
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
    restore_ammo_counts();
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
    restore_ammo_counts();
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
    // Fork deletes the bomb bag decrement on PC (fork d_a_alink.cpp:15789-15802
    // vs stock d_a_alink.cpp:14295) - bombs are meter-only.
    if (meter_enabled()) {
        snapshot_ammo_counts();
    }
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

// ============================================
// MODIFIED CODE - ALBW Magic Armor exposure batch (hunks C + D)
//
// BEFORE this batch the three hooks below ran UNGATED - a live all-off
// violation: vanilla players got armor-block forced off under 500 rupees plus
// a depleted latch stock has no concept of. Everything below now gates on the
// albw_magic_armor toggle. OFF = no retval writes, no latches, no costs, no
// registrations: setDamagePoint and checkMagicArmorNoDamage behave exactly as
// stock ships them (the vanilla NORMAL drain economy).
//
// ON = the fork's ALBW arms, reproduced at the setDamagePoint seam:
//   * fork d_a_alink_damage.inc:329-355 - checkMagicArmorNoDamage ALBW case
//     (deity >5000 exception; else canALBWArmorBlock && rupee >= threshold);
//   * fork d_a_alink_damage.inc:203-229 - setDamagePoint ALBW cost arm (flat
//     -500 by DIRECT setRupee clamp-at-0; deity -2500; damage suppressed);
//   * fork d_a_alink_damage.inc:804-838 - checkDamageAction body-hit arm
//     (defense-side encounter/taint registration; ALWAYS depower on an enemy
//     body hit; block only when wallet + meter can fund it).
//
// WHY not a whole-func port of setDamagePoint (DN-10 order of resort): the
// donor sites live in checkDamageAction and setDamagePoint bodies the mod
// cannot edit, and replacing setDamagePoint wholesale (whole_funcs +
// SKIP_ORIGINAL, armogohma-style) fails two checks:
//   1. stock's body calls dusk::AchievementSystem::get().signal(
//      "player_damaged") (stock d_a_alink_damage.inc:215); AchievementSystem
//      is NOT in dusklight_exports.def (0 hits), so a mod-side body would
//      silently drop host achievements - a hidden regression;
//   2. this symbol already carries pre/post hooks from TWO modules (this file
//      and region_port.cpp's damage-scale scope) tuned to run around the
//      ORIGINAL body; a SKIP_ORIGINAL replacement would make their ordering
//      framework-defined instead of composed.
// So the fork's arm is reproduced as an ADDITION around the original at the
// existing pre/post seam (documented deviation), with one receiver-boundary
// compensation: when the latched decision is "absorbed", stock's body still
// queues its own NORMAL drain (dComIfGp_setItemRupeeCount(-magnified*10),
// stock damage.inc:200); the post-hook measures that queue delta and cancels
// it exactly (the queue is a plain accumulator consumed later by the economy
// tick), leaving only the fork's flat cost. Sub-frame ordering note: the fork
// charges before its setDamagePoint call, this port charges in the post -
// same frame, and no consumer reads the wallet in between.
// ============================================
bool s_albw_dmg_active = false;   // inside a setDamagePoint this batch resolved
bool s_albw_dmg_block = false;    // latched checkMagicArmorNoDamage decision
bool s_albw_dmg_deity = false;    // deity arm: -2500, no depower
s32 s_albw_dmg_queue_snap = 0;    // rupee-queue snapshot for the NORMAL-drain cancel

// The fork resolves the attacker from Link's Tg cylinders inside
// checkDamageAction; same technique as region_port.cpp firstTgHit().
fopAc_ac_c* armor_tg_hit_actor(daAlink_c* link) {
    for (int i = 0; i < 3; i++) {
        if (link->mTgCyls[i].ChkTgHit()) {
            return link->mTgCyls[i].GetTgHitAc();
        }
    }
    return nullptr;
}

// fork d_a_alink_damage.inc:207-208 / :823-824. Deity thresholds (>5000 to
// stay active, -2500 per hit) are NOT part of the 500->1 divergence: untouched.
bool albw_armor_deity_active() {
    return dComIfGs_isItemFirstBit((u8)dItemNo_DEITY_ARMOR_e) != 0 &&
           dComIfGs_getRupee() > 5000;
}

// fork d_a_alink_damage.inc:210-215 - flat cost by DIRECT setRupee, clamped at
// 0 (never the item queue: the fork bypasses the gradual drain on purpose).
void albw_armor_pay(s32 cost) {
    const s32 newRupees = (s32)dComIfGs_getRupee() - cost;
    dComIfGs_setRupee((u16)(newRupees < 0 ? 0 : newRupees));
}

void on_armor_no_dmg_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr) {
        return;
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || !link->checkMagicArmorWearAbility()) {
        return;  // fork damage.inc:331-333 returns false here too - stock's answer stands
    }
    if (!albw_magic_armor_on()) {
        return;  // OFF: stock native answer untouched (all-off proof)
    }
    if (s_albw_dmg_active) {
        // Inside a setDamagePoint whose outcome the pre-hook already latched:
        // keep the body's view consistent with that decision (recomputing
        // here could flip mid-body once the post applies the depower).
        *static_cast<BOOL*>(retval) = s_albw_dmg_block ? TRUE : FALSE;
        return;
    }
    // fork d_a_alink_damage.inc:336-340 (ALBW case). Threshold 500 -> 1
    // (user-arbitrated divergence): repower-at-1 means a 1-499-rupee block
    // empties the wallet - intended. All five threshold sites move together
    // (clothes_pipeline.cpp P2a x2, changelink_port.inc, here + the pre below).
    if (albw_armor_deity_active()) {
        *static_cast<BOOL*>(retval) = TRUE;
        return;
    }
    *static_cast<BOOL*>(retval) =
        (albw_armor_can_block() && dComIfGs_getRupee() >= 1) ? TRUE : FALSE;
}

HookAction on_damage_point_pre(ModContext*, void* args, void*, void*) {
    s_albw_dmg_active = false;
    s_albw_dmg_block = false;
    s_albw_dmg_deity = false;
    if (!albw_magic_armor_on()) {
        return HOOK_CONTINUE;  // OFF: no latch, no writes - vanilla path
    }
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || !link->checkMagicArmorWearAbility()) {
        return HOOK_CONTINUE;
    }
    if (mods::arg<int>(args, 1) <= 0) {
        return HOOK_CONTINUE;  // heal/zero path never reaches the armor arms
    }

    // Defense-side encounter registration - fork damage.inc:815-817: any hit
    // on Link while the armor is worn is a taint event (blocked or not).
    fopAc_ac_c* attacker = armor_tg_hit_actor(link);
    if (attacker != nullptr) {
        albw_armor_encounter_hit(fopAcM_GetID(attacker), true);
    }

    s_albw_dmg_deity = albw_armor_deity_active();
    // Threshold 500 -> 1 (user-arbitrated divergence; see on_armor_no_dmg_post).
    s_albw_dmg_block = s_albw_dmg_deity ||
                       (albw_armor_can_block() && dComIfGs_getRupee() >= 1);
    // Fork depower rule: every absorbed non-deity hit depowers (damage.inc:213
    // and :831), and an enemy BODY hit depowers even when the block fails
    // (damage.inc:818-831 "always depower on body hit"). Attacker-less damage
    // (poly/fall) that cannot be blocked does not depower - the fork's
    // setDamagePoint arm only runs when checkMagicArmorNoDamage() came back
    // true.
    s_albw_dmg_queue_snap = g_dComIfG_gameInfo.play.getItemRupeeCount();
    s_albw_dmg_active = true;
    return HOOK_CONTINUE;
}

void on_damage_point_post(ModContext*, void*, void*, void*) {
    if (!s_albw_dmg_active) {
        return;  // toggle OFF, or a call the pre declined - nothing latched
    }
    s_albw_dmg_active = false;

    if (s_albw_dmg_block) {
        // Cancel stock's queued NORMAL drain (-magnified*10, stock
        // damage.inc:200) so ONLY the fork's flat cost lands. Restoring the
        // pre-call accumulator value before the economy tick consumes it is
        // exact - nothing else queues rupees inside setDamagePoint.
        const s32 delta =
            g_dComIfG_gameInfo.play.getItemRupeeCount() - s_albw_dmg_queue_snap;
        if (delta < 0) {
            g_dComIfG_gameInfo.play.setItemRupeeCount(-delta);
        }
        // fork damage.inc:207-216: deity -2500, else -500 (both clamped at 0).
        albw_armor_pay(s_albw_dmg_deity ? 2500 : 500);
    }
}

// ============================================
// NEW CODE - ALBW Magic Armor exposure batch (attack-side registration)
// fork d_a_alink_cut.inc:344-360 - when Link's sword/wolf attack connects with
// an enemy-group actor while the armor is worn, register it so the
// clean-encounter reward can fire even if that enemy never lands a hit.
// Post-hook translation note: the fork's block sits after the
// notSwordHitVibActor() early-return; those actors are special NPCs, which the
// fork's own fopAc_ENEMY_e group filter already excludes, so the registered
// set is identical.
// ============================================
// ============================================
// Fork d_a_alink_cut.inc:338-375 — daAlink_c::setSwordHitVibration carries TWO ALBW
// additions in one hunk: the magic-armor encounter registration (:355-360) and the FA
// sword fill (:367-370). Only the first was ported here; the fill was re-authored at
// the cc_at_check seam (focused_arts.cpp) with different gates, which lost all three of
// the donor's conditions and turned every sword contact — pots, signs, walls, NPCs, and
// BLOCKED hits on shielded enemies — into FA fill. Both halves now live in their donor
// host function, in donor order.
// ============================================
void on_sword_hit_vibration_post(ModContext*, void* args, void*, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    auto* gobj = mods::arg<dCcD_GObjInf*>(args, 1);
    if (link == nullptr || gobj == nullptr || !gobj->ChkAtHit()) {
        return;
    }
    fopAc_ac_c* hitAc = gobj->GetAtHitAc();

    // fork :367-370 — FA fill requires a clean connect: AtShieldHit covers both enemy
    // shield TGs and invulnerable armor clank surfaces, and both skip PlusDmg, so a
    // guarded hit deals no damage and grants no meter. Human form only.
    if (hitAc != NULL && fopAcM_GetGroup(hitAc) == fopAc_ENEMY_e && !link->checkWolf() &&
        !gobj->ChkAtShieldHit()) {
        dFocusedArts_onConnectedSwordHit();
    }

    if (!albw_magic_armor_on()) {
        return;  // OFF: no registration (all-off proof)
    }
    if (hitAc != NULL && fopAcM_GetGroup(hitAc) == fopAc_ENEMY_e &&
        link->checkMagicArmorWearAbility()) {
        albw_armor_attack_hit(fopAcM_GetID(hitAc));
    }
}

// ============================================
// A hook that fails to resolve must NOT abort mod_initialize.
//
// This helper used to call mods::set_error(..., MOD_ERROR, ...) and return
// false, which made mod_initialize return MOD_ERROR - so ONE unresolved symbol
// unloaded the ENTIRE mod. That is how a single missing hook target reached
// players as "Failed - Reason: <hook name>" with nothing loaded at all, on a
// build where every other feature was fine. It is the same doctrine fyrus.cpp
// already states for the boss hooks.
//
// Now the miss is LOUD and SCOPED: the feature that needed the hook is
// inactive for the run and says so by name in the log, and everything else
// still loads. Never make this silent - a quiet miss turns "never bound" into
// "plausibly wrong forever".
// ============================================
bool install(ModError*, const char* name, ModResult r) {
    if (r != MOD_OK) {
        if (svc_log != nullptr) {
            svc_log->error(mod_ctx, name);
            svc_log->error(mod_ctx,
                           "hook above did NOT install - that feature is inactive this run");
        }
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

// ============================================
// NEW CODE — ALBW Port (Deku Leaf glide) — meter bridges.
// Mirror the fork's dMeter2_canALBWDekuLeaf / onALBWDekuLeaf / onALBWDekuLeafStart
// and the bomb-drop's dMeter2_canALBWBomb / onALBWBomb (bombs are ammo-gated, not
// meter-gated in the fork, so canBomb is always true and onBomb runs the normal
// bomb cost path).
// ============================================
bool albw_meter_can_deku_leaf() {
    if (!albw_meter_is_enabled()) {
        return true;
    }
    return albw_meter_impl::g_locked || albw_meter_impl::g_meter > 0;
}

void albw_meter_on_deku_leaf() {
    albw_meter_impl::g_deku_leaf_active = true;
}

void albw_meter_on_deku_leaf_start() {
    if (!albw_meter_is_enabled()) {
        return;
    }
    albw_meter_impl::drain_meter(albw_meter_impl::kCostDekuLeafStart);
}

bool albw_meter_can_bomb() {
    return true;
}

void albw_meter_on_bomb() {
    albw_meter_impl::signal_drain(albw_meter_impl::g_flag_bomb);
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
    // ALBW Magic Armor batch: encounter tracker starts empty every boot.
    g_armorEncounterIDs.clear();
    g_armorTaintedIDs.clear();
    g_armorRewardBlocked = false;
    g_armorEncounterKillCount = 0;
    g_armorLastRoomNo = -1;
    g_lastRecover = std::chrono::steady_clock::now();
    g_lastSpinnerDrain = g_lastRecover;
    g_lastDomRodDrain = g_lastRecover;
    g_lastDekuLeafDrain = g_lastRecover;

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
        !install(error, "CutJumpInit",
                 mods::hook_add_pre<CutJumpInit>(svc_hook, gate_cut_jump_pre)) ||
        !install(error, "CutTurnInit",
                 mods::hook_add_pre<CutTurnInit>(svc_hook, gate_cut_turn_pre)) ||
        !install(error, "CutTurnChargeInit",
                 mods::hook_add_pre<CutTurnChargeInit>(svc_hook, gate_cut_turn_charge_pre)) ||
        !install(error, "CutHeadInit",
                 mods::hook_add_pre<CutHeadInit>(svc_hook, gate_cut_head_pre)) ||
        !install(error, "CutDownInit",
                 mods::hook_add_pre<CutDownInit>(svc_hook, gate_cut_down_pre)) ||
        !install(error, "CutLargeJumpChargeInit",
                 mods::hook_add_pre<CutLargeJumpChargeInit>(svc_hook,
                                                             gate_cut_large_jump_charge_pre)) ||
        !install(error, "CutLargeJumpInit",
                 mods::hook_add_pre<CutLargeJumpInit>(svc_hook, gate_cut_large_jump_init_pre)) ||
        !install(error, "CutLargeJumpCharge",
                 mods::hook_add_post<CutLargeJumpCharge>(svc_hook, on_cut_large_jump_charge_post)) ||
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
                 mods::hook_add_post<SetDamagePoint>(svc_hook, on_damage_point_post)) ||
        !install(error, "SetSwordHitVibration",
                 mods::hook_add_post<SetSwordHitVibration>(svc_hook,
                                                           on_sword_hit_vibration_post)))
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
    // Layout is NOT pushed here. dMeter2_c::_execute() calls moveKantera() every frame
    // the meter runs (after it validates mpMeterDraw natively), and our on_move_kantera_post
    // hook pushes the ALBW layout there - matching the fork, which applies its meter layout
    // inside the native meter cycle. Pushing from this free-running tick instead caught the
    // meter mid-construction on respawn (getMeterClass() live but mpMeterDraw uninitialized)
    // and crashed dereferencing the garbage draw pointer.
}
