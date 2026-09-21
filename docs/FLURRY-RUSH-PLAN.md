# Flurry Rush — implementation plan

Status: **scoped, not started.** The toggle is registered and hidden
(`albw_settings_ui.cpp:71-76`); the module compiles and hooks today but the
feature is roughly one third built.

---

## 1. What exists now

| piece | where | state |
|---|---|---|
| State module | `src/flurry_rush.cpp` (322 lines) | live |
| Hooks (8) | `src/flurry_hooks.cpp` | live, all typed `DEFINE_HOOK`, all portable |
| Gate | `flurry_rush.cpp:45-47` — `g_focused_arts && g_flurry_rush` | both default off |
| Sim time scale | `src/sim_time_scale.cpp` | **half-built** — see §4 |

What it actually does today: slow non-Link actor *movement*, hold the Z-lock,
and let the player's ordinary sidestep/backflip-cancel combo play out.

**Why it is "never completed": there is no Flurry Rush attack proc.** The
fork's entire combat behaviour — snap-lunge, three-swing chain, recovery chain
gate, slow-mo-widened hit detection — lives in
`FORK/src/d/actor/d_a_alink_flurry.inc` (407 lines) with no counterpart here.

Dead code to clean up when this lands: `dFlurryRush_shouldSuppressAlbwSpend()`
(`flurry_rush.cpp:148-150`) has no caller anywhere.

## 2. The design (user-specified, differs from the fork — see §6)

1. **Bought in the shop as a FOURTH Focused Arts tier**, after the existing
   tier unlocks. Reuses the FA tier row pattern in `rental_shop.cpp` rather
   than introducing a new purchasable concept.
2. **Session-only purchase. No save bit.** Precedent: Deity Armor is
   re-purchased each session (`rental_shop.cpp:169`). A `static bool` reset on
   session reset writes nothing.
3. **Trigger:** backflip while **Z-locked** and at the shield's **current max
   bash charges**.
4. **The backflip spends a bash charge.** So at max-1 you cannot enter: bashing
   and flurrying compete for one resource, and the bash bar becomes shared
   currency rather than two independent systems.

## 3. The trigger — cheapest part, all predicates exist

| predicate | exists? | where |
|---|---|---|
| backflip | **already hooked**, already filtering plain backflips | `procBackJumpInit` — `flurry_hooks.cpp:19`, `:36-44` |
| backflip landing | yes, hooked | `procBackJumpLandInit` — `flurry_hooks.cpp:21` |
| Z-lock | yes | `daPy_py_c::checkAttentionLock()`, already called at `flurry_rush.cpp:191` |
| charges / max | yes | `dShield_getBashCharges()` / `dShield_getMaxBashCharges()` (`shield.h:93-94`) |
| "bar is full" | **needs a one-liner** | add `dShield_isBashBarFull()` beside them. Do **not** reuse `dShield_canUseFullBarPunish()` — that is a pending-punish latch, not "bar is full" |
| spend a charge | yes | `loseBashCharges(1)` (`shield.cpp:506`) |

Work: replace the predicate body in `dFlurryRush_tryPerfectDodge`
(`flurry_rush.cpp:184-228`). Hours, not days.

**Side effect worth knowing:** dropping the fork's enemy-telegraph condition
also removes its **Bokoblin-only** restriction (`queryOcTelegraph`,
`flurry_rush.cpp:49-77`), which is the fork's own unfinished Phase 8. The
redesign makes Flurry Rush work against every enemy for free.

**And what it costs:** the fork gates entry on a 6-8 frame reaction window. The
new trigger is "have the resource, hold Z, backflip" — no reaction test. If a
skill check is wanted, the fork already built two places for it: the 2s start
gate (`kFlurryStartGateRealSeconds`, `flurry_rush.cpp:24`) and the recovery
chain gate (`d_a_alink_flurry.inc:383-404`). The skill moves from entry to
sustain.

## 4. The slow-mo — the real risk, and it predates this design

The fork's slow-mo is a **JSystem engine edit**:

```cpp
// FORK libs/JSystem/src/J3DGraphAnimator/J3DAnimation.cpp:140-145
void J3DFrameCtrl::update() {
#if TARGET_PC
    updateWithRateScale(dusk_world_sim_time_scale);
```

