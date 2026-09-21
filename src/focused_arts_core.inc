// === AUTO-EXTRACTED by port_tool.py — do not hand-edit the bodies ===
// diag defines forced 0 so scaffolding compiles out:

// ---- pulled fork-local dependencies (verbatim) ----
struct HiddenSkillAttackDebugSpec {
    const char* shortName;
    u16 eventBit;
    int cutType;
    int atp;
    bool faMelee;
    bool instakill;
    const char* note;
};

static constexpr u16 kFocusedArtsShopTierReg = static_cast<u16>(103 << 8) | 0xFF;

// [manual supplement — port_tool misses this constexpr array (complex-declaration
//  gap). VERBATIM from the fork d_focused_arts.cpp l.28-32.]
static constexpr int kFocusedArtsShopPrices[kFocusedArtsMaxTier] = {
    100,
    300,
    500,
};

// ============================================
// NEW CODE - ALBW Port (Flurry Rush as a fourth Focused Arts tier)
//
// DN-10 LEDGER: INVENTED, and recorded as such in docs/FLURRY-RUSH-PLAN.md §6
// item 3 with the user's go. The fork has no shop row for Flurry Rush at all -
// it gates the rush on a perfect dodge and nothing else. Selling it as a
// fourth tier is this project's design, so it gets its own constants rather
// than an extension of the fork's kFocusedArtsShopPrices (fork
// d_focused_arts.cpp:28-32), which stays byte-verbatim above.
//
// SESSION-ONLY, NO SAVE WRITE. kFocusedArtsShopTierReg carries tiers 1-3 and
// is deliberately NOT used here: the purchase lives in a static that dies with
// the session, the same shape as the Deity Armor row (rental_shop.cpp:170).
// This is also why kFocusedArtsMaxTier stays 3 - bumping it would drag
// getEffectiveTier / getMaxBank / hasSpecialFinishers along with it and give
// the meter a fourth bank segment nobody asked for.
//
// Price set by the user: 500, matching tier III rather than escalating past it.
// ============================================
static constexpr int kFlurryRushShopTier = kFocusedArtsMaxTier + 1;
static constexpr int kFlurryRushShopPrice = 500;

static bool s_flurryTierPurchased = false;

// The row is offered only once the saved tiers are exhausted, so it reads as
// "the tier after III" in the shop even though nothing about it is saved.
static bool flurryTierOffered() {
    return albw_cfg_bool(g_flurry_rush, false) && !s_flurryTierPurchased &&
           dFocusedArts_getPurchasedTier() >= kFocusedArtsMaxTier;
}

static constexpr int kItemFillStep = 2;

static constexpr int kRecentEventLines = 4;

static int s_bankCount = 0;

static int s_fillNumerator = 0;

static int s_spendColumn = 0;

static bool s_inSpendSequence = false;

static bool s_forceNewBaseDamage = false;

static bool s_jsFinisherLockoutActive = false;

static int s_backSliceAlbwSuppressFrames = 0;

static int s_maintainStackedFrames = 0;

static bool s_gsHurricaneFinisherArmed = false;

static int s_ebGreatSpinAoeFrames = 0;

static int s_mdForcedWolfFrames = 0;

static bool s_mdForcedWolfSawWolf = false;

static bool s_mdForcedWolfForceHuman = false;

static bool s_mdCriticalFinisher = false;

static int s_recentEventHead = 0;

// [manual supplement — port_tool misses a 2D char array + a constexpr struct
//  array (complex-declaration gap; same class of gap as armogohma's HIO class
//  decl). Both are VERBATIM from the fork d_focused_arts.cpp (l.54, l.194-208).]
static char s_recentEvents[kRecentEventLines][72] = {};

static constexpr HiddenSkillAttackDebugSpec kHiddenSkillAttackDebugSpecs[] = {
    {"Ending Blow", dSv_event_flag_c::F_0339, daPy_py_c::CUT_TYPE_FINISH_VERTICAL, 4, false, true,
     "T1 fin=GS AOE 50%"},
    {"Shield Attack", dSv_event_flag_c::F_0338, daPy_py_c::CUT_TYPE_NONE, 0, false, false,
     "bash/parry (not FA melee)"},
    {"Back Slice", dSv_event_flag_c::F_0340, daPy_py_c::CUT_TYPE_TWIRL, 3, true, false, nullptr},
    {"Helm Split", dSv_event_flag_c::F_0341, daPy_py_c::CUT_TYPE_HEAD_JUMP, 3, true, false, nullptr},
    {"Mortal Draw", dSv_event_flag_c::F_0342, daPy_py_c::CUT_TYPE_MORTAL_DRAW_A, 4, true, false,
     nullptr},
    {"Jump Strike", dSv_event_flag_c::F_0343, daPy_py_c::CUT_TYPE_LARGE_JUMP, 4, true, false, nullptr},
    {"Great Spin", dSv_event_flag_c::F_0344, daPy_py_c::CUT_TYPE_LARGE_TURN_LEFT, 4, true, false,
     "T1 fin=Hurricane"},
};

static constexpr int kEbGreatSpinAoeDurationFrames = 30;

static constexpr int kMdForcedWolfDurationFrames = 450;

static constexpr u16 kMdCriticalLifeQuarters = 4;

static constexpr u16 kGsHurricaneTestHitPower = 50;

// ---- NEW helpers (verbatim) ----
// [NEW helper] isMdCriticalLife
static bool isMdCriticalLife(u16 i_life) {
    return i_life <= kMdCriticalLifeQuarters;
}

// [NEW helper] clearMdForcedWolfWindow
static void clearMdForcedWolfWindow() {
    s_mdForcedWolfFrames = 0;
    s_mdForcedWolfSawWolf = false;
    s_mdForcedWolfForceHuman = false;
}

