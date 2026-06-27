#!/usr/bin/env python3
"""find_static_globals.py - find file-scope globals that can be marked `static`.

Goal: reduce global-symbol bloat by finding global variables (and optionally
functions) that are defined in one translation unit and never referenced from
any other one, so they can be given internal (`static`) linkage.

WHY OBJECT FILES, NOT GREP
--------------------------
This works on the *compiled object files* of a build (via arm-none-eabi-nm),
not on the source text. That makes it:

  * Build-aware by construction - conditional compilation
    (#ifdef PICOMITEVGA / PICOMITEWEB / HDMI / ...) is handled exactly as the
    compiler saw it. A reference that was #ifdef'd out is simply not present in
    the object, so it is not counted.
  * Accurate - a symbol is "referenced externally" iff some *other* object lists
    it as undefined (nm type `U`). No regex guessing about C declarations,
    macros, arrays or function pointers.

A global is a STATIC CANDIDATE for a build when:
  * it is a data symbol  - D (data) / B (bss) / R (rodata) / C (common)
    (or a function with --functions: T / W),
  * it has external/global linkage (nm uses an UPPERCASE type letter; lowercase
    means it is already static/local),
  * it is defined in exactly one of the project's own object files, and
  * no other object file references it (no `U` for it anywhere).

CROSS-VARIANT SAFETY (important)
--------------------------------
A symbol that looks local-only in one variant may be referenced from a file
that is only compiled in a *different* variant (e.g. net/ files exist only in
WEB builds). Marking such a symbol static would break that other variant.

A candidate is therefore only reported as **SAFE** if it is a candidate in
*every* analysed variant in which its source file is compiled. Because the
build system reuses one object directory per group (only the last-built
variant's objects survive in buildRP2040L / buildRP2350L), the reliable way to
cover many variants is the accumulate cache:

    # build a variant, then point the script at its object dir, accumulating:
    python tools/find_static_globals.py --accumulate .static_scan.json build
    # ...rebuild another variant into `build`, run again with the same cache...
    # finally, list only the symbols safe across everything seen so far:
    python tools/find_static_globals.py --accumulate .static_scan.json --report-only

Without --accumulate it analyses whatever build dirs are present right now
(auto-detect: build/, buildRP2040L/, buildRP2350L/) and unions just those.

USAGE
-----
  python tools/find_static_globals.py                 # auto-detect build dirs
  python tools/find_static_globals.py buildRP2040L buildRP2350L
  python tools/find_static_globals.py --functions     # also report functions
  python tools/find_static_globals.py --include-third-party
  python tools/find_static_globals.py --accumulate .static_scan.json [dirs...]
  python tools/find_static_globals.py --apply [dirs...]   # edit sources (SAFE only)

--apply only adds `static ` to the definition line. Re-build afterwards and
review `git diff`: if a candidate also had an `extern` declaration in a header
(the report flags these) you must delete that declaration too, or the compiler
will reject "static declaration follows non-static".
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
OBJ_SUBDIR = Path("CMakeFiles") / "PicoMite.dir"

# nm type letters. UPPERCASE = external/global linkage; lowercase = local/static.
VAR_TYPES = set("DBRCGSV")   # data / bss / rodata / common / small-data / weak-object
FUNC_TYPES = set("TW")       # text (function) / weak

# `<value> <type> <name>` then an optional tab/space-separated `file:line` (nm -l).
NM_LINE = re.compile(
    r"^(?P<val>[0-9a-fA-F]*)\s+(?P<type>[A-Za-z?-])\s+(?P<name>\S+)(?:[ \t]+(?P<loc>\S.*))?$"
)


def find_nm() -> str:
    p = shutil.which("arm-none-eabi-nm")
    if p:
        return p
    gcc = shutil.which("arm-none-eabi-gcc")
    if gcc:
        exe = "arm-none-eabi-nm" + (".exe" if os.name == "nt" else "")
        cand = Path(gcc).with_name(exe)
        if cand.exists():
            return str(cand)
    sys.exit("error: arm-none-eabi-nm not found on PATH (build the firmware first / "
             "add the toolchain bin dir to PATH).")


def norm(path: str) -> str:
    """Normalise a (possibly mixed-separator, Windows) path to forward slashes."""
    return path.strip().replace("\\", "/")


def is_sdk(path: str) -> bool:
    p = path.lower()
    return "pico-sdk" in p or "/_deps/" in p


def is_third_party(path: str) -> bool:
    return "third_party_mod/" in norm(path)


def split_loc(loc: str | None):
    """`D:\\dir/file.c:123` -> ('D:/dir/file.c', 123). Drive colon survives because
    we split on the LAST colon."""
    if not loc:
        return None, None
    head, sep, tail = loc.rpartition(":")
    if sep and tail.isdigit():
        return norm(head), int(tail)
    return norm(loc), None


def object_files(build_dir: Path):
    base = build_dir / OBJ_SUBDIR
    return sorted(base.rglob("*.obj")) if base.is_dir() else []


def obj_to_source(obj: Path, build_dir: Path) -> str:
    rel = obj.relative_to(build_dir / OBJ_SUBDIR).as_posix()
    return rel[:-4] if rel.endswith(".obj") else rel


def variant_of(build_dir: Path) -> str:
    f = build_dir / "build_limits.txt"
    if f.is_file():
        for line in f.read_text(errors="ignore").splitlines():
            if line.strip().startswith("COMPILE="):
                return line.split("=", 1)[1].strip()
    return build_dir.name


def parse_obj(nm: str, obj: Path):
    """Return (defined, undef): defined = [(type, name, srcfile, line)],
    undef = set(names referenced but not defined here)."""
    res = subprocess.run([nm, "-l", str(obj)], capture_output=True, text=True)
    defined, undef = [], set()
    for line in res.stdout.splitlines():
        m = NM_LINE.match(line)
        if not m:
            continue
        t, name, loc = m.group("type"), m.group("name"), m.group("loc")
        if t == "U":
            undef.add(name)
        else:
            src, ln = split_loc(loc)
            defined.append((t, name, src, ln))
    return defined, undef


def classify_build(nm: str, build_dir: Path, include_tp: bool, want_funcs: bool):
    """Analyse one build dir. Returns (variant, cands, defined_proj, external, nobjs).

    cands:        name -> (srcfile, line, type)  candidates in this variant
    defined_proj: set of project-defined global names (our files)
    external:     names defined by us but referenced elsewhere (or ambiguous)
    """
    wanted = set(VAR_TYPES) | (set(FUNC_TYPES) if want_funcs else set())
    defs = defaultdict(list)   # name -> [(objrel, type, src, line)]
    used = defaultdict(set)    # name -> {objrel referencing it}
    objs = object_files(build_dir)
    for obj in objs:
        objrel = obj_to_source(obj, build_dir)
        defined, undef = parse_obj(nm, obj)
        for name in undef:
            used[name].add(objrel)
        for t, name, src, ln in defined:
            if t not in wanted:           # lowercase (static) & unwanted kinds dropped here
                continue
            defs[name].append((objrel, t, src or objrel, ln))

    cands, defined_proj, external = {}, set(), set()
    for name, deflist in defs.items():
        proj = [d for d in deflist if not is_sdk(d[2]) and not is_sdk(d[0])]
        if not include_tp:
            proj = [d for d in proj if not is_third_party(d[2]) and not is_third_party(d[0])]
        if not proj:
            continue
        defined_proj.add(name)
        if len(proj) != 1:                # multiple project definitions -> never safe
            external.add(name)
            continue
        if used.get(name):                # referenced by some other object
            external.add(name)
            continue
        objrel, t, src, ln = proj[0]
        cands[name] = (src, ln, t)
    return variant_of(build_dir), cands, defined_proj, external, len(objs)


def kind_of(type_letter: str) -> str:
    return {
        "D": "data", "B": "bss", "R": "rodata", "C": "common",
        "G": "data", "S": "bss", "V": "weak-data", "T": "func", "W": "func",
    }.get(type_letter.upper(), type_letter)


# ---- accumulate cache -------------------------------------------------------
def load_cache(path: Path):
    if path.is_file():
        return json.loads(path.read_text())
    return {"defined": {}, "external": {}, "loc": {}}


def merge_into_cache(cache, variant, cands, defined_proj, external):
    d, e, loc = cache["defined"], cache["external"], cache["loc"]
    for name in defined_proj:
        d.setdefault(name, [])
        if variant not in d[name]:
            d[name].append(variant)
    for name in external:
        e.setdefault(name, [])
        if variant not in e[name]:
            e[name].append(variant)
    for name, (src, ln, t) in cands.items():
        loc.setdefault(name, [src, ln, t])   # first sighting wins; good enough for editing


def safe_from_tables(defined, external, loc):
    """A name is SAFE iff it is defined by us somewhere and never external."""
    out = {}
    for name, dvariants in defined.items():
        if name in external and external[name]:
            continue
        if name not in loc:
            continue
        out[name] = (loc[name], sorted(dvariants))
    return out


def main(argv):
    ap = argparse.ArgumentParser(description="Find globals that can be made static.")
    ap.add_argument("build_dirs", nargs="*",
                    help="build dirs to analyse (default: auto-detect build/ buildRP2040L/ buildRP2350L/)")
    ap.add_argument("--functions", action="store_true",
                    help="also report unreferenced global functions, not just variables")
    ap.add_argument("--include-third-party", action="store_true",
                    help="also consider third_party_mod/ sources (default: skip vendored libs)")
    ap.add_argument("--accumulate", metavar="CACHE.json",
                    help="merge this run into a JSON cache and judge SAFE across all runs so far")
    ap.add_argument("--report-only", action="store_true",
                    help="with --accumulate: do not analyse, just report from the existing cache")
    ap.add_argument("--apply", action="store_true",
                    help="edit sources to add `static` to SAFE candidates (review git diff after!)")
    args = ap.parse_args(argv)

    nm = find_nm()

    # Pick build dirs.
    if args.build_dirs:
        build_dirs = [Path(d) if Path(d).is_absolute() else (PROJECT_ROOT / d)
                      for d in args.build_dirs]
    else:
        build_dirs = [PROJECT_ROOT / n for n in ("build", "buildRP2040L", "buildRP2350L")
                      if (PROJECT_ROOT / n / OBJ_SUBDIR).is_dir()]

    cache = load_cache(Path(args.accumulate)) if args.accumulate else {"defined": {}, "external": {}, "loc": {}}

    analysed = []
    if not (args.accumulate and args.report_only):
        if not build_dirs:
            sys.exit("error: no build dirs with compiled objects found. Build a variant first "
                     "(e.g. buildpicomite.bat PICORP2350), then re-run.")
        for bd in build_dirs:
            if not (bd / OBJ_SUBDIR).is_dir():
                print(f"warning: {bd} has no {OBJ_SUBDIR} - skipping", file=sys.stderr)
                continue
            variant, cands, dproj, ext, nobjs = classify_build(
                nm, bd, args.include_third_party, args.functions)
            analysed.append((variant, cands, dproj, ext))
            print(f"analysed {bd.name:14s} variant={variant:14s} "
                  f"objects={nobjs:3d} candidates={len(cands)}", file=sys.stderr)
            if args.accumulate:
                merge_into_cache(cache, variant, cands, dproj, ext)

    # Build the SAFE table either from the cache (accumulate) or from this run's union.
    if args.accumulate:
        Path(args.accumulate).write_text(json.dumps(cache, indent=1, sort_keys=True))
        safe = safe_from_tables(cache["defined"], cache["external"], cache["loc"])
        variants_seen = sorted({v for vs in cache["defined"].values() for v in vs})
    else:
        defined, external, loc = {}, {}, {}
        for variant, cands, dproj, ext in analysed:
            for name in dproj:
                defined.setdefault(name, []).append(variant)
            for name in ext:
                external.setdefault(name, []).append(variant)
            for name, (src, ln, t) in cands.items():
                loc.setdefault(name, [src, ln, t])
        safe = safe_from_tables(defined, external, loc)
        variants_seen = sorted({v for v, *_ in analysed})

    # Flag candidates that also have an `extern` declaration in a header.
    headers = load_headers()

    # Group SAFE candidates by source file.
    by_file = defaultdict(list)
    for name, ((src, ln, t), dvariants) in sorted(safe.items()):
        externs = find_extern_decls(name, headers)
        by_file[src].append((ln or 0, name, kind_of(t), externs, dvariants))

    total = sum(len(v) for v in by_file.values())
    print()
    print(f"=== {total} global symbol(s) safe to mark static "
          f"(across variants: {', '.join(variants_seen) or 'none'}) ===")
    if args.accumulate:
        print(f"    (judged from accumulate cache {args.accumulate}; "
              f"the more variants you build+scan into it, the safer this list)")
    print("    NOTE: a candidate is only safe if EVERY variant that compiles its file was scanned.")
    print()
    for src in sorted(by_file, key=lambda s: rel(s)):
        items = sorted(by_file[src])
        print(f"{rel(src)}  ({len(items)})")
        for ln, name, kind, externs, dvariants in items:
            tag = f"  [extern in {', '.join(externs)} - remove it too]" if externs else ""
            where = "" if len(dvariants) > 1 else f"  (only in {dvariants[0]})"
            print(f"    {kind:9s} {name:32s} line {ln or '?'}{where}{tag}")
        print()

    if args.apply:
        apply_static(by_file)
    elif total:
        print("Re-run with --apply to add `static` to these definitions "
              "(then rebuild and review `git diff`).")
    return 0


def rel(path: str) -> str:
    try:
        return Path(norm(path)).resolve().relative_to(PROJECT_ROOT).as_posix()
    except Exception:
        return norm(path)


def load_headers():
    skip = ("/build/", "/buildrp2040l/", "/buildrp2350l/", "/_tmp/",
            "/pico-sdk/", "/.venv", "/_deps/")
    texts = {}
    for h in PROJECT_ROOT.rglob("*.h"):
        s = norm(str(h)).lower()
        if any(x in s for x in skip):
            continue
        try:
            texts[h] = h.read_text(errors="ignore")
        except OSError:
            pass
    return texts


def find_extern_decls(name, headers):
    pat = re.compile(r"\bextern\b[^;{}]*\b" + re.escape(name) + r"\b")
    hits = []
    for h, txt in headers.items():
        if name in txt and pat.search(txt):
            hits.append(h.relative_to(PROJECT_ROOT).as_posix())
    return hits


def apply_static(by_file):
    print("=== applying `static` (definition lines only) ===")
    applied = skipped = 0
    for src, items in by_file.items():
        p = Path(norm(src))
        if not p.is_absolute():
            p = PROJECT_ROOT / norm(src)
        if not p.is_file():
            print(f"  skip (file not found): {rel(src)}")
            skipped += len(items)
            continue
        lines = p.read_text(errors="ignore").splitlines(keepends=True)
        changed = False
        for ln, name, kind, externs, dvariants in sorted(items, reverse=True):  # bottom-up
            if not ln or ln > len(lines):
                print(f"  skip (no line): {rel(src)} {name}")
                skipped += 1
                continue
            text = lines[ln - 1]
            stripped = text.lstrip()
            indent = text[: len(text) - len(stripped)]
            if re.match(r"(static|extern|typedef)\b", stripped):
                print(f"  skip (already {stripped.split()[0]}): {rel(src)}:{ln} {name}")
                skipped += 1
                continue
            if not re.search(r"\b" + re.escape(name) + r"\b", text):
                print(f"  skip (name not on line - macro/multiline?): {rel(src)}:{ln} {name}")
                skipped += 1
                continue
            lines[ln - 1] = indent + "static " + stripped
            changed = True
            applied += 1
            note = f"  (also remove extern in {', '.join(externs)})" if externs else ""
            print(f"  static  {rel(src)}:{ln} {name}{note}")
        if changed:
            p.write_text("".join(lines))
    print(f"\napplied {applied}, skipped {skipped}. Rebuild and review `git diff`; "
          f"delete any now-redundant `extern` declarations flagged above.")


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
