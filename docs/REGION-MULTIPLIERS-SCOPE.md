# Region Multipliers — UI scope (fork parity)

> Status board / index: [CURRENT-STATE.md](CURRENT-STATE.md)
> Related plan: [SETTINGS-REORG-PLAN.md](SETTINGS-REORG-PLAN.md) (Difficulty tab)

**Scope only — no code.** Goal: document exactly how the fork presents Region
Multipliers, how our mod currently presents it, the delta, and what it would
take to match the fork. The user's ask: *"In the fork there was a master toggle
for health, rupees, and damage... one setting on the left pane that allows you to
select between a master at the top and an on/off for each of the other three
categories (all on the right pane). Our mod version is determinedly NOT that."*

---

## 0. Status — IMPLEMENTED (2026-09-22)

Built and installed; compiles clean. `albw_region_hp_build_panel(left, right,
error)` (`src/region_port.cpp`) now adds the HP steppers + standalone **Region
damage** toggle on the left, then a **Region Multipliers** group row
(`svc_ui->pane_add_group`) that opens `region_mult_build_subpanel` in the right
detail pane with **Master / Health / Rupees** — all always shown (§7 Q2). The tab
threads its `right` pane through `build_difficulty_tab`. Micro-decisions taken to
ship: **§7 Q1 → static label "Region Multipliers"** (SDK group label has no
dynamic getter; the live "On (HP+Rup)" summary was not worth an update-hook for
v1 — revisit if wanted). **§7 Q3 → HP category multipliers left flat** (not folded
into their own group this pass). Config layer untouched.

## 1. TL;DR

- **The config vars already match the fork 1:1** — nothing to add or rename.
  The difference is **purely how the controls are laid out**, not what they do.
- The fork uses **one left-pane "Region Multipliers" row** that opens a
  **right-pane subpanel** with `Master / Health / Rupees` on-off rows. **Region
  Damage is a SEPARATE standalone toggle** — it is NOT one of the master's axes.
  (The user's memory of "health, rupees, and damage under one master" is close
  but off by one: the master gates **Health + Rupees**; **Damage** stands alone.)
- Ours renders **four flat sibling toggles** in the Difficulty panel: Region
  damage, Region multipliers (master), Region HP, Region rupees — the HP/rupee
  axes grey out when the master is off.