// [NEW helper] kickForceWolfTransform
static void kickForceWolfTransform(daPy_py_c* i_player) {
    if (i_player == NULL || daPy_py_c::checkNowWolf()) {
        return;
    }
    i_player->onForceWolfChange();
    static_cast<daAlink_c*>(i_player)->checkGroundSpecialMode();
}

// [NEW helper] kickForceHumanTransform
static void kickForceHumanTransform(daPy_py_c* i_player) {
    if (i_player == NULL || !daPy_py_c::checkNowWolf()) {
        return;
    }
    i_player->onEndResetFlg0(daPy_py_c::ERFLG0_UNK_1);
    static_cast<daAlink_c*>(i_player)->checkWolfGroundSpecialMode();
}

// [NEW helper] beginMdForcedWolfWindow
static void beginMdForcedWolfWindow() {
    s_mdForcedWolfFrames = kMdForcedWolfDurationFrames;
    s_mdForcedWolfSawWolf = false;
    s_mdForcedWolfForceHuman = false;
    // Do not kick here — this runs inside procCutFinishInit after commonProcInit.
    // dFocusedArts_update (post-proc same frame) calls kickForceWolfTransform.
    logFaEvent("MD T1 finisher -> forced wolf @ <=1 heart (15s / manual)");
}

// [NEW helper] isHiddenSkillCutType
static bool isHiddenSkillCutType(int i_cutType) {
    switch (i_cutType) {
    case daPy_py_c::CUT_TYPE_TWIRL:
    case daPy_py_c::CUT_TYPE_JUMP:
    case daPy_py_c::CUT_TYPE_LARGE_JUMP:
    case daPy_py_c::CUT_TYPE_LARGE_JUMP_INIT:
    case daPy_py_c::CUT_TYPE_LARGE_JUMP_FINISH:
    case daPy_py_c::CUT_TYPE_LARGE_TURN_LEFT:
    case daPy_py_c::CUT_TYPE_LARGE_TURN_RIGHT:
    case daPy_py_c::CUT_TYPE_MORTAL_DRAW_A:
    case daPy_py_c::CUT_TYPE_MORTAL_DRAW_B:
    case daPy_py_c::CUT_TYPE_HEAD_JUMP:
        return true;
    default:
        return false;
    }
}

// [NEW helper] isInstaKillFinisherCutType
static bool isInstaKillFinisherCutType(int i_cutType) {
    return i_cutType == daPy_py_c::CUT_TYPE_FINISH_VERTICAL ||
           i_cutType == daPy_py_c::CUT_TYPE_FINISH_LEFT ||
           i_cutType == daPy_py_c::CUT_TYPE_FINISH_STAB;
}

// [NEW helper] getDamageTierSteps
static int getDamageTierSteps() {
    if (s_forceNewBaseDamage && !s_inSpendSequence && s_maintainStackedFrames <= 0) {
        return 0;
    }

    if (s_inSpendSequence || s_maintainStackedFrames > 0) {
        return dFocusedArts_getMaxBank();
    }

    return s_bankCount;
}

// [NEW helper] isSpecialFinisherSpendActive
static bool isSpecialFinisherSpendActive() {
    return dFocusedArts_isEnabled() && dFocusedArts_hasSpecialFinishers() &&
           s_spendColumn == 1 && s_maintainStackedFrames > 0;
}

// [NEW helper] applyPercentMult
static u16 applyPercentMult(u16 i_power, int i_numerator, int i_denominator) {
    if (i_power == 0 || i_numerator <= 0) {
        return i_power;
    }
    const u32 scaled =
        (static_cast<u32>(i_power) * static_cast<u32>(i_numerator)) / static_cast<u32>(i_denominator);
    return static_cast<u16>(std::min<u32>(scaled, 0xFFFFu));
}

// [NEW helper] previewFaMeleeDamage
static u16 previewFaMeleeDamage(u16 i_vanillaPower, int i_cutType, int tierSteps, bool i_finisherSpend) {
    if (!dFocusedArts_isEnabled() || i_vanillaPower == 0) {
        return i_vanillaPower;
    }
    if (!isHiddenSkillCutType(i_cutType) || isInstaKillFinisherCutType(i_cutType)) {
        return i_vanillaPower;
    }

    const int numerator = 4 + (2 * tierSteps);
    u16 resolved = applyPercentMult(i_vanillaPower, numerator, 10);

    if (i_finisherSpend && dFocusedArts_hasSpecialFinishers()) {
        switch (i_cutType) {
        case daPy_py_c::CUT_TYPE_MORTAL_DRAW_A:
        case daPy_py_c::CUT_TYPE_MORTAL_DRAW_B:
            if (isMdCriticalLife(dComIfGs_getLife()) || s_mdCriticalFinisher) {
                resolved = applyPercentMult(resolved, 3, 1);
            } else {
                resolved = applyPercentMult(resolved, 3, 2);
            }
            break;
        case daPy_py_c::CUT_TYPE_HEAD_JUMP:
            resolved = applyPercentMult(resolved, 3, 2);
            break;
        default:
            break;
        }
    }

    return std::max<u16>(resolved, 1);
}

// [NEW helper] vanillaLinkSwordPowerFromAtp
static u16 vanillaLinkSwordPowerFromAtp(int i_atp) {
    u8 power = static_cast<u8>(i_atp);
    if (power == 1) {
        return 1;
    }
    if (power == 2) {
        return 10;
    }
    if (power == 3) {
        return 30;
    }
    if (power == 6) {
        return 80;
    }
    if (power >= 4) {
        return 200;
    }
    return power;
}

