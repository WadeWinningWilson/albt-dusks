# Upstream report — file-local statics missing from the Linux symbol manifest

Draft to send to the Dusklight maintainers. Our side of the problem is planned
in [LINUX-HOOK-COVERAGE.md](LINUX-HOOK-COVERAGE.md); this is the root-cause fix
that would make that plan unnecessary.

Line references are to `dusklight` at the revision we build against.

---

## Summary

`HookService::resolve()` does not find file-local `static` functions on Linux.
It finds them on Windows, and we believe it finds them on Apple. The same mod,
same source, same hook declarations: the hooks install on Windows/macOS/iOS and
silently fail to install on Linux.

For our mod this costs six features on Linux — thirty hook sites across boss
and enemy actors (Armogohma, Diababa, Fyrus and its golem, the Goron kids,
Shadow Beasts), plus `fopAc_Execute`/`fopAc_Delete`, which alone carry region
damage/HP scaling and enemy death rupees. We can work around it (below), but
the workaround is per-mod and every other mod hooking a decomp `static` will
hit the same wall.

## What the contract says should work

`sdk/include/mods/svc/hook.h:117-127` documents `resolve()` as handling
*"non-exported (static) functions"*, and `DEFINE_HOOK_SYMBOL` exists precisely
*"for targets you can't name in C++: file-local statics"*. So this looks like a
platform gap in the implementation rather than an intended limitation — which
is why we're reporting rather than redesigning around it.

## Why we do not think it is the build config

We checked the obvious cause first and it does not hold up:

- `symgen manifest --embed` runs as a **POST_BUILD** command
  (`cmake/SymbolManifest.cmake:149-154`), before any stripping.
- The strip steps are `install(CODE ...)` (`CMakeLists.txt:672-689`), so they
  run later and cannot affect what symgen read.
- Both strips keep the symbol table regardless: Apple's `strip -S` removes
  debug symbols only, and Linux's `objcopy --strip-debug` leaves `.symtab`
  intact.

So on both Apple and Linux, symgen is handed an unstripped binary whose symbol
table contains the local symbols.

## The hypothesis

The input mode differs by platform (`cmake/SymbolManifest.cmake:131-135`):

```cmake
if (WIN32)
    set(_input --pdb    "$<TARGET_PDB_FILE:${target}>")
else ()
    set(_input --binary "$<TARGET_FILE:${target}>")
endif ()
```

Windows reads a PDB, which carries every symbol including statics — that is why
Windows works. Apple and Linux share `--binary`, but the two object formats put
local symbols in different places:

- **Mach-O** keeps local symbols in `LC_SYMTAB`, the same table a reader walks
  for everything else. A reader that handles Mach-O at all tends to get locals
  for free.
- **ELF** splits them: `.dynsym` holds only dynamic/exported symbols, and
  `STB_LOCAL` symbols live in `.symtab`.

**So our best guess is that symgen's ELF reader walks `.dynsym` and not
`.symtab`.** That single difference would produce exactly what we see: statics
present on Windows and Apple, absent on Linux, with no error anywhere.

We have not read symgen's source, so this is inference from the observable
behaviour and the format difference — please treat it as a starting point
rather than a diagnosis.

## Reproducing it

On a Linux build of the host, with the pinned symgen:

```bash
# Pick any file-local static in a decomp actor, e.g. daB_GM_Execute.
nm -a dusklight | grep ' t .*daB_GM_Execute'   # lowercase t = STB_LOCAL, in .symtab
nm -D dusklight | grep daB_GM_Execute          # expected: no output (.dynsym)

# Then check whether the embedded manifest has it.
symgen manifest --binary dusklight <dump/inspect flag>
```

If the symbol is in `.symtab` but not in the manifest, that confirms it.

## What we'd ask for, in order of preference

1. **Index `.symtab` locals in the ELF path**, matching Windows and Apple. This
   is the actual fix and needs no change in any mod.

2. **Failing that, make the gap detectable.** Right now a mod cannot tell
   "symbol genuinely absent" from "this platform doesn't index this symbol
   class" — `resolve()` returns `MOD_UNAVAILABLE` for both, so the only
   symptom is a feature quietly not working. Either a distinct status, or a
   flag/count on the manifest saying whether locals are indexed, would let mods
   fall back deliberately and log it loudly instead of failing silent.

3. **Document the limitation** if it is intentional. `hook.h:117-127` currently
   promises statics without qualification, and `DEFINE_HOOK_SYMBOL`'s stated
   reason for existing is file-local statics. A note that this is
   Windows/Apple-only today would have saved us an outage.

## Our workaround, for context

We can reach about two thirds of our targets without the manifest at all, by
reading addresses out of the game's own exported tables — `g_fpcPf_ProfileList_p`
for actor create/delete/execute, and `g_fopAc_Method` for the framework's — and
assigning them to `Entry::target` before `install()`, which skips the resolve
path entirely (`sdk/include/mods/svc/hook.hpp:170-185`).

That works and we may ship it, but it is worth saying that it is decomp-specific
knowledge every mod author would have to rediscover, and it does not help the
remaining third (internal state functions with no table entry). A manifest fix
covers all of it.

Happy to test a patch against our 30 sites — we have a case with good coverage
of the failure and a CI matrix building all eight platforms.
