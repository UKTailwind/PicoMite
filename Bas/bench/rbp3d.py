"""rbp3d.py PORT - Route B P3d: compiled calls to user FUNCTIONs in
expressions (docs/Interpreter_RouteB_Design.html).

A typed FUNCTION called in a LET's right-hand side or an IF's condition
compiles (one call a statement, no variable used after it but the LET's
target).  Every program runs with OPTION COMPILE OFF, ON and SHADOW and must
print the same, the count of calls made included: SHADOW must never make a
call twice.  Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("loop", """Function F2(a%, b%) As Integer
  F2 = a% * b% + 1
  calls = calls + 1
End Function
Dim Integer i, s, calls
For i = 1 To 200
  s = s + F2(i, 3)
Next
Print s; calls
""", True),
    ("result types", """Function Fi%(a%)
  Fi% = a% * 2
End Function
Function Ff!(x!)
  Ff! = x! / 4
End Function
Function Fa(x As Float) As Integer
  Fa = x * 10
End Function
Function Fb(n As Integer) As Float
  Fb = n / 3
End Function
Dim Integer r
Dim Float f
r = Fi%(21) : Print r
f = Ff!(3) : Print f
r = Fa(1.26) : Print r
f = Fb(2) : Print f
f = Fi%(5) + 0.5 : Print f
r = Ff!(10) : Print r
""", True),
    ("byref args", """Function Bump(v%) As Integer
  v% = v% + 10
  Bump = v%
End Function
Function Conv(x As Integer) As Integer
  x = x + 1
  Conv = x
End Function
Dim Integer a = 5, r
Dim Float fl = 2.6
r = Bump(a) : Print a; r
r = Bump(a + 1) : Print a; r
r = Conv(fl) : Print fl; r
""", True),
    ("if condition", """Function Big(n As Integer) As Integer
  Big = n > 5
End Function
Dim Integer i, c
For i = 1 To 10
  If Big(i) Then
    c = c + 1
  EndIf
Next
Print c
If c > 2 Then c = Big(9) * 100
Print c
""", True),
    ("recursion", """Function D(n As Integer) As Integer
  If n <= 0 Then D = 0 Else D = 1 + D(n - 1)
End Function
Function Fact(n As Integer) As Integer
  If n <= 1 Then Fact = 1 Else Fact = n * Fact(n - 1)
End Function
Print D(8); Fact(10)
""", True),
    ("reads before", """Dim Integer g = 5, r
Function Incg() As Integer
  g = g + 100
  Incg = 1
End Function
g = g + Incg() : Print g
r = g * 2 + Incg() : Print r; g
""", True),
    ("two calls", """Function Sq(n As Integer) As Integer
  Sq = n * n
  calls = calls + 1
End Function
Dim Integer r, calls, k = 3
r = Sq(3) + Sq(4) : Print r; calls
r = Sq(2) + k : Print r; calls
""", None),
    ("untyped fun", """Function U(n)
  U = n * 1.5
End Function
Dim Float r
r = U(4) : Print r
""", None),
    ("nested calls", """Function Inner(n As Integer) As Integer
  Inner = n + 1
End Function
Function Outer(n As Integer) As Integer
  Local Integer t
  t = Inner(n) * 2
  Outer = t + Inner(t)
End Function
Dim Integer r
r = Outer(5) : Print r
""", True),
    ("no args", """Dim Integer tick
Function Nxt() As Integer
  tick = tick + 1
  Nxt = tick * 10
End Function
Dim Integer r, i
For i = 1 To 5 : r = r + Nxt() : Next
Print r; tick
""", True),
    ("static exit", """Function Cnt() As Integer
  Static Integer n
  n = n + 1
  If n > 3 Then
    Cnt = -1
    Exit Function
  EndIf
  Cnt = n
End Function
Dim Integer r, i
For i = 1 To 5 : r = Cnt() : Print r; : Next
Print
""", True),
    ("local caller", """Function Dbl(v%) As Integer
  v% = v% * 2
  Dbl = v%
End Function
Sub Work
  Local Integer a = 3, b
  b = Dbl(a)
  Print a; b
End Sub
Work
""", True),
    ("error inside", """Function Bad(n As Integer) As Integer
  Bad = 10 \\ n
End Function
Dim Integer r
r = Bad(2) : Print r
r = Bad(0)
Print "not reached"
""", None),
    ("shadow once", """Dim Integer made
Function Side(n As Integer) As Float
  made = made + 1
  Side = n / 2
End Function
Dim Float f
Dim Integer i
For i = 1 To 7
  f = f + Side(i)
  If Side(i) > 2 Then f = f + 1
Next
Print f; made
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
    print("%-4s %-14s CODE %-6d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-90:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))
b.cmd("OPTION COMPILE OFF", 10)
print("RBP3D", "PASS" if ok else "FAIL")
b.close()
