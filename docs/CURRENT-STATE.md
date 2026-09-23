# Current state — dev.albt.albw

**The index and status board.** Start here. Updated 2026-09-22. When memory
fails, this is the source of truth for what is shipped, what is local, what is
broken, and what is planned. Every other doc is linked from here.

---

## 0. Release reality — READ FIRST

- **Last PUBLISHED build: `v0.2.7`.** That is the only tag on the portal repo
  (`WadeWinningWilson/A-Link-Between-Twilight`).
- **There is NO `v0.2.8`.** `mod.json` reads `0.2.8`, but that was only a
  `main` push for a CI build - never tagged, never released.
- So **~34 commits since `v0.2.7` are UNSHIPPED**, including fixes for bugs that
  are hurting players right now (see §1).
- Release procedure: [RELEASE-PROCEDURE.md](RELEASE-PROCEDURE.md). Gates:
  privacy scan (source + all 8 binary slices) -> `check_hooks.py` ->
  RelWithDebInfo/redistributable-CRT -> 8-platform CI -> verify artifact ->
  tag (needs the user's explicit go).

## 1. LIVE bugs in the shipped v0.2.7 (fixed locally, unshipped)

| Bug | Effect on v0.2.7 players | Fixed at | 
|---|---|---|
| **macOS total failure** | `fpcMtd_Execute` unresolved -> soul_of_light init returned MOD_ERROR -> **whole mod fails to load on macOS** | `b1950d3` (non-fatal init; soul of light degrades, mod loads) |
| **Wallet destruction (D-1)** | stored wallet tier 3; on uninstall every rupee pickup zeroes the wallet permanently | `ff8e979` (derive from Cave-of-Ordeals flag; repair on load) |
| **saveBitLabels collisions (D-2)** | mod flags aliased real designer flags; breaking two shields granted the Mirror shard + Fused Shadow | `b14bf58` + `8c3d4f4` (config.json) |
| **Tear-of-Light softlock** | die on your own tear -> model deform, alive-and-dead, softlock | `babca9a` / `d676e64` / `4dcc46a` |
| **Helm Splitter double-count** | Helm Splitter billed the FA meter twice vs Darknuts | `e530132` |

**These are the argument for cutting a release NOW.** DT (§3) is feature-gated
off, so it ships dormant and does not block.

## 2. Release blockers (must clear before tagging)

- Three probes STILL ON, must go to 0: `ALBW_DARKNUT_PROBE` (btn_probe.h),
  `ALBW_MAGICJAR_PROBE` (magic_jar_probe.h), `ALBW_SOUL_PROBE` (soul_probe.h).
  (Pulled 2026-09-22: `ALBW_DEVIL_PROBE` → 0, and the wolf-guard `[wguard]/[watk]`
  diagnostics removed from source entirely.) NEW bring-up probe ON:
  `ALBW_BOKO_DT_PROBE` (bokoblin_dt.h `[bokoDT]` audit) — pull once E_OC is validated.
- Bump `mod.json` past the untagged 0.2.8 to **0.2.9**.
- Magic jars: grant chain proven working (meter +3633 = 1/3), but the "player
  doesn't SEE it on the bar" question is open - a HUD read, not the grant.

## 3. Devil Trigger — working baseline, NOT shippable

- **Profile abstraction — ✅ BUILT (step 1, behavior-preserving).** The generic DT
  enforcement (arm → not-dead → not-mid-swing → second execute) now lives in
  `dAlbwDevil_execProfile` (devil_trigger); per-enemy specifics are a `DTProfile`
  struct (`isDead`, `redispatch`, optional pre/post-passes + `fallbackTick`;
  health reader stays via `registerHealthOverride`). The Darknut is the FIRST
  profile (`s_btnDtProfile` in btn_parry) and its `on_btn_execute_post` is now
  just the latch + `dAlbwDevil_execProfile`. Same Darknut behavior; check_hooks
  30/30. **Next: Bokoblin** = register a second `DTProfile` (+ its execute front
  door, and an audit-probe pass) — measures per-enemy patch cost. Portable-seam
  migration (typed hooks → one generic `g_fopAc_Method` actor dispatcher) only
  swaps the front door, not the profiles; file [UPSTREAM-LINUX-STATICS.md] first.
- **Bokoblin (E_OC) — ✅ SECOND profile BUILT (step 2 bring-up), awaiting audit.**
  `bokoblin_dt.cpp`: `s_ocDtProfile` + a POST hook on `daE_OC_c::execute` (typed
  member hook — portable, check_hooks 30/30). E_OC uses standard `health` (generic
  reader, NO override — unlike the Darknut's scratch pool) and defaults to NORMAL
  category, so it is DT-eligible out of the box; death predicate = `checkBeforeDeath`.
  Proves the abstraction: the profile is ~4 fields + 2 tiny fns, no changes to the
  generic core. **Audit probe `[bokoDT]` ON** (logs frac/action/dpos deltas across
  the second execute) to measure double-execute anomalies = per-enemy patch cost.
  `ALBW_BOKO_DT_PROBE` must go to 0 before release. E_OC has a multi-actor
  structure (`mpBattle`/`mpDamage`/`mpTalk`) — the audit will show if the wrong
  sub-instance arms or a value double-processes.

- Design: [DEVIL-TRIGGER-SCOPE.md](DEVIL-TRIGGER-SCOPE.md). Methods post-mortem:
  [DEVIL-TRIGGER-METHODS.md](DEVIL-TRIGGER-METHODS.md).
- Enemy enrages below **50%** HP (was 25%): faster + knockback-immune. Common
  and mid-boss only; bosses excluded by the tier gate.
- Current method: the **clean sub-step** (`42ca602` restored at `1444e46`),
  Darknut only. Neither crashed nor teleported - those were later "fixes" of
  mine (hybrid's cCcS::Set suppression, and safe-state gating), both reverted.
- Death-gate + 50% at `8090700`. HP-scaling-per-multiplier fixed (`42ca602`) -
  the Darknut's real pool is `field_0x6fc`/`field_0x700`, not `health`.
- **NOT shippable:** once-per-frame doublings remain (halved guard window,
  halved i-frames, damage-outside-window). These are STEP 2 - the SAFE fixes
  only (capture/restore i-frames, guard the window tick, `ClrTgHit` before the
  re-run; NONE touch collider registration, which is what crashed the hybrid).
- **Perfect-parry openings — ✅ BUILT + GENERALIZED + user-confirmed (scope §7).**
  A parry on any ARMED enemy opens an elongated window (`dAlbwDevil_openGuardWindow`,
  90f) in the generic DT policy module (`sGuardOpenFrames[]` + `sReactionPending[]`),
  wired at the shared `dShield_onShieldHit` seam (human + wolf) — NOT Darknut-
  specific. The Darknut consumes it via `dAlbwDevil_consumeOpenReaction` (once per
  parry) to fire its **full native bashed reaction** — head-lock + `ACT_YOROKE`
  (unarmored) / `ACT_GUARDH` (armored) + guard-open — bypassing only the bash-credit
  gate (a parry earns the opening). User-confirmed the enemy reacts reliably. The
  once-per-parry latch fixed the earlier 3× re-fire. DEVIL probe pulled (→0).
- The three methods (sub-step / direct scaling / hybrid) and why each failed are
  fully recorded in DEVIL-TRIGGER-METHODS; the sub-step and direct-scaling
  recipes are preserved there for reuse.

## 4. Port backlog — features wanting a port (NONE built)

### 4a. Wolf charge on a GUARDED hit — the user's actual request
> "Attacking an enemy that GUARDS still builds wolf charge."

**Status: DEFERRED — approach identified, seam still unresolved.** Investigated
in-game with probes (2026-09-22):
- The wolf-charge grant lives in `albw_wolf_cc_at_check` (`wolf_uty_port.inc`,
  called from wolf_combat's `cc_at_check` PRE hook), gated on `mAttackPower > 0`.
- **A guarded/armored bite NEVER reaches `cc_at_check`** — the guard/armor absorbs
  the wolf's AT before the enemy's attack-check runs (probe: biting a guarding
  Darknut produced ZERO `cc_at_check` hits; only the soft scarecrow — an NPC,
  correctly excluded — did). So the grant there can't fire for a guarded hit; the
  clean-bite grant (3/15) is unaffected.
- **The wolf's own AT flags don't help either:** `mAtCyl.ChkAtHit` and
  `mAtCyl.ChkAtShieldHit` did NOT fire when biting the guarding Darknut (they fire
  only on the soft scarecrow). So neither the enemy seam nor the wolf-AT-Tg seam
  sees a guarded/armored hit.
- **User direction:** make it WOLF-side and enemy-agnostic — "notice guards
  universally," "any connecting hit." The grant fn is ready
  (`dAlbwWolfCombat_onGuardedBite`, 1/15) but NOT wired.
- **NEXT:** capture the `[watk]`-style dump (all wolf attack-collider flags:
  mAtCyl/mAtSph × at/shield/co) on a guarding Darknut to find which flag/collider
  actually registers the guarded connect, then wire the 1/15 grant to it. Do NOT
  conflate with 4b.

### 4b. Wolf charge on HANG-BITE / chest-mash grabs — separate finding
The diff found the mod ported the grab-charge path for `e_s1` (Shadow Beast)
only; **seven enemies award none** where the fork does: `e_yc, e_dn, e_gi
(Gibdo), e_mf, e_po, e_vt, e_ymb`. Each needs a whole-actor `execute` port
mirroring `e_s1_hooks.cpp` + `port_tool.py`. Cost: +up to 7 to the
`check_hooks` baseline (30), all Linux/macOS-inert without per-platform mangled
names in `albw_symbols.h`. **User decision pending: targeted (just Gibdo) vs all
seven.** Full findings in `staging/wolf-charge-guard/` (agent report).

### 4c. Death toasts — Continue Here / Warp to Ordon
Scoped: [DEATH-TOAST-SCOPE.md](DEATH-TOAST-SCOPE.md). FEASIBLE via SDK
`UiService`; no save writes; fills the dead `albw_oocoo_on_warp_choice` seam.
Decisions pending: dialog vs fork-literal toast; confirm a Shade Watcher NPC
calls `dShadeRefuge_setRespawn`. Shade Watcher variant reuses `shade_refuge.cpp`.

### 4d. Wolf guard / parry — Midna's Shield  [BUILT ✅ — user-confirmed]
Scoped: [WOLF-GUARD-SCOPE.md](WOLF-GUARD-SCOPE.md). **Working in-game.** Files:
`wolf_guard.{h,cpp}`, `wolf_guard_hooks.cpp`, `guardRaised` +
`dShield_updateWolfGuardTracking` in shield.cpp, `dAlbwWolfCombat_onParry` (+1/15).
Confirmed pieces:
- **Timed parry** — reuses the shared parry engine via the generalized
  `guardRaised` predicate; fires the DT parry-opening (§3) so the enemy reacts.
- **Held guard** — blocks frontal hits at the `checkDamageAction` seam (SKIP + no-
  damage), incl. Darknut guard-break swings; drains the equipped shield's
  durability once per ~swing (24f cooldown; `dShield_onBlockHit` made wolf-aware).
  Does NOT call `dShield_onFailedGuardBlock` (its `procGuardBreakInit` deforms/
  kills the wolf — see §6 lessons).
- **Parry SFX** — spark only (`dShield_playWolfParryFeedback`; clang removed per
  user).
- **Midna's Shield on her face** — the equipped shield is redirected from the
  wolf's back to Midna's head joint during guard (POST hook on
  `setWolfItemMatrix`, NOT the human `setItemMatrix`); flip+yaw+offset tuned. The
  Midna body-lean anime was removed (pointless once the shield's on her face).
- **Unlock** — config-backed shop purchase ("Midna's Shield", 100r, first-twilight
  gate); True ALBW makes the row appear but does NOT auto-grant. No save writes.
- **No toggle of its own** — part of Wolf Link combat.
- **Deferred:** guarded-attack wolf-charge (§4a — seam unresolved).
**Sibling save-write bug FIXED** — arm/howl/charge purchases moved to the
`albw_save_flags` config allocator (`ALBW_FLAG_HOWL/ARM/CHARGE_PURCHASED`); no
more save-bit writes (713/714 were event registers). Availability reads + True
ALBW unchanged; existing owners re-unlock once. Wolf-form
counterpart to the human ALBW shield — **block + timed parry (NO bash), one input
(hold-R)**,
visually Midna raising the equipped shield. **Grounds on native seams end to
end:** wolf damage handler already reroutes on a wolf flag
(`checkWolfBarrierHitReverse`/`field_0x3100`, `d_a_alink_damage.inc:570/984`), so
NO human guard proc; parry reuses native `procFrontRollSuccessInit` + the
`albwBeginGuardOpenWindow` outcome; raise/hold/stow visual is field Midna animes
`ANM_S_TAKES/S_WAITS/S_PACKAWAY` (`daMidna_c`, NOT demo-locked — already the
instance our Midna-arm art drives). **Inherits the human shield verbatim:**
equipped shield parented to the wolf → per-shield durability
(`dAlbwHP_applyDurabilityMult`), break, HUD bar, and Parry-Master chip-to-reclaim
(`g_parry_master`) all for free. **Bash CUT** — a parry auto-opens a DT enemy
as-if-bashed (unified human+wolf, DT §7), so no proactive bash is needed; feature
is fully DT-independent. **One authored piece:** held-guard damage reduction =
DN-10 step 2 at the damage seam (user: held-guard is IN). **Optional:** a parry
adds 1/15 (mash-parity) to the wolf charge counter (`addWolfChargeSteps`,
parry-only, easily cut). **Unlock:** shop purchase after Midna's arm, save-**check** like siblings
(no writes), free under True ALBW, under wolf-combat toggle. Sequencing:
parry-first → held-guard → visual (+ optional charge feed). Typed member hooks,
no Linux/macOS-inert surface. Open at wire time: shield re-parent (wolf
back→Midna hand), Midna service-mode drivability, and how sibling wolf arts
persist a purchased unlock without a save write.

## 5. Cross-platform

- **Linux:** 30 hook sites inert (file-local statics). Coverage plan:
  [LINUX-HOOK-COVERAGE.md](LINUX-HOOK-COVERAGE.md). Upstream report ready to
  file: [UPSTREAM-LINUX-STATICS.md](UPSTREAM-LINUX-STATICS.md) - **send it
  before building Route 1** (if upstream fixes symgen's ELF `.symtab` reader,
  the whole workaround is unnecessary).
- **macOS:** `fpcMtd_Execute` (and likely small accessors like
  `dSv_memBit_c::isTbox`) inline out of the manifest - the same class of gap as
  Linux, different symbols. Soul of light degrades there (§1); a full fix means
  resolving those or moving the tick off the fragile hook.

## 6. Standing themes / traps (hard-won, do not relearn)

- **Uncalled-function class of bug:** [UNCALLED-PORT-SWEEP.md](UNCALLED-PORT-SWEEP.md).
  SEVEN ported-but-never-called functions became live bugs this cycle
  (onGuardAttackConnect, playParrySuccessFeedback, the two helm-splitter
  functions, pushTearRenderFlags, scaleHpValue, and the still-dead
  albw_oocoo_on_warp_choice that 4c would fill). Re-run the sweep before a
  release.
- **A hook miss must be LOUD and SCOPED, never fatal** - the macOS bug was this
  doctrine violated in soul_of_light. flurry_hooks.cpp / fyrus.cpp are the
  reference.
- **`_build_mod.bat` lies:** it prints "installed" after a failed link and after
  a copy blocked by the running game. Verify the installed `.dusk` mtime/size,
  not the log line.
- **Save writes:** [PORT-BATCH.md](PORT-BATCH.md) item 6 (B_TN identity),
  config.json migration done for flags + counters; anything STOCK reads stays in
  the save (see albw_save_flags.h). No new save writes for runtime state.
- **DN-10 donor-first**; **toggle off == provably stock**; **8 platforms via CI,
  local builds are compile checks.**

### Wolf-form / enemy-reaction ports — generalizable lessons (DT + Midna's Shield)
Hard-won this cycle; apply to any future wolf-combat or enemy-reaction feature.
1. **NEVER drive a human proc (or a fn that calls one) on the wolf.**
   `dShield_onFailedGuardBlock` → `link->procGuardBreakInit()` on the wolf =
   model deform → voids out of the world → death. Same class as the Zora `al_face`
   crash. Before calling any `dShield_*`/`daAlink_c` helper from wolf code, read it
   for a proc/pose/`procXxxInit`/`setActionMode` call. Use numbers-only helpers
   (`dShield_onBlockHit`, durability) or a wolf-safe variant.
2. **Wolf ≠ human at the seam.** The wolf uses SEPARATE functions:
   `setWolfItemMatrix` (not `setItemMatrix`) for item draw; no human guard proc, so
   the parry rides `checkDamageAction` (the shared damage dispatch) not
   `procGuardSlipInit`. Confirm the wolf path with a probe before hooking — a hook
   on the human function silently never fires in wolf form (cost us a whole round).
3. **Generalize a shared engine by widening ONE predicate**, not by duplicating.
   The whole human parry engine accepted the wolf once `checkUpperGuardAnime()` was
   swapped for `guardRaised()` (wolf flag OR human anime). Human path stays
   byte-identical.
4. **Policy module + per-actor enforcement = generic.** DT owns the state
   (`sArmed[]`/`sGuardOpenFrames[]`/`sReactionPending[]`); each actor consults it
   (`isGuardOpen`/`consumeOpenReaction`) and translates to its own native reaction.
   New DT enemies inherit the parry-opening for free. Set once-per-event with a
   consume-latch, never per-frame (the 3× re-fire bug).
5. **Per-frame vs per-event.** A `checkDamageAction`/execute hook fires every frame
   a swing overlaps — dedupe drains/reactions (durability cooldown; reaction latch)
   or you drain a whole bar / react 3× in one hit.
6. **Neutralize damage with SKIP_ORIGINAL + retval 0 AND clear the collider.**
   Clearing alone (HOOK_CONTINUE) didn't reliably block; a dangling collider was
   the DT crash. Do both.
7. **Guard/armor absorbs before the enemy's `cc_at_check`.** A guarded/armored hit
   never reaches the target's attack-check, and the wolf's own AT-Tg/AT-shield
   flags don't fire either — so "did I hit a guarding enemy" needs a WOLF-side
   any-connect detector, still unresolved (§4a).
8. **Probe = loud + multi-hypothesis + verify the RIGHT seam.** Two rounds were
   lost hooking the human function / wrong collider; one comprehensive dump
   (all colliders × all flags) beats guessing. Enemy ids in logs are `fpcNm_*`
   (0x213 = Darknut, 0x241 = scarecrow NPC) — decode before concluding.

## 7. Doc map

| Doc | Covers |
|---|---|
| **CURRENT-STATE.md** (this) | index + status board |
| [RELEASE-PROCEDURE.md](RELEASE-PROCEDURE.md) | the 6-gate release ritual |
| [DEVIL-TRIGGER-SCOPE.md](DEVIL-TRIGGER-SCOPE.md) | DT design + parry openings (§7) |
| [DEVIL-TRIGGER-METHODS.md](DEVIL-TRIGGER-METHODS.md) | DT sub-step/direct/hybrid post-mortem + recipes |
| [DEATH-TOAST-SCOPE.md](DEATH-TOAST-SCOPE.md) | death Continue/Warp toast port scope |
| [WOLF-GUARD-SCOPE.md](WOLF-GUARD-SCOPE.md) | wolf guard/parry (Midna's Shield) design scope |
| [SETTINGS-REORG-PLAN.md](SETTINGS-REORG-PLAN.md) | settings/menu reorg — target tab layout + move map |
| [FLURRY-RUSH-PLAN.md](FLURRY-RUSH-PLAN.md) | Flurry Rush status + remaining steps |
| [LINUX-HOOK-COVERAGE.md](LINUX-HOOK-COVERAGE.md) | Linux inert-hook plan (Routes 1-3) |
| [UPSTREAM-LINUX-STATICS.md](UPSTREAM-LINUX-STATICS.md) | upstream bug report (symgen ELF statics) |
| [UNCALLED-PORT-SWEEP.md](UNCALLED-PORT-SWEEP.md) | defined-but-never-called audit |
| [PORT-BATCH.md](PORT-BATCH.md) | earlier port batch + B_TN identity correction |
| [COMPAT-LAZYTWEAKS.md](COMPAT-LAZYTWEAKS.md) | Lazy Tweaks cross-compat |
