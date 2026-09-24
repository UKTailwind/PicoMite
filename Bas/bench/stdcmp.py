"""stdcmp.py BASE.json NEW.json [--all] - compare two stdset.py runs.

Prints every figure that moved by more than 1% (all of them with --all),
every CHECK that changed, and the median change per test. Cycles and
milliseconds are both "lower is better".
"""
import json, statistics, sys

a = [x for x in sys.argv[1:] if not x.startswith("--")]
show_all = "--all" in sys.argv
base, new = (json.load(open(f)) for f in a[:2])
bm, nm = base["metrics"], new["metrics"]
print("base %s %s  new %s %s" % (base["board"], base["started"], new["board"], new["started"]))
if base.get("cpuspeed") != new.get("cpuspeed"):
    print("WARNING cpuspeed differs:", base.get("cpuspeed"), new.get("cpuspeed"))
bad = 0
for t in sorted(set(base["checks"]) | set(new["checks"])):
    if base["checks"].get(t) != new["checks"].get(t):
        bad += 1
        print("CHECK CHANGED %s\n  base %s\n  new  %s" % (t, base["checks"].get(t), new["checks"].get(t)))
for e in new.get("errors", []):
    print("ERROR in new run:", e)
pertest = {}
rows = []
for k in sorted(set(bm) & set(nm)):
    if not bm[k]:
        continue
    ch = 100.0 * (nm[k] - bm[k]) / abs(bm[k])
    pertest.setdefault(k.split(":")[0], []).append(ch)
    if show_all or abs(ch) > 1.0:
        rows.append((ch, k))
for ch, k in sorted(rows):
    print("%+7.2f%%  %-70s %12s -> %s" % (ch, k[:70], bm[k], nm[k]))
print("\nmedian change per test:")
for t, v in sorted(pertest.items()):
    print("  %-16s %+6.2f%%  (%d figures, worst %+.2f%%, best %+.2f%%)" % (t, statistics.median(v), len(v), max(v), min(v)))
missing = sorted(set(bm) ^ set(nm))
if missing:
    print("\nfigures in only one run:", len(missing))
print("\nchecks changed:", bad)
