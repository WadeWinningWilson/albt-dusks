# Linux hook coverage — getting the six missing features onto Linux

**Status: planned, not started.** Nothing in here is implemented.

Thirty hook sites are inert on Linux, costing six features. This is the plan to
close them **without changing behaviour on the other seven platforms** — that
constraint is the user's and it shapes every choice below.

Release gates live in [RELEASE-PROCEDURE.md](RELEASE-PROCEDURE.md); step 2 there
is the gate that counts these sites, and §6 below says how it must change.

---

## 1. The finding: symbol lookup is not hooking

We had been treating "Linux can't resolve file-local statics" as "Linux can't
hook them." It isn't. The hook service takes a **raw address**:

```c
ModResult (*add_pre)(ModContext*, void* fn_addr, HookPreFn, const HookOptions*);
ModResult (*install)(ModContext*, void* fn_addr, void* trampoline, void** out_original);
```
(`dusklight-main/sdk/include/mods/svc/hook.h:90-103`)

`resolve()` — the thing that fails on Linux — is only one way to obtain that
address, and the manifest's own contract says so: it exists to resolve
*"non-exported (static) functions"* by name (`hook.h:117-127`). On Linux those
statics are absent from the embedded manifest, which is upstream's to fix.

But the game's own data structures already hold the addresses, and they are
exported.

### Why the other platforms cannot be affected

`mods::hook::install` (`sdk/include/mods/svc/hook.hpp:170-185`):

```cpp
Entry::hooks = hooks;
if (Entry::target == nullptr) {              // <-- only resolves when target is unset
    void* resolved = Entry::resolved_target();
    if (resolved == nullptr) return MOD_UNAVAILABLE;
    Entry::target = resolved;
}
return hooks->install(mod_ctx, Entry::target, trampoline, &g_orig);
```

`Entry::target` is `static inline void* target = nullptr;` (`hook.hpp:35`) —
public and settable. So the shape is **symbol first, address second**:

```cpp
ModResult r = mods::hook::install<Tag>(svc_hook);   // exactly today's path
if (r == MOD_UNAVAILABLE) {                          // only ever true on Linux today
    Tag::target = albw_profile_method(fpcNm_B_GM_e, ALBT_METHOD_EXECUTE);
    r = mods::hook::install<Tag>(svc_hook);
}
```

On Windows x64/arm64, macOS x64/arm64, iOS and Android the first call succeeds
and the second branch never runs — byte-for-byte the current code path. It is a
**capability** fallback, not `#ifdef __linux__`, so it cannot rot if a
platform's behaviour changes, and it reuses the macro's own generated
trampoline so the signature stays type-checked.

---

## 2. Route 1 — actor profiles. 16 of 30 sites.

`g_fpcPf_ProfileList_p` is exported (verified present in
`dusklight_exports.def`) and indexed by proc name
(`dusklight-main/src/f_pc/f_pc_profile.cpp:39`). The chain:

```
g_fpcPf_ProfileList_p[fpcNm_B_GM_e]     process_profile_definition*
  -> ->sub_method                        actor_method_class*      (f_op_actor.h:18)
  -> .base.base                          process_method_class     (f_pc_method.h:8-13)
       { create_method, delete_method, execute_method, is_delete_method }
```

Those pointers **are** `daB_GM_Create` / `daB_GM_Delete` / `daB_GM_Execute`. The
profile literally names them: `/* Actor SubMtd */ &l_daB_GM_Method`
(`dusklight-main/src/d/actor/d_a_b_gm.cpp:2271`).

| Module | Sites | Targets |
|---|---|---|
| `armogohma.cpp` | 3 | daB_GM_Create/Execute/Delete |
| `e_s1_hooks.cpp` | 3 | daE_S1_Create/Execute/Delete |
| `fyrus_golem.cpp` | 3 | daB_GO_Create/Execute/Delete |
| `fyrus_kids.cpp` | 2 | daB_GOS_Create/Execute |
| `diababa.cpp` | 2 | daB_BQ_Create/Execute |
| `fyrus.cpp` | 2 | daE_FM_Create/Execute |
| `fyrus_phases.cpp` | 1 | daE_FM_Execute |

## 3. Route 2 — `g_fopAc_Method`. 4 of 30 sites.

Also exported, and the thing every actor profile points at as its Leaf SubMtd
(`d_a_b_gm.cpp:2269`). `.base.base.execute_method` and `.delete_method` are
`fopAc_Execute` and `fopAc_Delete`.