- **Our SDK can reproduce the fork layout.** `svc_ui->pane_add_group(left, right,
  …)` is the native equivalent of the fork's `leftPane.add_select_button(…,
  rightPane, builder)`. Our tabs already hand us both panes.
- **One fidelity gap:** the SDK group-button label is **static**, so the left row
  cannot natively show the fork's live summary value (`Off` / `On (HP+Rup)`). A
  workaround exists (see §5) but it is not free.

---

## 2. What the FORK does (exact)

Source: `dusklight/src/dusk/ui/settings.cpp` (Difficulty tab, ~L1490–1617).

The fork's Difficulty tab is a **two-pane** layout (left = interactive column,
right = contextual detail). Region control is split into two independent things:

**(a) Region Damage** — a plain standalone left-pane option (`addSpeedrunDisabledOption`,
L1532). Not gated by anything. Help text explicitly: *"Independent of Region
Multipliers (enemy HP / rupees)."*

**(b) Region Multipliers** — a single left-pane **select-button** (L1537–1617):
- Its **displayed value is dynamic**, computed from the three bools:
  - master off → `"Off"`
  - master on, no axes → `"On (no axes)"`
  - master on → `"On (HP)"`, `"On (Rup)"`, or `"On (HP+Rup)"`.
- Selecting it **populates the right pane** with a section titled "Region
  Multipliers", a description, and **three On/Off rows**:
  - `Master` → `game.regionMult`
  - `Health (enemy HP)` → `game.regionMultHealth`
  - `Rupees (enemy-death payouts)` → `game.regionMultRupees`
- Each row is a pair of `Off` / `On` buttons (with `isSelected`) + a menu SFX +
  `config::Save()`. Speedrun mode disables all of them.
- Trailing RML note: Health stacks on category Health Multiplier; Rupees scale
  enemy-death grants only (shops unchanged).

Note: the fork subpanel does **not** functionally grey Health/Rupees when Master
is off — it just shows all three; the master gates them at consumption time. (The
`isModified` dot on the left row lights if any of the three differs from default.)

Config vars (fork `Settings::game`): `regionDamage`, `regionMult`,
`regionMultHealth`, `regionMultRupees` — all `ConfigVar<bool>`.

For reference, the fork uses the **same select-button→right-subpanel idiom** for
the true HP category multipliers ("Health Multiplier": Common/Mid-Boss/Boss/Final
number steppers in the right pane) — i.e. this is the fork's standard pattern for
a cluster of related knobs, not a one-off.

---

## 3. What OURS does (exact)

Source: `src/region_port.cpp` `albw_region_hp_build_panel` (L256–297), invoked
from the Difficulty tab in `src/albw_settings_ui.cpp` (L167) with the **left**
pane only (`albw_region_hp_build_panel(left, …)`; the right pane is in scope at
the call site but not passed down).

All controls are added **flat** into the one panel, in this order:
1. `Common HP` / `Mid-boss HP` / `Boss HP` / `Final HP` — number steppers
   (`g_hp_normal` / `g_hp_midboss` / `g_hp_boss` / `g_hp_final`).
2. `Link damage decrease` — number (`g_link_damage_decrease`).
3. `Incoming damage` — select (`g_incoming_damage_scale`).
4. `Region damage` — toggle (`g_region_damage`), standalone.
5. `Region multipliers (master)` — toggle (`g_region_mult`).
6. `Region HP` — toggle (`g_region_hp`), `is_disabled = region_mult_master_off`.
7. `Region rupees` — toggle (`g_region_mult_rupees`), same gate.

So items 5–7 already encode the master + two axes; they are just **flat siblings
that grey out** (via `region_mult_master_off`, L252) instead of living behind one
left-pane row that opens a subpanel. Items 1–3 (the true HP category multipliers +
Link/Incoming damage) are mixed into the **same** panel, whereas the fork keeps
those in their own separate select-button group.

---

## 4. Config-var mapping (fork ↔ ours) — already 1:1

| Fork (`Settings::game`) | Ours | Same semantics? |
|---|---|---|
| `regionDamage` | `g_region_damage` | yes — standalone incoming-COVER-damage scaler |
| `regionMult` (master) | `g_region_mult` | yes — master for the HP + rupee axes |
| `regionMultHealth` | `g_region_hp` | yes — spawn-HP × province table (name differs only) |
| `regionMultRupees` | `g_region_mult_rupees` | yes — enemy-death rupee scaler |

**No config registration, rename, or behavior change is required to reach fork
parity.** This is a UI-presentation task only.

---

## 5. Can our SDK reproduce the fork layout? — Yes, with one caveat

**Two-pane group button: fully supported.**
- `svc_ui->pane_add_group(ctx, group_pane, target_pane, const UiGroupDesc*, out)`
  (`sdk/include/mods/svc/ui.h` L373) — *"Add a group button to one pane that
  builds controls in its paired pane. The panes must be the left and right handles
  from the same UiTabBuildFn call."* This is precisely the fork's
  `leftPane.register_control(add_select_button(…), rightPane, builder)`.
- Our tabs already receive both panes: `build_*_tab(…, UiElementHandle left,
  UiElementHandle right, …)` (e.g. `build_combat_tab`, `albw_settings_ui.cpp`
  L44). The Difficulty tab currently discards `right`; we would just thread it
  into `albw_region_hp_build_panel`.
- The subpanel's three On/Off rows are expressible today. Either:
  - **`UI_CONTROL_TOGGLE`** per axis (simplest — one row each, matches meaning), or
  - the **`albw_ui_add_int_choice_group`** button-row idiom (`albw_common.cpp`
    L105) if we want literal Off/On button *pairs* like the fork. Toggles are the
    idiomatic mod-SDK choice and read the same.

**The one caveat — dynamic summary label.**
- The fork's left row shows a **live value string** (`Off` / `On (HP+Rup)`).
- `UiGroupDesc` (`ui.h` L254) is `{ label, build, user_data }` — the group
  button label is a **static `const char*`**; there is no `getValue`. So a plain
  group button reads `"Region Multipliers ▸"` with no live axes summary.
- Workarounds, in order of preference:
  1. **Accept a static label** (`"Region Multipliers"`). Simplest; the axes are
     one click away in the subpanel. Lowest risk, ~full parity on structure.
  2. **Update the label dynamically** via `control_set_label` (`ui.h` L440) from
     the tab/panel `update` callback (`UiPanelUpdateFn`) — recompute
     `Off/On (…)` each frame the window is open. Achieves exact fork fidelity but
     adds an update hook and per-frame string work; confirm the panel actually
     gets an `update` tick in our window setup before committing to this.
  3. **Keep it a `UI_CONTROL_SELECT`** enum on the left (Off / On (HP) / On (Rup)
     / On (HP+Rup)) backed by a derived value — rejected: it collapses three
     independent bools into one enum, diverging from the fork's data model and
     breaking independent per-axis defaults / `isModified`.

**Greying vs. fork behavior.** The fork does *not* grey Health/Rupees when Master
is off (it shows them always; the gate is functional). Ours currently greys them.
For parity we can either keep our grey-when-master-off (arguably clearer) or drop
it to match the fork exactly — cosmetic, user's call.

---

## 6. Proposed target (for the eventual build — not now)

Restructure `albw_region_hp_build_panel` so the Difficulty tab reads:

- **Region Damage** — standalone toggle on the left (unchanged, `g_region_damage`).
- **Region Multipliers** — a `pane_add_group` left row whose subpanel (right
  pane) contains a "Region Multipliers" section + description and three rows:
  - Master (`g_region_mult`)
  - Health / enemy HP (`g_region_hp`)
  - Rupees / enemy-death payouts (`g_region_mult_rupees`)
  - trailing note (stacks on category HP; shops unchanged).
- (Optional, out of this ask) mirror the fork by also moving the **HP category
  multipliers** — Common/Mid-boss/Boss/Final + Link damage + Incoming damage —
  into their own group/subpanel instead of the flat top of the panel.

**Work required:** thread `right` into `albw_region_hp_build_panel` (signature +
call site, `albw_settings_ui.cpp` L167); rewrite the panel body to add one group
+ subpanel builder; decide the label strategy (§5) and the grey-vs-show behavior
(§5). Config layer untouched. No new config vars, no `register_all_config` edit.

---

## 7. Open questions for the user

1. **Label fidelity:** accept the static `"Region Multipliers"` row (§5 option 1),
   or invest in the live `On (HP+Rup)` summary via `control_set_label` (§5 option 2)?
2. ~~**Grey vs. show:** keep our grey-out of the axes when Master is off, or match
   the fork and always show them (functional gate only)?~~ **DECIDED (user
   2026-09-22): match the fork — always show the axes; the master gates them
   functionally, no grey-out. Drop `region_mult_master_off` from the toggle rows.**
3. **Scope of the restructure:** just Region Multipliers, or also fold the HP
   category multipliers into their own fork-style group in the same pass (§6
   optional)?

---

## Cross-references
- Fork UI: `dusklight/src/dusk/ui/settings.cpp` L1532–1617.
- Ours: `src/region_port.cpp` L249–297; call site `src/albw_settings_ui.cpp` L164–169.
- SDK: `sdk/include/mods/svc/ui.h` — `pane_add_group` L373, `UiGroupDesc` L254,
  `UiTabBuildFn` L266, `control_set_label` L440.
- [SETTINGS-REORG-PLAN.md](SETTINGS-REORG-PLAN.md) — Difficulty & Economy tab.
- [CURRENT-STATE.md](CURRENT-STATE.md) — status board.
