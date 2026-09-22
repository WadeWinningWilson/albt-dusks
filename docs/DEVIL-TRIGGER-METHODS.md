# Devil Trigger — the three speed-up methods

> Status board / index: [CURRENT-STATE.md](CURRENT-STATE.md)

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

### RECIPE — the working sub-step implementation, preserved

This shipped and ran (commit f9e0345). Kept verbatim so switching back costs
nothing. Hook is `DEFINE_HOOK(&daB_TN_c::execute, BtnExecute)`, registered
with `hook_add_post`.

```cpp
bool s_inSubStep = false;   // execute() reaches damage_check, which we also
                            // hook - without this the sub-step recurses.

void on_btn_execute_post(ModContext*, void* args, void*, void*) {
    if (!s_featureReady || s_inSubStep) return;
    auto* self = self_of(args);
    if (self == nullptr || self->mType != 0) return;

    dAlbwDevil_tickActor(self);
    if (!dAlbwDevil_isArmed(self)) return;
    if (dAlbwDevil_isAttackLive(self)) return;  // swing live: stock timing

    s_inSubStep = true;
    self->AlbwBtn_c::execute();
    s_inSubStep = false;
}
```

To revive it as the HYBRID, this is unchanged - the two generic
neutralisations go elsewhere: a typed hook on `cCcS::Set` that returns early
while `s_inSubStep`, and a `ClrTgHit` pass over the actor's colliders (found
via the `mpObjTg` registry scan) immediately before the `execute()` re-call.

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

### TESTED — and it felt INCONSISTENT. Why.

Built as commit d60e3ce, played, rejected by the user: *"direct scaling feels
very inconsistent."*

The likely mechanism is the conditional restore, which is the part that looked
clever and was not. POST only restores the rate if it still equals what PRE
wrote — the guard that stops us stamping an old rate over a fresh `setAnm`.
But its corollary is that **on every frame `action()` calls `setAnm`, our
scale is simply discarded**. The Darknut changes state constantly (chase ->
attack -> recover -> guard), and each change re-seeds the rate, so the
speed-up applies on some frames and not others. Intermittent by construction.

Worse, it is intermittent in a way the player cannot read: he is fast while
holding one animation and normal the instant he transitions, which is exactly
when you would notice.

Fixing it inside this method means scaling at the `play()` call instead of
around `action()` — hooking `mDoExt_McaMorfSO::play`, or registering the DT
actor's `J3DFrameCtrl*` into the rate hook the flurry already owns. Both are
viable and neither was tried; if the hybrid disappoints, that is where this
method goes next rather than being abandoned.

### RECIPE — direct scaling, preserved

Animation half, bracketing `action()`:

```cpp
f32  s_animNatural[2] = {1.0f, 1.0f};
f32  s_animWrote[2]   = {0.0f, 0.0f};
bool s_animScaled     = false;

// PRE
const f32 scale = dAlbwDevil_speedScale(self);
if (scale > 1.0f) {
    mDoExt_McaMorfSO* morf[2] = {self->mpModelMorf1, self->mpModelMorf2};
    for (int i = 0; i < 2; ++i) {
        if (morf[i] == nullptr) continue;
        s_animNatural[i] = morf[i]->getPlaySpeed();
        s_animWrote[i]   = s_animNatural[i] * scale;
        morf[i]->setPlaySpeed(s_animWrote[i]);
    }
    s_animScaled = true;
}

// POST - conditional restore; THIS is what made it intermittent
for (int i = 0; i < 2; ++i) {
    if (morf[i] != nullptr && morf[i]->getPlaySpeed() == s_animWrote[i]) {
        morf[i]->setPlaySpeed(s_animNatural[i]);
    }
}
```

Movement half: multiply `fopAcM_GetSpeed_p(actor)` by
`world * dAlbwDevil_speedScale(actor)` in the `fopAcM_posMove` PRE hook and
divide by the SAME captured value in POST (`s_posMoveScale`). That half is
sound and is **kept** — the hybrid still uses it, so movement scaling and the
two-feature composition survive this switch.

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

## 3c. Hybrid, TESTED — crashes, and skips frames. Parked.

Built (b69534a / c879b54), played, rejected. Two findings, both recorded
because the future may want the mechanism even though now does not:

1. **The generic AT-registry signal is unreliable.** During `ACT_ATTACKL` the
   Darknut is mid-lunge (`truth=1`) yet the registry reported no attack live
   (`atRegistry=0`) - a flood of `signal MISMATCH act=10` right before a
   crash. Sub-stepping fired during the attack and re-running `execute()`
   mid-swing crashed. So the hybrid's headline claim - generic, no per-enemy
   knowledge - is FALSE for any enemy whose collider timing the registry
   misreads. It still needs a per-actor state list.
2. **Gated to safe locomotion states, the sub-step TELEPORTS.** Running a
   whole extra `execute()` in chase skips the in-between animation frames, so
   he snaps toward the player rather than running faster. A second full frame
   of logic is not the same as a frame at 2x - the interpolation is lost.

FUTURE USE: sub-stepping is right where you WANT discrete extra actions - an
enemy that genuinely acts twice (double attack, extra dash) rather than moves
smoothly faster. Kept in section 1's recipe for that. It is the wrong tool for
"smoothly faster", which is what Devil Trigger wants.

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
