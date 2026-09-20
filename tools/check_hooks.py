#!/usr/bin/env python3
"""Portability gate for hook declarations.

Two real outages came from how hook targets are named, and both were invisible
to a Windows build:

  * macOS/iOS  - ALBT_SYM prefixed Mach-O's leading '_' onto the Itanium name.
    The host resolves through symgen's manifest, whose contract is explicit
    ("no Mach-O leading underscore", dusklight src/dusk/mods/manifest.hpp:38-40),
    so EVERY mangled-string hook missed on Apple from the first release.
  * Linux      - file-local `static` functions are absent from that platform's
    embedded symbol manifest, so bare-name targets do not resolve there. This
    is upstream's to fix; nothing we spell differently helps.

This script fails the build on the two mistakes we CAN prevent, and reports the
Linux-fragile surface so it cannot grow silently.

Rules
  ERROR  a DEFINE_HOOK_SYMBOL whose target is a raw mangled string ("?..."
         or "_Z...") written inline instead of going through ALBT_SYM.
         That is a per-platform hardcode - the exact shape of the Apple bug.
  ERROR  a DEFINE_HOOK_SYMBOL whose target is a bare name that IS declared in
         an SDK header. Those must use DEFINE_HOOK(&fn, Tag): the compiler then
         mangles per target and the signature is type-checked, so it works
         everywhere. Twilit Realm's own randomizer uses the typed form for all
         but one of its hooks.
  INFO   a DEFINE_HOOK_SYMBOL whose target is a bare name with no header
         declaration - a file-local static. A string is the only option, but it
         is inert on Linux today. Counted, and compared against BASELINE below
         so additions are a deliberate, visible choice.

Usage: python tools/check_hooks.py [--sdk <dusklight checkout>] [--update-baseline]
"""

import argparse
import os
import re
import sys

# File-local statics reachable only by name, counted as CALL SITES (30 sites /
# 27 unique names - daE_FM_Execute is hooked from 2 files, fopAc_Execute from 3).
# Raising this means knowingly adding a feature that will be inert on Linux
# until upstream indexes statics there.
BASELINE = 30

HOOK_RE = re.compile(r'DEFINE_HOOK_SYMBOL\(\s*([^,]+?)\s*,', re.S)
BARE_RE = re.compile(r'^"([A-Za-z_][A-Za-z0-9_]*)"$')
MANGLED_RE = re.compile(r'^"(\?|_Z)')


def sdk_include_dirs(root):
    out = []
    for sub in ("include", "libs"):
        p = os.path.join(root, sub)
        if os.path.isdir(p):
            out.append(p)
    return out


def header_declares(name, include_dirs):
    """True if `name` appears as a declaration in any SDK header."""
    pat = re.compile(r'\b' + re.escape(name) + r'\s*\(')
    for d in include_dirs:
        for dirpath, _, files in os.walk(d):
            for f in files:
                if not f.endswith((".h", ".hpp", ".inc")):
                    continue
                try:
                    with open(os.path.join(dirpath, f), encoding="utf-8",
                              errors="replace") as fh:
                        if pat.search(fh.read()):
                            return True
                except OSError:
                    pass
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sdk", default=None)
    ap.add_argument("--update-baseline", action="store_true")
    args = ap.parse_args()

    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    sdk = args.sdk
    if sdk is None:
        for cand in (os.path.join(repo, "dusklight"),
                     os.path.join(os.path.dirname(repo), "dusklight-main")):
            if os.path.isdir(os.path.join(cand, "include")):
                sdk = cand
                break
    if sdk is None:
        print("check_hooks: no Dusklight checkout found; skipping "
              "(pass --sdk <path> to enable)")
        return 0
    incs = sdk_include_dirs(sdk)
    print(f"check_hooks: SDK headers from {sdk}")

    errors, statics = [], []
    src = os.path.join(repo, "src")
    for dirpath, _, files in os.walk(src):
        for f in sorted(files):
            if not f.endswith((".cpp", ".h", ".inc", ".hpp")):
                continue
            path = os.path.join(dirpath, f)
            rel = os.path.relpath(path, repo).replace("\\", "/")
            with open(path, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
            for m in HOOK_RE.finditer(text):
                target = m.group(1).strip()
                line = text.count("\n", 0, m.start()) + 1
                if target.startswith("ALBT_SYM") or not target.startswith('"'):
                    continue  # macro-routed: platform-aware by construction
                if MANGLED_RE.match(target):
                    errors.append(
                        f"{rel}:{line}: raw mangled target {target} - route it "
                        f"through ALBT_SYM in src/albw_symbols.h so every "
                        f"platform gets the right spelling")
                    continue
                bare = BARE_RE.match(target)
                if bare and header_declares(bare.group(1), incs):
                    errors.append(
                        f"{rel}:{line}: {target} is declared in an SDK header - "
                        f"use DEFINE_HOOK(&{bare.group(1)}, Tag) instead so the "
                        f"compiler mangles it per target")
                elif bare:
                    statics.append(f"{rel}:{line} {bare.group(1)}")

    print(f"check_hooks: {len(statics)} file-local-static hooks "
          f"(baseline {BASELINE}) - these are inert on Linux")
    for e in errors:
        print(f"::error::{e}" if os.environ.get("GITHUB_ACTIONS") else f"ERROR {e}")

    if args.update_baseline:
        print(f"check_hooks: set BASELINE = {len(statics)}")
        return 0
    if len(statics) > BASELINE:
        msg = (f"file-local-static hooks grew {BASELINE} -> {len(statics)}. Each "
               f"one is a feature that will not work on Linux. If that is "
               f"intended, raise BASELINE in tools/check_hooks.py in the same "
               f"commit.")
        print(f"::error::{msg}" if os.environ.get("GITHUB_ACTIONS") else f"ERROR {msg}")
        return 1
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
