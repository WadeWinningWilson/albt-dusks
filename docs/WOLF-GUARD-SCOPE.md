# Wolf Guard / Parry — design scope (Midna's Shield)

> Status board / index: [CURRENT-STATE.md](CURRENT-STATE.md)

**Status: scoped, not started. No code.** Design-first, per the user's pattern.
This is the wolf-form counterpart to the human ALBW shield system — a **held
guard** and a **timed parry** that feel synonymous with the human shield/parry,
expressed through **Midna raising the equipped shield** (the Ordon-shield-hold
pose the user identified in-game).

The point of this doc: the whole thing grounds on **native seams** end to end.
The only genuinely authored piece is the held-guard damage reduction, and that
sits at a seam the wolf damage handler *already* uses. Everything else is a
port or a reuse.

---

## 1. The fantasy

Wolf Link, with Midna on his back, gains the shield he already carries:

- **Hold** the shield button → Midna raises the equipped shield and holds it in
  front (the image the user shared — Midna, shield to her face, on the wolf's
  back). While held and **facing** the attacker, incoming frontal hits are
  reduced/negated. Sustained, like the human held guard.
- **Time it** against an incoming hit → a **parry**: the hit is negated and the
  attacker is **opened up** (the same guard-open window a bash/Darknut parry
  produces), giving the wolf a clean bite.
- Shares the human shield's **bash-charge economy** — same resource, same HUD.

Feel target: *the ALBW shield system, in wolf form.* Not a new economy, not a
new HUD — the same one, driven by the wolf.

**Decision (user): inherit the human shield systems verbatim — zero new knobs.**
- **Shield = human Link's currently-equipped shield.** The wolf can't switch
  shields itself, so it parents whatever shield human Link has equipped and
  carries that shield's profile. Durability, break behavior, and the durability
  HUD bar all follow for free via `dAlbwHP_applyDurabilityMult(profName, damage)`
  (`albw_combat.cpp:52`) keyed on the equipped shield's `profName`.
- **Durability is per-shield, same as human.** No wolf-specific durability.
- **Chip-to-reclaim only under Parry Master.** `g_parry_master`
  (`albw_settings_ui.cpp:215`, *"Failed blocks chip HP into a reclaim pool"*): a
  wolf held-block that isn't a clean parry chips HP into the reclaim pool **iff
  Parry Master is on** — identical flag, identical rule.

## 2. It does NOT need the human guard proc

The load-bearing finding. The human shield is welded to Link's human proc
machine — `checkShieldGet()` (has a shield item), the `0x70C52` guard mode flag,
`procGuardAttackInit()`, the human guard skeleton/pose. That is why
`shield_hooks.cpp:271` and `albw_manual_shield_button` (`shield_adapt.cpp:51`)
gate off `checkWolf()` / require `checkShieldGet()`. Dragging that proc onto the
wolf skeleton is the Zora-`al_face`-class crash, and it is **not** what we do.