// [NEW helper] applyEquippedSwordAttackMult
static u16 applyEquippedSwordAttackMult(u16 i_power) {
    if (i_power == 0) {
        return i_power;
    }

    u16 power = i_power;
    if (daPy_py_c::checkMasterSwordEquip()) {
        power = static_cast<u16>(std::min<u32>(static_cast<u32>(power) * 2u, 0xFFFFu));
    }
    if (daPy_py_c::checkWoodSwordEquip()) {
        power = static_cast<u16>(std::max<u16>(power / 2, 1));
    }
    return power;
}

// [NEW helper] getEquippedSwordLabel
static const char* getEquippedSwordLabel() {
    const u8 sword = dComIfGs_getSelectEquipSword();
    if (sword == dItemNo_WOOD_STICK_e) {
        if (dAlbwOutfitStats_allowsWoodHiddenSkills()) {
            return "Wood (Sumo kit)";
        }
        return "Wood (no FA fill)";
    }
    if (sword == dItemNo_SWORD_e) {
        return "Ordon";
    }
    if (sword == dItemNo_MASTER_SWORD_e) {
        return "Master";
    }
    if (sword == dItemNo_LIGHT_SWORD_e) {
        return "Light";
    }
    return "Unknown";
}

// [NEW helper] isHiddenSkillEventAcquired
static bool isHiddenSkillEventAcquired(u16 i_eventBit) {
    return dComIfGs_isEventBit(i_eventBit) != 0;
}

// [NEW helper] logFaEvent
static void logFaEvent(const char* i_msg) {
    std::snprintf(s_recentEvents[s_recentEventHead], sizeof(s_recentEvents[s_recentEventHead]), "%s",
                  i_msg);
    s_recentEventHead = (s_recentEventHead + 1) % kRecentEventLines;
    CONAV_LOG("fa", "%s", i_msg);
}

// [NEW helper] readShopTierReg
static u8 readShopTierReg() {
    return dComIfGs_getEventReg(kFocusedArtsShopTierReg);
}

// [NEW helper] writeShopTierReg
static void writeShopTierReg(u8 tier) {
    dComIfGs_setEventReg(kFocusedArtsShopTierReg, tier);
}

// [NEW helper] clampPurchasedTier
static int clampPurchasedTier(int tier) {
    if (tier < 0) {
        return 0;
    }
    if (tier > kFocusedArtsMaxTier) {
        return kFocusedArtsMaxTier;
    }
    return tier;
}

// [NEW helper] isCheatAllTiers
static bool isCheatAllTiers() {
    return albw_cfg_int(g_focused_arts_cheat, 0) != 0;
}

// [NEW helper] isCheatMaxBank
static bool isCheatMaxBank() {
    const auto mode = albw_cfg_int(g_focused_arts_cheat, 0);
    return mode == 3 ||
           mode == 4;
}

// [NEW helper] applyCheatMaxBankIfEnabled
static void applyCheatMaxBankIfEnabled() {
    if (!isCheatMaxBank() || !dFocusedArts_isEnabled() || s_inSpendSequence) {
        return;
    }

    const int maxBank = dFocusedArts_getMaxBank();
    if (maxBank <= 0 || s_bankCount >= maxBank) {
        return;
    }

    s_bankCount = maxBank;
    s_fillNumerator = 0;
    logFaEvent("cheat max bank");
}

