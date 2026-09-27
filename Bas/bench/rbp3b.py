"""rbp3b.py PORT - Route B P3b: compiled calls to user SUBs
(docs/Interpreter_RouteB_Design.html).

A call whose arguments are expressions and variables compiles: RC_CALL does
what DefinedSubFun does for a SUB, in its order.  Every program runs with
OPTION COMPILE OFF, ON and SHADOW and must print the same, errors included;
the ones marked compile must show compiled statements.  Leaves OPTION COMPILE
OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("args", """Dim Integer a = 3, b = 4, r
Dim Float f = 1.5, g
Sub Add3(x%, y%, z%)
  z% = x% + y%
End Sub
Sub AddF(x!, y!, z!)
  z! = x! * y!
End Sub
Add3 a, b, r : Print r
Add3 a * 2, b + 1, r : Print r
Add3 7, 8, r : Print r
AddF f, 2, g : Print g
AddF f, a, g : Print g
""", True),
    ("typed as", """Dim Integer n = 5, m
Dim Float x = 2.5, y
Sub T(p As Integer, q As Float, o As Integer, u As Float)
  o = p * 2 : u = q * 3
End Sub
T n, x, m, y : Print m; y
T x, n, m, y : Print m; y; n; x
""", True),
    ("untyped", """Dim Float x = 1.25, y
Dim Integer i = 3
Sub U(a, b, c)
  c = a + b
End Sub
U x, 2, y : Print y
U i, x, y : Print y; i
""", True),
    ("default integer", """Option Default Integer
Dim p = 7, q
Sub U(a, b, c)
  c = a * b
  a = a + 1
End Sub
U p, 3, q : Print p; q
U 2.6, 2, q : Print q
""", True),
    ("copy not ref", """Dim Float f = 2.7
Dim Integer i = 9
Sub P(n%)
  n% = n% + 100
  Print n%;
End Sub
P f : P i : Print : Print f; i
""", True),
    ("byval", """Dim Integer i = 5
Sub V(ByVal n%)
  n% = n% * 10
  Print n%;
End Sub
V i : V i + 1 : Print : Print i
""", True),
    ("byref", """Dim Integer i = 5
Dim Float f = 1.5
Sub R(ByRef n%)
  n% = n% * 10
End Sub
R i : Print i
On Error Skip
R f
Print MM.ErrMsg$
On Error Skip
R 3
Print MM.ErrMsg$
Print i; f
""", True),
    ("const arg", """Const K = 5
Dim Integer r
Sub C(n%, o%)
  n% = n% + 1
  o% = n%
End Sub
C K, r : Print K; r
""", None),
    ("nested byref", """Dim Integer acc
Sub Inner(a%)
  a% = a% + 1
End Sub
Sub Mid(b%)
  Inner b% : Inner b%
End Sub
Sub Outer(c%)
  Local k%
  For k% = 1 To 5 : Mid c% : Next
End Sub
Outer acc : Print acc
""", True),
    ("recursion", """Dim Integer moves
Sub Hanoi(n%, a%, b%, c%)
  If n% = 0 Then Exit Sub
  Hanoi n% - 1, a%, c%, b%
  moves = moves + 1
  Hanoi n% - 1, c%, b%, a%
End Sub
Hanoi 10, 1, 2, 3 : Print moves
""", True),
    ("no brackets", """Dim Integer r
Sub S x%, y%, z%
  z% = x% - y%
End Sub
S 10, 3, r : Print r
""", True),
    ("no params", """Dim Integer c
Sub Bump
  c = c + 1
End Sub
Bump : Bump : Bump : Print c
""", True),
    ("static", """Dim Integer tot
Sub St(n%)
  Static Integer calls
  calls = calls + 1
  tot = tot + n% * calls
End Sub
St 1 : St 2 : St 3 : Print tot
""", True),
    ("arg count", """Dim Integer r
Sub Two(a%, b%)
  r = a% + b%
End Sub
Two 1 : Print r
On Error Skip
Two 1, 2, 3
Print MM.ErrMsg$
Two 4, 5 : Print r
""", None),
    ("error in call", """Dim Integer r
Sub Dup(a%, a%)
  r = 1
End Sub
Sub Ok(a%)
  r = a%
End Sub
On Error Skip
Dup 1, 2
Print MM.ErrMsg$
Ok 7 : Print r
Ok 8 : Print r
""", None),
    ("error stops", """Dim Integer r
Sub Dup(a%, a%)
  r = 1
End Sub
Print "before"
Dup 1, 2
Print "not reached"
""", None),
    ("in a loop", """Dim Integer i, s, r
Sub Add3(a%, b%, c%)
  c% = a% + b%
End Sub
For i = 1 To 500
  Add3 i, 5, r
  s = s + r
Next
Print s
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
    print("%-4s %-20s CODE %-6d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-90:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))
b.cmd("OPTION COMPILE OFF", 10)
print("RBP3B", "PASS" if ok else "FAIL")
b.close()
