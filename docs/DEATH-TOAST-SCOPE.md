# SCOPE — Death-Choice Toast ("Continue Here / Warp to Ordon" + Shade Watcher alternate)

> Status board / index: [CURRENT-STATE.md](CURRENT-STATE.md)

**Target mod:** `dev.albt.albw` (`C:\Users\ryana\Documents\ALBT DUSKS STUFF`)
**Fork (donor):** `C:\Users\ryana\Documents\dusklight`
**Stock (baseline):** `C:\Users\ryana\Documents\dusklight-main`
**Status:** SCOPE ONLY — no code written, nothing built. Deliverable is this doc.

**One-line verdict:** FEASIBLE, not blocked. The toast UI is reachable from a mod
via the SDK `UiService` (`svc_ui->push_toast` / `svc_ui->dialog_push`), and the
death-choice flow can be driven through hooks the mod already owns. This is NOT a
Colossal-Wallet-tier block — the toast has a real SDK surface.

---

## 1. The fork's toast / notification mechanism

**What it is:** a host-internal RmlUi overlay system, driven by a single free
function.

- API: `void dusk::ui::push_toast(Toast toast) noexcept;`
  - decl: `src/dusk/ui/ui.hpp:106`
  - impl: `src/dusk/ui/ui.cpp:448-450` — pushes onto a file-static `sToasts` deque.
  - struct: `src/dusk/ui/ui.hpp:30-35`
    ```
    struct Toast { Rml::String type; Rml::String title; Rml::String content; clock::duration duration; };
    ```
  - rendered by the passive Overlay document (`src/dusk/ui/overlay.hpp:9`,
    `mCurrentToast` at `overlay.hpp:20`). RmlUi, host UI thread.
- It is NOT a native TP message box and NOT a bespoke actor. It is a host
  (dusk::) UI service.

**Is the fork's own symbol reachable from a mod? NO (directly).**
- `push_toast` / `dusk::ui::Toast` do NOT appear in
  `build/windows-msvc-relwithdebinfo/dusklight_exports.def` (grep: no matches).
  The fork's free function is host-internal with no exported symbol — a mod
  cannot link or `GetProcAddress` it.

**Is there an SDK service for it? YES.** `sdk/include/mods/svc/ui.h` (in
`dusklight-main/sdk`, the SDK the mod builds against) wraps the SAME host overlay
system as a versioned service:
- `UiService::push_toast(ModContext*, const UiToastDesc*)` — `svc/ui.h` (member
  `push_toast` in the `UiService` struct; `UiToastDesc` has `type`, `title_rml`,
  `body_rml`, `duration_ms`). This is the direct donor-equivalent of the fork's
  `push_toast` — same host toast, mod-facing.
- **`UiService::dialog_push(ModContext*, const UiDialogDesc*, UiDialogHandle*)`**
  — a modal dialog with N action buttons (`UiDialogAction` has `label` +
  `on_pressed` callback). This is a richer surface than the fork's bare toast and
  fits the choice UX better (see §4).
- The mod ALREADY imports and uses this service: `src/mod.cpp:66`
  `IMPORT_SERVICE(UiService, svc_ui);`, and `svc_ui->pane_add_*` /
  `window_push` throughout `src/albw_settings_ui.cpp` and `src/albw_common.cpp`.
  `push_toast`/`dialog_push` are on the same already-bound vtable — zero new
  service wiring required.

**Verdict for §1:** mechanism is a host UI service; it is reachable by the mod
through the SDK `UiService` the mod already holds.

---

## 2. The death-choice flow (fork)

All in `src/d/d_gameover.cpp`, inside the native `dGameover_c` game-over actor.
The fork implements the choice as a NEW state machine step:

- New proc slot `PROC_ALBW_WARP_CHOICE` added to both dispatch tables:
  `init_process[]` (`d_gameover.cpp:110-123`) and `move_process[]`
  (`d_gameover.cpp:126-139`), each guarded `#if TARGET_PC`.
- File-static choice state: `sALBWWarpChoice` (-1 waiting / 0 A / 1 B),
  `sALBWWarpInDungeon`, `sALBWWarpDelay` — `d_gameover.cpp:146-149`.
- Gate `albwWarpChoiceAllowed()` — `d_gameover.cpp:151-153`:
  `return !dMeter2_isWolfForm() || dComIfGs_isItemFirstBit(dItemNo_MASTER_SWORD_e);`
  (pre-Master-Sword Wolf Link skips the choice — vanilla continue — to avoid
  Ordon softlocks.)

