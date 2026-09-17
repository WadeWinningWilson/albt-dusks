#!/usr/bin/env python3
"""Write dist/albt_full_plugin.dusk — one multiplatform bundle for all hosts.

A .dusk file is a zip archive. The combined ship file contains BOTH native libraries;
Dusklight picks the right one at load time based on your OS:

  lib/windows-amd64/mod.dll   — Windows x64
  lib/linux-x86_64/mod.so     — Linux x64

You install the SAME filename on every platform. Do not ship separate Windows/Linux dusks
to players unless debugging a single platform locally (build/mods/ and build-linux/mods/).

Requires:
  build/mods/albt_full_plugin.dusk          (windows-amd64)
  build-linux/mods/albt_full_plugin.dusk    (linux-x86_64) — or dist/albw.dusk fallback

CI uses merge_mod.py + symgen for verified metadata; local dev uses this script.
"""

from __future__ import annotations

import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WIN = ROOT / "build" / "mods" / "albt_full_plugin.dusk"
LINUX = ROOT / "build-linux" / "mods" / "albt_full_plugin.dusk"
LINUX_FALLBACK = ROOT / "dist" / "albw.dusk"
OUT = ROOT / "dist" / "albt_full_plugin.dusk"
PLATFORMS_TXT = b"""A Link Between Twilight - multiplatform bundle
==============================================

This ONE .dusk file works on every supported platform. Install the same file on Windows
and Linux; the game loads the matching native library automatically.

  lib/windows-amd64/mod.dll   Windows x64 (stock Dusklight)
  lib/linux-x86_64/mod.so     Linux x64

Dev-only single-platform builds (do not distribute to players):
  build/mods/albt_full_plugin.dusk        Windows only
  build-linux/mods/albt_full_plugin.dusk  Linux only
"""


def fail(msg: str) -> None:
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(1)


def lib_entries(archive: zipfile.ZipFile) -> list[str]:
    return [n for n in archive.namelist() if n.startswith("lib/") and not n.endswith("/")]


def write_entry(out: zipfile.ZipFile, name: str, data: bytes) -> None:
    info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
    out.writestr(info, data)


def main() -> None:
    if not WIN.is_file():
        fail(f"Windows bundle missing — run _build_mod.bat first: {WIN}")

    linux_path = LINUX if LINUX.is_file() else LINUX_FALLBACK
    if not linux_path.is_file():
        fail(f"Linux bundle missing — run build-linux.cmd or keep {LINUX_FALLBACK}")

    with zipfile.ZipFile(WIN) as win_arc, zipfile.ZipFile(linux_path) as linux_arc:
        win_libs = lib_entries(win_arc)
        linux_libs = lib_entries(linux_arc)
        if not any("windows-amd64" in n for n in win_libs):
            fail(f"{WIN} has no lib/windows-amd64/mod.dll")
        if not any("linux-x86_64" in n for n in linux_libs):
            fail(f"{linux_path} has no lib/linux-x86_64/mod.so")

        OUT.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as out:
            for name in win_arc.namelist():
                if name.startswith("lib/"):
                    continue
                if name.endswith("/"):
                    write_entry(out, name, b"")
                    continue
                write_entry(out, name, win_arc.read(name))

            write_entry(out, "PLATFORMS.txt", PLATFORMS_TXT)

            for arc, names in ((win_arc, win_libs), (linux_arc, linux_libs)):
                for name in names:
                    write_entry(out, name, arc.read(name))

    z = zipfile.ZipFile(OUT)
    libs = lib_entries(z)
    print(f"wrote {OUT} ({OUT.stat().st_size} bytes)")
    print("multiplatform — one file, native libs:", ", ".join(libs))
    if not any("windows-amd64" in n for n in libs):
        fail("output missing windows-amd64 — Windows hosts will not load this bundle")
    if not any("linux-x86_64" in n for n in libs):
        fail("output missing linux-x86_64 — Linux hosts will not load this bundle")


if __name__ == "__main__":
    main()