**The wolf damage handler already reroutes incoming damage on a wolf-owned
flag.** `daAlink_c::checkWolfBarrierHitReverse()` (fork
`d_a_alink_wolf.inc:1985`) is consulted inside the damage path
(`d_a_alink_damage.inc:570`, `:984`): when `mDamageTimer != 0`, if `field_0x3100`
(the wolf's charge state) is set, the hit is rerouted — into a bounce-reverse, a
`procFrontRollSuccessInit()`, or a large-damage — **instead of** the normal
`procDamageInit()`. That is exactly the shape a guard needs: *a wolf flag,
checked at the damage seam, that changes how a hit resolves.* Completely
independent of `0x70C52` and the human proc. The damage code already branches
`!checkModeFlg(0x70C52) && !checkWolf()` throughout — human-guard and wolf are
already separate lanes.

So a wolf guard is: **a new wolf-guard-active flag, consulted at the same seam
`checkWolfBarrierHitReverse` already lives at.** No human proc.

## 3. DN-10 ladder — piece by piece

| Shield-feel piece | Donor / source | DN-10 status |
|---|---|---|
| **Bash-charge economy** (charges/max/spend) | `dShield_getBashCharges` / `getMaxBashCharges` / `loseBashCharges` — pure state, no proc | **port verbatim**, form-agnostic |
| **Parry outcome** (enemy opens up) | `albwBeginGuardOpenWindow` / `albwTryApplyBashGuardBreakFromHit` — enemy-side, **already built** for Darknut/DT §7 | **reuse** |
| **Timed parry** (negate a hit on good timing) | **native** `procFrontRollSuccessInit` / the barrier-reverse *success* branch — the wolf already reverses a well-timed defensive input into a success proc | **step 1**, near-verbatim |
| **Facing check** (guard only forward) | geometry (`cLib_distanceAngleS(shape_angle.y, attacker)`), form-agnostic | free |
| **Visual — raise / hold / stow** | **native** field Midna animes `ANM_S_TAKES` / `ANM_S_WAITS` / `ANM_S_PACKAWAY` (see §4) | **step 1**, verbatim |
| **Shield geometry** | already on the wolf's back via WEAPONR joint remap (joint 15, `d_a_alink_wolf.inc:537`) | exists |
| **Held guard** (sustained damage reduction) | **no native wolf guard exists** — mirror the `checkWolfBarrierHitReverse` *shape* with a new wolf-guard flag at the damage seam | **step 2** (receiver translation at the consumption boundary) — see §6 |

**One authored piece only** (the held-guard mitigation). Everything else is a
port or a reuse. That is the DN-10 accounting.

## 4. The visual — resolved, and NOT demo-locked

The user's open worry was whether the shield-hold pose is a demo model. **It is
not.** The field companion `daMidna_c` (`d_a_midna.h:185`, enum `daMidna_ANM`)
carries a purpose-built cluster:

- `0x31 ANM_S_TAKES` — Midna **takes/raises the shield**
- `0x32 ANM_S_WAITS` — Midna **holds the shield up** ← the pose in the image
- `0x33 ANM_S_PACKAWAY` — Midna **stows the shield**

Driven through `daMidna_c::setUpperAnimeAndSe(daMidna_ANM)`. Two facts make this
a clean fit:

1. **They are UPPER-body animes.** Midna's lower body keeps its riding/cling
   pose while her arms work the shield — layered, no conflict with the wolf ride.
2. **We already drive this exact instance.** Our Midna-arm art
   (`daAlbwMidnaArm_c`) has *no model of its own* (`d_a_albw_midna_arm.cpp:281`)
   — it reaches into the real `daMidna_c` (publishes a reach point its hair
   chain chases). Calling `setUpperAnimeAndSe` on her is in-pattern with code we
   already ship.

Raise → hold → stow maps one-to-one onto guard-raise → guard-hold →
guard-release. `d_a_dmidna` ("Dying/White Midna", endgame) was a red herring —
not our pose.

`ANM_S_TAKES`/`S_WAITS`/`S_PACKAWAY` are Midna's arm skeleton — **shield-model-
agnostic**, so whatever shield the player has equipped reads fine, not just the
Ordon one.

## 5. Input — one gesture

**Decision (user): a single input. No bash, so no R+B, no dedicated button.**

- **Held guard = held-R** (`getHoldLockR`), un-gated from `checkShieldGet` for
  the wolf. In wolf form held-R is Z-target/lock-on — but that is *also* true in
  human form, where hold-R deliberately does target+guard together (TP-style).
  So "hold R to lock **and** raise Midna's shield" **mirrors the human system
  exactly**, which is the stated feel goal — a parallel, not a conflict.
- **Parry = a well-timed raise within the hold** — block active in the hit
  window, exactly like the human ALBW parry (a timed block, not a separate
  button). No new input.

Since the wolf has no bash (§6b), the free wolf-art directions (`d-pad Down`
etc.) stay free.

## 6. The one authored piece — held-guard mitigation (step 2)

No native wolf "brace and tank" exists; the only wolf damage-reroute is the
*offensive* charge (`field_0x3100`). So held-guard damage reduction is
instance-authored logic. It is DN-10 **step 2 (receiver translation at the
consumption boundary)** — sanctioned because step 1 is *proven* absent (searched:
`checkWolfBarrierHitReverse` is the sole wolf reroute and it is a charge move,
not a guard) — but it is authored code and wants the user's explicit nod, per
the ladder. **User has said: held-guard is IN.**

Shape (no code here, just the seam):
- A wolf-guard-active flag (set while held-R + facing, wolf, grounded).
- Consulted in the wolf branch of the damage handler alongside the existing
  `checkModeFlg(0x70C52)` / `checkWolf()` tests — when set and the hit is
  frontal, route to "blocked" (no `procDamageInit`, optional guard-slip
  feedback + a bash-charge tick) instead of a normal hit.
- Reuses the human facing/angle math and the `dShield_*` charge economy so the
  *numbers* are the human shield's, not invented.

Hook surface: the damage entry (`daAlink_c::execDamage` / `procDamageInit`) is a
header-declared member → typed `DEFINE_HOOK`, **portable** (no `check_hooks`
baseline growth, works on Linux/macOS). Confirm the exact frontal-hit conditional
to translate before writing.

## 6b. Bash — CUT. Parry is the universal opener.

**Decision (user, deliberated): the wolf does NOT bash. At all.** Earlier this
section proposed a DT-gated proactive bash; it is retired because the unified
parry rule makes it redundant.

The reasoning: **against DT enemies, a parry automatically makes the enemy react
as if bashed** — it fires the (elongated) guard-open window itself
([DEVIL-TRIGGER-SCOPE.md](DEVIL-TRIGGER-SCOPE.md) §7, now unified for human *and*
wolf). So every case is covered without a bash verb:

- **Normal enemy** — flinches/knocks back from bites already; a parry opens it.
  No bash needed.
- **DT enemy** (knockback-immune, enraged) — the parry auto-opens it as-if-bashed.
  No bash needed.

A bash's only unique value was *proactively* opening a knockback-immune target
without waiting to be hit. But DT enemies are *more* aggressive, so parry windows
are plentiful, and the reactive parry already delivers the opening. There is no
case left where a proactive wolf bash earns its input. **Cut.**

Consequence: the wolf shield is **block + parry only**, one input (§5), and the
whole feature is **DT-independent** — the parry-auto-opens-DT behavior lives in
the DT enemy's guard-open (DT §7), which the wolf parry simply triggers like any
other parry. Nothing here waits on DT step 2.

## 6c. Parry feeds the wolf charge counter (optional, user idea)

**Decision (user, tentative — keep, flagged optional): a successful wolf parry
adds to the wolf charge counter.** Cheap and skill-gated:

- The counter is the "15" steps system — `addWolfChargeSteps(n)`
  (`wolf_combat.cpp:933`), `kWolfChargeStepsPerCharge = 15`; a bite is 3/15, a
  mash 1/15. A parry success calls `addWolfChargeSteps(kWolfChargeParrySteps)`.
- **Parry only, NOT held-block.** A passive held-guard feeding charge would let
  turtling farm the meter; a *parry* feeding it rewards skill — the same
  parry-vs-block distinction the system already draws (Parry Master chips a
  *failed* block, not a parry; a skillful bite is 3, a cheap mash is 1).
- **Value: 1/15, mash-parity (user).** A parry nudges the meter but does not
  rival biting — defense contributes the smallest step, so a parry-heavy style
  is fed without ever out-earning aggression. `kWolfChargeParrySteps = 1`.
- **One-way conditional feed, no wire-crossing.** One `addWolfChargeSteps` call,
  gated on both the wolf-charge feature and the shield feature being on; no-op if
  either is off. Trivially cut if playtest says it muddies the economy.
- NOT the same as [CURRENT-STATE.md](CURRENT-STATE.md) §4a (guarded *bites*
  building charge) — that is a separate open question on the same counter; do not
  conflate.

## 7. Sequencing

1. **Parry-first. ✅ BUILT — compiles green, check_hooks 0 regressions, awaiting
   in-game test.** Files: `wolf_guard.{h,cpp}` (state predicate), `guardRaised`
   generalization + `dShield_updateWolfGuardTracking` in `shield.cpp`,
   `wolf_guard_hooks.cpp` (execute-post tracking + checkDamageAction parry seam),
   `dAlbwWolfCombat_onParry` (+1/15), config-backed unlock
   (`ALBW_FLAG_WOLF_GUARD_PURCHASED` + True ALBW). Reuses the shared parry engine
   entirely; the two hooks are typed member hooks (portable, no baseline growth).
   What it does today: in wolf form, hold R to raise the guard (no visual yet),
   and a well-timed block negates the hit, grants a parry charge, plays the parry
   feedback, and adds 1/15 wolf charge. **No toggle of its own** — gated by Wolf
   Link combat + the shield parry engine + the unlock; testable via **True ALBW**.
2. **Held-guard (step 2). ✅ BUILT.** In the `checkDamageAction` hook: a frontal
   (~180° arc, `cLib_targetAngleY` vs `shape_angle.y`), non-guard-break hit while
   the guard is up is BLOCKED — reusing the human failed-block chain
   (`dShield_onFailedGuardBlock` + `dParryMaster_onFailedBlock` chip-to-reclaim +
   `dShield_onBlockHit`/`destroyFromDurability`). Guard-break attacks (AtSpl
   9/10/11) pass through and break the guard, as for human Link.
3. **Visual. ✅ BUILT.** `dWolfGuard_tick` drives the companion `daMidna_c`
   (`daPy_py_c::getMidnaActor`) through `ANM_S_TAKES` (raise) → `ANM_S_WAITS`
   (hold) → `ANM_S_PACKAWAY` (stow) on the guard's rising/falling edges. Upper-body
   animes, ride pose preserved. Raise duration (`kRaiseFrames = 10`) is a tunable
   first guess.
4. **Parry → wolf charge (§6c). ✅ BUILT.** `dAlbwWolfCombat_onParry()` → +1/15.
5. **Shop row. ✅ BUILT.** "Midna's Shield", 100r, in `rental_shop.cpp`
   (`VISIBLE_WOLF_GUARD`): availability = first-twilight gate (as soon as the shop
   opens, like howl) via `dWolfGuard_shouldShowShopRow`; purchase persists in
   config (`dWolfGuard_tryPurchase`), never the save; free under True ALBW.

**The whole feature is DT-independent** (bash is cut, §6b). The parry-auto-opens
a DT enemy through the DT enemy's own guard-open (DT §7) — the wolf parry just
triggers it like any parry — so nothing here waits on DT step 2. Each stage is
togglable and independently verifiable; the one authored piece (step 2) is
quarantined from the native pieces.

## 8. Gating / invariants / unlock

- **No toggle of its own (user).** Wolf guard is part of **Wolf Link combat** —
  it turns on with that feature (+ the shield parry engine + the unlock), and is
  only *mentioned* in the wolf-combat setting's help text. `dWolfGuard_isEnabled`
  = `g_wolf_combat` && `albw_shield_parry_enabled` && `dWolfGuard_isUnlocked`.
- **Unlock = config-backed, NEVER a save write ("B", user-decided). ✅ BUILT.**
  Purchase persists in `ALBW_FLAG_WOLF_GUARD_PURCHASED` (config.json via the
  `albw_save_flags` allocator), not a save event bit. `dWolfGuard_isUnlocked` =
  that flag **OR** True ALBW (free there). Shop availability
  (`dWolfGuard_shouldShowShopRow`) is a pure READ — shown once wolf combat is on,
  not already unlocked, and Midna's arm is unlocked (the "after Midna's arm"
  sequencing) or True ALBW. **Still to wire:** the actual shop ROW
  (price/name/desc + a `tryPurchase` that calls `dWolfGuard_unlock`), mirroring
  `dAlbwWolfArts_*ArmShop*` but with config persistence. Until then it is
  unlockable via **True ALBW** (testable now).
