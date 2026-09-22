# Devil Trigger — the three speed-up methods

Companion to [DEVIL-TRIGGER-SCOPE.md](DEVIL-TRIGGER-SCOPE.md), which covers the
trigger, tiers and knockback immunity. This document is only about **how an
enemy is made faster**, and what each choice costs.

Bosses are out of scope in all three — they need their own planning.

---

## 0. Switching between them is a three-line change

Worth stating first, because it determines how much any of this is a
commitment. Everything in the Darknut implementation is method-INDEPENDENT:

| Piece | `btn_parry.cpp` | Method-specific? |
|---|---|---|
| HP override (`btnHealthFraction`) | `:737` | no |
| Arming / latch (`dAlbwDevil_tickActor`) | `:775` | no |
| Knockback immunity | `:624` | no |
| Probe / heartbeat | `:791-799` | no |
| Override registration | `:882` | no |
| **The speed-up itself** | **`:830-832`** | **yes — this is the whole choice** |

```cpp
s_inSubStep = true;
self->AlbwBtn_c::execute();   // <- the entire "sub-stepping" decision
s_inSubStep = false;
```

Method 2 replaces those three lines. Method 3 keeps them and adds two hooks
elsewhere. Nothing else moves.

---

## 1. Sub-stepping (what is built today)

**Mechanism.** Call the actor's `execute()` a second time per frame while no
attack collider is live.

**Generality: excellent.** `execute()` is reachable generically, so this works
on an unmapped enemy with no per-actor code at all.

**Correctness: poor, and the Darknut proved it on the first test.**
`execute()` bundles "acting" with once-per-frame work:

| Doubled | Consequence |
|---|---|
| `damage_check()` (via `action()`) | pass 2 sees the guard window pass 1 just opened, so the eleven `field_0xaa2 == 0 &&` gates flip and a hit that clanged is re-evaluated as a hit that lands — **the "damage outside the usual window" the user observed** |
| `mInvincibilityTimer` | i-frames 20 -> 10; enemy re-hittable twice as fast |
| our guard-open window tick | the bash reward drops 90 -> 45 (P1) and 75 -> 37 (P2) |
| `cc_set()` | colliders registered twice for **zero** gain (stored by pointer, both resolve to the same final position) and double consumption of fixed arrays |
| `mVibrationTimer` | rumble cut short |
| sound / particle calls in `action()` | suspected doubled swing audio — **unmeasured** |

**Why the exceptions do not generalise.** They are not shared concepts:
`mInvincibilityTimer` is a Darknut member (`d_a_b_tn.h:166`; no i-frame concept
exists on `fopEn_enemy_c` at all), `damage_check` is **34** separate methods,
`cc_set` is **38**, `execute` is **245**. Excepting them per actor means
reading each `execute()` body and proving nothing in it is once-per-frame.

### The Darknut fight under this method
Relentless and correct-feeling: he charges (`speedF` 4.3 in `executeChaseH`) at
double rate, recovers instantly, re-engages before your swing finishes. But you
get free damage you should not have, and your bash reward is quietly halved.
The right feel, reached through four bugs.

---

## 2. Direct scaling

**Mechanism.** Two halves, which must land together or the enemy foot-slides:

- **movement** — scale the speed vector in the `fopAcM_posMove` hook we already
  own. **Already generic: 106 enemy actors flow through it.**
- **animation** — `mDoExt_McaMorfSO::setPlaySpeed`, inline. Needs the actor's
  morf pointer, which is a compile-time per-actor line:
  `case fpcNm_B_TN_e: return static_cast<daB_TN_c*>(a)->mpModelMorf2;`

**Generality: the weak point.** 139 enemy headers own a `McaMorfSO`. Names
cluster (`mpMorf` 50, `mpModelMorf` 42, `mpMorfSO` 12 — about 80%) but the
offset differs per class, so it is **one table line per enemy you enable**, and
nothing works until that line exists. There is a `J3DModel* model` on the actor
base (`f_op_actor.h:307`) but that is the model, not the animator — no generic
route.

