# Port Batch — feature parity pass (v0.3.0 target)

Working tracker for the current porting batch. Rules that bind this work:

- **DN-10 / port native subsystems.** Read the fork's own system and PORT it
  (prefer `tools/port/port_tool.py`); no baking, no receiver-side
  reconstruction unless step 1 is proven insufficient.
- **Additive.** Never delete receiver code to make room — add and label.
- **Match the fork verbatim** where behavior is defined there.
- Build (`_build_mod.bat`) must be green before each commit; commit per feature.

Branch: `main` (working copy). Heartbeat loop: 60s.

## Current config scaffolding (already registered in mod.cpp)

Several handles are pre-declared/registered but **not fully wired** — audit each:
`g_region_damage` (bool F), `g_region_mult` (bool T), `g_region_mult_rupees`
(bool T), `g_link_damage_decrease` (int 1), `g_sumo_outfit_fists` (bool F),
`g_soulbound_potion` (bool F), `g_dpad_quick_swap` (bool F).

---

## HIDE

- [x] **Flurry Rush toggle** — hidden in `albw_settings_ui.cpp` (feature code +
  `g_flurry_rush` registration retained; incomplete mechanic no longer exposed).

## GET (port from fork)

- [ ] **1. Soulbound Red potion** — Souls-flask red potion, SLOT_11: 2-heart
  heal, 2→3 charges via 80r shop upgrade. Handle `g_soulbound_potion` exists.
  Ref memory: project_albw_soulbound_potion. Fork src TBD.
- [ ] **2. Finish Quick Swap port** — regression: Zora **crashes with Sumo** on
  swap (never crashed before). `src/quick_swap.cpp` present. Needs crash
  root-cause (symbolicate) + fork diff.
- [ ] **3. Magic Armor in shop** — add Magic Armor to the Postman/rental
  catalog (`src/rental_shop.cpp`). Fork shop entry TBD.
- [ ] **4. Wolf Link howl songs (rest)** — port remaining howl songs;
  fork gates them by story progress — **make all accessible, no save bits.**
  `src/wolf_howl_combat.cpp` present.
- [ ] **5. Sumo fists only** — wire `g_sumo_outfit_fists`: Sumo outfit fights
  unarmed (fists), no weapon. Ref memory: project_sumo_outfit.
- [ ] **6. Darknut enemy changes for parry system** — port fork's Darknut
  (d_a_e_dn?) changes that integrate with the parry system.
- [ ] **7. Region damage multiplier master + deps organization** — wire the
  region damage-multiplier master toggle + its dependents; organize the settings
  pane with the fork's **left/right pane ordering** and master→dependent nesting.

## ADD (new, fork-referenced)

- [ ] **8. Incoming-damage multiplier scaler** — new player damage-TAKEN
  multiplier. Model on **Outfit Stats** (`src/outfit_stats.*`,
  `dAlbwOutfitStats_get*Mult`) and the existing `g_link_damage_decrease` int.

---

## Log

- Flurry toggle hidden.