- **SIBLING SAVE-WRITE BUG — ✅ FIXED (user OK'd the one-time re-unlock trade).**
  All three wolf-art purchases moved off the save file to the `albw_save_flags`
  config allocator: howl `saveBitLabels[713]` and arm `[714]` (both in the
  710-714 event-REGISTER range — corrupting) and charge `F_0814` now persist as
  `ALBW_FLAG_HOWL/ARM/CHARGE_PURCHASED`. `is*Unlocked` and `albw_wolf_get_max_charges`
  read the config flags; `unlock*` write them. Availability reads
  (`is_dark_clear_lv(0/1)`, `F_0264`) and True ALBW are unchanged. Existing owners
  re-unlock once (registers can't be migrated reliably).
- **Off == provably stock** (feature gated; no damage reroute, no anime driven,
  held-R stays plain lock-on when locked out).
- **No save writes** — runtime combat state only; never a save bit for runtime
  state (`albw_save_flags`/config precedent).
- **8 platforms via CI**; the damage-seam and Midna-anime hooks are typed member
  hooks, so no Linux/macOS-inert surface is added (unlike the wolf-charge
  per-actor ports in [CURRENT-STATE.md](CURRENT-STATE.md) §4b).

## 8a. In-game test findings (2026-09-22)

### Second test — CRASH root cause (FIXED)
The wolf deformed, voided out, and died while guarding. Root cause: the wolf
held-block called `dShield_onFailedGuardBlock`, whose last line is
`link->procGuardBreakInit()` (shield.cpp) — the **human** guard-break proc, driven
onto the **wolf** skeleton every blocked frame. **Fixed:** the wolf held-block is
now NEGATE-ONLY (clear collider + SKIP_ORIGINAL + retval 0); it calls none of the
human failed-block chain. Durability drain + Parry-Master chip for the wolf are
deferred until there is a wolf-safe variant that omits `procGuardBreakInit`.

**Log confirmed the parry WORKS:** `[wguard] at_spl=1 onShield=1 guardOpen=1` — the
parry registered and the DT window opened. The Darknut *sword* swings (`at_spl=10`)
showed `onShield=0` — they landed outside the parry window (a block, not a parry),
i.e. a window-width/timing tuning matter, not a wiring failure. (`[devil] signal
MISMATCH` warnings are pre-existing DT isAttackLive probe noise, unrelated.)

### First test — earlier findings (below) still apply
Three issues found; the two functional ones are FIXED, the visual one needs a
model attach.

1. **Parry didn't break the Darknut's guard — ✅ FIXED.** The generic parry-open
   only lifted *no-flinch*; it never opened the Darknut's actual *guard*
   (`field_0xaa2`), so hits still bounced. `btn_parry.cpp on_btn_action_pre` now,
   while `dAlbwDevil_isGuardOpen(self)`, re-arms `albwBeginGuardOpenWindow` so the
   native guard stays broken open across the DT window.
2. **Guarding still took damage from Darknuts — ✅ FIXED.** Darknut swings are
   guard-break class (AtSpl 9/10/11); the held-block was *skipping* those. It now
   blocks them too (the ALBW shield supersedes the natural guard-break, as for
   human Link), charging durability by class. It does NOT call
   `dShield_onFailedGuardBreakBlock` — that ends in the human `procGuardBreakInit`,
   wrong for the wolf; blocking + durability cost is the wolf-safe translation.
3. **Shield doesn't appear at Midna's face — NOT a free anime placement.**
   Confirmed against the donor: `daMidna_c::modelCallBack` handles only
   head/backbone/hair joints — **no shield/held-item joint**, no shield-model
   member, and `ANM_S_TAKES/S_WAITS/S_PACKAWAY` are *face* animes
   (`d_a_midna.cpp:2427-2431`). In the Ordon scene the shield was **separate prop
   geometry**, not attached to her skeleton. So "shield to her face" needs an
   explicit draw: position the equipped shield model at Midna's head/face joint
   each frame during the guard (instance-authored, cosmetic). Also, the current
   `setUpperAnimeAndSe(ANM_S_*)` drives face-anime IDs through the upper-body path,
   so the pose is approximate — the correct pose + the shield draw are one
   follow-up package. NEEDS user go (it is authored cosmetic work).

## 8b. Implementation architecture (parry slice) — investigated, ready to build

The parry engine is more reusable than expected. `dShield_onShieldHit`,
`dShield_updateGuardTracking`, and `isGuardInputHeld` all funnel their
"is the guard raised?" test through `daAlink_c::checkUpperGuardAnime()` — a
human-only anime. **Generalize that one predicate** and the existing parry engine
accepts the wolf with the human path byte-identical:

```cpp
// shield.cpp (new helper), replaces the 3 checkUpperGuardAnime() parry uses
bool guardRaised(const daAlink_c* i_link) {
    if (i_link->checkWolf()) return dWolfGuard_isActive(i_link);  // wolf flag
    return i_link->checkUpperGuardAnime();                        // human, unchanged
}
```

Five parts:

1. **`wolf_guard.{h,cpp}` (new).** Per-frame state: wolf-guard-active =
   `checkWolf()` && held-R (un-gated from `checkShieldGet`) && grounded &&
   feature-on && unlocked. Drives Midna `ANM_S_TAKES → S_WAITS → S_PACKAWAY`.
   Exposes `dWolfGuard_isActive(link)`. Ticked from the link-execute-post hook.
2. **`shield.cpp` — `guardRaised` generalization.** Swap the 3 parry-path
   `checkUpperGuardAnime()` uses (`isGuardInputHeld` :555, `updateGuardTracking`
   :1181, `onShieldHit` :1234) for `guardRaised`. Human = identical (`!checkWolf`
   short-circuits). Additive.
3. **`dShield_updateWolfGuardTracking(link)` (new, shield.cpp).** Minimal onset
   bookkeeping for the wolf (mirror :1179-1197 — `sSimFrame++`, `sGuardOnsetFrame`
   on rising edge of the wolf flag, `clampChargesToTier`). NOT the human
   durability/helm/HUD block. Form-exclusive with the human tracker, so the
   shared `sSimFrame`/`sGuardOnsetFrame` never double-advance. Needed because the
   human `updateGuardTracking` only runs from `setShieldGuard` (a human seam), so
   the parry window would never advance in wolf form otherwise.
4. **Hook `daAlink_c::checkDamageAction` (PRE, new).** This is the shared damage
   dispatch (fork `d_a_alink_damage.inc:465-986`); the human guard/parry
   sub-branch (:708-780) only runs when human Link guards, so the wolf falls
   through to `procDamageInit`. The PRE hook, wolf-only: reproduce the small hit
   collider-scan (the `var_r29` loop, :575-581 — `mTgCyls[i].ChkTgHit()`, and
   `mAtSph` for wolf), derive `at_spl`/attacker/hitPos, then
   `dShield_onShieldHit(link, at_spl, attacker)`. On success: 
   `dShield_playParrySuccessFeedback`, `addWolfChargeSteps(1)` (§6c), clear the
   consumed collider (`ClrTgHit`/`mCcStts.ClrTg`+`ClrAt` — the DT dangling-
   collider lesson), `retval = 0`, `HOOK_SKIP_ORIGINAL`. Else `HOOK_CONTINUE`
   (stock takes the hit — held-block reduction is step 2, not the parry slice).
   `checkDamageAction` is a header-declared member → typed `DEFINE_HOOK`,
   portable.
5. **Config/unlock.** `g_wolf_guard` bool; shop unlock after Midna's arm (§8).

Guard-open on the parried enemy is NOT in the parry slice for generic enemies
(`albwBeginGuardOpenWindow` is Darknut/DT-specific) — it fires where the enemy
supports it and is the DT §7 auto-open otherwise. Slice 1 = negate + charge +
feedback.

## 9. Open questions before code

1. **Shield re-parent.** The equipped shield rides the wolf's back (WEAPONR
   joint 15). During `ANM_S_TAKES`/`S_WAITS` it must appear in Midna's hands.
   Does the anime carry its own shield attach (authored with the shield in her
   grip), or do we re-parent the shield model from the wolf-back joint to a
   Midna hand joint for the guard duration? — model-callback question, verify
   against how the original Ordon event attached it.
2. **Midna service mode.** `S_WAITS` was authored in a specific event context.
   Confirm it's drivable while Midna is in her ordinary wolf-ride/cling state
   (she may need a state nudge; check `checkMidnaPosState`).
3. **Input disambiguation** (§5) — R+B vs a dedicated d-pad Down.
4. **Parry timing window** — where exactly to sample the incoming hit
   (`mDamageTimer` seam, mirroring `checkWolfBarrierHitReverse`'s `mDamageTimer
   != 0` gate) and how wide the window is vs the human parry.

---

## Cross-references

- [CURRENT-STATE.md](CURRENT-STATE.md) — status board (this is §4d of the port
  backlog).
- [DEVIL-TRIGGER-SCOPE.md](DEVIL-TRIGGER-SCOPE.md) §7 — the perfect-parry
  guard-open mechanism reused here for the parry outcome.
- [FLURRY-RUSH-PLAN.md](FLURRY-RUSH-PLAN.md) — the bash-charge economy
  (`dShield_*`) and the human parry seam this mirrors.
