#pragma once

#include <cstddef>

#include "dolphin/types.h"
#include "mods/api.h"

// ============================================
// NEW CODE — ALBW Port (Focused Arts)
// Full public API, matching the fork's include/d/d_focused_arts.h. The module
// body (src/focused_arts_core.inc) is ported VERBATIM from the fork's
// src/d/d_focused_arts.cpp via tools/port/port_tool.py; only settings/meter ABI
// were substituted (dusk::getSettings -> albw_cfg_*, dMeter2_isALBWLocked ->
// albw_meter_is_locked, dMeter2_drainALBWToLockout -> albw_meter_drain_to_lockout).
// ============================================

static constexpr int kFocusedArtsMaxTier = 3;
static constexpr int kFocusedArtsFillDenominator = 12;

// --- enable / cheat / suppression ---
bool dFocusedArts_isEnabled();
bool dFocusedArts_isDebugOverlayEnabled();
bool dFocusedArts_shouldSuppressAlbwMeterDrain();
bool dFocusedArts_shouldSuppressHiddenSkillAlbw();

// --- tiers / bank ---
int dFocusedArts_getPurchasedTier();
int dFocusedArts_getEffectiveTier();
int dFocusedArts_getMaxBank();
bool dFocusedArts_hasSpecialFinishers();
int dFocusedArts_getBankCount();
int dFocusedArts_getFillNumerator();
int dFocusedArts_getFillDenominator();
int dFocusedArts_getChargedDamageTier();
int dFocusedArts_getSpendColumn();
bool dFocusedArts_isInSpendSequence();
bool dFocusedArts_isSpendReady();
bool dFocusedArts_hasSpecialFinisherAvailable();

// --- shop (Postman Upgrades page) ---
int dFocusedArts_getShopTierPrice(int tier);
int dFocusedArts_getNextShopTierPrice();
int dFocusedArts_getNextShopTierIndex();
bool dFocusedArts_canPurchaseShopTier();
bool dFocusedArts_tryPurchaseShopTier();
bool dFocusedArts_shouldShowShopTierRow();
const char* dFocusedArts_getShopTierName(int tier);
const char* dFocusedArts_getShopTierDesc(int tier);

// Flurry Rush is sold as a fourth tier (session-only, no save bit) — see the
// DN-10 ledger block in focused_arts_core.inc. This is the unlock predicate
// dFlurryRush_isEnabled consults.
bool dFocusedArts_hasFlurryRushTier();

// --- lifecycle / combat events ---
void dFocusedArts_resetRuntimeState();
void dFocusedArts_onStageLoad();
void dFocusedArts_update();
void dFocusedArts_fillBank();
void dFocusedArts_clearOneBankCharge();
void dFocusedArts_onConnectedSwordHit();
void dFocusedArts_onConnectedItemHit();
bool dFocusedArts_canPerfectDodgeSpend(int spendGate, int barCost);
bool dFocusedArts_onPerfectDodgeSpend(int spendGate, int barCost);
void dFocusedArts_onPlayerHiddenSkillUse();
void dFocusedArts_onHiddenSkillChargeStart();
void dFocusedArts_onDamageTaken();
void dFocusedArts_onHiddenSkillProcStarted(int cutType);
void dFocusedArts_onBackSliceFinisherEnded();

// --- damage resolution (cc_at_check consumption) ---
u16 dFocusedArts_resolveMeleeDamage(u16 vanillaPower, int cutType);
void dFocusedArts_applyItemDamageBoost(u16& io_attackPower);
bool dFocusedArts_isEndingBlowGreatSpinAoeActive();
u16 dFocusedArts_getEndingBlowGreatSpinAoePower(u16 greatSpinAttackPower);
u16 dFocusedArts_getGsHurricaneHitPower();

// --- special finisher state (finisher-visual launch is deferred: Group C) ---
bool dFocusedArts_isJsFinisherLockoutActive();
bool dFocusedArts_isOnFinalSpendCharge();
bool dFocusedArts_isSpecialFinisherSpendActive();
bool dFocusedArts_shouldLaunchGsHurricaneFinisher();
bool dFocusedArts_tryArmGsHurricaneFinisher();
bool dFocusedArts_consumeGsHurricaneFinisherArmed();
bool dFocusedArts_onEndingBlowContact();
bool dFocusedArts_isMdForcedWolfActive();

// --- debug overlay ---
int dFocusedArts_getHiddenSkillAttackDebugLineCount();
const char* dFocusedArts_getHiddenSkillAttackDebugSwordLabel();
void dFocusedArts_formatHiddenSkillAttackDebugLine(int index, char* o_buf, size_t bufSize);
const char* dFocusedArts_getLastEventText();
const char* dFocusedArts_getRecentEventLine(int index);

// --- mod lifecycle ---
ModResult albw_focused_arts_init(ModError* error);
ModResult albw_focused_arts_shutdown(ModError* error);
void albw_focused_arts_tick();
