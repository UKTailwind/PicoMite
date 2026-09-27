"""rbp3a.py PORT - Route B P3a: compiled statements on locals and parameters
(docs/Interpreter_RouteB_Design.html).

Inside a SUB or FUNCTION a name compiles with the type it has there: its
LOCAL, STATIC or CONST, its parameter (suffix, AS or OPTION DEFAULT's type),
a FUNCTION's own name; a bind takes the local at this level as findvar does,
and a BYREF parameter or a STATIC binds to the data it points to.  Every
program runs with OPTION COMPILE OFF, ON and SHADOW and must print the same.
Then the G3 kernels K1 and K3 as written (in SUBs), timed.  Leaves OPTION
COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("local types", """Dim Integer gi = 5
Dim Float gf = 2.5
Sub T1
  Local Integer a, b
  Local Float x
  Local c%
  Local y!
  a = 3 : b = a * gi + 1
  x = gf * 2 + a
  c% = b Mod 7
  y! = x / 4
  Print a; b; x; c%; y!
End Sub
T1 : T1
Print gi; gf
""", True),
    ("parameters", """Dim Integer n = 10, s, ai = 1
Dim Float bf = 1.5, cf = 2, fx = 2.7
Sub P(n%, s%)
  Local i%
  s% = 0
  For i% = 1 To n%
    s% = s% + i%
  Next
End Sub
Sub Q(a As Integer, b As Float, c)
  a = a + 1 : b = b * 2 : c = c + 0.5
End Sub
P n, s : Print s
P 5 + 1, s : Print s
Q ai, bf, cf : Print ai; bf; cf
Q ai, 3.5, cf : Print ai; cf
Q ai + 0, bf, 7 : Print ai; bf
Q fx, bf, cf : Print fx; bf; cf
""", True),
    ("shadowing", """Dim Float x = 1.25
Dim Integer y = 7
Sub S1(x%)
  Local y!
  y! = x% * 1.5
  x% = x% + 1
  Print x%; y!
End Sub
Sub S2
  x = x + 1
  y = y * 2
  Print x; y
End Sub
S1 4 : Print x; y
S2 : S1 x : S2
""", True),
    ("recursion", """Function Fact(n As Integer) As Integer
  If n <= 1 Then Fact = 1 Else Fact = n * Fact(n - 1)
End Function
Function SumTo(n%) As Integer
  Local i%, t%
  For i% = 1 To n% : t% = t% + i% : Next
  SumTo = t%
End Function
Function Rec(d%) As Integer
  Local v%
  v% = d% * 10
  If d% > 0 Then v% = v% + Rec(d% - 1)
  Rec = v%
End Function
Print Fact(8); SumTo(100); Rec(5)
""", True),
    ("static const", """Sub Counter
  Static Integer cnt
  Const K = 3
  cnt = cnt + K
  Print cnt;
End Sub
Counter : Counter : Counter : Print
""", True),
    ("exit loops", """Dim Integer total
Sub L(m%)
  Local i%, j%
  For i% = 1 To m%
    j% = 0
    Do While j% < i%
      j% = j% + 1
      If j% = 3 Then Exit Do
    Loop
    total = total + j%
    If i% = 7 Then Exit Sub
  Next
End Sub
L 10 : L 2 : Print total
""", True),
    ("default integer", """Option Default Integer
Sub D(a, b)
  Local c
  c = a / b
  Print c; a * b; a \\ b
End Sub
D 7, 2 : D 9, 4
""", True),
    ("implicit", """Sub Imp
  q = 5 : q = q + 1 : Print q
End Sub
Imp : Imp
""", None),
    ("type copy", """Sub TC(v)
  Local w
  w = v * 2
  v = v + 1
  Print w; v
End Sub
Dim Integer iv = 4
Dim Float fv = 2.5
TC iv : TC fv : TC 7 : Print iv; fv
""", True),
    ("byref chain", """Dim Integer acc
Sub Inner(a%)
  a% = a% + 1
End Sub
Sub Outer(b%)
  Local k%
  For k% = 1 To 5 : Inner b% : Next
  b% = b% * 10
End Sub
Outer acc : Print acc
""", True),
    ("local vs global", """Dim Float z = 0.5
Sub Zl
  Local z As Integer
  z = 3 : z = z * 2 : Print z
End Sub
Zl : z = z * 4 : Print z : Zl
""", True),
    ("function result", """Function Poly(x)
  Poly = x * x + 2 * x + 1
End Function
Function Tri%(n%)
  Local i%
  For i% = 1 To n% : Tri% = Tri% + i% : Next
End Function
Print Poly(3); Poly(0.5); Tri%(10)
""", True),
    ("explicit", """Option Explicit
Dim Integer g1 = 2
Sub E(p1 As Integer)
  Local Integer l1 = 4
  Local Float l2
  l2 = p1 * l1 + g1
  g1 = g1 + p1
  Print l2; g1
End Sub
E 3 : E 5
""", True),
]

K1 = """Sub k1(n%, s%)
  Local i%
  s% = 0
  For i% = 1 To n%
    s% = s% + (i% And 7) * 3
    If s% > 1000000 Then s% = s% - 1000000
  Next i%
End Sub
Dim Integer s
Dim Float t
t = Timer
k1 200000, s
Print "K"; s; Timer - t
"""
K3 = """Sub k3(w, h, xd, yd, rOfs, iOfs, cRe, cIm, mit, ck)
  Local X, Y, CX, CY, Zr, Zi, COUNT, new_Zr, new_Zi
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
End Sub
Dim Float ck, t
t = Timer
k3 40, 30, 3 / 40, 2 / 30, -2, -1, -0.7, 0.27015, 60, ck
Print "K"; ck; Timer - t
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
    bad = [l for l in outs["OFF"][0] + outs["ON"][0] if "rror" in l]
    m = re.search(r"CODE (\d+)", outs["ON"][1])
    code = int(m.group(1)) if m else -1
    good = same and not bad and (want_code is None or code > 0)
    ok = ok and good
    print("%-4s %-16s CODE %-6d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-80:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))

for kname, src in (("K1", K1), ("K3", K3)):
    res = {}
    for mode in ("OFF", "ON", "SHADOW"):
        b.cmd("OPTION COMPILE " + mode, 10)
        body, stat = run(src + STAT)
        m = re.search(r"K *(-?[\d.]+) +([\d.]+)", " ".join(body))
        res[mode] = (m.group(1), float(m.group(2))) if m else None
        print("%s %-6s %s  %s" % (kname, mode, " ".join(body)[-40:], stat))
    if res.get("OFF") and res.get("ON") and res.get("SHADOW"):
        ok = ok and res["OFF"][0] == res["ON"][0] == res["SHADOW"][0]
        print("%s in a SUB: %.2fx" % (kname, res["OFF"][1] / res["ON"][1]))
    else:
        ok = False
b.cmd("OPTION COMPILE OFF", 10)
print("RBP3A", "PASS" if ok else "FAIL")
b.close()
