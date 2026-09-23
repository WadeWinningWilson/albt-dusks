# Settings descriptions — full review

> Status board / index: [CURRENT-STATE.md](CURRENT-STATE.md)
> Sibling docs: [SETTINGS-REORG-PLAN.md](SETTINGS-REORG-PLAN.md) (tab layout),
> [REGION-MULTIPLIERS-SCOPE.md](REGION-MULTIPLIERS-SCOPE.md).

**Review only — proposed rewrites, no code applied yet.** Every player-facing
help string currently rendered by the settings window, its verdict, and a
proposed replacement where it needs one. The bar: a description should read like
in-game copy for the player, not like a dev note. The tells I flagged as
"sounds like AI / dev-jargon": internal phase/tier labels (P3, Tier C2, phase 0),
roadmap promises ("when rental ships", "follow-up passes"), engineer verbs ("bill
the meter", "respect lockout gates"), bundle IDs, and terse sentence-fragments
that skip a verb.

Sources: `src/albw_settings_ui.cpp` (most), plus the self-built panels in
`region_port.cpp`, `soul_of_light.cpp`, `stick_cycle.cpp`, `enemy_rupees.cpp`.

---

## 1. Approved rewrites — FINAL copy (user 2026-09-22)

These are the user's own final strings. Apply verbatim (keep the `~…~`
in-progress markers and the `!` exactly as written). **Junior Postman mail moved
to §4 — it is being removed from the menu, not reworded.**

| # | Setting | Final copy to apply |
|---|---|---|
| 1 | **ALBW meter** | "An ALBW like stamina meter that tracks items and player sword swings. Be sure to test your items when you're all out of stamina for secret effects that may even get that stamina back!" |
| 2 | **Focused Arts** | "New moves and systems for the Hidden Skills, build your meter for new finishers and damage payoffs!" |
| 3 | **LoP HUD mode** | "A Soulslike HUD inspired by Lies of P" |
| 4 | **Shield HUD visibility** | "Choose what type of parry charges stay on screen and when" |
| 5 | **Boss Refinement** | "New and improved Boss Fights! Currently supported: Armogohma, Fyrus" |
| 6 | **Master Quest hearts** | "Upgrade health straight through the Postman's shop!" |
| 7 | **Soul of Light** | "Halves Rupees on death and drops a tear of light behind ~May be buggy, in progress~" |
| 8 | **Enemy death rupees** | "Tilt the TP economy in your favor with more rupees per enemy kill" |
| 9 | **Postman death strip** | "ALBW item stripping, 13 of your items get sent to the postman shop on your death ~In Progress~" |
| 10 | **Hold-A crawl** | "Crawl, just like the name implies" |
| 11 | **End-Game Transform** | "Unlock wolf transform early, but you still need to progress the game for certain features" |

### Batch 2 — FINAL copy (user 2026-09-22)

Second set of the user's own final strings. Apply verbatim except the three typo
fixes noted below the table.

| Setting | Final copy to apply |
|---|---|
| **Shield durability** | "Normal blocks wear and tear your shield until it breaks. Perfect Parries keep your shield's health! Buy your shields again easily at the Postman shop" |
| **Parry Master** | "Failed parries chip your health, so strike back to earn that health again!" |
| **Flurry Rush** | "Sold at the rental shop after all the Focused Arts are bought. Backflip while locking onto an enemy with a full shield bash bar to unleash a fast combo! Requires Focused Arts" |
| **Devil Trigger** | "Below 50% health enemies become fed up with Link and unleash their true speed and damage, parry them to stand a chance! Currently covered: Darknuts and Bokoblins" |
| **Quick-Equip Wheel** *(rename label: "Quick Equip Wheel" → "Quick-Equip Wheel")* | "With Extra Item Slot on, hold LB to quickly select an item for battle. Tap for normal behavior ~In Progress~" |
| **Stick Cycle Lock-on** | "While Z-Targeting, flick the right stick left or right to cycle nearby enemies" |
| **Outfit Stats** | "Specific outfits now have different stats, equip sumo if you want to test your limits. Also unlocks Zora swim for non Zora outfits" |
| **ALBW Magic Armor** | "Turns the armor into a risk and reward. Defeating an enemy gains you 300 rupees, getting hit drains you by 500. ~In Progress~" |
| **True ALBW** | "Tired of waiting until you save Talo? Unlock the postman's shop and all purchasables at any point. Postman himself is by Link's House" |
| **Soulbound Red Potion** | "Souls-style flask, heal 2 hearts per swig mid-combat and reclaim drinks on death. Upgrades available through the shop ~In Progress~" |

**Launcher quick toggles (Batch 2):**
| Quick toggle | Final copy |
|---|---|
| ALBW meter | "Toggle the ALBW meter on/off" |
| **True ALBW** *(NEW quick toggle — add per reorg plan §3)* | "Unlock the Postman's shop and all purchasables early. Postman himself is in Ordon near Link's House" |

**Typo fixes applied (flag if unwanted):** `Souls-sytle`→`Souls-style` (Soulbound
potion); `Link's Huse`→`Link's House` (True ALBW quick toggle); the control label
is renamed **"Quick Equip Wheel" → "Quick-Equip Wheel"** (hyphenated, user
2026-09-22). The
minor note wording difference is intentional-looking and kept as written: the
**True ALBW setting** says "by Link's House"; the **True ALBW quick toggle** says
"in Ordon near Link's House".

---

## 2. Minor — acceptable but could be smoothed (optional)

