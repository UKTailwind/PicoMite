"""rbp5a.py PORT - Route B P5a: the pure numeric built-in functions (SIN,
COS, TAN, ATN, SQR, EXP, LOG, DEG, RAD, INT, FIX, ABS, SGN, PI, and RND,
whose values the programs do not print) compiled
(docs/Interpreter_RouteB_Design.html).  Every program runs with OPTION
COMPILE OFF, ON and SHADOW and must print the same, errors included; a line
starting TIME is left out of the comparison and shown.  Leaves OPTION
COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("trig", """Dim Float x, a, b, c, d
Dim Integer i
For i = -8 To 8
  x = i / 3
  a = a + Sin(x) : b = b + Cos(x) : c = c + Atn(x)
  If Abs(Cos(x)) > 0.01 Then d = d + Tan(x)
  a = a + Sin(i) + Cos(i)
Next
Print a; b; c; d
""", True),
    ("sqr exp log deg rad", """Dim Float x, a, b, c, d, e
Dim Integer i
For i = 1 To 20
  x = i * 0.7
  a = a + Sqr(x) + Sqr(i)
  b = b + Exp(x / 10) + Exp(i / 10)
  c = c + Log(x) + Log(i)
  d = d + Deg(x) + Deg(i)
  e = e + Rad(x) + Rad(i)
Next
Print a; b; c; d; e
""", True),
    ("int fix", """Dim Float x
Dim Integer i, a, b, c, d
For i = -10 To 10
  x = i / 4
  a = a + Int(x) : b = b + Fix(x)
  c = c + Int(i) + Fix(i)
  d = d + Int(x * 1000.5)
Next
Print a; b; c; d; Int(-2.5); Fix(-2.5); Int(2.999999)
""", True),
    ("abs sgn types", """Dim Float x, fa
Dim Integer i, ia, s
For i = -5 To 5
  x = i * 1.5
  fa = fa + Abs(x)
  ia = ia + Abs(i)
  s = s + Sgn(x) * 10 + Sgn(i)
Next
Print fa; ia; s; Abs(-7); Abs(-7.5); Sgn(-0.0); Sgn(0)
""", True),
    ("pi", """Dim Float r, a
Dim Integer i
For i = 1 To 10
  r = i / 2
  a = a + Pi * r * r
Next
Print a; Pi
""", True),
    ("nested", """Dim Float x, t
Dim Integer i
For i = 1 To 12
  x = i / 5
  t = t + Sin(Cos(x)) + Sqr(Abs(Sin(x))) + Int(Exp(x)) + Atn(Tan(x / 2))
Next
Print t
""", True),
    ("option angle degrees", """Option Angle Degrees
Dim Float a, b, c, d
Dim Integer i
For i = 0 To 360 Step 15
  a = a + Sin(i) : b = b + Cos(i)
  If i <> 90 And i <> 270 Then c = c + Tan(i)
  d = d + Atn(i / 100) + Sin(i + 0.5)
Next
Print a; b; c; d
""", True),
    ("sqr negative", """Dim Float x = 4
Dim Integer i
For i = 1 To 3
  Print Sqr(x)
  x = x - 5
Next
""", None),
    ("log zero", """Dim Float x = 2
Dim Integer i
For i = 1 To 3
  Print Log(x)
  x = x - 1
Next
""", None),
    ("int too large", """Dim Float x = 1E17
Dim Integer i, n
For i = 1 To 4
  n = Int(x)
  Print n
  x = x * 1000
Next
""", None),
    ("second argument", """Dim Float a
Dim Integer i
For i = 1 To 3
  a = a + Sin(i, 2)
Next
Print a
""", None),
    ("string argument", """Dim Float a
Print "go"
a = Sin("x")
""", None),
    ("user function inside", """Function Half(v As Float) As Float
  Half = v / 2
End Function
Dim Float a
Dim Integer i
For i = 1 To 5
  a = a + Sin(Half(i))
Next
Print a
""", True),
    ("anchor line", """Dim Float xnut(11, 20), t = 0.37, arg, dpsi, deps
Dim Integer i, j
For i = 0 To 20
  For j = 0 To 11 : xnut(j, i) = (i + 1) * (j - 5) / 7 : Next
Next
For i = 0 To 20
  arg = xnut(1, i) * 0.3 + xnut(2, i) * 0.7
  dpsi = (xnut(6, i) + xnut(7, i) * t) * Sin(arg) + xnut(10, i) * Cos(arg) + dpsi
  deps = (xnut(8, i) + xnut(9, i) * t) * Cos(arg) + xnut(11, i) * Sin(arg) + deps
Next
Print dpsi; deps
""", True),
    ("rnd", """Dim Float r, lo = 1, hi = 0
Dim Integer i, n, k
For i = 1 To 2000
  r = Rnd
  If r < lo Then lo = r
  If r > hi Then hi = r
  If Rnd >= 0 And Rnd < 1 Then Inc n
  Inc k, Int(Rnd(7) * 0)
Next
Do While Rnd < 2
  Inc k
  If k > 20 Then Exit Do
Loop
Do
  Inc k
Loop Until Rnd >= 0
Print lo >= 0; hi < 1; lo < 0.05; hi > 0.95; n; k
""", True),
    ("speed", """Dim Float x, s
Dim Integer i
Timer = 0
For i = 1 To 20000
  x = i / 1000
  s = s + Sin(x) * Cos(x) + Sqr(x)
Next
Print s
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
    print("%-4s %-22s CODE %-6d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-90:]))
    if outs["OFF"][2]:
        print("     time OFF %s  ON %s" % (outs["OFF"][2][0][5:].strip(), outs["ON"][2][0][5:].strip() if outs["ON"][2] else "?"))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))
b.cmd("OPTION COMPILE OFF", 10)
print("RBP5A", "PASS" if ok else "FAIL")
b.close()
