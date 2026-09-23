# Settings & menu reorganization — plan

> Status board / index: [CURRENT-STATE.md](CURRENT-STATE.md)

**Status: IMPLEMENTED (2026-09-22).** Built and installed; compiles clean. The
6-tab layout below is live in `src/albw_settings_ui.cpp`. Resolved open items:
QoL kept as its own (thin) tab per user "the six new tab groupings seem
appropriate"; Postman mail test bypass + Junior Postman mail rows removed from the
window (their config vars `g_postman_mail_test` / `g_postman_mail` stay registered
for logic/config.json — not unregistered). Region Multipliers rebuilt to the fork
always-show group/subpanel in the same pass (see
[REGION-MULTIPLIERS-SCOPE.md](REGION-MULTIPLIERS-SCOPE.md)).

The original target layout for
`src/albw_settings_ui.cpp`; implementation is purely moving `albw_ui_add_toggle`/
`albw_ui_add_select` calls between the `build_*_tab` functions and renaming
sections/tabs. **No config-var renames, no behavior changes** — every existing
`g_*` handle keeps its name and meaning; the mapping tables below are the checklist
so nothing is dropped.

---

## 1. Principles (user-directed)

- **Retire "Tier B" entirely.** It is a dev-internal label and a dumping ground;
  no tab or section may carry that name. Every toggle in it gets a functional home.
- **Group by FUNCTION**, not by development order.
- **All shield-related settings grouped together** (mechanics AND shield HUD) in
  one place, not split across tabs.
- **Outfits/forms get their own group** (Outfit Stats, Sumo Fists, …).
- **Dev/test toggles are stricken from the UI entirely** — they do not appear as
  selectable settings. Specifically **Postman mail test bypass** is removed from
  the window. (The config var `g_postman_mail_test` may remain for config.json/dev
  use, but it is NOT rendered — confirm at wire time whether to keep the var or
  delete it too.)
- **No "Dev/Test" tab** — the dev toggles are removed, not relocated.

---

## 2. Target tab layout (6 tabs)

### Tab 1 — Combat
- **Meter & Arts:** ALBW meter (`g_meter_enabled`), Focused Arts (`g_focused_arts`),
  Flurry Rush (`g_flurry_rush`)
- **Shield & Parry** *(all shield grouped here, per directive — mechanics + shield
  HUD):* Manual shielding (`g_manual_shield`), Shield parry & bash (`g_shield_parry`),
  Shield durability (`g_shield_durability`), Parry Master (`g_parry_master`),
  Parry charge icons (`g_parry_icons_mode`), Shield HUD visibility (`g_shield_hud_visibility`)
- **Enemies:** Devil Trigger (`g_devil_trigger`), Boss Refinement (`g_boss_refinement`)
- **Wolf Link:** Wolf Link combat (`g_wolf_combat`)

### Tab 2 — HUD *(non-shield HUD)*
- LoP HUD mode (`g_lop_hud_mode`), Epona dash-spur HUD (`g_epona_spur_hud`),
  Boss HP bars (`g_boss_hp_bars`), Enemy HP bars (`g_enemy_hp_bars`)

### Tab 3 — Items & Outfits
- **Items:** Extra Item Slot (`g_extra_item_slot_mode`), Quick-Equip Wheel
  (`g_quick_equip_wheel`), Deku Leaf glide (`g_deku_leaf`), Soulbound Red Potion
  (`g_soulbound_potion`)
- **Outfits & Forms:** Outfit Stats (`g_outfit_stats`), Sumo Fists Only
  (`g_sumo_outfit_fists`), End-Game Transform (`g_end_game_transform`)

### Tab 4 — Difficulty & Economy
- **Difficulty:** Region HP panel (`g_region_hp` + sub-knobs), Master Quest hearts
  (`g_master_quest`)  *(user: MQ → Difficulty)*
- **Economy:** Enemy death rupees (`g_kill_rupees`), ALBW Magic Armor (`g_albw_magic_armor`)

### Tab 5 — Progression & Story
- **Rental & Story:** Junior Postman mail (`g_postman_mail`), Postman death strip
  (`g_postman_rental`), True ALBW (`g_true_albw`)
- **Progression:** Soul of Light panel (`g_recovery_orb` + sub-controls)  *(user:
  Soul of Light → Progression)*

### Tab 6 — Quality of Life
- Stick cycle (panel, `g_stick_cycle`), Hold-A crawl (`g_hold_a_crawl`)

---

## 3. Launcher (Mods-list panel) — Quick toggles

Add **True ALBW** to the quick toggles (user request). New set:
- ALBW meter (`g_meter_enabled`), Extra item slot (`g_extra_item_slot_enabled`),
  **True ALBW (`g_true_albw`)**.

(The "Open ALBT settings…" button and the notes blurb are unchanged.)

---

## 4. Stricken from the UI

- **Postman mail test bypass** (`g_postman_mail_test`) — removed from the settings
  window (dev/test only). Decision pending: keep the config var for config.json
  access, or delete it entirely.
- Any other dev/test toggle discovered at wire time gets the same treatment (none
  others are currently rendered — `g_wolf_arts_dev_test`, `g_focused_arts_cheat`
  are already config-only).

---

## 5. Edge-case placements (user-decided)

