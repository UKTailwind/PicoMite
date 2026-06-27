#!/usr/bin/env python3
"""find_dead_globals.py - find file-scope globals that are NEVER used.

Companion to find_static_globals.py. That script finds globals with no
*external* references (-> mark `static`). This one finds globals with no
references AT ALL (-> candidates for *deletion*).

WHY A SEPARATE SIGNAL
---------------------
nm cannot tell whether a global is used inside its own translation unit (an
intra-TU reference is resolved internally and never shows up as undefined). So
the static-finder lumps "used only in its own file" together with "used
nowhere". To separate out the truly-dead ones we use a different, authoritative
source: the linker.

The firmware is compiled with -fdata-sections (every global gets its own
.bss/.data/.rodata.<name> section) and linked with --gc-sections, so the
linker garbage-collects sections nothing references. Every such removal is
listed in the .map file under "Discarded input sections". A project global
whose section was discarded is unused in that build's final image.

A global is reported DEAD (safe to delete) when, in EVERY variant whose source
file is compiled:
  * it is defined (nm: a global data symbol in one of our own objects), and
  * its section was discarded by the linker (.map).
If any variant keeps it (uses it), it is not dead.

To tie a discarded section to the right symbol (a static in another file may
share a name), the discarded section's owning object must be the object that
defines the global.

CROSS-VARIANT SAFETY
--------------------
Same as the static-finder: a global dead in HDMIWEB may be live in PICOMIN.
"DEAD" therefore means dead in every analysed variant that compiles its file.
Use --build-all (or accumulate across several variant scans) for a trustworthy
list. This tool only REPORTS - deletion is a human decision, so there is no
--apply.

USAGE
-----
  python tools/find_dead_globals.py                       # auto-detect build dirs
  python tools/find_dead_globals.py buildRP2040L buildRP2350L
  python tools/find_dead_globals.py --accumulate .dead_scan.json build
  python tools/find_dead_globals.py --accumulate .dead_scan.json --report-only
  python tools/find_dead_globals.py --build-all           # full sweep (slow)
  python tools/find_dead_globals.py --functions           # also dead functions
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import find_static_globals as fsg   # shared helpers (read-only import)

MAP_NAME = "PicoMite.elf.map"
DEFAULT_CACHE = ".dead_scan.json"
# Discarded-section line: ` .bss.<name>` / `.data.<name>` / `.rodata.<name>` / `.text.<name>`
SEC_RE = re.compile(r"^\s+\.(?:bss|data|rodata|text)\.(?P<name>[A-Za-z_]\S*)\b")


def map_obj_to_src(objpath: str) -> str:
    """`.../CMakeFiles/PicoMite.dir/graphics/Draw.c.obj` -> `graphics/Draw.c`."""
    p = fsg.norm(objpath)
    marker = "PicoMite.dir/"
    i = p.find(marker)
    if i >= 0:
        p = p[i + len(marker):]
    return p[:-4] if p.endswith(".obj") else p


def defined_globals(nm, build_dir, include_tp, want_funcs):
    """name -> (src, line, type, objrel) for global data (and funcs) we define."""
    wanted = set(fsg.VAR_TYPES) | (set(fsg.FUNC_TYPES) if want_funcs else set())
    out = {}
    for obj in fsg.object_files(build_dir):
        objrel = fsg.obj_to_source(obj, build_dir)
        if fsg.is_sdk(objrel):
            continue
        if not include_tp and fsg.is_third_party(objrel):
            continue
        defined, _ = fsg.parse_obj(nm, obj)
        for t, name, src, ln in defined:
            if t not in wanted:
                continue
            s = fsg.norm(src or objrel)
            if fsg.is_sdk(s):
                continue
            if not include_tp and fsg.is_third_party(s):
                continue
            out.setdefault(name, (s, ln, t, objrel))
    return out


def discarded_sections(build_dir):
    """Set of (name, src) the linker dropped, from the .map Discarded list.

    Handles wrapped entries (long section name on its own line, address+object
    on the following line)."""
    mapf = build_dir / MAP_NAME
    out = set()
    if not mapf.is_file():
        return out
    lines = mapf.read_text(errors="ignore").splitlines()
    in_block = False
    pending = None  # section name awaiting its object path on a later line
    for line in lines:
        if "Discarded input sections" in line:
            in_block = True
            continue
        if not in_block:
            continue
        if "Memory Configuration" in line or line.startswith("Linker script and memory map"):
            break
        m = SEC_RE.match(line)
        if m:
            name = m.group("name")
            objtok = _object_token(line)
            if objtok:
                out.add((name, map_obj_to_src(objtok)))
                pending = None
            else:
                pending = name           # object path is on a following line
            continue
        if pending:                      # continuation line carrying the object path
            objtok = _object_token(line)
            if objtok:
                out.add((pending, map_obj_to_src(objtok)))
                pending = None
    return out


def _object_token(line: str):
    for tok in line.split():
        if tok.endswith(".obj") or tok.endswith(".o") or ".a(" in tok:
            return tok
    return None


def scan_variant(nm, build_dir, include_tp, want_funcs):
    """Return (variant, defined{name:(src,line,type)}, used{names}, loc)."""
    defined = defined_globals(nm, build_dir, include_tp, want_funcs)
    discarded = discarded_sections(build_dir)
    used, loc = set(), {}
    for name, (src, ln, t, objrel) in defined.items():
        loc[name] = (src, ln, t)
        if (name, objrel) not in discarded:   # kept by the linker -> reachable/used
            used.add(name)
    return fsg.variant_of(build_dir), {n: loc[n] for n in defined}, used, loc


def merge(cache, variant, defined, used, loc):
    d, u, lc = cache["defined"], cache["used"], cache["loc"]
    for name in defined:
        d.setdefault(name, [])
        if variant not in d[name]:
            d[name].append(variant)
    for name in used:
        u.setdefault(name, [])
        if variant not in u[name]:
            u[name].append(variant)
    for name, t in loc.items():
        lc.setdefault(name, list(t))


def dead_from_cache(cache):
    """DEAD = defined somewhere and used nowhere."""
    out = {}
    for name, dvariants in cache["defined"].items():
        if cache["used"].get(name):
            continue
        if name in cache["loc"]:
            out[name] = (cache["loc"][name], sorted(dvariants))
    return out


def run_build_all(nm, variants, cache_path, bat, include_tp, want_funcs):
    import os
    import subprocess
    if os.name != "nt":
        sys.exit("error: --build-all drives buildpicomite.bat (Windows); not supported here.")
    bat_path = bat if Path(bat).is_absolute() else str(fsg.PROJECT_ROOT / bat)
    if not Path(bat_path).is_file():
        sys.exit(f"error: build script not found: {bat_path}")
    cache = {"defined": {}, "used": {}, "loc": {}}
    built, failed = [], []
    for i, v in enumerate(variants, 1):
        print(f"\n========== [{i}/{len(variants)}] building {v} ==========", flush=True)
        r = subprocess.run(["cmd", "/c", bat_path, v], cwd=str(fsg.PROJECT_ROOT))
        if r.returncode != 0:
            print(f"  !! build FAILED for {v} (exit {r.returncode})", file=sys.stderr)
            failed.append(v)
            continue
        bd = fsg.find_dir_for_variant(v)
        if not bd:
            print(f"  !! cannot locate built objects for {v}", file=sys.stderr)
            failed.append(v)
            continue
        variant, defined, used, loc = scan_variant(nm, bd, include_tp, want_funcs)
        merge(cache, variant, defined, used, loc)
        Path(cache_path).write_text(json.dumps(cache, indent=1, sort_keys=True))
        print(f"  scanned {bd.name} variant={variant} defined={len(defined)} "
              f"used={len(used)} dead-so-far={len(dead_from_cache(cache))}")
    print(f"\nbuild-all done: built {len(built) or len(variants) - len(failed)}/{len(variants)}"
          + (f"; FAILED: {', '.join(failed)}" if failed else ""))
    if failed:
        print("WARNING: failed variants were not scanned; the DEAD list may be overstated.",
              file=sys.stderr)
    return cache


def report(dead, variants_seen, cache_note):
    headers = fsg.load_headers()
    by_file = defaultdict(list)
    for name, ((src, ln, t), dvariants) in sorted(dead.items()):
        externs = fsg.find_extern_decls(name, headers)
        by_file[src].append((ln or 0, name, fsg.kind_of(t), externs, dvariants))

    total = sum(len(v) for v in by_file.values())
    print()
    print(f"=== {total} global symbol(s) DEAD (never used) "
          f"across variants: {', '.join(variants_seen) or 'none'} ===")
    if cache_note:
        print(f"    (from cache {cache_note}; the more variants scanned, the safer this list)")
    print("    These are candidates for DELETION. A symbol is only listed if its section was")
    print("    linker-discarded in EVERY scanned variant that compiles its file.")
    print()
    for src in sorted(by_file, key=lambda s: fsg.rel(s)):
        items = sorted(by_file[src])
        print(f"{fsg.rel(src)}  ({len(items)})")
        for ln, name, kind, externs, dvariants in items:
            tag = f"  [also remove extern in {', '.join(externs)}]" if externs else ""
            where = "" if len(dvariants) > 1 else f"  (only built in {dvariants[0]})"
            print(f"    {kind:9s} {name:32s} line {ln or '?'}{where}{tag}")
        print()
    if total:
        print("Review carefully before deleting: a global only built in ONE variant, or used")
        print("only via inline asm / linker KEEP / a debugger, can be a false positive.")
    return 0


def main(argv):
    ap = argparse.ArgumentParser(description="Find globals that are never used (deletion candidates).")
    ap.add_argument("build_dirs", nargs="*",
                    help="build dirs to analyse (default: build/ buildRP2040L/ buildRP2350L/)")
    ap.add_argument("--functions", action="store_true", help="also report dead global functions")
    ap.add_argument("--include-third-party", action="store_true",
                    help="also consider third_party_mod/ sources (default: skip)")
    ap.add_argument("--accumulate", metavar="CACHE.json",
                    help="merge this run into a JSON cache and judge DEAD across all runs so far")
    ap.add_argument("--report-only", action="store_true",
                    help="with --accumulate: do not analyse, just report from the cache")
    ap.add_argument("--build-all", nargs="*", metavar="VARIANT", default=None,
                    help="build each variant (default: all bat-supported) and accumulate into a "
                         "fresh cache (--accumulate path or .dead_scan.json). Slow but trustworthy.")
    ap.add_argument("--bat", default="buildpicomite.bat", help="build script for --build-all")
    args = ap.parse_args(argv)

    nm = fsg.find_nm()

    if args.build_all is not None:
        variants = args.build_all if args.build_all else fsg.ALL_VARIANTS
        cache_path = args.accumulate or str(fsg.PROJECT_ROOT / DEFAULT_CACHE)
        cache = run_build_all(nm, variants, cache_path, args.bat,
                              args.include_third_party, args.functions)
        dead = dead_from_cache(cache)
        seen = sorted({v for vs in cache["defined"].values() for v in vs})
        return report(dead, seen, cache_path)

    if args.build_dirs:
        build_dirs = [Path(d) if Path(d).is_absolute() else (fsg.PROJECT_ROOT / d)
                      for d in args.build_dirs]
    else:
        build_dirs = [fsg.PROJECT_ROOT / n for n in ("build", "buildRP2040L", "buildRP2350L")
                      if (fsg.PROJECT_ROOT / n / fsg.OBJ_SUBDIR).is_dir()]

    cache = (json.loads(Path(args.accumulate).read_text())
             if args.accumulate and Path(args.accumulate).is_file()
             else {"defined": {}, "used": {}, "loc": {}})

    if not (args.accumulate and args.report_only):
        if not build_dirs:
            sys.exit("error: no build dirs with compiled objects found. Build a variant first.")
        for bd in build_dirs:
            if not (bd / fsg.OBJ_SUBDIR).is_dir():
                print(f"warning: {bd} has no objects - skipping", file=sys.stderr)
                continue
            if not (bd / MAP_NAME).is_file():
                print(f"warning: {bd} has no {MAP_NAME} (link it first) - skipping", file=sys.stderr)
                continue
            variant, defined, used, loc = scan_variant(nm, bd, args.include_third_party, args.functions)
            merge(cache, variant, defined, used, loc)
            print(f"analysed {bd.name:14s} variant={variant:14s} defined={len(defined):4d} "
                  f"used={len(used):4d}", file=sys.stderr)
        if args.accumulate:
            Path(args.accumulate).write_text(json.dumps(cache, indent=1, sort_keys=True))

    dead = dead_from_cache(cache)
    seen = sorted({v for vs in cache["defined"].values() for v in vs})
    return report(dead, seen, args.accumulate)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