// [NEW helper] getSwordFillStep
static int getSwordFillStep() {
    const u8 sword = dComIfGs_getSelectEquipSword();
    if (sword == dItemNo_WOOD_STICK_e) {
        if (dAlbwOutfitStats_allowsWoodHiddenSkills()) {
            return kItemFillStep;
        }
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

// [NEW helper] clearFillProgress
static void clearFillProgress() {
    s_fillNumerator = 0;
}

// [NEW helper] addFillSteps
static void addFillSteps(int i_steps) {
    if (i_steps <= 0) {
        return;
    }

    if (s_inSpendSequence) {
        return;
    }

    const int maxBank = dFocusedArts_getMaxBank();
    if (maxBank <= 0 || s_bankCount >= maxBank) {
        return;
    }

    s_fillNumerator += i_steps;
    while (s_fillNumerator >= kFocusedArtsFillDenominator && s_bankCount < maxBank) {
        s_fillNumerator -= kFocusedArtsFillDenominator;
        s_bankCount++;
        s_forceNewBaseDamage = false;
        if (s_jsFinisherLockoutActive) {
            s_jsFinisherLockoutActive = false;
            logFaEvent("JS finisher bonus cleared (new bank)");
        }
        char buf[72];
        std::snprintf(buf, sizeof(buf), "bank +1 -> %d/%d fill=%d/12", s_bankCount, maxBank,
                      s_fillNumerator);
        logFaEvent(buf);
    }
}

// [NEW helper] beginSpendCharge
static void beginSpendCharge(int i_spendColumn, bool i_freeAlbw) {
    s_spendColumn = i_spendColumn;
    s_maintainStackedFrames = 300;
    if (s_bankCount > 0) {
        s_bankCount--;
    }
    char buf[72];
    std::snprintf(buf, sizeof(buf), "spend T%d bank=%d fin=%d %s", i_spendColumn, s_bankCount,
                  (i_spendColumn == 1 && dFocusedArts_hasSpecialFinishers()) ? 1 : 0,
                  i_freeAlbw ? "noALBW" : "ALBW");
    logFaEvent(buf);
    if (s_bankCount <= 0) {
        s_inSpendSequence = false;
    }
}

// [NEW helper] dFocusedArts_isEnabled
bool dFocusedArts_isEnabled() {
    return albw_cfg_bool(g_focused_arts, false);
}

// [NEW helper] dFocusedArts_isDebugOverlayEnabled
bool dFocusedArts_isDebugOverlayEnabled() {
    if (!dFocusedArts_isEnabled()) {
        return false;
    }
    const auto mode = albw_cfg_int(g_focused_arts_cheat, 0);
    return mode == 2 ||
           mode == 4;
}

// [NEW helper] dFocusedArts_shouldSuppressAlbwMeterDrain
bool dFocusedArts_shouldSuppressAlbwMeterDrain() {
    if (!dFocusedArts_isEnabled()) {
        return false;
    }

    // Back Slice T1: 90 frames (~3s) — all ALBW spends (see albwDrainMeter).
    return s_backSliceAlbwSuppressFrames > 0;
}

// [NEW helper] dFocusedArts_shouldSuppressHiddenSkillAlbw
bool dFocusedArts_shouldSuppressHiddenSkillAlbw() {
    if (!dFocusedArts_isEnabled()) {
        return false;
    }

    if (s_backSliceAlbwSuppressFrames > 0) {
        return true;
    }

    // Later dumps in the spend sequence (T2 / T1). First dump still pays:
    // onALBW* runs before onPlayerHiddenSkillUse sets s_inSpendSequence.
    return s_inSpendSequence;
}

// [NEW helper] dFocusedArts_getPurchasedTier
int dFocusedArts_getPurchasedTier() {
    return clampPurchasedTier(readShopTierReg());
}

// [NEW helper] dFocusedArts_getEffectiveTier
int dFocusedArts_getEffectiveTier() {
    if (!dFocusedArts_isEnabled()) {
        return 0;
    }
    if (isCheatAllTiers()) {
        return kFocusedArtsMaxTier;
    }
    return dFocusedArts_getPurchasedTier();
}

// [NEW helper] dFocusedArts_getMaxBank
int dFocusedArts_getMaxBank() {
    return dFocusedArts_getEffectiveTier();
}

// [NEW helper] dFocusedArts_hasSpecialFinishers
bool dFocusedArts_hasSpecialFinishers() {
    return dFocusedArts_getEffectiveTier() >= kFocusedArtsMaxTier;
}

// [NEW helper] dFocusedArts_getShopTierPrice
int dFocusedArts_getShopTierPrice(int tier) {
    if (tier == kFlurryRushShopTier) {
        return kFlurryRushShopPrice;
    }
    if (tier < 1 || tier > kFocusedArtsMaxTier) {
        return 0;
    }
    return kFocusedArtsShopPrices[tier - 1];
}

// [NEW helper] dFocusedArts_getNextShopTierPrice
int dFocusedArts_getNextShopTierPrice() {
    const int nextTier = dFocusedArts_getNextShopTierIndex();
    if (nextTier == 0) {
        return 0;
    }
    return dFocusedArts_getShopTierPrice(nextTier);
}

// [NEW helper] dFocusedArts_canPurchaseShopTier
bool dFocusedArts_canPurchaseShopTier() {
    if (!dFocusedArts_shouldShowShopTierRow()) {
        return false;
    }
    return dFocusedArts_getNextShopTierIndex() != 0;
}

// [NEW helper] dFocusedArts_tryPurchaseShopTier
bool dFocusedArts_tryPurchaseShopTier() {
    if (!dFocusedArts_canPurchaseShopTier()) {
        return false;
    }
    // ============================================
    // NEW CODE - the tier-4 branch. NO dComIfGs_setEventReg: the Flurry Rush
    // tier is session-only by design, so it never touches the save. Tiers 1-3
    // keep the fork's reg write untouched below.
    // ============================================
    if (dFocusedArts_getNextShopTierIndex() == kFlurryRushShopTier) {
        s_flurryTierPurchased = true;
        return true;
    }
    writeShopTierReg(static_cast<u8>(dFocusedArts_getPurchasedTier() + 1));
    return true;
}

// [NEW helper] dFocusedArts_hasFlurryRushTier
// The unlock predicate the Flurry Rush module asks. isCheatAllTiers() grants
// it for the same reason it grants tiers 1-3 - it is the project's existing
// "skip the shop" switch, and testing the rush should not require 1000 rupees.
bool dFocusedArts_hasFlurryRushTier() {
    return albw_cfg_bool(g_flurry_rush, false) && (isCheatAllTiers() || s_flurryTierPurchased);
}

// [NEW helper] dFocusedArts_getShopTierName
const char* dFocusedArts_getShopTierName(int tier) {
    switch (tier) {
    case 1:
        return "Focused Arts I";
    case 2:
        return "Focused Arts II";
    case 3:
        return "Focused Arts III";
    case kFlurryRushShopTier:
        return "Focused Arts IV";
    default:
        return "Focused Arts";
    }
}

// [NEW helper] dFocusedArts_getShopTierDesc
const char* dFocusedArts_getShopTierDesc(int tier) {
    // Shop line written by the user - keep it verbatim.
    if (tier == kFlurryRushShopTier) {
        return "You're a natural at parrying, yes? Perhaps try backflipping once you've "
               "stored enough energy, might be just the rush you need";
    }
    return "I noticed you are adept with that sword you carry! I found this scroll "
           "left discarded on the Hyrule Castle grounds. Care to take a look?";
}

// [NEW helper] dFocusedArts_resetRuntimeState
void dFocusedArts_resetRuntimeState() {
    // Session-only tier: it dies with the session, exactly like the Deity
    // Armor rental. Nothing to clear from the save because nothing was written.
    s_flurryTierPurchased = false;
    s_bankCount = 0;
    s_fillNumerator = 0;
    s_spendColumn = 0;
    s_inSpendSequence = false;
    s_forceNewBaseDamage = false;
    s_jsFinisherLockoutActive = false;
    s_backSliceAlbwSuppressFrames = 0;
    s_maintainStackedFrames = 0;
    s_gsHurricaneFinisherArmed = false;
    s_ebGreatSpinAoeFrames = 0;
    clearMdForcedWolfWindow();
    s_mdCriticalFinisher = false;
    s_recentEventHead = 0;
    for (int i = 0; i < kRecentEventLines; i++) {
        s_recentEvents[i][0] = '\0';
    }
    logFaEvent("reset");
    applyCheatMaxBankIfEnabled();
}

// [NEW helper] dFocusedArts_onStageLoad
void dFocusedArts_onStageLoad() {
    if (!dFocusedArts_isEnabled()) {
        return;
    }

    // Bank + partial fill persist across stage loads / doors / editor warps.
    // Only clear in-flight combat windows that cannot survive a transition
    // (spend sequence, MD forced-wolf, JS lockout, short suppress timers).
    // Progress still drains via combat rules (damage, HS charge start, spends).
    s_spendColumn = 0;
    s_inSpendSequence = false;
    s_forceNewBaseDamage = false;
    s_jsFinisherLockoutActive = false;
    s_backSliceAlbwSuppressFrames = 0;
    s_maintainStackedFrames = 0;
    s_gsHurricaneFinisherArmed = false;
    s_ebGreatSpinAoeFrames = 0;
    clearMdForcedWolfWindow();
    s_mdCriticalFinisher = false;
    logFaEvent("stage load (bank/fill kept)");
    applyCheatMaxBankIfEnabled();
}

// [NEW helper] dFocusedArts_getBankCount
int dFocusedArts_getBankCount() {
    return s_bankCount;
}

// [NEW helper] dFocusedArts_fillBank
void dFocusedArts_fillBank() {
    if (!dFocusedArts_isEnabled() || s_inSpendSequence) {
        return;
    }
    const int maxBank = dFocusedArts_getMaxBank();
    if (maxBank <= 0 || s_bankCount >= maxBank) {
        return;
    }
    s_bankCount = maxBank;
    s_fillNumerator = 0;
    logFaEvent("tutorial dummy fill");
}

// [NEW helper] dFocusedArts_clearOneBankCharge
void dFocusedArts_clearOneBankCharge() {
    if (!dFocusedArts_isEnabled() || s_bankCount <= 0) {
        return;
    }
    s_bankCount--;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "clear 1 bank -> %d/%d", s_bankCount,
                  dFocusedArts_getMaxBank());
    logFaEvent(buf);
}

// [NEW helper] dFocusedArts_getFillNumerator
int dFocusedArts_getFillNumerator() {
    return s_fillNumerator;
}

// [NEW helper] dFocusedArts_getFillDenominator
int dFocusedArts_getFillDenominator() {
    return kFocusedArtsFillDenominator;
}

// [NEW helper] dFocusedArts_isSpendReady
bool dFocusedArts_isSpendReady() {
    if (!dFocusedArts_isEnabled()) {
        return false;
    }
    const int maxBank = dFocusedArts_getMaxBank();
    return maxBank > 0 && s_bankCount >= maxBank;
}

// [NEW helper] dFocusedArts_hasSpecialFinisherAvailable
bool dFocusedArts_hasSpecialFinisherAvailable() {
    return dFocusedArts_isSpendReady() && dFocusedArts_hasSpecialFinishers();
}

// [NEW helper] dFocusedArts_getSpendColumn
int dFocusedArts_getSpendColumn() {
    return s_spendColumn;
}

// [NEW helper] dFocusedArts_isInSpendSequence
bool dFocusedArts_isInSpendSequence() {
    return s_inSpendSequence;
}

// [NEW helper] dFocusedArts_getChargedDamageTier
int dFocusedArts_getChargedDamageTier() {
    return s_bankCount;
}

// [NEW helper] dFocusedArts_getLastEventText
const char* dFocusedArts_getLastEventText() {
    const int idx = (s_recentEventHead + kRecentEventLines - 1) % kRecentEventLines;
    return s_recentEvents[idx][0] != '\0' ? s_recentEvents[idx] : "(none)";
}

// [NEW helper] dFocusedArts_getRecentEventLine
const char* dFocusedArts_getRecentEventLine(int i_index) {
    if (i_index < 0 || i_index >= kRecentEventLines) {
        return "";
    }
    const int idx =
        (s_recentEventHead + kRecentEventLines - 1 - i_index + kRecentEventLines) % kRecentEventLines;
    return s_recentEvents[idx];
}

// [NEW helper] dFocusedArts_onConnectedSwordHit
void dFocusedArts_onConnectedSwordHit() {
    if (!dFocusedArts_isEnabled()) {
        return;
    }
    if (s_inSpendSequence) {
        return;
    }
    if (albw_meter_is_locked()) {
        return;
    }

    const int step = getSwordFillStep();
    if (step <= 0) {
        return;
    }

    const int fillBefore = s_fillNumerator;
    const int bankBefore = s_bankCount;
    addFillSteps(step);
    if (s_fillNumerator != fillBefore || s_bankCount != bankBefore) {
        char buf[72];
        std::snprintf(buf, sizeof(buf), "sword fill +%d -> %d/12 bank=%d/%d", step, s_fillNumerator,
                      s_bankCount, dFocusedArts_getMaxBank());
        logFaEvent(buf);
    }
}

// [NEW helper] dFocusedArts_onConnectedItemHit
void dFocusedArts_onConnectedItemHit() {
    if (!dFocusedArts_isEnabled()) {
        return;
    }
    if (s_inSpendSequence) {
        return;
    }
    addFillSteps(kItemFillStep);
    char buf[72];
    std::snprintf(buf, sizeof(buf), "item fill +%d -> %d/12 bank=%d/%d", kItemFillStep,
                  s_fillNumerator, s_bankCount, dFocusedArts_getMaxBank());
    logFaEvent(buf);
}

// [NEW helper] dFocusedArts_canPerfectDodgeSpend
bool dFocusedArts_canPerfectDodgeSpend(int i_spendGate, int i_barCost) {
    if (i_barCost <= 0) {
        return true;
    }

    if (!dFocusedArts_isEnabled()) {
        return false;
    }

    if (s_inSpendSequence) {
        return false;
    }

    return s_bankCount >= i_spendGate && s_bankCount >= i_barCost;
}

// [NEW helper] dFocusedArts_onPerfectDodgeSpend
bool dFocusedArts_onPerfectDodgeSpend(int i_spendGate, int i_barCost) {
    if (i_barCost <= 0) {
        return true;
    }

    if (!dFocusedArts_canPerfectDodgeSpend(i_spendGate, i_barCost)) {
        return false;
    }

    s_bankCount -= i_barCost;
    if (s_bankCount < 0) {
        s_bankCount = 0;
    }

    clearFillProgress();
    char buf[72];
    std::snprintf(buf, sizeof(buf), "flurry dodge spend cost=%d bank=%d/%d", i_barCost, s_bankCount,
                  dFocusedArts_getMaxBank());
    logFaEvent(buf);
    return true;
}

// [NEW helper] dFocusedArts_onPlayerHiddenSkillUse
void dFocusedArts_onPlayerHiddenSkillUse() {
    if (!dFocusedArts_isEnabled()) {
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
        beginSpendCharge(s_bankCount, laterDump);
        return;
    }

    clearFillProgress();
    char buf[72];
    std::snprintf(buf, sizeof(buf), "HS fill reset bank=%d/%d", s_bankCount, maxBank);
    logFaEvent(buf);
}

// [NEW helper] dFocusedArts_onHiddenSkillChargeStart
void dFocusedArts_onHiddenSkillChargeStart() {
    if (!dFocusedArts_isEnabled()) {
        return;
    }
    if (s_fillNumerator <= 0) {
        return;
    }

    clearFillProgress();
    char buf[72];
    std::snprintf(buf, sizeof(buf), "HS charge start drained fill bank=%d/%d", s_bankCount,
                  dFocusedArts_getMaxBank());
    logFaEvent(buf);
}

// [NEW helper] dFocusedArts_onDamageTaken
void dFocusedArts_onDamageTaken() {
    if (!dFocusedArts_isEnabled()) {
        return;
    }
    if (s_fillNumerator <= 0) {
        return;
    }

    clearFillProgress();
    char buf[72];
    std::snprintf(buf, sizeof(buf), "damage drained fill bank=%d/%d", s_bankCount,
                  dFocusedArts_getMaxBank());
    logFaEvent(buf);
}

// [NEW helper] dFocusedArts_update
void dFocusedArts_update() {
    static bool s_prevEnabled = false;
    const bool enabled = dFocusedArts_isEnabled();
    if (enabled != s_prevEnabled) {
        dFocusedArts_resetRuntimeState();
        s_prevEnabled = enabled;
    }
    if (!enabled) {
        return;
    }

    if (s_backSliceAlbwSuppressFrames > 0) {
        s_backSliceAlbwSuppressFrames--;
    }

    if (s_maintainStackedFrames > 0) {
        s_maintainStackedFrames--;
    }

    if (s_ebGreatSpinAoeFrames > 0) {
        s_ebGreatSpinAoeFrames--;
    }

    if (s_mdForcedWolfForceHuman || s_mdForcedWolfFrames > 0) {
        daPy_py_c* player = daPy_getPlayerActorClass();
        if (player == NULL) {
            clearMdForcedWolfWindow();
        } else if (s_mdForcedWolfForceHuman) {
            if (!daPy_py_c::checkNowWolf()) {
                clearMdForcedWolfWindow();
            } else {
                kickForceHumanTransform(player);
            }
        } else if (!daPy_py_c::checkNowWolf()) {
            if (s_mdForcedWolfSawWolf) {
                // Manual transform back ends the window early.
                clearMdForcedWolfWindow();
                logFaEvent("MD forced wolf ended (manual transform)");
            } else {
                kickForceWolfTransform(player);
            }
        } else {
            s_mdForcedWolfSawWolf = true;
            s_mdForcedWolfFrames--;
            if (s_mdForcedWolfFrames <= 0) {
                s_mdForcedWolfForceHuman = true;
                kickForceHumanTransform(player);
                logFaEvent("MD forced wolf ended (10s timer)");
            }
        }
    }

    if (s_jsFinisherLockoutActive && !albw_meter_is_locked()) {
        s_jsFinisherLockoutActive = false;
        logFaEvent("JS finisher bonus ended (lockout cleared)");
    }

    applyCheatMaxBankIfEnabled();
}

// [NEW helper] dFocusedArts_onHiddenSkillProcStarted
void dFocusedArts_onHiddenSkillProcStarted(int i_cutType) {
    if (!isSpecialFinisherSpendActive()) {
        return;
    }

    switch (i_cutType) {
    case daPy_py_c::CUT_TYPE_LARGE_JUMP_INIT:
    case daPy_py_c::CUT_TYPE_LARGE_JUMP:
    case daPy_py_c::CUT_TYPE_LARGE_JUMP_FINISH:
        albw_meter_drain_to_lockout();
        s_jsFinisherLockoutActive = true;
        logFaEvent("JS T1 finisher -> ALBW lockout + item +300%");
        break;
    case daPy_py_c::CUT_TYPE_TWIRL:
        s_backSliceAlbwSuppressFrames = 90;
        logFaEvent("Back Slice T1 finisher -> 3s no ALBW spend");
        break;
    case daPy_py_c::CUT_TYPE_MORTAL_DRAW_A:
    case daPy_py_c::CUT_TYPE_MORTAL_DRAW_B: {
        const u16 life = dComIfGs_getLife();
        const u16 maxLife = dComIfGs_getMaxLifeGauge();
        s_mdCriticalFinisher = isMdCriticalLife(life);
        if (!s_mdCriticalFinisher && life > 0 && maxLife > 0) {
            const u16 drain = std::max<u16>(1, static_cast<u16>(maxLife / 2));
            dComIfGs_setLife(life > drain ? static_cast<u8>(life - drain) : 1);
            logFaEvent("MD T1 finisher -> -half HP");
        } else if (s_mdCriticalFinisher) {
            if (dAlbwWolfCombat_isEnabled()) {
                beginMdForcedWolfWindow();
            } else {
                logFaEvent("MD T1 finisher @ <=1 heart (Wolf Link Combat off — no forced wolf)");
            }
        }
        break;
    }
    case daPy_py_c::CUT_TYPE_HEAD_JUMP:
        dShield_fillBashChargesToMax();
        logFaEvent("Helm T1 finisher -> max bash charges");
        break;
    default:
        break;
    }
}

// [NEW helper] dFocusedArts_resolveMeleeDamage
u16 dFocusedArts_resolveMeleeDamage(u16 i_vanillaPower, int i_cutType) {
    if (!dFocusedArts_isEnabled() || i_vanillaPower == 0) {
        return i_vanillaPower;
    }
    if (!isHiddenSkillCutType(i_cutType) || isInstaKillFinisherCutType(i_cutType)) {
        return i_vanillaPower;
    }

    const int tierSteps = getDamageTierSteps();
    int numerator = 4 + (2 * tierSteps);
    int denominator = 10;
    u16 resolved = applyPercentMult(i_vanillaPower, numerator, denominator);

    if (isSpecialFinisherSpendActive()) {
        switch (i_cutType) {
        case daPy_py_c::CUT_TYPE_MORTAL_DRAW_A:
        case daPy_py_c::CUT_TYPE_MORTAL_DRAW_B:
            if (isMdCriticalLife(dComIfGs_getLife()) || s_mdCriticalFinisher) {
                resolved = applyPercentMult(resolved, 3, 1);
            } else {
                resolved = applyPercentMult(resolved, 3, 2);
            }
            break;
        case daPy_py_c::CUT_TYPE_HEAD_JUMP:
            resolved = applyPercentMult(resolved, 3, 2);
            break;
        default:
            break;
        }
    }

    return std::max<u16>(resolved, 1);
}

// [NEW helper] dFocusedArts_applyItemDamageBoost
void dFocusedArts_applyItemDamageBoost(u16& io_attackPower) {
    if (!dFocusedArts_isEnabled() || io_attackPower == 0 || s_jsFinisherLockoutActive) {
        return;
    }

    int tierSteps = getDamageTierSteps();
    if (tierSteps <= 0 && s_maintainStackedFrames <= 0 && !s_inSpendSequence) {
        return;
    }

    int bonusTenths = tierSteps * 5;
    if (isSpecialFinisherSpendActive()) {
        bonusTenths += 5;
    }

    io_attackPower = applyPercentMult(io_attackPower, 10 + bonusTenths, 10);
}

// [NEW helper] dFocusedArts_isJsFinisherLockoutActive
bool dFocusedArts_isJsFinisherLockoutActive() {
    return s_jsFinisherLockoutActive && albw_meter_is_locked();
}

// [NEW helper] dFocusedArts_isOnFinalSpendCharge
bool dFocusedArts_isOnFinalSpendCharge() {
    return dFocusedArts_isEnabled() && dFocusedArts_hasSpecialFinishers() &&
           s_inSpendSequence && s_bankCount == 1;
}

// [NEW helper] dFocusedArts_isSpecialFinisherSpendActive
bool dFocusedArts_isSpecialFinisherSpendActive() {
    return isSpecialFinisherSpendActive();
}

// [NEW helper] dFocusedArts_shouldLaunchGsHurricaneFinisher
bool dFocusedArts_shouldLaunchGsHurricaneFinisher() {
    return dFocusedArts_isOnFinalSpendCharge();
}

// [NEW helper] dFocusedArts_tryArmGsHurricaneFinisher
bool dFocusedArts_tryArmGsHurricaneFinisher() {
    if (!dFocusedArts_isOnFinalSpendCharge()) {
        return false;
    }

    s_gsHurricaneFinisherArmed = true;
    dFocusedArts_onPlayerHiddenSkillUse();
    logFaEvent("GS T1 finisher armed -> hurricane");
    return true;
}

// [NEW helper] dFocusedArts_consumeGsHurricaneFinisherArmed
bool dFocusedArts_consumeGsHurricaneFinisherArmed() {
    if (!s_gsHurricaneFinisherArmed) {
        return false;
    }

    s_gsHurricaneFinisherArmed = false;
    return true;
}

// [NEW helper] dFocusedArts_onEndingBlowContact
bool dFocusedArts_onEndingBlowContact() {
    if (!isSpecialFinisherSpendActive()) {
        return false;
    }

    s_ebGreatSpinAoeFrames = kEbGreatSpinAoeDurationFrames;
    logFaEvent("EB T1 finisher -> GS AOE 50%");
    return true;
}

// [NEW helper] dFocusedArts_isEndingBlowGreatSpinAoeActive
bool dFocusedArts_isEndingBlowGreatSpinAoeActive() {
    return dFocusedArts_isEnabled() && s_ebGreatSpinAoeFrames > 0;
}

// [NEW helper] dFocusedArts_getGsHurricaneHitPower
u16 dFocusedArts_getGsHurricaneHitPower() {
    return kGsHurricaneTestHitPower;
}

// [NEW helper] dFocusedArts_getEndingBlowGreatSpinAoePower
u16 dFocusedArts_getEndingBlowGreatSpinAoePower(u16 i_greatSpinAttackPower) {
    return applyPercentMult(i_greatSpinAttackPower, 1, 2);
}

// [NEW helper] dFocusedArts_shouldShowShopTierRow
bool dFocusedArts_shouldShowShopTierRow() {
    if (!dFocusedArts_isEnabled() || isCheatAllTiers()) {
        return false;
    }
    // Unlocks after Hero's Shade teaches Ending Blow (first sword hidden skill).
    // True ALBW unlocks the full tier catalog from the start, bypassing this story
    // gate (feature-on + tier < max still apply below).
    if (!dComIfGs_isEventBit(dSv_event_flag_c::F_0339) && !albw_cfg_bool(g_true_albw, false)) {
        return false;
    }
    // The tier-4 clause: once tiers 1-3 are bought out the row keeps standing
    // as long as Flurry Rush is on and unbought.
    return dFocusedArts_getPurchasedTier() < kFocusedArtsMaxTier || flurryTierOffered();
}

// [NEW helper] dFocusedArts_getNextShopTierIndex
int dFocusedArts_getNextShopTierIndex() {
    const int next = dFocusedArts_getPurchasedTier() + 1;
    if (next == kFlurryRushShopTier && flurryTierOffered()) {
        return kFlurryRushShopTier;
    }
    if (next < 1 || next > kFocusedArtsMaxTier) {
        return 0;
    }
    return next;
}

// [NEW helper] dFocusedArts_onBackSliceFinisherEnded
void dFocusedArts_onBackSliceFinisherEnded() {
    if (!isSpecialFinisherSpendActive()) {
        return;
    }

    s_forceNewBaseDamage = true;
    logFaEvent("Back Slice finisher ended -> new base damage");
}

// [NEW helper] dFocusedArts_getHiddenSkillAttackDebugLineCount
int dFocusedArts_getHiddenSkillAttackDebugLineCount() {
    return static_cast<int>(sizeof(kHiddenSkillAttackDebugSpecs) / sizeof(kHiddenSkillAttackDebugSpecs[0]));
}

// [NEW helper] dFocusedArts_getHiddenSkillAttackDebugSwordLabel
const char* dFocusedArts_getHiddenSkillAttackDebugSwordLabel() {
    return getEquippedSwordLabel();
}

// [NEW helper] dFocusedArts_formatHiddenSkillAttackDebugLine
void dFocusedArts_formatHiddenSkillAttackDebugLine(int i_index, char* o_buf, size_t i_bufSize) {
    if (o_buf == NULL || i_bufSize == 0) {
        return;
    }
    o_buf[0] = '\0';

    if (i_index < 0 ||
        i_index >= static_cast<int>(sizeof(kHiddenSkillAttackDebugSpecs) / sizeof(kHiddenSkillAttackDebugSpecs[0])))
    {
        return;
    }

    const HiddenSkillAttackDebugSpec& spec = kHiddenSkillAttackDebugSpecs[i_index];
    const bool acquired = isHiddenSkillEventAcquired(spec.eventBit);

    if (!acquired) {
        std::snprintf(o_buf, i_bufSize, "[ ] %-11s --", spec.shortName);
        return;
    }

    if (spec.instakill) {
        std::snprintf(o_buf, i_bufSize, "[x] %-11s instakill", spec.shortName);
        return;
    }

    if (!spec.faMelee) {
        std::snprintf(o_buf, i_bufSize, "[x] %-11s %s", spec.shortName,
                      spec.note != nullptr ? spec.note : "n/a");
        return;
    }

    const u16 vanilla = applyEquippedSwordAttackMult(vanillaLinkSwordPowerFromAtp(spec.atp));
    const int tierSteps = getDamageTierSteps();
    const bool finisher = isSpecialFinisherSpendActive();
    const u16 now = previewFaMeleeDamage(vanilla, spec.cutType, tierSteps, finisher);
    const u16 t1 = previewFaMeleeDamage(vanilla, spec.cutType, 1, false);
    const u16 t2 = previewFaMeleeDamage(vanilla, spec.cutType, 2, false);
    const u16 t3 = previewFaMeleeDamage(vanilla, spec.cutType, 3, false);
    const u16 t1Fin = previewFaMeleeDamage(vanilla, spec.cutType, 1, true);

    if (finisher && t1Fin != now) {
        std::snprintf(o_buf, i_bufSize, "[x] %-11s V=%-3u now=%u T1/2/3=%u/%u/%u fin=%u", spec.shortName,
                      vanilla, now, t1, t2, t3, t1Fin);
        return;
    }

    if (spec.note != nullptr && dFocusedArts_hasSpecialFinishers()) {
        std::snprintf(o_buf, i_bufSize, "[x] %-11s V=%-3u now=%u T1/2/3=%u/%u/%u (%s)", spec.shortName,
                      vanilla, now, t1, t2, t3, spec.note);
        return;
    }

    std::snprintf(o_buf, i_bufSize, "[x] %-11s V=%-3u now=%u T1/2/3=%u/%u/%u", spec.shortName, vanilla,
                  now, t1, t2, t3);
}

// [NEW helper] dFocusedArts_isMdForcedWolfActive
bool dFocusedArts_isMdForcedWolfActive() {
    return dFocusedArts_isEnabled() && s_mdForcedWolfFrames > 0;
}

// ---- MODIFIED stock fns: ADDED hunks only (place at hook seams) ----