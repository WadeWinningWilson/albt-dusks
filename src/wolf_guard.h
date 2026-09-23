// ============================================
// NEW CODE — ALBW Port — "Midna's Shield" (wolf guard / parry)
//
// Wolf-form counterpart to the human ALBW shield. Slice 1 (parry-first): the
// wolf-guard-active state that the shared parry engine (shield.cpp) consumes in
// place of the human checkUpperGuardAnime(). Held guard, the Midna raise/hold/
// stow visual, and the DT auto-open follow.
//
// Full design + architecture: docs/WOLF-GUARD-SCOPE.md
// ============================================
#pragma once

class daAlink_c;

// Installs the wolf-guard hooks (per-frame parry-window tracking + the
// checkDamageAction parry seam). Loud-and-scoped on a miss, never fatal.
// ModResult/ModError come from the includer (matches wolf_combat.h convention).
ModResult albw_wolf_guard_init(ModError* error);

// Feature gate: wolf guard has NO toggle of its own — it is part of Wolf Link
// combat. Active when wolf combat is on, the shield parry engine is on, AND the
// player has unlocked it (dWolfGuard_isUnlocked). See WOLF-GUARD-SCOPE §8.
bool dWolfGuard_isEnabled();

// Unlock state (config-backed, NEVER a save write — WOLF-GUARD-SCOPE §8, "B").
// Unlocked = purchased (ALBW_FLAG_WOLF_GUARD_PURCHASED in config.json) OR True ALBW.
bool dWolfGuard_isUnlocked();

// Persist the shop purchase — writes the config flag only, never the save file.
void dWolfGuard_unlock();

// Shop availability: show the row as soon as the wolf-arts shop itself is
// available (first twilight cleared, like the howl row) and it isn't already
// unlocked — or True ALBW. A pure READ; nothing is written.
bool dWolfGuard_shouldShowShopRow();

// Shop row surface (mirrors dAlbwWolfArts_*ShopRow, config-backed purchase).
int         dWolfGuard_getShopPrice();
const char* dWolfGuard_getShopName();
const char* dWolfGuard_getShopDesc();
bool        dWolfGuard_tryPurchase();  // config write only, never the save file

// True while the wolf holds its guard this frame: enabled + checkWolf() + the
// guard button (held-R) down. This is the wolf's stand-in for the human
// checkUpperGuardAnime(), consumed by shield.cpp guardRaised().
bool dWolfGuard_isActive(const daAlink_c* i_link);

// Per-frame tick (called from the link-execute post seam). Slice 1: placeholder
// for the Midna raise/hold/stow visual (ANM_S_TAKES/S_WAITS/S_PACKAWAY), wired
// once the mechanic is verified in-game.
void dWolfGuard_tick(daAlink_c* i_link);
