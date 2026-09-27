"""rbcall.py PORT - the cost of a SUB call and a FUNCTION call, text against
compiled (Route B P3).  Each loop runs N calls; the time per iteration is
printed for OPTION COMPILE OFF and ON, with the empty loop as the baseline.
Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

N = 20000
LOOPS = [
    ("empty loop", """Dim Integer i, s, r
Dim Float t
t = Timer
For i = 1 To %d
  s = s + i
Next
Print "T"; Timer - t; s
""" % N),
    ("SUB, 3 args", """Sub Add3(a%%, b%%, c%%)
  c%% = a%% + b%%
End Sub
Dim Integer i, s, r
Dim Float t
t = Timer
For i = 1 To %d
  Add3 i, 5, r
  s = s + r
Next
Print "T"; Timer - t; s
""" % N),
    ("SUB, no args", """Sub Bump
  s = s + 1
End Sub
Dim Integer i, s
Dim Float t
t = Timer
For i = 1 To %d
  Bump
Next
Print "T"; Timer - t; s
""" % N),
    ("SUB, 2 locals", """Sub Twice(a%%)
  Local Integer x, y
  x = a%% * 2 : y = x + 1
  s = s + y
End Sub
Dim Integer i, s
Dim Float t
t = Timer
For i = 1 To %d
  Twice i
Next
Print "T"; Timer - t; s
""" % N),
    ("FUNCTION, 2 args", """Function F2(a%%, b%%) As Integer
  F2 = a%% * b%% + 1
End Function
Dim Integer i, s
Dim Float t
t = Timer
For i = 1 To %d
  s = s + F2(i, 3)
Next
Print "T"; Timer - t; s
""" % N),
]

b = pc3.PC3(sys.argv[1])
b.attention()


def run(src):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(300)).replace("\r", "")
    m = re.search(r"T *([\d.]+) +(-?\d+)", out)
    return (float(m.group(1)), m.group(2)) if m else (None, out[-100:])


base = {}
for name, src in LOOPS:
    res = {}
    for mode in ("OFF", "ON"):
        b.cmd("OPTION COMPILE " + mode, 10)
        res[mode] = run(src)
    base[name] = res
    off, on = res["OFF"], res["ON"]
    if off[0] is None or on[0] is None:
        print("%-18s BAD %s | %s" % (name, off, on))
        continue
    print("%-18s OFF %7.2f us/iter  ON %7.2f us/iter  %.2fx  %s" %
          (name, off[0] * 1000 / N, on[0] * 1000 / N, off[0] / on[0], "" if off[1] == on[1] else "RESULTS DIFFER"))
b.cmd("OPTION COMPILE OFF", 10)
b.close()