with Link exempted by `daPy_frameCtrl_c::updateFrame()` calling
`updateWithRateScale(1.0f)` (`FORK/src/d/actor/d_a_player.cpp:31-38`,
comment: *"Flurry Rush slows the world, not Link"*). Stock has neither.

Note also that the fork drives the scale every sim tick from `fapGm_Execute`
(`f_ap_game.cpp:946-955`) while the mod sets it on entry/exit — equivalent
today, but not once a second consumer of the scale exists.

**There is no service alternative.** Every SDK service header was checked; no
host-level time-scale API exists in Dusklight 2.0.1.

**Route (DN-10 step 1):** port the fork's `updateWithRateScale` — it is
`update()` verbatim with one changed line (`mFrame += mRate * rateScale`) — and
hook `&J3DFrameCtrl::update` PRE (header-declared:
`JSystem/J3DGraphAnimator/J3DAnimation.h:980`, so portable typed hook, no
`check_hooks` baseline change). Hook `&daPy_frameCtrl_c::updateFrame` to force
Link back to 1.0.

**Four risks, in order:**

1. **Hotness.** `J3DFrameCtrl::update` runs for every animation in the game —
   actors, UI, materials, particles — many times per frame. The early-out on
   `scale >= 0.999f` must be the literal first statement or it costs FPS with
   the feature *off*. That early-out is also what keeps "toggle off == stock".
2. **Fidelity delta on `SKIP_ORIGINAL`** — resolved, and it is not a fork
   oversight. Stock's `update()` opens with
   `IF_DUSK(dusk::interp::material::Update materialUpdate(*this));`, a
   frame-interpolation guard. The fork's scaled variant lacks it because
   `dusk/interp` **does not exist anywhere in the fork tree** — it never had
   the guard to drop. Our ported body cannot keep it either: the header lives
   in the host's `src/`, not the SDK, and `dusklight_exports.def` carries zero
   `interp@dusk` symbols. Net cost: non-Link material animation loses frame
   interpolation for the duration of a rush window. Link keeps it, because Link
   runs the original — closer to stock than the fork's own
   `updateWithRateScale(1.0f)`. Asking upstream to export it is the real DN-10
   step 1 if this ever matters visually.
3. **Blast radius.** Scaling all J3D frame controllers also slows UI, cutscene
   actors and material animation. The fork accepts this.
4. ~~It obsoletes existing code.~~ **CORRECTED after the port landed.** The
   `fopAcM_posMove` hooks (`flurry_hooks.cpp:69-106`) do **NOT** double-apply
   and must **not** be deleted: position advance and animation-frame advance
   are different quantities and both are needed. The fork has its own
   `fopAcM_posMove` edit (`f_op_actor_mng.cpp:823-849`) with the same ALINK
   exemption. They ARE a paraphrase and want a proper re-port later — the donor
   scales `pos` (and `i_movePos`) and never touches `speed`, whereas the mod
   scales `speed` and divides it back (lossy, and `speed` reads wrong during
   the call), skips `i_movePos`, and lacks the quick-equip clause.

**Land this alone and verify it before anything stacks on it.** It is
independently testable: enemies visibly slow, Link does not.

## 5. The attack proc — port via the overlay technique

`PROC_FLURRY_RUSH` (fork id 0x162) does not exist in stock's proc enum or
table; adding an id would index past the table. Use the **alink proc overlay**
already shipped twice here (Hurricane Spin; sibling technique for Deku Leaf):
keep Link in an existing valid proc, hook that proc's per-frame method PRE, run
the ported body while mod state says "rush active", `HOOK_SKIP_ORIGINAL`, and
exit via `procWaitInit()`.

Host proc: **`PROC_CUT_NORMAL`** (`d_a_alink.h:1053`, methods `:1899`/`:1900`) —
the fork's `flurryBeginSwing` is a cut-normal swing with a custom chain gate.

**Every symbol `d_a_alink_flurry.inc` touches is present in stock.** Three
small gaps, all donor constants or an existing substitution:

