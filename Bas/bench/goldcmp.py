"""goldcmp.py REFDIR NEWDIR - compare a goldens.py run with the committed reference.

Compares every <name>.out (and .out2) after the same normalisation goldens.py
uses (no CRs, no 'Time taken'), and lists the programs whose output changed,
appeared or vanished.  A step that is not meant to change behaviour must show
none (except where a program depends on files an earlier one wrote, e.g. rtest
reads the w1.bmp that wtest writes).
"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from goldens import norm  # noqa: E402

ref, new = sys.argv[1], sys.argv[2]


def outputs(d):
    return {f for f in os.listdir(d) if re.search(r"\.out2?$", f)}


r, n = outputs(ref), outputs(new)
changed = []
for f in sorted(r & n):
    a = norm(open(os.path.join(ref, f), encoding="utf-8", errors="replace").read())
    b = norm(open(os.path.join(new, f), encoding="utf-8", errors="replace").read())
    if a != b:
        changed.append(f)
print("compared %d outputs: %d changed, %d only in reference, %d only in new run"
      % (len(r & n), len(changed), len(r - n), len(n - r)))
for f in changed:
    print("  CHANGED", f)
for f in sorted(r - n):
    print("  MISSING", f)
for f in sorted(n - r):
    print("  NEW    ", f)
