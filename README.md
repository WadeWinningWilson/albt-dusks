# A Link Between Twilight Dusk

A stock Dusklight `.dusk` mod that ports the **A Link Between Twilight** feature
set onto an unmodified `dusklight` runtime. It builds against
[dusklight](https://github.com/TwilitRealm/dusklight) **v2.0.0** — never the ALBT
fork — so it runs on the same `dusklight` executable players already have.

**Display name (Mods panel):** A Link Between Twilight
**Mod id:** `dev.albt.albw`
**Filename (per-platform build):** `albt_full_plugin.dusk`
**Filename (combined release):** `A Link Between Twilight.dusk`

## Install

1. Download **`A Link Between Twilight.dusk`** from the
   [Releases page](https://github.com/WadeWinningWilson/A-Link-Between-Twilight/releases)
   (latest: **v0.2.1**). This one file carries the native library for every
   supported platform.
2. Drop it into your Dusklight `mods` folder:
   - **Windows:** `%AppData%\TwilitRealm\Dusklight\mods\`
   - Other platforms: the `mods` folder of your Dusklight install.
3. Launch Dusklight and enable **A Link Between Twilight** in the Mods panel.

## Platforms

The released bundle is built by CI for all 8 Dusklight targets and merged into a
single `.dusk`:

| | |
|---|---|
| `linux-x86_64` | `linux-aarch64` |
| `macos-arm64` | `macos-x86_64` |
| `ios-arm64` | `android-aarch64` |
| `windows-amd64` | `windows-arm64` |


The release is **built reproducibly by GitHub Actions** from this repository
([`.github/workflows/build.yml`](.github/workflows/build.yml)). The published
`.dusk` is exactly what CI compiles from the tagged source, so you can verify the
binary against the code:

```bash
git checkout v0.2.1   # the exact source the v0.2.1 release was built from
```

## Build (local — a compile check)

CI is the source of truth for shippable builds (all 8 platforms). Locally you can
compile-check the native library for your host:

```bat
_build_mod.bat
```

Output: `build/mods/albt_full_plugin.dusk`, packed to
`dist/albt_full_plugin.dusk` (via `tools/pack_dist.py`) and installed to your
local Dusklight `mods` folder.

Linux cross-build from Windows (requires [Zig](https://ziglang.org) on `PATH`, or
set the `ZIG` env var):

```bat
build-linux.cmd
```

The multi-platform release bundle is produced in CI by `tools/merge_mod.py` +
[symgen](https://github.com/encounter/symgen), merging every platform's artifact
into one `.dusk`.

## SDK pin

The Dusklight SDK version is pinned by `DUSKLIGHT_VERSION` in `CMakeLists.txt`
(currently `e9b120544c…`, Dusklight **v2.0.0**). The mod declares
`FEATURES game webgpu` — `webgpu` drives the Tear of Light glow via `GfxService`.

## Notes

Do **not** load this collective bundle alongside standalone mods that duplicate
the same hooks (e.g. this + `dev.albt.region_hp`).

Soul of Light differs by shape from the standalone: the collective **halves the
wallet on death**, whereas the standalone **only spawns the recovery orb** after
something else has already taken rupees (Lazy Tweaks *Lose Rupees*).

## ALBT fork

The Dusklight fork in `Documents/dusklight` remains the WW / True ALBW / content
lab. **This** repo is the player-facing mod that runs on the stock `dusklight`
runtime.