**Correctness: nothing runs twice.** Damage evaluated once, i-frames intact,
bash window intact, colliders once, audio once. All four of the sub-stepping
bugs are absent by construction rather than by exception.

### The Darknut fight under this method
**Better than first assumed, because of a finding specific to this enemy.** The
`mTimer3` attack cooldown lives inside `if (mType == 1)` in
`checkNormalAttackAble` — the **zako** branch. For the boss Darknut
(`mType == 0`) that function simply `return 1`s, so his cadence is **not
timer-gated**. It is gated by animation (swing -> recovery -> chase, via
`mpModelMorf2->isStop()`) and geometry (`checkAttackAble` = distance < 500 and
facing within ~0x3000).

So scaling animation speeds his attack cadence too, and scaling movement gets
him back into that 500-unit cone almost immediately. Phase 1 becomes an
armoured wall that never lets you reset spacing; phase 2 becomes frantic —
with a full-length bash reward and honest hit trading.

---

## 3. Hybrid — sub-step with generic neutralisations

**Mechanism.** Method 1, plus two fixes that are generic rather than per-actor:

1. **Suppress `cCcS::Set` while sub-stepping.** `void cCcS::Set(cCcD_Obj*)` is
   header-declared and typed-hookable — **one hook removes double collider
   registration for every actor**, and with it the array-overflow risk.
2. **Clear the actor's Tg hit flags before the extra pass.** `cCcD_Obj` exposes
   `ClrAtHit` / `ClrTgHit` / `ClrCoHit` (`c_cc_d.h:379-381`), and the actor's
   colliders are findable through the same registry scan `dAlbwDevil_isAttackLive`
   already uses. Every `damage_check` — all 34 variants — then sees "no hits".
   **One generic scan fixes the damage bug for every actor.**

**Irreducible residue:** bespoke timers still tick twice, because no shared
i-frame concept exists. That means attacks come more often (intended) and
i-frames shorten (a small player *advantage*, not an unfair enemy). Fixable per
actor in one line where it matters.

**Unmeasured risk:** a second `action()` can still fire sound and particle calls
twice. Not corrupting, but it would sound wrong. Cheap to measure by counting
`startCreatureSound` calls per frame on an enraged actor. If it does double,
suppressing the audio manager during the sub-step is plausibly another one-hook
generic fix.

**Economics.** This is the inversion that matters: the hybrid **works out of the
box on an unmapped enemy**, and per-actor work becomes *optional polish written
when playtesting shows a problem*. Direct scaling requires a table line
**before any enemy works at all**.

### The Darknut fight under this method
As sub-stepping felt, minus the bugs — and slightly more aggressive than direct
scaling, because the state machine itself advances twice, so he re-evaluates
range and re-commits faster. Needs the one-line i-frame fix (we already know the
member) and the audio question answered.

---

## 4. Comparison

| | Sub-step | Direct scaling | Hybrid |
|---|---|---|---|
| Works on an unmapped enemy | yes | **no** | yes |
| Per-actor cost | audit `execute()` | 1 table line, **required** | 1 line, **optional** |
| Damage correctness | **broken** | correct | correct |
| i-frames | halved | intact | halved (1-line fix) |
| Bash reward | halved | intact | intact |
| Collider arrays | 2x pressure | intact | intact |
| Audio | suspect | intact | **unmeasured** |
| Attack cadence | faster (timers) | faster (animation) | faster (both) |

## 5. Recommendation

**The Darknut cannot decide this.** Its cadence is animation-gated, so direct
scaling and the hybrid land in nearly the same place on it — and it is the one
enemy we have already fully mapped, which is exactly the condition under which
direct scaling's weakness does not show.

So split the decision:

- **Use the Darknut to decide the FEEL** — how fast is fun, whether halved
  i-frames read as a problem, whether the bash reward should shorten while
  enraged.
- **Decide the FRAMEWORK on the second enemy**, an unmapped common one, where
  the difference *is* the per-actor table line.

Switching costs three lines (§0), so trying more than one is cheap.
