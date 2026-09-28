"""rbcache.py PORT - Route B's bind cache across calls (option C, P6 F4): a
record's cached binds are stamped with the globals' generation and its own
level's locals' (Symbols.h), which a call's locals do not move, so the main
program and a loop in a SUB keep them across the calls they make.  Every program runs with OPTION COMPILE
OFF, ON and SHADOW and must print the same; a line starting TIME is left out
of the comparison and shown.  Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("main around calls", """Dim Integer i, x, y
Sub S3
  Local Integer a, b
  a = 1 : b = a + 1
  x = x + b
End Sub
For i = 1 To 200
  S3
  x = x + i * 2
  y = y + x Mod 7
Next
Print x; y
""", True),
    ("local hides a global mid-loop", """Dim Integer a = 100, r, i
Sub S
  Local Integer j
  For j = 1 To 3
    r = r + a
    If j = 1 Then Local Integer a
    a = a + 5
  Next
End Sub
For i = 1 To 3 : S : Next
Print r; a
""", True),
    ("gosub from a sub into main code", """Dim Integer g = 1, t, i
Sub S
  Local Integer g
  g = 50
  GoSub Lbl
End Sub
For i = 1 To 4
  S
  GoSub Lbl
Next
Print t
End
Lbl:
  t = t + g
  Return
""", None),  # (END comes before the label, so the STAT line never runs)
    ("erase and dim between calls", """Dim Integer i, s
Dim Float v = 1.5
Sub T
  Local Integer k
  k = 2
End Sub
For i = 1 To 6
  T
  s = s + v
  If i = 3 Then Erase v : Dim Integer v = 7
Next
Print s; v
""", True),
    ("option default between calls", """Dim Integer i, n
Sub U
  Local Integer k
  k = 1
End Sub
For i = 1 To 4
  U
  n = n + i
  If i = 2 Then Option Default Integer
Next
Print n
""", True),
    ("sub loop around calls", """Dim Integer r
Sub Inner(v As Integer)
  Local Integer w, x
  w = v * 2 : x = w + 1
  r = r + x
End Sub
Sub Outer
  Local Integer i, a, b
  Timer = 0
  For i = 1 To 3000
    Inner i
    a = a + i : b = b + a Mod 7
  Next
  Print a; b; r
  Print "TIME "; Timer
End Sub
Outer
Outer
""", True),
    ("a callee's local hides a caller's global", """Dim Integer g = 5
Sub C1
  Local Integer g
  g = 100
End Sub
Sub A
  Local Integer i, s
  For i = 1 To 5
    s = s + g
    C1
    s = s + g
  Next
  Print s; g
End Sub
A
""", True),
    ("a local made mid-loop after calls", """Dim Integer a = 100, r, i
Sub C1
  Local Integer a
  a = 1
End Sub
Sub S
  Local Integer j
  For j = 1 To 3
    r = r + a
    C1
    If j = 2 Then Local Integer a : a = 7
    r = r + a
  Next
End Sub
For i = 1 To 2 : S : Next
Print r; a
""", True),
    ("interrupt locals during a sub loop", """Dim Integer ticks
Sub Tk
  Local Integer i, s
  i = 5 : s = 9
  ticks = ticks + 1
End Sub
Sub L
  Local Integer i, s
  For i = 1 To 20000
    s = s + i Mod 3
  Next
  Print s
End Sub
SetTick 1, Tk
L
SetTick 0, Tk
Print ticks > 0
""", True),
    ("a gosub's local in a sub", """Dim Integer t, x = 3
Sub S
  Local Integer x, k
  x = 10
  For k = 1 To 4
    GoSub Lb
    t = t + x
  Next
End Sub
S
Print t; x
End
Lb:
  Local Integer x
  x = 1000
  t = t + x
  Return
""", None),
    ("speed", """Dim Integer i, x, y, z
Sub S3
  Local Integer a
  a = 1
End Sub
Timer = 0
For i = 1 To 5000
  S3
  x = x + i : y = y + x Mod 7 : z = z + (y And 3)
Next
Print x; y; z
Print "TIME "; Timer
""", True),
]

b = pc3.PC3(sys.argv[1])
b.attention()
ok = True


def run(src):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120)).replace("\r", "")
    lines = [l for l in out.split("\n") if l.strip() and l.strip() not in ("RUN", ">")]
    stat = [l for l in lines if l.startswith("STAT ")]
    times = [l for l in lines if l.startswith("TIME ")]
    body = [l for l in lines if not l.startswith("STAT ") and not l.startswith("TIME ")]
    return body, (stat[0][5:] if stat else ""), times


for name, src, want_code in PROGS:
    outs = {}
    for mode in ("OFF", "ON", "SHADOW"):
        b.cmd("OPTION COMPILE " + mode, 10)
        outs[mode] = run(src + STAT)
    same = outs["OFF"][0] == outs["ON"][0] == outs["SHADOW"][0]
    m = re.search(r"CODE (\d+)", outs["ON"][1])
    code = int(m.group(1)) if m else -1
    good = same and (want_code is None or code > 0)
    ok = ok and good
    print("%-4s %-32s CODE %-6d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-80:]))
    if outs["OFF"][2]:
        print("     time OFF %s  ON %s" % (outs["OFF"][2][0][5:].strip(), outs["ON"][2][0][5:].strip() if outs["ON"][2] else "?"))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))
b.cmd("OPTION COMPILE OFF", 10)
print("RBCACHE", "PASS" if ok else "FAIL")
b.close()
