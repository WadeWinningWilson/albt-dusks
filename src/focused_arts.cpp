#include "focused_arts.h"

#include "albw_common.h"
#include "albw_game.h"
#include "config_vars.h"
#include "meter_bridge.h"
#include "shield_adapt.h"
#include "mods/hook.hpp"

#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "d/actor/d_a_player.h"

#define private public
#include "d/actor/d_a_alink.h"
#undef private

namespace {

static int s_bankCount = 0;
static int s_fillNumerator = 0;
static int s_spendColumn = 0;
static bool s_inSpendSequence = false;
static int s_backSliceSuppressFrames = 0;

static constexpr int kItemFillStep = 2;
// Fork event reg byte 103 — purchased FA shop tiers (MQ uses 100–102).
static constexpr u16 kShopTierReg = static_cast<u16>(103u << 8) | 0xFFu;
static constexpr int kBackSliceSuppressFrames = 90;
static constexpr int kFocusedArtsShopPrices[kFocusedArtsMaxTier] = {100, 250, 500};

bool fa_enabled() {
    return albw_cfg_bool(g_focused_arts, false);
}

void writeShopTierReg(u8 tiers) {
    albw_game::set_event_reg(kShopTierReg, tiers);
}

int clampTier(int tier) {
    if (tier < 0) {
        return 0;
    }
    if (tier > kFocusedArtsMaxTier) {
        return kFocusedArtsMaxTier;
    }
    return tier;
}

int readPurchasedTier() {
    return clampTier(static_cast<int>(albw_game::get_event_reg(kShopTierReg) & 0x0F));
}

int getSwordFillStep() {
    const u8 sword = albw_game::select_equip_sword();
    if (sword == dItemNo_WOOD_STICK_e) {
        return 0;
    }
    if (sword == dItemNo_SWORD_e) {
        return kItemFillStep;
    }
    if (sword == dItemNo_MASTER_SWORD_e || sword == dItemNo_LIGHT_SWORD_e) {
        return 1;
    }
    return 0;
}

void clearFillProgress() {
    s_fillNumerator = 0;
}

void addFillSteps(int steps) {
    if (steps <= 0 || s_inSpendSequence) {
        return;
    }

    const int maxBank = dFocusedArts_getMaxBank();
    if (maxBank <= 0 || s_bankCount >= maxBank) {
        return;
    }

    s_fillNumerator += steps;
    while (s_fillNumerator >= kFocusedArtsFillDenominator && s_bankCount < maxBank) {
        s_fillNumerator -= kFocusedArtsFillDenominator;
        s_bankCount++;
    }
}

void reset_runtime_state() {
    s_bankCount = 0;
    s_fillNumerator = 0;
    s_spendColumn = 0;
    s_inSpendSequence = false;
    s_backSliceSuppressFrames = 0;
}

void beginSpendCharge(int spendColumn) {
    s_spendColumn = spendColumn;
    if (s_bankCount > 0) {
        s_bankCount--;
    }
    if (s_bankCount <= 0) {
        s_inSpendSequence = false;
    }
}

bool meter_locked() {
    return albw_meter_is_locked();
}

DEFINE_HOOK(cc_at_check, FaCcAtCheck);
DEFINE_HOOK(&daAlink_c::procCutTurnChargeInit, CutTurnChargeInit);
DEFINE_HOOK(&daAlink_c::procDamageInit, ProcDamageInit);

void on_cut_turn_charge_post(ModContext*, void*, void*, void*) {
    if (fa_enabled() && dAlbw_isHiddenSkillReworkEnabled()) {
        dFocusedArts_onHiddenSkillChargeStart();
    }
}

void on_damage_init_post(ModContext*, void*, void*, void*) {
    if (fa_enabled()) {
        dFocusedArts_onDamageTaken();
    }
}

void on_cc_at_check_post(ModContext*, void* args, void*, void*) {
    if (!fa_enabled()) {
        return;
    }

    auto* enemy = mods::arg<fopAc_ac_c*>(args, 0);
    auto* info = mods::arg<dCcU_AtInfo*>(args, 1);
    if (enemy == nullptr || info == nullptr || info->mpCollider == nullptr ||
        info->mHitBit == 0 || info->mAttackPower <= 0)
    {
        return;
    }

    if (info->mpCollider->ChkAtType(AT_TYPE_NORMAL_SWORD | AT_TYPE_MASTER_SWORD)) {
        dFocusedArts_onConnectedSwordHit();
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

void dFocusedArts_onHiddenSkillProcStarted(int cutType) {
    if (!fa_enabled()) {
        return;
    }
    if (cutType == daPy_py_c::CUT_TYPE_TWIRL) {
        s_backSliceSuppressFrames = kBackSliceSuppressFrames;
    }
}

void dFocusedArts_onHiddenSkillChargeStart() {
    if (!fa_enabled() || s_fillNumerator <= 0) {
        return;
    }
    clearFillProgress();
}

void dFocusedArts_onDamageTaken() {
    if (!fa_enabled() || s_fillNumerator <= 0) {
        return;
    }
    clearFillProgress();
}

bool dFocusedArts_shouldSuppressAlbwSpend() {
    return fa_enabled() && (s_backSliceSuppressFrames > 0 || s_inSpendSequence);
}

bool dFocusedArts_isEnabled() {
    return fa_enabled();
}

bool dFocusedArts_shouldSuppressAlbwMeterDrain() {
    return fa_enabled() && s_backSliceSuppressFrames > 0;
}

bool dFocusedArts_shouldSuppressHiddenSkillAlbw() {
    if (!fa_enabled()) {
        return false;
    }
    if (s_backSliceSuppressFrames > 0) {
        return true;
    }
    return s_inSpendSequence;
}

int dFocusedArts_getEffectiveTier() {
    if (!fa_enabled()) {
        return 0;
    }
    const int purchased = readPurchasedTier();
    // Mod default: at least one bank column when FA is on (shop rows are Tier B).
    return purchased > 0 ? purchased : 1;
}

int dFocusedArts_getMaxBank() {
    return dFocusedArts_getEffectiveTier();
}

int dFocusedArts_getBankCount() {
    return s_bankCount;
}

void dFocusedArts_clearOneBankCharge() {
    if (!fa_enabled() || s_bankCount <= 0) {
        return;
    }
    s_bankCount--;
}

int dFocusedArts_getFillNumerator() {
    return s_fillNumerator;
}

int dFocusedArts_getFillDenominator() {
    return kFocusedArtsFillDenominator;
}

bool dFocusedArts_isInSpendSequence() {
    return s_inSpendSequence;
}

bool dFocusedArts_canPerfectDodgeSpend(int spendGate, int barCost) {
    if (barCost <= 0) {
        return true;
    }
    if (!fa_enabled() || s_inSpendSequence) {
        return false;
    }
    return s_bankCount >= spendGate && s_bankCount >= barCost;
}

bool dFocusedArts_onPerfectDodgeSpend(int spendGate, int barCost) {
    if (!dFocusedArts_canPerfectDodgeSpend(spendGate, barCost)) {
        return false;
    }
    s_bankCount -= barCost;
    if (s_bankCount < 0) {
        s_bankCount = 0;
    }
    clearFillProgress();
    return true;
}

void dFocusedArts_onConnectedSwordHit() {
    if (!fa_enabled() || s_inSpendSequence || meter_locked()) {
        return;
    }
    const int step = getSwordFillStep();
    if (step > 0) {
        addFillSteps(step);
    }
}

void dFocusedArts_onPlayerHiddenSkillUse() {
    if (!fa_enabled()) {
        return;
    }

    const int maxBank = dFocusedArts_getMaxBank();
    if (maxBank <= 0) {
        return;
    }

    if (s_bankCount > 0 && (s_inSpendSequence || s_bankCount >= maxBank)) {
        const bool laterDump = s_inSpendSequence;
        if (!s_inSpendSequence && s_bankCount >= maxBank) {
            s_inSpendSequence = true;
        }
        beginSpendCharge(s_bankCount);
        (void)laterDump;
        return;
    }

    clearFillProgress();
}

void dFocusedArts_onStageLoad() {
    if (!fa_enabled()) {
        return;
    }
    // Bank + partial fill persist across doors; clear in-flight combat windows only.
    s_spendColumn = 0;
    s_inSpendSequence = false;
    s_backSliceSuppressFrames = 0;
}

void dFocusedArts_update() {
    static bool s_prevEnabled = false;
    const bool enabled = fa_enabled();
    if (enabled != s_prevEnabled) {
        reset_runtime_state();
        s_prevEnabled = enabled;
    }
    if (s_backSliceSuppressFrames > 0) {
        s_backSliceSuppressFrames--;
    }
}

int dFocusedArts_getPurchasedTiers() {
    return clampTier(static_cast<int>(albw_game::get_event_reg(kShopTierReg) & 0x0F));
}

int dFocusedArts_getNextShopTierIndex() {
    const int next = dFocusedArts_getPurchasedTiers() + 1;
    if (next < 1 || next > kFocusedArtsMaxTier) {
        return 0;
    }
    return next;
}

int dFocusedArts_getNextShopTierPrice() {
    const int next = dFocusedArts_getNextShopTierIndex();
    if (next <= 0) {
        return 0;
    }
    return kFocusedArtsShopPrices[next - 1];
}

bool dFocusedArts_shouldShowShopTierRow() {
    if (!fa_enabled()) {
        return false;
    }
    // Ending Blow / 2nd secret technique gate (fork F_0339).
    if (!albw_game::is_event_bit(dSv_event_flag_c::F_0339)) {
        return false;
    }
    return dFocusedArts_getPurchasedTiers() < kFocusedArtsMaxTier;
}

bool dFocusedArts_canPurchaseShopTier() {
    return dFocusedArts_shouldShowShopTierRow();
}

bool dFocusedArts_tryPurchaseShopTier() {
    if (!dFocusedArts_canPurchaseShopTier()) {
        return false;
    }
    writeShopTierReg(static_cast<u8>(dFocusedArts_getPurchasedTiers() + 1));
    return true;
}

const char* dFocusedArts_getShopTierName(int tier) {
    switch (tier) {
    case 1:
        return "Focused Arts I";
    case 2:
        return "Focused Arts II";
    case 3:
        return "Focused Arts III";
    default:
        return "Focused Arts";
    }
}

const char* dFocusedArts_getShopTierDesc(int /*tier*/) {
    return "I noticed you are adept with that sword you carry! I found this scroll "
           "left discarded on the Hyrule Castle grounds. Care to take a look?";
}

ModResult albw_focused_arts_init(ModError* error) {
    if (!install(error, "FaCcAtCheckPost",
                 mods::hook_add_post<FaCcAtCheck>(svc_hook, on_cc_at_check_post)) ||
        !install(error, "FaCutTurnChargePost",
                 mods::hook_add_post<CutTurnChargeInit>(svc_hook, on_cut_turn_charge_post)) ||
        !install(error, "FaDamageInitPost",
                 mods::hook_add_post<ProcDamageInit>(svc_hook, on_damage_init_post)))
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_focused_arts_shutdown(ModError*) {
    return MOD_OK;
}

void albw_focused_arts_tick() {
    dFocusedArts_update();
}
