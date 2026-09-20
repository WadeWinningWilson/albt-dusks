# Release procedure — A Link Between Twilight (`dev.albt.albw`)

**Follow every step, in order, every time. No skipping, no reordering.**
Each gate exists because skipping it once already shipped a broken build to
players.

Nothing is pushed or tagged until step 6, and step 6 needs the user's explicit
go.

---

## 1. Privacy check

Source, then binaries. Both. Source alone is not enough — the leak that mattered
most (absolute paths carrying a username and a temp session directory) was in a
tool config, not in `src/`.

```bash
# tracked source: identifiers, personal paths, machine paths
git grep -nIiE "ryana|C:\\\\Users|C:/Users|/Users/[a-z]|@gmail|ALBT DUSKS STUFF|Decomps|/home/[a-z]" \
  -- . ':!*.png' ':!dist/*'

# debug file-writers must all be 0
grep -rn "define ALBW_DEBUG_DUMPS\|define D_ALBW_ARMO_PURSUIT_TEST\|define D_ALBW_OUTFIT_SWAP_DEBUG\|define ALBW_WOLFHIT_PROBE\|define ALBW_RESROW_PROBE" src/
```

Then the **shipped binaries** (after step 5, before step 6): scan every slice in
the combined `.dusk` for `ryana`, drive paths, `/home/<user>`, `@gmail`,
`Documents/dusklight`, `Decomps`, and `AppData\Local\Temp`. Apple slices may
legitimately contain `/Users/runner/work/albt-dusks` — that is GitHub's hosted
runner and a public repo name, not personal data.

**Why:** `39e0ee9` — a staged port wrote absolute fork/stock/out paths into
`tools/port/bgos.json`, including a temp session directory. Caught by this step.

## 2. Hook portability gate

```bash
python tools/check_hooks.py
```

Must exit 0. It fails on a raw per-platform mangled string that bypasses
`ALBT_SYM`, on a bare-name target the SDK headers declare (use
`DEFINE_HOOK(&fn, Tag)`), and on growth in file-local-static hooks.

If the static count grew and that is intended, raise `BASELINE` in
`tools/check_hooks.py` **in the same commit** — knowingly shipping a feature
that is inert on Linux.

**Why:** two cross-platform outages, both invisible to a Windows build. The Apple
`ALBT_SYM` underscore made every symbol-string hook miss on macOS/iOS from the
first release, and file-local statics do not resolve on Linux at all.

## 3. Confirm the build is NOT debug

Two independent checks — the CI line, and the artifact itself.

```bash
# the configure line CI actually used
gh run view --repo WadeWinningWilson/albt-dusks --job <linux job id> --log \
  | grep -i "CMAKE_BUILD_TYPE"
# expect: args=(-G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ...)
```

Then dump the Windows slices' import tables. They must import the
**redistributable** CRT:

- correct: `MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll`, `api-ms-win-crt-*`
- **broken**: `MSVCP140D.dll`, `VCRUNTIME140D.dll`, `ucrtbased.dll`

The debug CRT is not redistributable. It exists only on machines with Visual
Studio, so a debug-CRT build fails to load for every ordinary player with
Win32 error 126, *"The specified module could not be found"* — which names
`mod.dll` but actually means its dependency is missing.

**Why:** v0.2.0 → v0.2.5 all shipped the debug CRT and were unloadable for every
player without Visual Studio. CI had drifted from the upstream mod-template,
which sets the build type explicitly. The workflow now also has a `dumpbin`
gate, but verify the artifact anyway — a gate can be edited out.

## 4. Build all 8 platforms in CI

Local builds are compile checks only; they are Windows-only and must never be
distributed. Push the branch (not a tag) to get the full matrix:

- linux-x86_64, linux-aarch64
- macos-arm64, macos-x86_64, ios-arm64
- windows-amd64, windows-arm64
- android-aarch64
- plus **Combine bundles**

All 9 jobs must be green. A tag is what publishes; a branch push builds only
(`if: startsWith(github.ref, 'refs/tags/')` guards the release step), so this is
safe to run before the go decision.

## 5. Verify the combined artifact

Download the `mod-combined` artifact and confirm, on the actual file:

- `mod.json` version is the **new** version (the portal rejects a duplicate
  version — the git tag is not what it validates)
- 8 slices present
- release CRT on both Windows slices (step 3)
- privacy clean on all slices (step 1)

## 6. Push for release — **needs the user's explicit go**

Only now:

```bash
# bump mod.json to the new version first, and commit it
git push origin main
git tag vX.Y.Z && git push origin vX.Y.Z
```

The tag push publishes to `WadeWinningWilson/A-Link-Between-Twilight`.

**The GitHub release does not reach players.** The Dusklight portal copy is what
users download, and uploading it is a manual step the user performs. A release
is not finished until that upload happens.

---

## Standing notes

- **`mod.json` must be bumped every release.** The portal validates the package
  version, not the tag. A duplicate is rejected outright.
- **Never distribute a local build.** `_build_mod.bat` produces a Windows-only
  2-slice package and reinstalls it as `albt_full_plugin.dusk`, which then
  **collides** with an installed `dev.albt.albw.dusk` (same mod id). Park one
  before testing the other.
- **Credentials are never handled here.** The release token is a repo secret the
  user set themselves; portal upload is the user's step.
