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

- Three probes ON, must go to 0: `ALBW_DARKNUT_PROBE` (btn_probe.h),
  `ALBW_MAGICJAR_PROBE` (magic_jar_probe.h), `ALBW_SOUL_PROBE` (soul_probe.h).
- Bump `mod.json` past the untagged 0.2.8 to **0.2.9**.
- Magic jars: grant chain proven working (meter +3633 = 1/3), but the "player
  doesn't SEE it on the bar" question is open - a HUD read, not the grant.

## 3. Devil Trigger — working baseline, NOT shippable

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
- **Planned (scope §7):** perfect-parry openings - a parry on a DT enemy
  triggers its native `albwBeginGuardOpenWindow` with a longer window than a
  bash. Bundled with step 2.
- The three methods (sub-step / direct scaling / hybrid) and why each failed are
  fully recorded in DEVIL-TRIGGER-METHODS; the sub-step and direct-scaling
  recipes are preserved there for reuse.

## 4. Port backlog — features wanting a port (NONE built)

### 4a. Wolf charge on a GUARDED hit — the user's actual request
> "Attacking an enemy that GUARDS still builds wolf charge."

**Status: NEEDS INVESTIGATION - may be a bug, not a port.** A wolf bite awards
charge via `cc_at_check` (mod `wolf_uty_port.inc:536`) gated on
`mAttackPower > 0`. Guard is resolved TARGET-side (in the enemy's
`damage_check`) AFTER `cc_at_check`, so in principle a guarded bite should
already award charge in both trees. The wolf-charge diff
(`staging/wolf-charge-guard/`) found **no separate fork mechanism** for a
shield/no-connect bite. So the open question is: does our mod actually award on
a guarded bite in-game? If not, is `mAttackPower` being zeroed before the wolf
block, or does the shield deflect before `cc_at_check` runs at all?
**NEXT: probe `cc_at_check` against a shielding enemy (Bokoblin/Bulblin) in
wolf form; decide port vs bug vs new-feature from the result.** This is the one
the user cares about - do NOT conflate with 4b.

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

## 7. Doc map

| Doc | Covers |
|---|---|
| **CURRENT-STATE.md** (this) | index + status board |
| [RELEASE-PROCEDURE.md](RELEASE-PROCEDURE.md) | the 6-gate release ritual |
| [DEVIL-TRIGGER-SCOPE.md](DEVIL-TRIGGER-SCOPE.md) | DT design + parry openings (§7) |
| [DEVIL-TRIGGER-METHODS.md](DEVIL-TRIGGER-METHODS.md) | DT sub-step/direct/hybrid post-mortem + recipes |
| [DEATH-TOAST-SCOPE.md](DEATH-TOAST-SCOPE.md) | death Continue/Warp toast port scope |
| [FLURRY-RUSH-PLAN.md](FLURRY-RUSH-PLAN.md) | Flurry Rush status + remaining steps |
| [LINUX-HOOK-COVERAGE.md](LINUX-HOOK-COVERAGE.md) | Linux inert-hook plan (Routes 1-3) |
| [UPSTREAM-LINUX-STATICS.md](UPSTREAM-LINUX-STATICS.md) | upstream bug report (symgen ELF statics) |
| [UNCALLED-PORT-SWEEP.md](UNCALLED-PORT-SWEEP.md) | defined-but-never-called audit |
| [PORT-BATCH.md](PORT-BATCH.md) | earlier port batch + B_TN identity correction |
| [COMPAT-LAZYTWEAKS.md](COMPAT-LAZYTWEAKS.md) | Lazy Tweaks cross-compat |