| Setting | Current | Note |
|---|---|---|
| Shield parry & bash | "Perfect-guard window, bash charges, meter gain/loss on block." | Reads as a fragment list. Optional: "Adds a perfect-guard window, bash charges, and meter gain or loss when you block." |
| Region rupees | "…(x3 extra when Region damage is on). Requires the region master." | The "x3 extra" aside is confusing; the "Requires the region master" line changes anyway when we adopt the fork always-show layout ([REGION-MULTIPLIERS-SCOPE.md](REGION-MULTIPLIERS-SCOPE.md)). Fold this rewrite into the region rebuild and mirror the fork's wording. |
| Quick toggle "ALBW meter" | "Toggle the ALBW energy meter suite." | "suite" is filler; fine. |
| Quick toggle "Extra item slot" | "Z third item + Midna on Left d-pad." | Fragment; fine for a quick toggle. |
| ALBW Magic Armor | "ALBW economy: repowers with rupees, flat 500-rupee block cost, +300 for clean encounters. Off = vanilla drain." | Slightly listy but concrete and player-useful; keep. |

---

## 3. Still on current copy — not yet reworded (10)

After §1 batches 1 & 2, these ten are the only rendered gameplay settings left on
their original text. Not flagged as blockers, but several still lean jargon-y;
noted per row. Awaiting the user's call: keep, or send finals for a third batch.

| Setting | Current copy | Jargon note |
|---|---|---|
| Manual shielding | "Hold ZR (R2) to guard without Z-target." | button codes only; clear enough |
| Shield parry & bash | "Perfect-guard window, bash charges, meter gain/loss on block." | fragment list |
| Parry charge icons | "Icon style for shield-bash charge HUD when shield parry is on." | "charge HUD" jargon |
| Epona dash-spur HUD | "Show the Epona dash-spur icons while riding. Off hides them. Does not affect wolf charges, shield HUD, or parry icons." | fine |
| Boss HP bars | "LoP-style name + bar for lock-on dungeon bosses." | fragment |
| Enemy HP bars | "Floating bar + current/max numbers over each regular enemy's head." | fine |
| Extra Item Slot | (long control help `kExtraItemSlotHelp`: Off / Extra Only / Extra + Quick Swap button map) | control map, inherent |
| Deku Leaf glide | "WW Deku Leaf: R+A launches a gust takeoff, hold to keep rising, then glide (drains the meter). Bomb button drops a live bomb mid-glide." | "WW" donor label |
| Sumo Fists Only | "Hide Link's sword/shield for a bare-knuckle look while the Sumo Outfit is worn." | fine |
| Wolf Link combat | "Grants new field-attack stun, bite charges, wolf howl, Midna's hand, charge HUD, Midna's shield. Once unlocked in the shop, press d-pad up for wolf howl and d-pad right for Midna's Hand; hold R to raise Midna's shield (parry to open enemies)." | long but player-facing |

**Launcher quick toggle not yet given copy:** Extra item slot — "Z third item +
Midna on Left d-pad." (ALBW meter and True ALBW quick copy are in §1 Batch 2.)

**Region panel (deferred to the region rebuild — [REGION-MULTIPLIERS-SCOPE.md](REGION-MULTIPLIERS-SCOPE.md)):**
Common HP, Mid-boss HP, Boss HP, Final HP, Link damage decrease, Incoming damage,
Region damage, Region multipliers (master), Region HP, Region rupees. Reworded
when the panel is restructured to the fork always-show layout, not here.

---

## 4. Stricken items — removed from the menu (no rewrite)

- **Junior Postman mail** (`g_postman_mail`) — **user 2026-09-22: doesn't apply,
  remove this setting's visibility from the menu.** Delete its
  `albw_ui_add_toggle` row. (The config var can stay for config.json/dev use, or
  be removed with it — confirm at wire time, same call as `g_postman_mail_test`.)
  This supersedes its slot in [SETTINGS-REORG-PLAN.md](SETTINGS-REORG-PLAN.md)
  (was Progression & Story › Rental & Story) — update that plan's move map.
- **Postman mail test bypass** ("Editor-equivalent: skip story gates for mail
  spawn (test only).") — dev/test toggle; [SETTINGS-REORG-PLAN.md](SETTINGS-REORG-PLAN.md)
  removes it from the window. Don't spend effort rewording it.
- Section header **"Tier B"** and tab title **"Story & Tier B"** — retired by the
  reorg. The strings die with the restructure.

---

## 5. Code finding — orphaned duplicate description

`albw_enemy_rupees_build_panel` (`src/enemy_rupees.cpp:472`) defines a **second,
different** description for the same `g_kill_rupees` var:

> "Kill rupees — Add rupees when enemies die and when listed bosses are defeated.
> Region multipliers apply when enabled. Vanilla drops stay."

It is **declared in `modules.h:20` but never called** — the live Difficulty tab
uses the inline "Enemy death rupees" toggle instead. So the game shows the terse
"Wallet credit…" copy and this fuller one is dead. Two options at wire time:
1. Delete `albw_enemy_rupees_build_panel` (and its `modules.h` decl) as dead code, or
2. Adopt its (better) wording for the live toggle and then delete the function.

Its "Region multipliers apply when enabled" line is actually clearer than the live
copy — worth harvesting.

---

## 6. How to proceed

All of §1 are pure string edits in the `albw_ui_add_*` calls (plus the four
self-built panels). No config, no behavior, no hook changes. They can land:
- **now**, as a standalone descriptions pass (low risk), or
- **folded into the settings reorg** ([SETTINGS-REORG-PLAN.md](SETTINGS-REORG-PLAN.md))
  so the strings are touched once. The region ones (§2) should wait for the
  region rebuild regardless.

Awaiting your go / any per-line vetoes before editing.

## Cross-references
- [SETTINGS-REORG-PLAN.md](SETTINGS-REORG-PLAN.md), [REGION-MULTIPLIERS-SCOPE.md](REGION-MULTIPLIERS-SCOPE.md)
- [CURRENT-STATE.md](CURRENT-STATE.md)