| Setting | Decision |
|---|---|
| Master Quest hearts | **Difficulty** |
| Boss Refinement | **Combat** (Enemies) |
| Soul of Light | **Progression** |
| Wolf Link combat | **Combat** (moved out of the old Story tab) |
| Shield HUD (icons/visibility) | grouped **with shield** in Combat, not on the HUD tab |

Confirmed:
- **End-Game Transform → Items & Outfits (Outfits & Forms)** (user 2026-09-22).

Still to confirm:
- **Quality of Life** tab is thin after the moves (Stick cycle + Hold-A crawl).
  Acceptable, or fold those two into another tab and drop to 5 tabs? Confirm.

---

## 6. Full move map (old → new) — implementation checklist

Every currently-rendered control and where it goes:

| Control (config var) | Old location | New location |
|---|---|---|
| ALBW meter (`g_meter_enabled`) | Combat › ALBW combat | Combat › Meter & Arts |
| Manual shielding (`g_manual_shield`) | Combat › ALBW combat | Combat › Shield & Parry |
| Shield parry & bash (`g_shield_parry`) | Combat › ALBW combat | Combat › Shield & Parry |
| Shield durability (`g_shield_durability`) | Combat › ALBW combat | Combat › Shield & Parry |
| Focused Arts (`g_focused_arts`) | Combat › ALBW combat | Combat › Meter & Arts |
| Flurry Rush (`g_flurry_rush`) | Combat › ALBW combat | Combat › Meter & Arts |
| Devil Trigger (`g_devil_trigger`) | Combat › ALBW combat | Combat › Enemies |
| LoP HUD mode (`g_lop_hud_mode`) | Combat › Shield HUD | HUD |
| Parry charge icons (`g_parry_icons_mode`) | Combat › Shield HUD | Combat › Shield & Parry |
| Shield HUD visibility (`g_shield_hud_visibility`) | Combat › Shield HUD | Combat › Shield & Parry |
| Epona dash-spur HUD (`g_epona_spur_hud`) | Combat › Shield HUD | HUD |
| Stick cycle (`g_stick_cycle`) | QoL › Field QoL | Quality of Life |
| Soul of Light (`g_recovery_orb`) | QoL › Field QoL | Progression & Story › Progression |
| Hold-A crawl (`g_hold_a_crawl`) | QoL › Field QoL | Quality of Life |
| Extra Item Slot (`g_extra_item_slot_mode`) | QoL › Extra item slot | Items & Outfits › Items |
| Quick-Equip Wheel (`g_quick_equip_wheel`) | QoL › Extra item slot | Items & Outfits › Items |
| Region HP panel (`g_region_hp` …) | Difficulty › Difficulty | Difficulty & Economy › Difficulty |
| Enemy death rupees (`g_kill_rupees`) | Difficulty › Economy | Difficulty & Economy › Economy |
| ALBW Magic Armor (`g_albw_magic_armor`) | Difficulty › Economy | Difficulty & Economy › Economy |
| Junior Postman mail (`g_postman_mail`) | Story › Postman & mail | Progression & Story › Rental & Story |
| Postman mail test bypass (`g_postman_mail_test`) | Story › Postman & mail | **STRICKEN (removed)** |
| Wolf Link combat (`g_wolf_combat`) | Story › Wolf Link | Combat › Wolf Link |
| Parry Master (`g_parry_master`) | Story › Tier B | Combat › Shield & Parry |
| Boss HP bars (`g_boss_hp_bars`) | Story › Tier B | HUD |
| Enemy HP bars (`g_enemy_hp_bars`) | Story › Tier B | HUD |
| Boss Refinement (`g_boss_refinement`) | Story › Tier B | Combat › Enemies |
| Postman death strip (`g_postman_rental`) | Story › Tier B | Progression & Story › Rental & Story |
| True ALBW (`g_true_albw`) | Story › Tier B | Progression & Story › Rental & Story (+ launcher quick toggle) |
| End-Game Transform (`g_end_game_transform`) | Story › Tier B | Items & Outfits › Outfits & Forms *(confirm)* |
| Deku Leaf glide (`g_deku_leaf`) | Story › Tier B | Items & Outfits › Items |
| Outfit Stats (`g_outfit_stats`) | Story › Tier B | Items & Outfits › Outfits & Forms |
| Sumo Fists Only (`g_sumo_outfit_fists`) | Story › Tier B | Items & Outfits › Outfits & Forms |
| Master Quest hearts (`g_master_quest`) | Story › Tier B | Difficulty & Economy › Difficulty |
| Soulbound Red Potion (`g_soulbound_potion`) | Story › Tier B | Items & Outfits › Items |

Nothing is left in "Tier B" → the section and the old "Story & Tier B" tab title
disappear.

---

## 7. Implementation notes

- Pure UI refactor: reshuffle the `build_*_tab` bodies, rename tab titles
  (`on_open_settings_window`) and section headers, add True ALBW to
  `build_mods_launcher`, and delete the Postman-mail-test `albw_ui_add_toggle`.
- The tab array in `on_open_settings_window` grows 4 → 6; update `tab_count`.
- No `config_vars` changes and no `register_all_config` changes (unless we also
  delete `g_postman_mail_test`).
- Verify against the move map (§6) that all 30+ controls land somewhere and none
  are duplicated.

## Cross-references
- [CURRENT-STATE.md](CURRENT-STATE.md) — status board.