**Entry** — `saveMove_proc()` `d_gameover.cpp:385-416`: once the Continue menu is
confirmed (`dMs_c->getSaveStatus() == 3`), for real death only
(`getGameOverType()==0`) + postman unlocked (`isAlbwPostmanUnlocked()`) + gate,
routes `mProc = PROC_ALBW_WARP_CHOICE`; otherwise clears `sALBWWarpChoice = -1`
and goes to `PROC_SAVE_CLOSE`.

**Prompt** — `warpChoice_init()` `d_gameover.cpp:539-570`: pushes the toast and
sets a 60-frame debounce (`sALBWWarpDelay = 60`).
```
dusk::ui::push_toast({ .title="Choose Warp Destination", .content=content, .duration=8s });
```

**Poll** — `warpChoice_proc()` `d_gameover.cpp:572-590`: after the debounce,
`dComIfG_getTrigA(PAD_1)` → choice 0, `dComIfG_getTrigB(PAD_1)` → choice 1; on a
choice calls `dALBWOocoo_onWarpChoice(sALBWWarpChoice)` and advances to
`PROC_SAVE_CLOSE`.

**Apply** — `saveClose_proc()` `d_gameover.cpp:420-515`, branches on the choice.

### Exact option list (ALL options, enumerated)

| Slot | Label (no shade set) | Label (shade set) | Action | Cite |
|------|----------------------|-------------------|--------|------|
| **A / choice 0 — default** | "Continue Here" | — | Vanilla continue: `setGameoverStatus(2)` → restart-room = the last door walked through. **No warp.** | `d_gameover.cpp:555`, apply fallthrough `:484-487` |
| **A / choice 0 — shade** | — | "Last Shade Watcher" | `setLife(getMaxLife())` + `setRestartRoom(shade pos/angle/room)` + `setRestartRoomParam(setParamData(room,0,0xC9,0))` + `setNextStage(shade stage, -1, room, -1, 0, 0x45,…)` + `armRespawnCamera()`. Skips `setGameoverStatus(2)` (stage change drives reload). | `d_gameover.cpp:561`, apply `:458-479` |
| **B / choice 1** | "Ordon Village" | "Ordon Village" | `setLife(getMaxLife())` + `dALBWOocoo_onWarpChoice(1)` + `setNextStage("F_SP103", 0, 1, -1)` (Outside Link's House, room 1, spawn 0). Skips `setGameoverStatus(2)`. | `d_gameover.cpp:555/561`, apply `:441-451` |

Notes:
- The A label was deliberately relabelled from "Dungeon Entrance" to "Continue
  Here": choice 0 never warps (`d_gameover.cpp:547-555` comment). Same label in
  and out of dungeons.
- `dALBWOocoo_onDeathWarpContext(deathStage, diedInDungeon)` is called earlier at
  death (`d_gameover.cpp:194`) to capture died-in-dungeon context for the Oocoo
  shop, independent of the toast.

### Three-way (stock → fork → mod)

- **Stock** `dusklight-main/src/d/d_gameover.cpp`: NO `warpChoice`, NO
  `push_toast`, NO "Ordon" (grep: no matches). Proc enum stops at
  `PROC_DELETE_WAIT` (8) — `dusklight-main/include/d/d_gameover.h:34-42`. The
  toasted choice is 100% a fork addition.
- **Fork** `dusklight/src/d/d_gameover.cpp`: adds slot 9
  `PROC_ALBW_WARP_CHOICE` + `warpChoice_init/_proc` + toast, as above.
- **Mod** `ALBT DUSKS STUFF`: has the *destinations* and *context* helpers
  already ported but the *choice UI has no caller yet*:
  - `src/oocoo.cpp:113 albw_oocoo_on_warp_choice(int)` — DEFINED, **no caller in
    the mod** (grep: only decl + def). This is the dead seam the port fills.
  - `src/oocoo.cpp:97 albw_oocoo_on_death_context(...)` — already CALLED from
    `src/parry_hooks.cpp:94` inside a `dGameover_c::_create` post-hook.
  - Ordon warp primitive already exercised: `src/oocoo.cpp:172`
    `setNextStage(sDeathDungeonStage,…)` (Oocoo shop path).

---

## 3. The Shade Watcher alternate

**Trigger:** `dShadeRefuge_hasRespawn()` — a **session flag**, NOT a location /
proximity check.
- Fork: `src/d/d_albw_shade_refuge.cpp:54-56` — `return isEnabled() && sHasRespawn;`
- `sHasRespawn` is set by `dShadeRefuge_setRespawn(stage,room,pos,angleY)` when
  Link **rests at a Shade Watcher** (fork `d_a_albw_shade_watcher.cpp` calls it;
  one active slot, overwrites previous). Gated by a master enable
  (`isEnabled()`), fork default OFF.

**How the options differ** (`d_gameover.cpp:556-563`): when `hasRespawn()` is
true, the A slot flips from "Continue Here" to "Last Shade Watcher" **everywhere**
(in and out of dungeons); B stays "Ordon Village". The apply branch at
`d_gameover.cpp:458-479` respawns Link at the saved watcher (point -1 +
`setRestartRoom`, fall-recovery getup mode `0x45`, key `0xC9`, camera snap). When
no watcher is set it falls back to the plain "Continue Here / Ordon Village"
toast.

**Does the mod already have the hub state this keys off? YES — fully ported.**
`src/shade_refuge.cpp` / `src/shade_refuge.h` are "Ported VERBATIM from the
fork's d_albw_shade_refuge" and expose the identical surface the choice needs:
- `dShadeRefuge_hasRespawn()` — `shade_refuge.cpp:69-71`
- `dShadeRefuge_getStage/getRoom/getPos/getAngleY` — `shade_refuge.cpp:73-99`
- `dShadeRefuge_armRespawnCamera()` — `shade_refuge.cpp:85-87`
- `dShadeRefuge_isEnabled()` reads the **mod-owned** config key `g_shade_refuge`
  (`shade_refuge.cpp:58-63`) instead of the host `settings.game.shadeRefuge`,
  because ConfigService is scoped to the mod — same meaning, mod storage.
- The whole existing refuge return-warp (`dShadeRefuge_executePendingWarp`,
  `shade_refuge.cpp:173-199`) already performs the exact
  `setLife`+`setRestartRoom`+`setRestartRoomParam`+`setNextStage(…,0x45,…)`+
  `armRespawnCamera` sequence the death-choice A-branch needs — it can be reused
  verbatim as the "Last Shade Watcher" apply.

**Caveat:** the *watcher that SETS* the respawn (`dShadeRefuge_setRespawn`
caller, the fork's `d_a_albw_shade_watcher` NPC) — confirm it is actually spawned
in the mod. `shade_refuge.h:30` references `dShadeRefuge_trySpawnOnDefeat()`
(defined in fork `d_s_room.cpp`) with no mod-side definition visible in
`src/shade_refuge.cpp`. If no watcher ever calls `setRespawn` in the mod,
`hasRespawn()` is always false and the alternate never fires (the plain
Continue/Ordon toast still works). **Data question to resolve before building:**
is a Shade Watcher NPC that calls `dShadeRefuge_setRespawn` live in this .dusk?
(project_stargazer_mod memory + `src/d/actor/d_a_albw_shade_watcher.cpp` in the
fork are the reference.)

---

## 4. Portability verdict (a / b / c)

**Toast/dialog UI: (a) — an SDK service a mod can call.**
- `svc_ui->push_toast` (fork-faithful toast) and `svc_ui->dialog_push` (modal
  with action buttons) are both on the `UiService` vtable the mod already binds
  (`mod.cpp:66`). No upstream export needed. The fork's own
  `dusk::ui::push_toast` is host-internal/unexported, but the SDK routes the same
  host overlay — DN-10 donor-first is satisfied by calling the same host toast
  service through its SDK surface, not by reconstructing a toast.

**Death-choice control flow: (b) — a native mechanism the mod drives via hooks
(with one unavoidable receiver-boundary translation).**
- The fork implements the choice as a NEW proc slot (`PROC_ALBW_WARP_CHOICE`) in
  the native `dGameover_c` file-static dispatch tables (`init_process[]`,
  `move_process[]`, `d_gameover.cpp:110-139`). **A mod cannot add a proc-table
  slot to a native file-static actor in the host binary** (the "File-static port
  wall" / DN state-machine class). The mod also builds against STOCK headers
  whose enum has no slot 9 (`dusklight-main/include/d/d_gameover.h:34-42`), so
  `PROC_ALBW_WARP_CHOICE` and `warpChoice_init/_proc` literally do not exist mod-
  side and must not be referenced.
- What the mod CAN hook (all typed, present in the stock header, so portable
  across the fork/stock header split):
  - `dGameover_c::_create` — ALREADY hooked (`parry_hooks.cpp:31`).
  - `dGameover_c::saveMove_proc` (`dusklight-main/include/d/d_gameover.h:62`).
  - `dGameover_c::saveClose_proc` (`…:64`).
  - `dGameover_c::_execute` (`…:48`) if a per-frame hold is needed.
- **Because the intermediate PROC state is unavailable, the choice must be
  resolved between `saveMove_proc` (Continue confirmed) and `saveClose_proc`
  (branch applied). Two mod-side patterns:**
  - **Pattern A (recommended): `svc_ui->dialog_push` modal + hold.** On the
    first frame after Continue is confirmed, push a modal dialog with actions
    "Continue Here"/"Last Shade Watcher" (A) and "Ordon Village" (B); its
    `on_pressed` callbacks set a mod-owned `sChoice` and call
    `albw_oocoo_on_warp_choice`. Hold the actor (post/pre hook on `_execute` or
    `saveMove_proc` returning `HOOK_SKIP_ORIGINAL`, or pin `mProc` at SAVE_MOVE)
    until `sChoice >= 0`, then let it advance; hook `saveClose_proc` to apply the
    branch using the already-ported refuge / Ordon primitives. The modal blocks
    input natively — no raw A/B poll or debounce needed. This is the cleanest fit
    for the hook model.
  - **Pattern B (fork-literal): `svc_ui->push_toast` + raw A/B poll + hold.**
    Reproduce `warpChoice_init/_proc` exactly (toast text, 60-frame debounce,
    `getTrigA/getTrigB`) inside a mod-owned mini-state driven from an `_execute`
    hook, holding the actor until a button is read. Higher fidelity to the fork's
    exact UX, but re-implements the poll/debounce and the actor-hold by hand.
- **DN-10 tension to put to the user:** the fork's *literal* mechanism is
  toast + native-proc input poll. The mod cannot host the native proc slot, so
  SOME receiver translation is mandatory (proven impossible to port the slot —
  file-static host actor). Pattern A swaps the toast+poll for a dialog (same host
  UI service family); Pattern B keeps the toast+poll and only translates the
  state-holding. **User must rule which counts as donor-faithful.** Neither is a
  reconstruction of the *toast itself* — both use the host UI service.

**Not (c).** Contrast Colossal-Wallet, which needed a host feature with zero SDK
surface. Here the surface exists (`UiService`), so the feature is not blocked.

---

## 5. Save-write check (standing rule: NO save writes for runtime/gameplay state)

The fork's choice flow writes **no save-card data** — all state is volatile RAM /
game-state:
- `sALBWWarpChoice`, `sALBWWarpInDungeon`, `sALBWWarpDelay` — file-static RAM
  (`d_gameover.cpp:147-149`).
- Shade slot is explicitly volatile: "Volatile session RAM — not written to the
  save card." (`dusklight/include/d/d_albw_shade_refuge.h:13`).
- Apply primitives are game-state (RAM), not save commits:
  `dComIfGs_setLife` (current HP), `dComIfGs_setRestartRoom` /
  `setRestartRoomParam` (restart data, same void-restart convention the engine
  already uses), `dComIfGp_setNextStage` (queued transition). None flush the save
  file.
- `dComIfGs_addDeathCount()` (`d_gameover.cpp:188`) DOES bump the death counter,
  but it is on the **death path itself** (stock + fork), NOT part of the warp
  choice — the port adds no new save write there.

**Rule for the port:** the mod must persist NOTHING to the save card. The mod
already follows this — `soul_of_light.cpp` carries a long block on how a stray
`offLightDropGetFlag` write corrupted the save. The Ordon/shade branch must use
only `setLife`/`setRestartRoom`/`setNextStage`, matching
`shade_refuge.cpp:186-198` which is already save-clean. **Flag for review:**
confirm the mod's `albw_game::set_restart_room*` wrappers write game-state only
(they mirror the fork), not a save commit.

---

## 6. Hook list (typed `DEFINE_HOOK` vs bare `DEFINE_HOOK_SYMBOL`)

All hooks needed are **typed** member-function `DEFINE_HOOK` (mangled per target,
Linux-active, do NOT add to the `check_hooks` bare-symbol baseline of 30). No new
`DEFINE_HOOK_SYMBOL` required.

| Hook | Kind | Status | Purpose | Cite |
|------|------|--------|---------|------|
| `&dGameover_c::_create` | typed | ALREADY installed | capture death context (already calls `albw_oocoo_on_death_context`) | `parry_hooks.cpp:31,94` |
| `&dGameover_c::saveMove_proc` | typed | NEW | detect Continue-confirmed; kick the choice UI / begin hold | stock hdr `d_gameover.h:62` |
| `&dGameover_c::saveClose_proc` | typed | NEW | apply the branch (Continue / Last Shade Watcher / Ordon) | stock hdr `d_gameover.h:64` |
| `&dGameover_c::_execute` | typed | NEW (Pattern A hold or Pattern B poll) | hold the actor until the choice resolves | stock hdr `d_gameover.h:48` |

Per-target notes:
- All four exist in the STOCK header the mod builds against — portable across all
  8 platforms and across the fork/stock header divergence. `warpChoice_*` are
  fork-only and MUST NOT be referenced.
- Empty-function caveat: `saveMove_proc`/`saveClose_proc` have substantial bodies
  in stock — hookable (funchook rejects empty stock bodies). Confirm at build.
- No host export is required for the toast/dialog — `UiService` is a bound
  vtable service, not a linked symbol.

---

## 7. Risk / sequencing

**Risks**
1. **Actor-hold is the crux.** Holding a native `dGameover_c` mid-state-machine
   from a mod hook (Pattern A/B) is the delicate part — the fork got a real PROC
   slot; the mod must stall `_execute`/`saveMove_proc` without desyncing the
   save-menu teardown. Prototype the hold in isolation first.
2. **Shade-watcher setter may be absent.** If no NPC calls
   `dShadeRefuge_setRespawn` in this .dusk (see §3 caveat), the alternate never
   triggers. Resolve as a data question before building the alternate branch.
3. **Input source.** Pattern B needs `getTrigA/getTrigB` (or the mod's input
   service). Per the "GC Trigger Input Gotcha" memory, never gate on
   trigL/trigR; A/B face buttons are fine. Pattern A (modal) sidesteps this.
4. **Gate parity.** `albwWarpChoiceAllowed()` (wolf-without-Master-Sword skips
   the choice) must be reproduced from mod-reachable state
   (`dMeter2_isWolfForm` equivalent + `isItemFirstBit(MASTER_SWORD)`), else pre-MS
   Wolf Link gets an Ordon softlock the fork specifically guards against
   (`d_gameover.cpp:151-153`).
5. **DN-10 sign-off.** The mandatory receiver translation (no native proc slot)
   needs the user's ruling on Pattern A vs B before code (§4).

**Sequencing (when the user greenlights a build)**
1. Confirm §3 caveat (is a Shade Watcher live in the mod?) and §4 pattern choice.
2. Add the mod-owned choice state + gate parity (`albwWarpChoiceAllowed`).
3. Wire `saveMove_proc`/`_execute` hooks: on type-0 death + postman + gate, push
   the toast (Pattern B) or dialog (Pattern A) and hold.
4. Wire `saveClose_proc` hook: branch on choice → reuse
   `dShadeRefuge_executePendingWarp`-style apply (shade) /
   `setNextStage("F_SP103",0,1,-1)` + `setLife(max)` (Ordon) / do-nothing
   (Continue = vanilla). Call `albw_oocoo_on_warp_choice` for the Oocoo context.
5. Verify NO save write (§5); toggle-off == provably stock (choice never shown,
   vanilla continue).
6. Compile-check all 8 platforms; run `check_hooks` (baseline 30 unchanged — no
   new bare symbols).

---

### Appendix — key cites
- Fork toast: `src/dusk/ui/ui.hpp:30-35,106`; `src/dusk/ui/ui.cpp:448-450`;
  `src/dusk/ui/overlay.hpp:9,20`.
- Fork choice: `src/d/d_gameover.cpp:110-139,146-153,385-416,420-515,539-590`.
- Fork shade refuge: `include/d/d_albw_shade_refuge.h:13`;
  `src/d/d_albw_shade_refuge.cpp:54-56,173-199`.
- Not exported: `build/windows-msvc-relwithdebinfo/dusklight_exports.def` (no
  `push_toast`/`Toast`).
- Stock baseline: `dusklight-main/src/d/d_gameover.cpp` (no toast/warp);
  `dusklight-main/include/d/d_gameover.h:34-48,62,64`.
- SDK service: `dusklight-main/sdk/include/mods/svc/ui.h` (`UiService`,
  `push_toast`, `dialog_push`, `UiToastDesc`, `UiDialogDesc`).
- Mod state already present: `src/mod.cpp:66`; `src/shade_refuge.cpp/.h`;
  `src/oocoo.cpp:97,113,172`; `src/parry_hooks.cpp:31,94`;
  `src/albw_settings_ui.cpp` (svc_ui usage).
