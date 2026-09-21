# Upstream report — file-local statics missing from the Linux symbol manifest

Draft for the Dusklight issue tracker. Our own workaround plan is
[LINUX-HOOK-COVERAGE.md](LINUX-HOOK-COVERAGE.md); this is the root-cause fix
that would make it unnecessary.

**Laid out to match upstream's own intake form** — headings below map 1:1 onto
the fields in `.github/ISSUE_TEMPLATE/bug-report.yml`, so it can be pasted
field by field.

---

## Which template, and the certification checkbox

File as **Bug Report**, not Feature Request. Its first field is a *required*
checkbox:

> The bug occurs on an **official release** of Dusklight, without any forks or
> third-party patches installed

**We can tick that honestly, but only if the report is framed around the
manifest and not around our mod.** The defect reproduces on a stock Linux
build with nothing loaded: the symbol is in the binary's `.symtab` and absent
from the embedded manifest. No mod, no fork, no patch. Our mod is how we
*found* it and belongs in the "Texture Packs and Mods" field as context — it
must not be the repro, or the checkbox becomes a lie and the issue gets closed
as a fork problem.

Asks 2 and 3 in the final section are **Feature Requests** (that template is
three fields and has no certification), and should be filed separately rather
than buried in a bug.

---

## Bug Description

`HookService::resolve()` does not find file-local `static` functions on Linux.
It finds them on Windows, and appears to find them on Apple. Same source, same
hook declaration, resolves on Windows/macOS/iOS and returns `MOD_UNAVAILABLE`
on Linux.

`sdk/include/mods/svc/hook.h:117-127` documents `resolve()` as handling
*"non-exported (static) functions"*, and says `DEFINE_HOOK_SYMBOL` exists
*"for targets you can't name in C++: file-local statics"* — so this reads as a
platform gap rather than an intended limit.

## Steps to Reproduce

On an official Linux build, with the symgen version the build pins:

```bash
# Any file-local static in a decomp actor. Lowercase 't' = STB_LOCAL, in .symtab.
nm -a dusklight | grep ' t .*daB_GM_Execute'   # present
nm -D dusklight | grep daB_GM_Execute          # absent (.dynsym only)

# Now check the embedded manifest for the same name.
symgen manifest --binary dusklight <dump/inspect flag>
```

In `.symtab` but not in the manifest confirms it. Nothing needs to be loaded.

## Expected Behavior

A file-local static present in the binary's symbol table resolves through the
manifest on Linux, as it does on Windows and Apple.

## Current Behavior

It does not resolve. `resolve()` returns `MOD_UNAVAILABLE`, indistinguishable
from a symbol that genuinely does not exist, so the only symptom is a feature
quietly not working.

## OS / Architecture

Linux x86-64 and Linux ARM64 (their dropdown spells the former "Linux x84-64").
Not reproducible on Windows x86-64/ARM64, macOS ARM64/x86-64, or iOS ARM64.

## Texture Packs and Mods

Disclose ours here, not in the repro: *A Link Between Twilight*
(`dev.albt.albw`), which is how the gap was found — 30 hook sites across boss
and enemy actors are inert on Linux. **The repro above needs no mod.**

## Additional Context — what we ruled out, and the hypothesis

We checked the obvious cause first and it does not hold:

- `symgen manifest --embed` is a **POST_BUILD** command
  (`cmake/SymbolManifest.cmake:149-154`), so it runs before any stripping.
- The strip steps are `install(CODE ...)` (`CMakeLists.txt:672-689`) and
  therefore later.
- Neither strip would matter regardless: Apple's `strip -S` is debug-only, and
  Linux's `objcopy --strip-debug` leaves `.symtab` intact.

So symgen is handed an unstripped binary on both platforms, and the difference
must be inside its readers. The input mode differs
(`cmake/SymbolManifest.cmake:131-135`):

```cmake
if (WIN32)
    set(_input --pdb    "$<TARGET_PDB_FILE:${target}>")
else ()
    set(_input --binary "$<TARGET_FILE:${target}>")
endif ()
```

Windows reads a PDB, which carries every symbol — that explains Windows. Apple
and Linux share `--binary`, but the formats disagree about where local symbols
live: **Mach-O keeps them in `LC_SYMTAB` alongside everything else, while ELF
splits `STB_LOCAL` into `.symtab`, away from `.dynsym`.**

An ELF reader walking only `.dynsym` would produce exactly this: statics on
Windows and Apple, nothing on Linux, no error anywhere.

We have not read symgen's source, so this is inference from observable
behaviour plus the format difference — offered as a starting point, not a
diagnosis.

---

## The three asks, in preference order

1. **Index `.symtab` locals in the ELF path** (the Bug above). No mod changes
   anywhere.

2. **Make the gap detectable** — *separate Feature Request.* A mod currently
   cannot distinguish "symbol absent" from "this platform does not index this
   symbol class"; both are `MOD_UNAVAILABLE`. A distinct status, or a manifest
   flag saying whether locals are indexed, would let mods fall back
   deliberately and log it rather than fail silently. **This is the one worth
   pressing if (1) is expensive** — it turns an invisible hole into a
   diagnosable one for every mod.

3. **Document the limitation** if it is intentional — *separate Feature
   Request.* `hook.h:117-127` promises statics without qualification.

## Our workaround, offered as context

About two thirds of our targets are reachable without the manifest, by reading
addresses out of exported game tables (`g_fpcPf_ProfileList_p` for actor
create/delete/execute, `g_fopAc_Method` for the framework's) and assigning to
`Entry::target` before `install()`, which skips resolve entirely
(`sdk/include/mods/svc/hook.hpp:170-185`).

Worth saying: that is decomp-specific knowledge each mod author would have to
rediscover, and it does not help internal state functions with no table entry.
A manifest fix covers all of it.

Happy to test a patch — we have 30 affected sites and CI building all eight
platforms.
