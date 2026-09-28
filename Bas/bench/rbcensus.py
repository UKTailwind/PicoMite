"""rbcensus.py OUT.txt [OUT.txt ...] - the fallback census of a game run under
OPTION COMPILE ON with its profiling twin (gbelitep.bas and the rest, which
turn OPTION PROFILING on): [PERF] counts only statements that ran as text,
so its "top commands by dispatch count" are the fallbacks by command, and
"user_subs" the SUB calls made by DefinedSubFun and RC_CALL together.  With
the STAT line (MM.INFO(COMPILE)), RAN is every statement the stream ran and
CODE the compiled ones.  Run each game with
  gamebench.py PORT run gbelitep.bas --runs 1 --label census_elite
after OPTION COMPILE ON, and add what Print MM.Info(COMPILE) says after it
to the output file."""
import re, sys

for fn in sys.argv[1:]:
    text = open(fn, encoding="utf-8", errors="replace").read().replace("\r", "")
    m = re.search(r"\[PERF\] elapsed=(\d+) us\s+statements=(\d+).*?user_subs=(\d+)", text)
    print("==", fn)
    if not m:
        print("   no [PERF] report")
        continue
    total, subs = int(m.group(2)), int(m.group(3))
    print("   text statements %d (of which SUB calls %d)" % (total, subs))
    st = re.search(r"(?:COMPILED|NONE) \d+ REUSED \d+ STMTS \d+ RAN (\d+) MISS (\d+) CODE (\d+)", text)
    if st:
        ran, miss, code = int(st.group(1)), int(st.group(2)), int(st.group(3))
        print("   stream ran %d, compiled %d (%.1f%%), map misses %d" % (ran, code, 100.0 * code / max(ran, 1), miss))
    top = text.split("[PERF] top commands by dispatch count:")[1] if "[PERF] top commands" in text else ""
    rows = re.findall(r"^\s+(\d+)\s+(\S.*?)\s*$", top.split("[PERF] top SUBs")[0], re.M)
    for n, name in rows:
        print("   %10s  %5.1f%%  %s" % (n, 100.0 * int(n) / max(total, 1), name))
