# Uncalled-port sweep — bodies we ported and never call

**Why this exists.** In a single day, five separate live defects turned out to
be the same shape: a function ported faithfully from the fork, compiling,
declared in a header — and never called. Four of the five were player-visible
bugs.

| found | function | symptom |
|---|---|---|
| morning | `dShield_onGuardAttackConnect` | NO enemy reacted to a shield bash |
| morning | `dShield_playParrySuccessFeedback` | a successful parry landed silently |
| afternoon | `dShield_chargeHelmSplitterMeterOnce` | (layer 2 of the helm fix) |
| afternoon | `dShield_tryBeginHelmSplitter` | (layer 2 of the helm fix) |
| afternoon | `pushTearRenderFlags` | **every death revoked the Vessel of Light** |

Finding these one bug report at a time is not a strategy. This is the sweep.

## Method

1. Every non-static function DEFINED in `src/*.cpp` / `src/*.inc` whose name
   carries a project prefix (`dShield_`, `dFocusedArts_`, `dAlbw*`, `albw_`,
   `dFlurry*`, `dParry*`, `dMeter2_`). **840 scanned.**
2. Minus every one called anywhere in `src/*.cpp|inc`, excluding its own
   definition line by file+line (NOT by pattern — a first attempt excluded any
   line ending in `{`, which silently swallowed real calls like
   `if (dShield_isBashBarFull()) {` and produced 200 false positives).
   **129 uncalled.**
3. Of those, the ones whose names are SIDE-EFFECTING (`on*`, `try*`, `apply*`,
   `set*`, `fill*`, `sync*`, `notify*`, `charge*`, `store*`, `clear*`,
   `request*`, `mark*`, `begin*`) rather than queries (`is*`, `has*`, `get*`,
   `should*`, `can*`, `check*`). An unused query is harmless; an unused
   side-effecting function is a behaviour we believe we shipped and did not.
   **54.**
4. Cross-checked against whether **the fork calls it**. That is the test that
   turns a name into a finding: the fork calling it and us not is a gap by
   definition. **40.**

## The 40 — fork calls them, we never do

Grouped by whether this is a gap in something we SHIP or scaffolding for
something not yet wired. **This column is a judgement call and needs
per-entry confirmation before anyone acts on it.**

### Likely LIVE gaps — shipped features
| function | fork call site |
|---|---|
| `dShield_onSmallGuardHit` | `d_a_alink_damage.inc:769` |
| `dShield_chargeHelmSplitterMeterOnce` | `d_a_alink_cut.inc:2592` |
| `dShield_tryBeginHelmSplitter` | `d_a_alink_cut.inc:2587` |
| `dFocusedArts_onEndingBlowContact` | `d_a_alink_cut.inc:2467` |
| `dFocusedArts_onBackSliceFinisherEnded` | `d_a_alink_cut.inc:1678` |
| `dFocusedArts_onPerfectDodgeSpend` | `d_albw_flurry_rush.cpp:599` |
| `dFocusedArts_fillBank` | `d_a_npc_kakashi.cpp:518` |
| `dAlbwWardrobe_storeOutfitOnDeath` | `d_meter2.cpp:951` |
| `dAlbwWolfCombat_fillCharges` | `d_a_npc_kakashi.cpp:519` |
| `dAlbwWolfStun_syncColliders` | `d_a_e_oc.cpp:2816` |
| `dMeter2_onArmorAttackHit` | `d_a_alink_cut.inc:359` |
| `dMeter2_onArmorEncounterHit` | `d_a_alink_damage.inc:816` |
| `dAlbwSumoTest_clearWorn` | `d_albw_rental.cpp:1334` (+3) |
| `dAlbwSumoTest_onVanillaClothesMenuLeave` | `d_menu_collect.cpp:1401` (+2) |

### Expected — feature not wired yet
Morpheel (13): `morpheelTick`, `morpheelEnsureInit`, `morpheelSetFightLive`,
`morpheelOnEyeHooked`, `morpheelOnEyeDepleted`, `morpheelRequestTentacleGrab`,
`morpheelConsumeTentacleGrab`, `morpheelNotifyTentacleGrabCaught`,
`morpheelTryRootTentacle`, `morpheelMarkBombsSpawned`, `morpheelSnapRingBomb`,
`morpheelDrawChuBubble`, plus `d_a_e_oct_bg` support — the boss actor itself is
unported.

Diababa actor-side (6): `diababaOnPoisonDamage`, `diababaOnSideHeadDamage`,
`diababaSetPendingHangAfterAppear`, `diababaSetRetaliationPoison`,
`diababaTakeChipLookMAlternate`, `diababaTakePendingHangAfterAppear`.

Lockout lane (6): `onBlockWhileBomblingActive`, `onBomblingDestroyed`,
`onCcObjSet`, `onDoubleClawSlashEnd`, `onDoubleHookshotFired`,
`syncConfuseAtBits` — tracked in `src/enemy_lockout.cpp`.

Debug/editor (2): `dAlbwWardrobe_fillDebugSnapshot`,
`dAlbwPotion_editorSetSoulboundEnabled` — fork calls these from its own ImGui
tools, which we do not have. Not gaps.

## What to do with this

Work the LIVE list top-down, one at a time, each verified the way the helm
guard was: find the fork's call site, confirm the position a hook can occupy is
the donor's position, and prove the gap in-game before and after. Do **not**
bulk-wire them — a call in the wrong place is worse than no call, and several
of these sit mid-function where no pre/post hook is faithful.

## Keeping it from recurring

The sweep is cheap to re-run. The honest options are to run it before each
release as an advisory (it needs human triage, so it cannot be a hard gate), or
to record the current 40 as a baseline and fail CI when the number grows — the
same shape as `check_hooks.py`'s file-local-static baseline. Neither is wired
up yet; deciding that is a separate call.
