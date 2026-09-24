"""pcs_lines.py OUTPUT.txt PicoMite.elf FUNC [FUNC ...] [--top N]

Split the PC samples that fall inside the named functions by source line
(arm-none-eabi-addr2line against the exact ELF that was flashed), as a
share of all samples in the run.
"""
import collections, importlib.util, os, re, subprocess, sys

A2L = r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.3 rel1\bin\arm-none-eabi-addr2line.exe"
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("pcs_report", os.path.join(here, "pcs_report.py"))
rep = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rep)


def main():
    out_txt, elf = sys.argv[1], sys.argv[2]
    top = int(sys.argv[sys.argv.index("--top") + 1]) if "--top" in sys.argv else 20
    funcs = [a for a in sys.argv[3:] if not a.startswith("--") and not a.isdigit()]
    text = open(out_txt, encoding="utf-8", errors="replace").read()
    total = int(re.search(r"\[PCS\] samples=(\d+)", text).group(1))
    pcs = [(int(a, 16), int(n)) for a, n in re.findall(r"\[PCS\] ([0-9a-f]{8}) (\d+)", text)]
    syms = [s for s in rep.symbols(elf) if s[2] in funcs]
    for a, size, name in syms:
        mine = [(pc, n) for pc, n in pcs if a <= pc < a + size]
        if not mine:
            continue
        res = subprocess.run([A2L, "-e", elf] + ["%x" % pc for pc, _ in mine], capture_output=True, text=True).stdout.splitlines()
        byline = collections.Counter()
        for (pc, n), loc in zip(mine, res):
            m = re.search(r"([^/\\]+):(\d+)", loc)
            byline["%s:%s" % (m.group(1), m.group(2)) if m else loc] += n
        print("== %s  %.1f%% of all samples" % (name, 100.0 * sum(n for _, n in mine) / total))
        for k, n in byline.most_common(top):
            print("   %5.2f%%  %s" % (100.0 * n / total, k))


if __name__ == "__main__":
    main()
