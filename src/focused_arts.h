#pragma once

#include "mods/api.h"

static constexpr int kFocusedArtsMaxTier = 3;
static constexpr int kFocusedArtsFillDenominator = 12;

bool dFocusedArts_isEnabled();
bool dFocusedArts_shouldSuppressAlbwMeterDrain();
bool dFocusedArts_shouldSuppressHiddenSkillAlbw();

int dFocusedArts_getEffectiveTier();
int dFocusedArts_getMaxBank();
int dFocusedArts_getBankCount();
int dFocusedArts_getFillNumerator();
int dFocusedArts_getFillDenominator();
bool dFocusedArts_isInSpendSequence();

bool dFocusedArts_canPerfectDodgeSpend(int spendGate, int barCost);
bool dFocusedArts_onPerfectDodgeSpend(int spendGate, int barCost);

void dFocusedArts_onConnectedSwordHit();
void dFocusedArts_onPlayerHiddenSkillUse();
void dFocusedArts_onHiddenSkillProcStarted(int cutType);
void dFocusedArts_onHiddenSkillChargeStart();
void dFocusedArts_onDamageTaken();
void dFocusedArts_onStageLoad();
void dFocusedArts_update();
bool dFocusedArts_shouldSuppressAlbwSpend();

// Soft-clear one banked charge (lockout water-bomb consequence). No-op if bank empty.
void dFocusedArts_clearOneBankCharge();

// Postman Upgrades page — Focused Arts tier purchases (fork d_focused_arts shop API).
bool dFocusedArts_shouldShowShopTierRow();
bool dFocusedArts_canPurchaseShopTier();
bool dFocusedArts_tryPurchaseShopTier();
int dFocusedArts_getPurchasedTiers();
int dFocusedArts_getNextShopTierIndex();
int dFocusedArts_getNextShopTierPrice();
const char* dFocusedArts_getShopTierName(int tier);
const char* dFocusedArts_getShopTierDesc(int tier);

ModResult albw_focused_arts_init(ModError* error);
ModResult albw_focused_arts_shutdown(ModError* error);
void albw_focused_arts_tick();
