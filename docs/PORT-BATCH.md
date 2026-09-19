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

- [x] **1. Soulbound Red potion** — DONE, all 4 slices built green: grant
  (f9d056e), drink/heal/consume via AlbwPotionLink_c subclass (f1d627c),
  HUD/ring/save counts (883e1f4), refills on death + refuge return (d64a017).
  Notes: green-potion addALBWFraction is dead code on the always-RED path
  (link-only stub); Shade-Watcher rest NPC isn't ported here so refill sits on
  the refuge return-warp. Needs in-game verification of hook resolution.
- [~] **2. Finish Quick Swap port (Zora+Sumo crash)** — **NEEDS USER DECISION.**
  Root cause: stock daAlink_c::changeLink NULL-derefs al_face.bmd on a Zora↔Sumo
  swap because Zora's Zmdl arc has no al_face (only zl_face). The fork fixes it
  INSIDE changeLink (borrows Kmdl's al_face for the Zora base). The dusk already
  has the fix primitives (dAlbwSumoTest_sumoFaceData + Kmdl donor) but nothing
  consumes them because changeLink isn't replaced. Options:
    (A) DN-10 step 1 (fork-faithful): replace changeLink via HOOK_SKIP_ORIGINAL +
        ported body. Blocked as a DN-10 escalation — its Magic branch drags 5
        no-linkage file-statics + a model callback; cannot self-approve. Needs go.
    (B) Interim stopgap (DN-10 step 2): force the swap-transient flags so stock
        picks the resident zl_face over al_face-from-Zmdl (edits in
        clothes_pipeline.cpp). Stops the crash; trades it for a 1-frame cosmetic
        (zl_face on the sumo body). Lower risk but touches live swap flag logic.
  Not implementing blind — awaiting user's pick of (A) or (B).
- [x] **3. Magic Armor in shop** — restored fork purchase row (500r), grant via
  dAlbwOutfit_equip. Commit 8b60a34.
- [x] **4. Wolf Link howl songs (rest)** — all 6 duets ungated (no save bits) in
  both pool builders. Commit 6e6d571.
- [x] **5. Sumo fists only** — mechanic was already wired; exposed the settings
  toggle. Commit e8f598d.
- [~] **6. Darknut enemy changes for parry system** — **NEEDS USER DECISION.**
  Research found the fork's Darknut changes are ALL confuse/lockout/wolf/rupee,
  ZERO parry content; the dusk parry system is player-side/hook-based so Darknuts
  are already parryable with nothing ported. No parry source exists on either
  side. Options for the user: (a) a parry *fix* (player-side shield_hooks) if a
  specific behavior is wrong in-game, or (b) port the fork's Darknut
  confuse/lockout feature (a different, non-parry feature). Not building blind.
- [x] **7. Region damage multiplier master + deps organization** — defaults
  aligned to fork, master→dependent nesting w/ is_disabled gating, Difficulty +
  Economy sections. Commit 42c5866.

## ADD (new, fork-referenced)

- [x] **8. Incoming-damage multiplier scaler** — g_incoming_damage_scale
  (0.5x/1x/2x/4x) composed in on_damage_mag_post; select next to Link damage
  decrease. Commit 42c5866.

---

## Log

- Flurry toggle hidden (d30ecba).
- Magic Armor (8b60a34), wolf howl ungate (6e6d571), sumo fists toggle (e8f598d).
- Region org + incoming scaler (42c5866).
- Darknut: flagged for user — no parry source; do not fabricate.
- Soulbound potion: starting (grant path + init first).
