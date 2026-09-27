"""rbp4a.py PORT - Route B P4a: numeric array elements
(docs/Interpreter_RouteB_Design.html).

An element of an integer or float array compiles, read or assigned, with
findvar's conversions, checks and errors in findvar's order.  Every program
runs with OPTION COMPILE OFF, ON and SHADOW and must print the same, errors
included.  Then the G3 kernels K2 (insertion sort) and K4 (findleap) as
written, timed.  Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("one dim", """Dim Integer a%(20), i, s
Dim Float f(20), g
For i = 0 To 20
  a%(i) = i * i
  f(i) = i / 4
Next
For i = 0 To 20
  s = s + a%(i)
  g = g + f(i) * 2
Next
Print s; g; a%(7); f(3)
""", True),
    ("base 1", """Option Base 1
Dim Integer a(5), i, s
For i = 1 To 5 : a(i) = i * 10 : Next
For i = 1 To 5 : s = s + a(i) : Next
Print s; a(1); a(5)
""", True),
    ("two three dim", """Dim Integer m(3, 4), c(2, 2, 2), i, j, k, s
For i = 0 To 3
  For j = 0 To 4
    m(i, j) = i * 10 + j
  Next
Next
For i = 0 To 2 : For j = 0 To 2 : For k = 0 To 2
  c(i, j, k) = m(i, j) + k * 100
Next : Next : Next
For i = 0 To 2 : s = s + c(i, 1, 2) + m(3, i) : Next
Print s; m(2, 3); c(2, 2, 1)
""", True),
    ("float index", """Dim Integer a(10), i
For i = 0 To 10 : a(i) = i : Next
Dim Float x = 2.6, y = 4.4
Print a(x); a(y); a(x + y); a(1.5); a(2.5)
a(x) = 99 : Print a(3)
""", True),
    ("out of bounds", """Dim Integer a(5), i
For i = 0 To 5 : a(i) = i : Next
i = 6
Print a(i)
""", None),
    ("write oob", """Dim Integer a(5), i = 6
a(i) = 1
""", None),
    ("below base", """Dim Integer a(5), i = -1
Print a(i)
""", None),
    ("index order", """Dim Integer m(5, 5), i = -1, z = 0
Print m(i, 1 \\ z)
""", None),
    ("wrong count", """Dim Integer a(5), i = 1
Print a(i, i)
""", None),
    ("rhs error", """Dim Integer a(5), i = 7, z = 0
a(i) = 1 \\ z
""", None),
    ("local arrays", """Sub S(n As Integer)
  Local Integer v(10), i, t
  For i = 0 To n : v(i) = i * 3 : Next
  For i = 0 To n : t = t + v(i) : Next
  Print t;
End Sub
S 5 : S 10 : Print
""", True),
    ("array param", """Dim Integer a(10), i
Sub FillUp(x%(), n%)
  Local i%
  For i% = 0 To n% : x%(i%) = i% * 2 : Next
End Sub
Function Sum%(x%(), n%)
  Local i%
  For i% = 0 To n% : Sum% = Sum% + x%(i%) : Next
End Function
FillUp a(), 10
Print Sum%(a(), 10); a(4)
""", True),
    ("byref element", """Dim Integer a(5)
Sub Inc(x%)
  x% = x% + 1
End Sub
a(3) = 10
Inc a(3) : Inc a(3)
Print a(3)
""", None),  # (its one element assignment runs once, before the DIM's variable is bound)
    ("conditions", """Dim Integer a(10), i, c
For i = 0 To 10 : a(i) = i Mod 3 : Next
For i = 0 To 10
  If a(i) = 2 Then c = c + 1
Next
i = 0
Do While a(i) < 2 : i = i + 1 : Loop
Print c; i
For i = a(1) To a(2) + 3 : c = c + 1 : Next
Print c
""", True),
    ("erase redim", """Dim Integer i, s
Dim Integer a(3)
For i = 1 To 4
  a(1) = i
  s = s + a(1)
  If i = 2 Then Erase a : Dim Float a(5)
Next
Print s; a(1)
""", True),
    ("string array", """Dim s$(3), i