| Module | Sites |
|---|---|
| `region_port.cpp` | 2 — fopAc_Execute, fopAc_Delete |
| `boss_refinement_hooks.cpp` | 1 — fopAc_Execute |
| `enemy_rupees.cpp` | 1 — fopAc_Execute |

Smallest surface, biggest single unlock: one exported symbol restores region
damage/HP scaling, enemy death rupees and the boss-refinement bookends.
**Prototype here first.**

## 4. Route 3 — the state functions. The remaining 10.

`e_fm_*` and `b_bq_*` are internal statics with no profile entry. They are
dispatched from a plain `switch (i_this->mAction)`
(`dusklight-main/src/d/actor/d_a_e_fm.cpp:2612-2650`), so the same moments are
reachable from the actor's Execute — which Route 1 makes portable — by
branching on the same field.

| Module | Sites |
|---|---|
| `fyrus.cpp` | 5 — e_fm_down, e_fm_normal, e_fm_fight_run, e_fm_stop, e_fm_damage_run |
| `fyrus_phases.cpp` | 3 — e_fm_n_fight, e_fm_f_fight, e_fm_fire |
| `diababa.cpp` | 2 — b_bq_damage, b_bq_wait |

**This is not a lookup trick, it is a different seam**, and that is what makes
it the hard one — see §7.

---

## 5. What Linux gets, per route

| | Routes 1+2 only | plus Route 3 |
|---|---|---|
| Armogohma | full | full |
| Shadow Beast (E_S1) | full | full |
| Fyrus golem | full | full |
| Goron kids | full | full |
| Region damage + HP scaling | full | full |
| Enemy death rupees | full | full |
| Boss-refinement bookends | full | full |
| **Fyrus behaviour layer** | partial (create/execute only) | full |
| **Diababa** | partial (create/execute only) | full |

Routes 1+2 alone: **20 of 30 sites, and every feature at least partially
restored.** Fyrus and Diababa become partially refined rather than fully
vanilla.

---

## 6. What must change in the release gates

Cross-reference: [RELEASE-PROCEDURE.md](RELEASE-PROCEDURE.md) **step 2**.

`tools/check_hooks.py` currently counts bare-name `DEFINE_HOOK_SYMBOL` sites and
compares against `BASELINE = 30`, described as *"reports the Linux-fragile
surface so it cannot grow silently."*

Once a site has a working address fallback it is **no longer Linux-fragile**,
but it still looks like a bare-name hook to the script. So the gate's rule has
to change with the code, in the same commit:

- the count must distinguish *"bare name, no fallback"* (still fragile) from
  *"bare name with a registered address fallback"* (fine),
- `BASELINE` drops as sites are converted, and
- the rule should flip from **counting** to **failing on any new unfallbacked
  bare-name hook** — otherwise a zero baseline silently permits reintroducing
  the problem.

Do not convert code without updating the gate; a passing gate that no longer
measures the thing it names is worse than no gate.

---

## 7. Risks, and the one that is not solved

1. **Identical code folding.** Hooking by address rather than name means two
   trivially identical statics folded by the linker share one address.
   `resolve()` reports `MOD_CONFLICT` for that case (`hook.h:126`); the address
   route would silently hook both. Audit the 20 targets once; do not assume.
2. **Timing.** The profile list must be populated before it is read, so the
   fallback cannot run at `mod_initialize`.
3. **Signature.** Reusing the `DEFINE_HOOK_SYMBOL` trampoline keeps the
   signature type-checked. Any hand-rolled address hook would lose that — which
   is the argument for one shared `ALBT_PROFILE_HOOK` helper over 20 bespoke
   call sites.
4. **Route 3 placement — UNSOLVED.** A pre-hook on Execute fires *before* the
   switch; a pre-hook on the state function fires at the case body. Those are
   the same moment only if nothing between Execute entry and the switch mutates
   `mAction`. Getting this wrong is the exact mistake that cost this project
   three regressions, so it is verified per state or not done.

---

## 8. Sequencing

1. **Route 2 prototype** — 4 sites, one exported symbol, proves the fallback
   end-to-end and surfaces the ICF question on the smallest surface.
2. **Route 1 sweep** — mechanical once the helper exists.
3. **Gate update** (§6) — in the same commit as the conversions it describes.
4. **Route 3** — only after §7.4 is answered per site, and only in a shape that
   cannot alter the other seven platforms.

Upstream indexing file-local statics on Linux remains the real fix, and would
light up all thirty sites with no code change here. Worth asking for regardless
of whether this plan is built.
