# Devil Trigger — enemy enrage below 25% HP

> Status board / index: [CURRENT-STATE.md](CURRENT-STATE.md)

**Status: scoped, not started. INVENTED — no donor.**

User's design: an enemy below 25% HP speeds up and ignores knockback. The
inverse of Flurry Rush. The constraint was stated up front and it is the right
one: *everything* must speed up together — animation, attack collision,
movement — not just the animation.

Scope for now: **common enemies and mid-bosses only.**

> **DN-10.** The fork has no Devil Trigger. Every line here is invented, so
> there is no donor to port and no reference to match. Allowed with the user's
> go (given), but it means the collision behaviour has to be *tuned in game*,
> not verified against a fork.

---

## 1. What already exists and is reusable

More than expected — three of the four pieces are built.

| Need | Already have | Where |
|---|---|---|
| Enemy tier (common / mid-boss / boss / final) | `dAlbwHP_getCategory(s16 profName)` → `dAlbwHP_NORMAL / MID_BOSS / BOSS / FINAL / EXCLUDED` | `src/hp_mult_port.h:19`, tables `region_port.cpp:50-65` |
| Current-HP fraction | `fill = actor->health / peak`, `peak = field_0x560 ? : health` | `src/enemy_hp_bars.cpp:121-147` |
| Per-actor animation rate | `mDoExt_McaMorfSO::setPlaySpeed(f32)` / `getPlaySpeed()`, inline | `m_Do_ext.h:35-36` |
| Per-actor position scaling | the `fopAcM_posMove` PRE/POST pair | `src/flurry_hooks.cpp:23` |

The 25% trigger is therefore nearly free: the tier gate and the HP fraction are
both already computed elsewhere in the mod.

**Not reusable:** `albw::set_sim_time_scale`. It **clamps to <= 1.0**
(`sim_time_scale.cpp:23`) and is a single **global** float — slow-only and
world-wide by construction, where this needs fast and per-actor. Do not widen
it; the flurry depends on its current contract.

## 2. The collision problem, and the answer that does NOT work

The attractive design is to sub-step: hook `fopAc_Execute` (already hooked by
three modules, and reachable portably through `g_fopAc_Method` — see
[LINUX-HOOK-COVERAGE.md](LINUX-HOOK-COVERAGE.md)) and run a Devil-Trigger
actor's execute twice per frame. Animation, timers, movement and the state
machine would all advance together with **no per-enemy knowledge at all**.

The hope was that the second execute would also re-register the actor's
colliders, so the attack would be tested at two points along its sweep.

**It does not.** `cCcS::Set` (`c_cc_s.cpp:56-78`) appends the **pointer**:

```c
mpObjAt[mObjAtCount] = obj;
mObjAtCount++;
```

Both entries point at the same `cCcD_Obj`, which by check time holds only its
**final** position. Registering twice tests the same place twice. No sweep
coverage is gained, and the fixed arrays are consumed twice as fast — with an
`OS_REPORT` AT-overflow warning waiting at the end of that.

