"""Pair meshview.bas AUTOMODE output with the engine's DIAGNOSE verdicts.

Reads the captured console text (file argument, or stdin) and reports, per ship
and orientation, every polygon where the engine's visibility disagrees with the
blueprint normal's prediction, ignoring near edge-on faces.  Also tabulates the
TIME lines.

Usage:  python meshcheck.py capture.txt
"""
import re, sys

EDGE_ON = 0.02   # |expected dot| below this fraction of |v0| * |n| is edge-on: ignore

text = open(sys.argv[1], encoding="latin-1").read() if len(sys.argv) > 1 else sys.stdin.read()
lines = [l.strip() for l in text.splitlines()]

ships = {}
exp = {}        # (ship, orient) -> {poly: dot}
diag = {}       # (ship, orient) -> {poly: (dot, verdict)}
times = []
cur = None
for l in lines:
    m = re.match(r"SHIP\s+(\d+)\s+(.*?)\s+nv\s+(\d+)\s+nf\s+(\d+)\s+nf0\s+(\d+)\s+size\s+(\d+)", l)
    if m:
        ships[int(m.group(1))] = m.group(2)
        continue
    m = re.match(r"EXP\s+(\d+)\s+(\d+)\s+(\d+)\s+(-?[\d.eE+-]+)", l)
    if m:
        exp.setdefault((int(m.group(1)), int(m.group(2))), {})[int(m.group(3))] = float(m.group(4))
        continue
    m = re.match(r"DIAG\s+(\d+)\s+(\d+)", l)
    if m:
        cur = (int(m.group(1)), int(m.group(2)))
        diag[cur] = {}
        continue
    m = re.match(r"Face\s+(\d+)\s+at distance\s+(-?[\d.eE+-]+)\s+dot product is\s+(-?[\d.eE+-]+)\s+so the face is\s+(Hidden|Showing)", l)
    if m and cur is not None:
        diag[cur][int(m.group(1))] = (float(m.group(3)), m.group(4))
        continue
    m = re.match(r"TIME\s+(\d+)\s+(\d+)\s+(\d+)\s+(-?[\d.]+)", l)
    if m:
        times.append((int(m.group(1)), int(m.group(2)), int(m.group(3)), float(m.group(4))))

# polygon kinds from ships.json, so sliver lines (whose normal is forced perpendicular
# to the line and so may not match the host face) are reported separately from real faces
import json, os
kinds = {}
try:
    js = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite", "data", "ships.json")))
    order = ["SIDEWINDER", "VIPER", "MAMBA", "PYTHON", "COBRA_MK_3", "THARGOID", "CORIOLIS", "MISSILE",
             "ASTEROID", "CANISTER", "THARGON", "ESCAPE_POD"]
    for i, n in enumerate(order):
        kinds[i] = [p["kind"] for p in js[n]["polygons"]]
except Exception as ex:
    print("(ships.json not read: %s)" % ex)

total_bad, total_line = 0, 0
print("%-12s %5s %5s %8s %6s %s" % ("ship", "orient", "polys", "faceBAD", "sliver", "details"))
for key in sorted(exp):
    e, d = exp[key], diag.get(key, {})
    bad, linebad, skipped, missing = [], [], 0, 0
    for p, ed in sorted(e.items()):
        if p not in d:
            missing += 1
            continue
        dd, verdict = d[p]
        exp_show = ed < 0
        if abs(ed) < 1e-6 or abs(dd) < 1e-3:
            skipped += 1
            continue
        if exp_show != (verdict == "Showing"):
            item = "p%d exp %.0f eng %.3f %s" % (p, ed, dd, verdict)
            if kinds.get(key[0], [None] * 999)[p] == "line":
                linebad.append(item)
            else:
                bad.append(item)
    total_bad += len(bad)
    total_line += len(linebad)
    det = "; ".join((bad + linebad)[:6]) + (" ..." if len(bad + linebad) > 6 else "")
    print("%-12s %5d %5d %8d %6d %s" % (ships.get(key[0], "?"), key[1], len(e), len(bad), len(linebad),
                                         det + (" (%d edge-on skipped)" % skipped if skipped else "") + (" (%d MISSING from DIAG)" % missing if missing else "")))
print("\nTOTAL real-face mismatches: %d, sliver-line deviations: %d, over %d ship/orientation sets" % (total_bad, total_line, len(exp)))

if times:
    print("\n%-12s %5s %6s %6s %6s" % ("ship", "mode", "near", "mid", "far"))
    by = {}
    for s, mode, z, ms in times:
        by.setdefault((s, mode), []).append((z, ms))
    for (s, mode) in sorted(by):
        row = sorted(by[(s, mode)])
        print("%-12s %5s %6.2f %6.2f %6.2f   (z %s)" % (ships.get(s, "?"), "wire" if mode == 0 else "solid",
                                                       row[0][1], row[1][1], row[2][1], "/".join(str(z) for z, _ in row)))
