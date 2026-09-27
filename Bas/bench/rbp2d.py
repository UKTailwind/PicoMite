"""rbp2d.py PORT - Route B P2d: compiled FOR (docs/Interpreter_RouteB_Design.html).

FOR does cmd_for's stack work and evaluation in its order, with its NEXT
found at compile time by cmd_for's own scan (ForFindNext).  Every program
runs with OPTION COMPILE OFF, ON and SHADOW and must print the same.  Also
K1 of the G3 kernels at program level, timed.  Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("int float step", """Dim Integer i, s
Dim Float x, t
For i = 1 To 10 : s = s + i : Next
For x = 0 To 1 Step 0.25 : t = t + x : Next
For i = 10 To 1 Step -3 : s = s + i : Next
Print s; t; i; x
""" + STAT, lambda n: n > 0),
    ("zero trips", """Dim Integer i, s = 7
For i = 5 To 1 : s = s + 100 : Next
For i = 1 To 0 : s = s + 100 : Next
Print s; i
""" + STAT, None),
    ("nested", """Dim Integer i, j, s
For i = 1 To 20
  For j = i To 20
    s = s + j
  Next j
Next i
Print s; i; j
""" + STAT, lambda n: n > 0),
    ("reentry", """Dim Integer i, j, s
For j = 1 To 5
  For i = 1 To 100
    If i = 3 Then GoTo skip1
  Next i
skip1:
  s = s + i
Next j
Print s
""" + STAT, lambda n: n > 0),
    ("exit for multi", """Dim Integer i, j, s
For i = 1 To 5
  For j = 1 To 5
    s = s + 1
  Next j, i
For i = 1 To 50
  If i = 7 Then Exit For
  s = s + i
Next
Print s; i; j
""" + STAT, lambda n: n > 0),
    ("limit uses var", """Dim Integer i, s
i = 100
For i = 1 To i + 3
  s = s + i
Next
Print s; i
""" + STAT, lambda n: n > 0),
    ("too many", """Dim Integer a, b, c, d, e, f, g, h, k, l, m
Dim Integer n, o, q, r, t, u, v, w, y, z
For a = 1 To 1 : For b = 1 To 1 : For c = 1 To 1 : For d = 1 To 1 : For e = 1 To 1
For f = 1 To 1 : For g = 1 To 1 : For h = 1 To 1 : For k = 1 To 1 : For l = 1 To 1
For m = 1 To 1 : For n = 1 To 1 : For o = 1 To 1 : For q = 1 To 1 : For r = 1 To 1
For t = 1 To 1 : For u = 1 To 1 : For v = 1 To 1 : For w = 1 To 1 : For y = 1 To 1
For z = 1 To 1
Print "deep"
Next : Next : Next : Next : Next : Next : Next : Next : Next : Next : Next
Next : Next : Next : Next : Next : Next : Next : Next : Next : Next
""" + STAT, None),
    ("no next", """Dim Integer i
For i = 1 To 3
Print "x"
""" + STAT, None),
    # DO and LOOP (P2d, second part)
    ("do forms", """Dim Integer i, s, t, u, v
Do While i < 10 : s = s + i : i = i + 1 : Loop
i = 0
Do Until i >= 5 : t = t + i : i = i + 1 : Loop
i = 0
Do : u = u + i : i = i + 1 : Loop While i < 7
i = 0
Do : v = v + 1 : i = i + 1 : Loop Until i = 4
i = 0
Do
  i = i + 1
  If i = 6 Then Exit Do
Loop
Print s; t; u; v; i
""" + STAT, lambda n: n > 0),
    ("do nested skip", """Dim Integer i, j, s
Do While i < 5
  j = 0
  Do While j < i
    s = s + j
    j = j + 1
  Loop
  i = i + 1
Loop
Do While s < 0 : s = -1 : Loop
Print s; i; j
""" + STAT, lambda n: n > 0),
    ("do float cond", """Dim Float x
Dim Integer n
x = 1
Do While x < 100.5
  x = x * 1.7
  n = n + 1
Loop
Print x; n
""" + STAT, lambda n: n > 0),
    # the bind cache: a variable erased and made again (another type, then
    # the first type in a new slot) must never be read through a stale address
    ("cache erase", """Dim Integer i
