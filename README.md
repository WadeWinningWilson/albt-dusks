# A Link Between Twilight Full Plugin (`dev.albt.albw`)

Stock Dusklight `.dusk` for the full ALBT feature set. Builds against [dusklight-main](https://github.com/TwilitRealm/dusklight) (Game ABI 2), never the ALBT fork.

**Display name (Mods panel):** A Link Between Twilight Full Plugin  
**Mod id:** `dev.albt.albw`  
**Filename:** `albt_full_plugin.dusk`

## Where is the file?

| Purpose | Path |
|---------|------|
| **Ship / install this (all platforms)** | `dist/albt_full_plugin.dusk` — **one file** for Windows amd64 and Linux x86_64 |
| Windows-only dev build | `build/mods/albt_full_plugin.dusk` |
| Linux-only dev build | `build-linux/mods/albt_full_p


Do **not** load the collective bundle alongside standalones that duplicate the same hooks (e.g. collective + `dev.albt.region_hp`).

Soul of Light differs by shape: the collective **halves the wallet on death**; the standalone **only spawns the orb** after something else already took rupees (Lazy Tweaks Lose Rupees).

## Build (Windows)

```bat
_build_mod.bat
```

Output: `build/mods/albt_full_plugin.dusk`

## Build (Linux cross from Windows)

```bat
build-linux.cmd
```

Requires [Zig](https://ziglang.org) on your `PATH` (or set the `ZIG` environment
variable to the executable). Broader cross-platform build support is a work in
progress, in collaboration with **Lazy Tweaks**.

## Combined bundle

Merge Windows + Linux artifacts into `dist/albt_full_plugin.dusk` (both `lib/windows-amd64/mod.dll` and `lib/linux-x86_64/mod.so`). After `_build_mod.bat`, run `python tools/pack_dist.py` (or CI `merge_mod.py` + symgen).

Pin: `DUSKLIGHT_VERSION` in `CMakeLists.txt` (currently `a775418c66…`).

## ALBT fork

The dusklight fork in `Documents/dusklight` remains the WW / True ALBW / content lab. This repo is the player-facing mod on stock `dusklight.exe`.
