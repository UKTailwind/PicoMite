"""pcs_src.py OUTPUT.txt PicoMite.elf FUNCTION [--top N] - where inside one
function the PC samples fall: each sampled address resolved to its source line
with addr2line, summed per line, busiest first.  The companion of
pcs_report.py, for a function that report shows is hot."""
import re, subprocess, sys, collections

BIN = r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.3 rel1\bin"
text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
elf, fn = sys.argv[2], sys.argv[3]
top = int(sys.argv[sys.argv.index("--top") + 1]) if "--top" in sys.argv else 30
pcs = [(int(a, 16), int(n)) for a, n in re.findall(r"\[PCS\] ([0-9a-f]{8}) (\d+)", text)]
total = sum(n for _, n in pcs)
nm = subprocess.run([BIN + r"\arm-none-eabi-nm.exe", "-S", "--defined-only", elf], capture_output=True, text=True).stdout
lo = hi = None
for l in nm.splitlines():
    p = l.split()
    if len(p) == 4 and p[3] == fn:
        lo = int(p[0], 16)
        hi = lo + int(p[1], 16)
        break
if lo is None:
    sys.exit("no symbol " + fn)
mine = [(a, n) for a, n in pcs if lo <= a < hi]
out = subprocess.run([BIN + r"\arm-none-eabi-addr2line.exe", "-e", elf] + ["%x" % a for a, _ in mine],
                     capture_output=True, text=True).stdout.splitlines()
per = collections.Counter()
for (a, n), where in zip(mine, out):
    per[where.split("\\")[-1].split("/")[-1].split(" ")[0]] += n
sub = sum(n for _, n in mine)
print("%s: %d of %d samples (%.1f%%)" % (fn, sub, total, 100.0 * sub / max(total, 1)))
for where, n in per.most_common(top):
    print("  %5.1f%%  %6d  %s" % (100.0 * n / max(total, 1), n, where))
