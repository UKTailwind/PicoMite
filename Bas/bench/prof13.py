"""prof13.py PORT - OPTION PROFILING per-SUB times with interrupt SUBs and
ON GOSUB (bug report item 13).  Needs a build with per-SUB times (RP2350).

An interrupt SUB and an ON GOSUB open a level without opening a profiling
frame, but their END SUB / RETURN closes one, so they closed the frame of the
SUB that was running: its time stopped at the first tick or ON GOSUB, and the
interrupt SUB was never timed.  Work runs 1 s with a 50 ms SETTICK SUB firing
into it and Work2 runs 0.5 s after an ON GOSUB; both must be timed in full and
the tick SUB must appear with its calls."""
import sys, os, re, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

SRC = """Option Profiling On
SetTick 50, Tk
Work
SetTick 0, Tk
Work2
End
Sub Work
  Local t = Timer
  Do : Loop Until Timer - t > 1000
End Sub
Sub Work2
  Local t = Timer
  On 1 GoSub L1
  Do : Loop Until Timer - t > 500
End Sub
Sub Tk
  n = n + 1
End Sub
L1: Return
"""

b = pc3.PC3(sys.argv[1])
b.attention()
b.upload(SRC, 30)
b.drain(0.1)
out = b.run(30)
b.send_line("Option Profiling Off")
b.drain(0.5)
b.close()

times = {}
sect = False
for l in out.splitlines():
    if "top SUBs by exclusive" in l:
        sect = True
        continue
    if l.startswith("[PERF]"):
        sect = False
    m = re.match(r"\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\w+)", l)
    if sect and m:
        times[m.group(5).lower()] = (int(m.group(2)), int(m.group(3)))   # incl_us, calls
for name in ("work", "work2", "tk"):
    print("%-6s incl_us, calls = %s" % (name, times.get(name)))
ok = (times.get("work", (0, 0))[0] > 900000 and times.get("work2", (0, 0))[0] > 450000
      and times.get("tk", (0, 0))[1] >= 15)
print("PROF13", "PASS" if ok else "FAIL")