For i = 0 To 3 : s$(i) = Str$(i) + "x" : Next
Print s$(2); Len(s$(3))
""", None),
]

K2 = """Sub k2(a%(), n%)
  Local i%, j%, v%
  For i% = 1 To n% - 1
    v% = a%(i%)
    j% = i% - 1
    Do While j% >= 0
      If a%(j%) <= v% Then Exit Do
      a%(j% + 1) = a%(j%)
      j% = j% - 1
    Loop
    a%(j% + 1) = v%
  Next i%
End Sub
Dim a%(299), N%, x%, i%, ck%
Dim Float t
N% = 300 : x% = 12345
For i% = 0 To N% - 1 : x% = (x% * 1103515245 + 12345) And &H7FFFFFFF : a%(i%) = x% Mod 100000 : Next i%
t = Timer
k2 a%(), N%
t = Timer - t
For i% = 1 To N% - 1 : If a%(i%) < a%(i% - 1) Then ck% = -1
Next : If ck% = 0 Then ck% = a%(0) + a%(150) * 3 + a%(299) * 7
Print "K"; ck%; t
"""
K4 = """Dim jdleap(28), leapsec(28), TOT, t
Dim Integer i
For i = 1 To 28 : Read jdleap(i), leapsec(i) : Next i
Sub findleap(jday, leapsecond)
  Local i As integer
  If (jday <= jdleap(1)) Then
    leapsecond = leapsec(1)
    Exit Sub
  End If
  If (jday >= jdleap(28)) Then
    leapsecond = leapsec(28)
    Exit Sub
  End If
  For i = 1 To 27
    If (jday >= jdleap(i) And jday < jdleap(i + 1)) Then
      leapsecond = leapsec(i)
      Exit Sub
    End If
  Next i
End Sub
Sub k4(n%, tot)
  Local k%, jd, ls
  tot = 0
  For k% = 1 To n%
    jd = 2441000 + (k% Mod 17000)
    findleap jd, ls
    tot = tot + ls
  Next k%
End Sub
t = Timer
k4 4000, TOT
Print "K"; TOT; Timer - t
Data 2441317.5, 10, 2441499.5, 11, 2441683.5, 12, 2442048.5, 13, 2442413.5, 14
Data 2442778.5, 15, 2443144.5, 16, 2443509.5, 17, 2443874.5, 18, 2444239.5, 19
Data 2444786.5, 20, 2445151.5, 21, 2445516.5, 22, 2446247.5, 23, 2447161.5, 24
Data 2447892.5, 25, 2448257.5, 26, 2448804.5, 27, 2449169.5, 28, 2449534.5, 29
Data 2450083.5, 30, 2450630.5, 31, 2451179.5, 32, 2453736.5, 33, 2454832.5, 34
Data 2456109.5, 35, 2457204.5, 36, 2457754.5, 37
"""

b = pc3.PC3(sys.argv[1])
b.attention()
ok = True


def run(src):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120)).replace("\r", "")
    lines = [l for l in out.split("\n") if l.strip() and l.strip() not in ("RUN", ">")]
    stat = [l for l in lines if l.startswith("STAT ")]
    body = [l for l in lines if not l.startswith("STAT ")]
    return body, (stat[0][5:] if stat else "")


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
    print("%-4s %-16s CODE %-6d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-90:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))

for kname, src in (("K2", K2), ("K4", K4)):
    res = {}
    for mode in ("OFF", "ON", "SHADOW"):
        b.cmd("OPTION COMPILE " + mode, 10)
        body, stat = run(src + STAT)
        m = re.search(r"K *(-?[\d.]+) +([\d.]+)", " ".join(body))
        res[mode] = (m.group(1), float(m.group(2))) if m else None
        print("%s %-6s %s  %s" % (kname, mode, " ".join(body)[-50:], stat))
    if res.get("OFF") and res.get("ON") and res.get("SHADOW"):
        ok = ok and res["OFF"][0] == res["ON"][0] == res["SHADOW"][0]
        print("%s in a SUB: %.2fx" % (kname, res["OFF"][1] / res["ON"][1]))
    else:
        ok = False
b.cmd("OPTION COMPILE OFF", 10)
print("RBP4A", "PASS" if ok else "FAIL")
b.close()