- `CUT_NM_PARAM_LEFT/RIGHT/VERTICAL` — file-scope enum, fork
  `d_a_alink_cut.inc:24`; reproduce verbatim
- `l_halfAtnWaitBaseAnime` — file-static data `cXyz(3.5f, 97.0f, -7.0f)`, fork
  `d_a_alink.cpp:133`; reproduce verbatim
- `manualShieldBlocksSwordInput()` is fork-only; the mod already has
  `albw_manual_shield_blocks_sword()` (`shield_adapt.h:56`) — straight swap

Extraction goes through `tools/port/port_tool.py`, not by hand.

## 6. DN-10 ledger — what is a port and what is invented

**Ported (donor exists, must be used):** the proc state machine, sword profile
table, start gate, chain gate, snap-lunge, widened hit check, the sim-scale
mechanism, the Link-exempt frame controller, the shop-row pattern.

**Invented (needs the user's go — recorded here as given):**
1. The trigger predicate. The fork gates on an *enemy telegraph*; this design
   gates on *player resource state*.
2. Dropping the Focused Arts perfect-dodge spend economy
   (`dFocusedArts_canPerfectDodgeSpend` / `onPerfectDodgeSpend`) in favour of
   bash charges.
3. Buying it as a fourth FA tier — no donor precedent.

## 7. Sequencing

1. ~~Slow-mo port, alone, verified~~ **DONE, user-confirmed** ("every enemy in
   the world slowed down").
2. ~~Proc overlay via port_tool~~ **DONE, user-confirmed** ("Flurry works,
   thoroughly").
3. ~~Trigger predicate swap + `dShield_isBashBarFull()` + charge spend~~ **DONE.**
4. ~~Shop tier row, session-only~~ **DONE.** No new shop code was needed — the
   `VISIBLE_FA_TIER` row is entirely data-driven from the `dFocusedArts_*ShopTier*`
   accessors, so tier 4 is a table extension in `focused_arts_core.inc`.
   `kFocusedArtsMaxTier` deliberately stays **3**: raising it would drag
   `getEffectiveTier` / `getMaxBank` / `hasSpecialFinishers` with it and give the
   meter a fourth bank segment. The purchase is a `static bool`, cleared by
   `dFocusedArts_resetRuntimeState`, and writes nothing to the save.
   `g_focused_arts_cheat` grants it so testing is free.
5. **NEXT.** Re-port the `fopAcM_posMove` scaling from the donor (see §4 item 4 —
   a paraphrase to replace, NOT code to delete) and remove the dead
   `shouldSuppressAlbwSpend`.
6. ~~Un-hide the toggle~~ **DONE** — exposed, and its help text now describes the
   shipped behaviour rather than the half-built one.

### Still open beyond the original sequence

- **Sword draw on entry** — FIXED and user-confirmed. Every stock route into a
  sword swing is gated on `mEquipItem == 0x103`; the overlay was not, because the
  fork's dodge trigger always runs with the sword already out. Stock's own
  instant-equip idiom (`swordEquip(TRUE)` → `commonChangeItem()` →
  `resetUpperAnime`) now runs in `flurryOverlayProcInit`.
- **`dFlurryRush_cancelOnSwordEquipChange`** (fork `d_albw_flurry_rush.cpp:339`,
  called from fork `d_meter2_info.cpp:1735`) is **not ported** — a sword change
  landing inside a rush window. Narrow, but it is donor code we are missing.
- **D-1: rush invincibility** is not ported. Damage currently interrupts a rush;
  the fork makes you immune for its duration. Needs the user's call.
- **Hit-cap shape** — the user is considering per-shield or sword+shield instead
  of the current per-sword table. `spendGate` / `barCost` in the profile struct
  are dead fields today; `queryOcTelegraph` is dead code. Do not delete either
  until the hit-cap decision lands, since a redesign may want the slots.
- **Probes** — `ALBW_FLURRY_PROBE` and `ALBW_MAGICJAR_PROBE` must both be 0
  before a release build (docs/RELEASE-PROCEDURE.md).

## 8. Not in scope

Aerial-bow mode (`dFlurryRushMode_AerialBow`) is unfinished in the **fork** too
— its Phase 7. Not port debt; do not treat it as a gap.