Dim Float a = 1.5, b
For i = 1 To 6
  b = a * 2
  Print b;
  If i = 2 Then Erase a : Dim Integer a = 7
  If i = 4 Then Erase a : Dim Float a = 0.25
  S
Next
Print
Sub S
  Local q
  q = 1
End Sub
""" + STAT, None),
    ("loop has while", """Dim Integer i
Do While i < 3
  i = i + 1
Loop While i < 5
""" + STAT, None),
]

b = pc3.PC3(sys.argv[1])
b.attention()
ok = True


def run(src):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(60)).replace("\r", "")
    lines = [l for l in out.split("\n") if l.strip() and l.strip() not in ("RUN", ">")]
    stat = [l for l in lines if l.startswith("STAT ")]
    body = [l for l in lines if not l.startswith("STAT ")]
    return body, (stat[0][5:] if stat else "")


for name, src, want_code in PROGS:
    outs = {}
    for mode in ("OFF", "ON", "SHADOW"):
        b.cmd("OPTION COMPILE " + mode, 10)
        outs[mode] = run(src)
    same = outs["OFF"][0] == outs["ON"][0] == outs["SHADOW"][0]
    m = re.search(r"CODE (\d+)", outs["ON"][1])
    code = int(m.group(1)) if m else -1
    good = same and (want_code is None or want_code(code))
    ok = ok and good
    print("%-4s %-15s CODE %-5d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-90:]))
    if not same:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))

# K1 of the G3 kernels at program level: an integer loop with a single-line IF
src = """Dim Integer n = 200000, s, i
Dim Float t
t = Timer
s = 0
For i = 1 To n
  s = s + (i And 7) * 3
  If s > 1000000 Then s = s - 1000000
Next i
Print "K1"; s; Timer - t
""" + STAT
res = {}
for mode in ("OFF", "ON"):
    b.cmd("OPTION COMPILE " + mode, 10)
    body, stat = run(src)
    m = re.search(r"K1 *(-?\d+) +([\d.]+)", " ".join(body))
    res[mode] = float(m.group(2)) if m else None
    print("K1 %-3s %s  %s" % (mode, " ".join(body)[-40:], stat))
if res.get("OFF") and res.get("ON"):
    print("K1 speedup %.2fx" % (res["OFF"] / res["ON"]))

# K3 of the G3 kernels (Julia, no PIXEL) at program level: FOR, DO WHILE, floats
src = """Dim Float w = 40, h = 30, xd, yd, rOfs, iOfs, cRe, cIm, mit, ck
Dim Float X, Y, CX, CY, Zr, Zi, COUNT, new_Zr, new_Zi, t
xd = 3.0 / w : yd = 2.0 / h : rOfs = -2.0 : iOfs = -1.0
cRe = -0.7 : cIm = 0.27015 : mit = 60
t = Timer
For X = 0 To (w - 1)
  CX = X * xd + rOfs
  For Y = 0 To (h - 1)
    CY = Y * yd + iOfs
    Zr = CX
    Zi = CY
    COUNT = 0
    Do While ((COUNT <= mit) And ((Zr * Zr + Zi * Zi) < 4))
      new_Zr = Zr * Zr - Zi * Zi + cRe
      new_Zi = 2 * Zr * Zi + cIm
      Zr = new_Zr
      Zi = new_Zi
      COUNT = COUNT + 1
    Loop
    ck = ck + COUNT
  Next Y
Next X
Print "K3"; ck; Timer - t
""" + STAT
res = {}
for mode in ("OFF", "ON", "SHADOW"):
    b.cmd("OPTION COMPILE " + mode, 10)
    body, stat = run(src)
    m = re.search(r"K3 *(-?[\d.]+) +([\d.]+)", " ".join(body))
    res[mode] = (m.group(1), float(m.group(2))) if m else None
    print("K3 %-6s %s  %s" % (mode, " ".join(body)[-40:], stat))
if res.get("OFF") and res.get("ON"):
    ok = ok and res["OFF"][0] == res["ON"][0] == (res["SHADOW"] or ("?",))[0]
    print("K3 speedup %.2fx" % (res["OFF"][1] / res["ON"][1]))
b.cmd("OPTION COMPILE OFF", 10)
print("RBP2D", "PASS" if ok else "FAIL")
b.close()
