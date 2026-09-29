"""rbp5d.py PORT - Route B P5d's second round (docs/Interpreter_RouteB_Design.html):
a variable bound as it is made; a single-line IF whose THEN or ELSE part uses a
variable that is not bound yet (optional binds, RC_PARTCHK); &H, &O and &B
literals.  Every program runs with OPTION COMPILE OFF, ON and SHADOW and must
print the same, errors included.  A want of ("fb", n) also asks that at most n
statements ran as text under ON (RAN - CODE): the statements the change is
about compiled.  Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    # bound as made: a variable made by DIM or by its first assignment, and not
    # read since, no longer sends a statement that uses it to text
    ("dim then if", """Dim Integer i, n
Dim Float x
For i = 1 To 100
  If x > 3 Then n = n + 1
Next
Print n
""", ("fb", 8)),
    ("assigned then if", """Dim Integer i
quit = 0
For i = 1 To 100
  If i < 0 Then quit = 1
Next
Print quit
""", ("fb", 8)),
    # optional binds: the part's variable made by nothing before the IF
    ("never made, never taken", """Dim Integer i
For i = 1 To 100
  If i < 0 Then quit = 1
Next
Print quit
""", ("fb", 8)),
    ("never made, taken once", """Dim Integer i
For i = 1 To 100
  If i = 50 Then quit = i
Next
Print quit
""", ("fb", 8)),
    ("else parts made late", """Dim Integer i
For i = 1 To 100
  If i Mod 3 Then a = a + 1 Else b = b + 1
Next
Print a; b
""", ("fb", 8)),
    ("else never taken", """Dim Integer i
For i = 1 To 100
  If i > 0 Then c = c + 1 Else z = 1
Next
Print c; z
""", ("fb", 8)),
    ("element in the part", """Dim Integer i
For i = 1 To 20
  If i = 10 Then Dim k(3) : k(2) = 7
  If i > 10 Then k(1) = k(1) + i
Next
Print k(1); k(2)
""", None),
    ("string in the part", """Dim Integer i
For i = 1 To 30
  If i = 30 Then s$ = "end" Else t$ = "x"
Next
Print s$; " "; t$
""", ("fb", 10)),
    ("call in the condition", """Dim Integer i
Function F(n As Integer) As Integer
  F = n
End Function
For i = 1 To 50
  If F(i) < 0 Then quit = 1
Next
Print quit
""", None),
    ("call in the part", """Dim Integer i
Function G(n As Integer) As Integer
  G = n * 3
End Function
For i = 1 To 50
  If i = 7 Then q = G(i) + 1
Next
Print q
""", ("fb", 12)),
    ("erased, then made again", """Dim Integer i
For i = 1 To 6
  If i = 1 Then w = 5
  If i = 3 Then Erase w
  If i > 3 Then w = w + i
Next
Print w
""", None),
    # an element of a 2-d array passed to a SUB or FUNCTION is passed by
    # reference (the goldens' byrefelem): its index's comma is not the argument's end
    ("2-d element by reference", """Dim Integer m(2, 2), i
Sub Bump(v As Integer)
  v = v + 1
End Sub
Function Twice(v As Integer) As Integer
  v = v * 2
  Twice = v
End Function
m(1, 1) = 7
m(2, 2) = 5
For i = 1 To 3
  Bump m(1, 1)
Next
r = Twice(m(2, 1 + 1)) + m(2, 2)
Print m(1, 1); m(2, 2); r
""", None),
    # BYVAL's array check looked at g_vartbl[0] for an expression argument: with
    # an array in slot 0 (a SUB whose first parameter is one) "5" was an array
    ("byval expression, array in slot 0", """Dim Integer d(3)
Sub Inner(ByVal n As Integer, a() As Integer)
  Print "inner"; n; a(1)
End Sub
Sub Outer(x() As Integer)
  Inner 5, x()
  Inner 2 + 3, x()
End Sub
d(1) = 42
Outer d()
""", None),
    ("byval array refused", """Dim Integer d(3)
Sub T(ByVal a)
  Print a
End Sub
T d()
""", None),
    # &H, &O and &B literals, read by getvalue itself
    ("based literals", """Dim Integer i, s, t
For i = 1 To 100
  s = s + (&HFF And i) + &O17 * &B101
  If i And &H0F Then t = t + &h10
Next
Print s; t; &H; &HFFFFFFFFFFFFFFFF; &B; &O777; &hAbC
""", ("fb", 8)),
    ("based colour", """Dim Integer i
For i = 0 To 40
  Pixel i, 10, &HFF00FF
  Line 0, i, 20, i, 1, &H00FF00
Next
Print "drawn"
""", ("fb", 8)),
    ("type prefix error", """Dim Integer x
x = 1
x = &Q12
""", None),
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


for name, src, want in PROGS:
    outs = {}
    for mode in ("OFF", "ON", "SHADOW"):
        b.cmd("OPTION COMPILE " + mode, 10)
        outs[mode] = run(src + STAT)
    same = outs["OFF"][0] == outs["ON"][0] == outs["SHADOW"][0]
    m = re.search(r"RAN (\d+) MISS \d+ CODE (\d+)", outs["ON"][1])
    fb = int(m.group(1)) - int(m.group(2)) if m else -1
    good = same and (want is None or (m is not None and fb <= want[1]))
    ok = ok and good
    print("%-4s %-26s TEXT %-5d %s" % ("ok" if good else "BAD", name, fb, " | ".join(outs["ON"][0])[-80:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s  %s" % (mode, outs[mode][0], outs[mode][1]))
b.cmd("OPTION COMPILE OFF", 10)
print("RBP5D", "PASS" if ok else "FAIL")
b.close()
