# Devil Trigger — enemy enrage below 25% HP

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
