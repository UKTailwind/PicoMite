"""rbp3d.py PORT - Route B P3d: compiled calls to user FUNCTIONs in
expressions (docs/Interpreter_RouteB_Design.html).

A FUNCTION called in a LET's right-hand side, an IF's condition or an INC's
value compiles, any number of calls a statement, with the variables used after
a call bound again if it erased one; an untyped FUNCTION as OPTION DEFAULT's
type in text order, which a guard at the statement's start checks.
Every program runs with OPTION COMPILE OFF, ON and SHADOW and must print the
same, the count of calls made included: SHADOW must never make a call twice.
A want of ("fb", n) also asks that at most n statements ran as text under ON
(RAN - CODE), so the calling statement itself compiled.  Leaves OPTION
COMPILE OFF."""
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
Dim Integer r, calls, k = 3, i
For i = 1 To 50 : r = Sq(3) + Sq(i) : Next
Print r; calls
r = Sq(2) + k : Print r; calls
If Sq(2) + Sq(3) = 13 Then r = Sq(k) Else r = -1
Print r; calls
""", ("fb", 20)),
    ("four calls", """Function FNS(U,V)
  FNS=(U*E+V*F)-Int(U*E+V*F)
End Function
E=Sqr(3):F=Sqr(7)
For X=0 To 40
  C=Int(X/7): D=Int(X/5)
  G=G+(FNS(C,D)+FNS(C+1,D)+FNS(C,D+1)+FNS(C+1,D+1))
Next
Print G
""", ("fb", 20)),
    ("inc calls", """Dim Integer g, i, A(5)
Dim Float f
Function Tw(n As Integer) As Integer
  Tw = n * 2
End Function
Function Un(n)
  Un = n / 3
End Function
For i = 1 To 5
  Inc g, Tw(i)
  Inc A(i), Tw(i) + Tw(1)
  Inc f, Un(i)
Next
Print g; A(3); f
""", ("fb", 20)),
    ("inc changed", """Dim Integer g = 1, A(3), i
Function Bump() As Integer
  g = g + 100
  A(2) = A(2) + 10
  Bump = 1
End Function
For i = 1 To 3
  Inc g, Bump()
  Inc A(2), Bump()
Next
Print g; A(2)
""", ("fb", 20)),
    ("rebind", """Dim Integer a = 5, b = 7, r, i
Function Zap(n As Integer) As Integer
  Dim junk(3)
  Erase junk
  Zap = n * 2
End Function
For i = 1 To 20
  r = Zap(i) + a + Zap(b) + b
Next
Print r
""", ("fb", 90)),
    ("untyped fun", """Function U(n)
  U = n * 1.5
End Function
Dim Float r
Dim Integer i
For i = 1 To 100 : r = r + U(i) : Next
Print r
r = U(4) : Print r
""", ("fb", 20)),
    ("untyped int", """OPTION DEFAULT INTEGER
Function U(n)
  U = n * 3 / 2
End Function
Dim r, i
For i = 1 To 100 : r = r + U(i) : Next
Print r
""", ("fb", 20)),
    ("default moves", """Function U(n)
  U = n / 4
End Function
Dim Float r
Dim Integer i
For i = 1 To 4
  r = U(i) : Print r;
  If i = 2 Then Execute "OPTION DEFAULT INTEGER"
Next
Print
""", None),
    ("default none", """OPTION DEFAULT NONE
Function U(n As Integer)
  U = n
End Function
Dim Integer r
r = U(2) : Print r
""", None),
    ("fns form", """Function FNS(U,V)
  Q=(U*E+V*F)-Int(U*E+V*F)
  T=1-(A-U)*(A-U)-(B-V)*(B-V)
  If T<=0 Then FNS=0 Else FNS=(3-2*T)*T*T*Q
End Function
E=Sqr(3):F=Sqr(7)
For X=0 To 9
  A=X/7: B=X/5: C=Int(A): D=Int(B)
  G=G+FNS(C,D)
  Z=FNS(C+1,D)
  If FNS(C,D+1)>0.5 Then
    G=G+1
  EndIf
Next
Print G;Z
""", ("fb", 20)),
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
    m = re.search(r"RAN (\d+)", outs["ON"][1])
    fb = int(m.group(1)) - code if m else 1 << 30
    if isinstance(want_code, tuple):
        good = same and fb <= want_code[1]
    else:
        good = same and (want_code is None or code > 0)
    ok = ok and good
    print("%-4s %-14s CODE %-6d TEXT %-5d %s" % ("ok" if good else "BAD", name, code, fb, " | ".join(outs["ON"][0])[-80:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))
# a FUNCTION that erases a variable its caller's statement reads after the
# call: the text evaluator makes the variable again, the compiled statement
# cannot and says so (the one known difference; see RBRebind).  r = v first:
# a statement's first run binds by text a variable only DIM has made
# (Zap's DIM and ERASE run as text in every call: 80 of "rebind"'s text)
src = """Dim Integer v = 3, r
Function Wipe() As Integer
  Erase v
  Wipe = 1
End Function
r = v
r = Wipe() + v
Print r
"""
outs = {}
for mode in ("OFF", "ON"):
    b.cmd("OPTION COMPILE " + mode, 10)
    outs[mode] = run(src + STAT)
good = outs["OFF"][0][:1] == [" 1"] and any("A FUNCTION changed a variable" in l for l in outs["ON"][0])
ok = ok and good
print("%-4s %-14s OFF %s | ON %s" % ("ok" if good else "BAD", "erase used", outs["OFF"][0][:1], outs["ON"][0][:1]))
b.cmd("OPTION COMPILE OFF", 10)
print("RBP3D", "PASS" if ok else "FAIL")
b.close()