So: **sub-stepping advances the actor correctly but does not fix collision.**
At 2x an attack sweeps twice as far between the single collision resolve and
can pass straight through Link without ever being tested there — *missed* hits,
not extra ones. This is the same desync the flurry documents in the other
direction (`flurry_port.inc`: *"Slow-mo desyncs enemy hurt spheres (0.1x anim)
from Link sword (1.0x)"*), which is why `flurryCheckSwordHit` carries a
fallback range test.

## 3. Three honest routes

**A. Sub-step + widen the attack collider.** Run execute N times, then scale the
AT sphere radius by roughly the extra per-frame sweep. Cheap, no per-enemy
offsets, and widening has precedent in the flurry. Cost: an approximation, and
visibly generous hitboxes at higher multipliers.

**B. Sub-step + cap the multiplier.** Keep N low enough (~1.5x) that the
per-frame sweep stays inside Link's hurt sphere, so tunnelling never occurs.
No collision code at all — the fix is a design constraint. Cost: a modest
speed-up is all you can ever have.

**C. Speed the GAPS, not the swings.** Scale approach, recovery and idle; leave
the attack animation itself at 1.0x. Collision is untouched and correct by
construction. Cost: needs per-enemy knowledge of which action modes count as
"attacking" — for the Darknut we already have that (`mActionMode1`, the
`ACT_ATTACKH` / `ACT_ATTACKSHIELDH` states we hook today).

**Recommendation: see 3b - the user's C-plus-movement refinement supersedes this.**

~~B first, C as the target.~~ Kept for the reasoning: B proves the trigger, the tier
gate and the knockback immunity end-to-end with zero collision risk. C is the
version that actually reads well — a relentless enemy whose telegraphs stay
readable — and it is *safer* than A, not merely prettier: the swing keeps stock
timing, so nothing about the attack can desync.

A is the one to avoid. A widened hitbox on a fast enemy is the combination
players experience as unfair, and it is the hardest of the three to tune.

> **SUPERSEDED IN PART.** The three speed-up methods and what each costs are
> now in [DEVIL-TRIGGER-METHODS.md](DEVIL-TRIGGER-METHODS.md), written after
> the first Darknut test. Two claims below did not survive it: sub-stepping
> was thought safe once gated to non-attack states (it is not - `action()`
> calls `damage_check()`, so damage is evaluated twice), and the per-enemy
> "attacking" state lists were thought necessary (the generic AT-registry
> signal works). Read the methods doc for the current picture.

## 3b. THE CHOSEN SHAPE — C plus movement, via state-gated sub-stepping

User asked whether C could also speed enemy *movement*. It can, and the way it
combines is better than either half alone.

**Movement and locomotion animation must scale TOGETHER or the enemy
foot-slides** — feet planted at stock rate while the body travels faster is
the classic tell. Scaling `speedF` alone would produce exactly that.

Sub-stepping gives both for free, and here is the point: **the collision
objection in §2 only applies to attack sweeps.** Gate the sub-step to
NON-ATTACK states and there is no attack collider live to miss. So:

| Enemy state | Treatment |
|---|---|
| approach / chase / recover / idle | sub-step N times — movement, locomotion animation, timers and state machine all advance together, in sync, no per-enemy morf offset needed |
| attacking | untouched at 1.0x — telegraph, swing timing and the AT collider all stock |

That is route C with movement included, and it costs nothing in collision
fidelity because the two concerns are disjoint in time.

Residual exposure, and it is small and in the forgiving direction: the
enemy's own body/hurt sphere moves faster, so Link's sword can occasionally
miss a fast-approaching enemy. A missed player hit reads as "I mistimed it";
an enemy attack passing through you reads as broken. This design only risks
the former.

### How to know it is attacking, without per-enemy tables

Per-enemy action-mode lists work (the Darknut is already mapped:
`ACT_ATTACKH` / `ACT_ATTACKSHIELDH`) but do not scale to 106 actors.

A generic signal may exist: **ask the collision system whether this actor has
an AT collider registered.** `cCcS` holds `mpObjAt[0x100]` with `mObjAtCount`
(`c_cc_s.h:14,21`) and every `cCcD_Obj` knows its owner via `GetAc()`
(`c_cc_d.h:185`). Scanning for the actor answers "is an attack live right now"
with no per-enemy knowledge at all.

Two caveats before trusting it, both needing confirmation in game:

1. **One frame of lag.** The AT list is filled during execute, so a check made
   while sub-stepping reads the PREVIOUS frame. Acceptable for a state gate -
   attacks last many frames - but it means the first frame of an attack may
   still be sub-stepped. Worth a probe.
2. **Permanent-AT enemies.** Contact-damage actors may keep an AT collider set
   at all times, in which case they would never qualify for Devil Trigger.
   That is a SAFE failure (no speed-up) rather than a dangerous one, but it
   would quietly exclude a chunk of the roster, so measure which enemies it
   silently drops before relying on it.

Fallback if the generic signal proves unreliable: per-enemy lists, shipped
incrementally, Darknut first.

### Debt this touches

The `fopAcM_posMove` PRE/POST pair (`flurry_hooks.cpp:100-136`) is already
flagged in [FLURRY-RUSH-PLAN.md](FLURRY-RUSH-PLAN.md) §4 as a lossy paraphrase
needing a re-port from the donor - it scales `speed` and divides it back, where
the donor scales `pos`. If Devil Trigger leans on the same seam, two features
depend on that re-port instead of one. Do it before building this, not after.

## 4. Knockback immunity — the easy half

Not a physics change: it is suppressing the stagger/damage state transition.
Per-enemy, but shallow. For a Darknut that is the `ACT_YOROKE` path we are
already inside (`btn_port.inc`). The generic seam is the actor's damage
handler; a pre-hook refusing the stagger transition while Devil Trigger is on
covers most enemies.

Flag: immunity must **not** also suppress *damage*. Ignoring knockback while
still taking hits is the intent; accidentally gating the damage path too would
make low-HP enemies unkillable — the exact failure this feature could cause,
and it would look like the feature "working" right up until nothing dies.

## 5. Risks

1. **Collision array overflow.** Sub-stepping double-registers every
   Devil-Trigger actor's colliders. A room of common enemies all below 25% at
   once is the worst case, and the arrays are fixed-size with only an
   `OS_REPORT` on overflow. Route C avoids this entirely; route B should still
   be measured with a full room.
2. **Re-entrant execute.** Not every actor tolerates its execute running twice
   in a frame — anything latching "did I already do X this frame" will misfire.
   Needs a per-enemy allowlist, not blanket application.
3. **Timers vs animation drift.** If animation is scaled but frame-counting
   timers are not, the enemy animates fast and still waits the stock number of
   frames between attacks. Sub-stepping avoids this by construction; a
   pure-animation approach does not.
4. **The 25% edge.** `health` fluctuates around the threshold, so a naive test
   flickers the mode on and off. Needs latching: arm once below 25%, never
   disarm until death or respawn.
5. **Feature gate.** Toggle off == provably stock, as always.

## 6. Sequencing

1. **Re-port `fopAcM_posMove`** from the donor first (FLURRY-RUSH-PLAN §4). It
   is already debt, and this feature would be the second thing standing on it.
2. **Trigger + tier gate + latch**, with a probe and **no** speed change —
   confirm it arms on the right enemies at the right HP and never flickers
   around the 25% edge.
3. **Probe the generic attacking-now signal** (§3b) against the Darknut, whose
   action modes we already know, so we can compare the AT-registry answer to
   the ground truth. This decides generic-vs-per-enemy before any speed code.
4. **Knockback immunity alone.** Visible, useful, zero collision risk - and it
   is the half that needs no speed decision at all.
5. **State-gated sub-step at a low N** (~1.5x), measured for collider-array
   headroom with a full room of low-HP enemies.
6. Raise N only once 5 is clean, and only as far as it still reads fair.

---

## 7. Perfect-parry openings — ✅ BUILT (generalized, not Darknut-specific)

**Implemented as a generic DT policy primitive (`devil_trigger.{h,cpp}`), green,
check_hooks clean.** A per-armed-enemy guard-open window (`sGuardOpenFrames[]`,
parallel to `sArmed[]`, `kDevilParryOpenFrames = 90`, ticked down in
`dAlbwDevil_tickActor`). `dAlbwDevil_openGuardWindow(actor)` sets it (no-op unless
the enemy is armed); `dAlbwDevil_isGuardOpen(actor)` reads it.

- **Set from the shared parry seam:** `dShield_onShieldHit` (which BOTH human and
  wolf parries call) opens the window on the parried enemy. One wiring point,
  human + wolf covered, and generic to any enemy.
- **Consumed per DT-enforced actor:** each actor lifts its enrage while the window
  is open. The Darknut (`btn_parry.cpp`) adds `!dAlbwDevil_isGuardOpen(self)` to
  its no-flinch condition, so during the window stagger transitions
  (ACT_YOROKE/DAMAGEH/DAMAGEL) are allowed again — the enemy reacts to hits, i.e.
  opens. Future DT-enforced enemies inherit the window for free; they just consult
  `isGuardOpen` in their own enrage enforcement. **Nothing calls the B_TN-only
  `albwBeginGuardOpenWindow` from the shared path — that was the un-generalized
  version this replaces.**

This is the piece that makes a wolf/human parry *visibly do something* to an
enraged enemy (the WOLF-GUARD test dependency).

An enraged enemy is harder to open: the bash guard-window is shorter under
Devil Trigger (the sub-step ticks `field_0xaa2` twice, 90 -> 45). The user's
answer is not only to fix that halving but to add a **skill-rewarded counter**:
a perfect parry against a DT enemy **automatically** makes it react as if
bashed, creating a new, *elongated* opening (the user: "elongated by one of our
three ways to do that" — pick the widening approach here).

> **Unified for human AND wolf, and automatic.** Against a DT enemy, a parry
> *automatically* makes the enemy react as if it had been bashed — it fires the
> (elongated) guard-open window itself. Human Link and wolf Link
> ([WOLF-GUARD-SCOPE.md](WOLF-GUARD-SCOPE.md)) share this one rule. This is *why*
> the wolf needs no bash (WOLF-GUARD §6b): the reactive parry already delivers
> the opening in every case, so there is no proactive-bash job left to do.

**This reuses a native seam we already ported — it is not a new system.** The
Darknut already has `albwBeginGuardOpenWindow(u8 frames)` and
`albwTryApplyBashGuardBreakFromHit()` (mod `src/btn_port.inc`; fork
`d_a_b_tn.cpp:1337/1408`): a bash cracks the guard and opens a window where
hits land. A perfect parry can trigger the same opening, paid for by the parry
timing instead of bash charges.

### Chosen flavour: parry = free guard-break, with a LONGER window
- On a successful perfect parry (`dShield_onShieldHit` returns true /
  `dParryMaster_onPerfectParry`), if the attacker is a DT-armed enemy that
  supports a guard-open, call that enemy's `albwBeginGuardOpenWindow`.
- Give the PARRY window a longer base than the bash window (bash 90/75 ->
  parry e.g. 120), so parrying is the *dueling answer* to an enraged enemy:
  bashing is short and competes for charges, parrying cracks them wider.
- Composes with the window-halving fix (step-2 work): once the double-tick is
  guarded, the parry window is full-length by design; until then the longer
  base partly compensates.

### Alternatives considered (kept on record)
- **Stagger:** drop the enemy into its native stagger (`ACT_YOROKE`). Simpler,
  blunter, less distinct from a bash.
- **Mini-flurry:** reuse the DT anim-boost infrastructure IN REVERSE to slow the
  parried enemy briefly (per-actor slow, not the world). Thematically ties to
  Flurry Rush; most new code, least certain feel.

### Seam + cost
- Trigger seam: the perfect-parry success path the shield system already owns
  (`dShield_onShieldHit` / `dParryMaster_onPerfectParry`), plus attacker
  identification (the enemy whose attack was parried).
- Per-enemy: the guard-open is per-actor. The Darknut has it; other DT enemies
  would each need their own opening seam - the same per-actor reality DT
  already lives with. The Darknut (test case) has everything needed.

### Dependency
Bundle with the step-2 once-per-frame fixes (i-frames, bash window, damage) -
this mechanic reads correctly only once the window-halving is fixed, and DT is
not shippable until those land regardless.
