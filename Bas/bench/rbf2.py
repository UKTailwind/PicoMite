"""rbf2.py PORT record|check FILE[,FILE...] - P6 F2 (the local region as a stack): local
variables in recursion, hiding, nesting, arrays, strings, structures, GOSUB,
interrupts, errors, the limit and SAVE/RESTORE CONTEXT.  Every program runs
with OPTION COMPILE OFF and ON; "record" saves what a firmware printed (run it
on the one before the change) and "check" compares another firmware with it.
A line starting TIME is left out of the comparison and shown.  Leaves OPTION
COMPILE OFF."""
import sys, os, json, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

LOCALS40 = ", ".join("a%d" % i for i in range(1, 41))
PROGS = [
    ("recursion", """Function Fact(n As Integer) As Integer
  Local Integer r
  If n <= 1 Then
    r = 1
  Else
    r = n * Fact(n - 1)
  EndIf
  Fact = r
End Function
Function Fib(n As Integer) As Integer
  Local Integer a, b
  If n < 2 Then Fib = n : Exit Function
  a = Fib(n - 1) : b = Fib(n - 2)
  Fib = a + b
End Function
Print Fact(8); Fib(9)
"""),
    ("hiding and nesting", """Dim Integer a = 1, b = 2
Dim s$ = "g"
Sub Inner(x As Integer)
  Local Integer a
  Local s$
  a = x * 10 : s$ = "i" + Str$(x)
  Print "I"; a; b; s$
End Sub
Sub Outer
  Local Integer b
  Local s$ = "o"
  b = 5
  Print "O1"; a; b; s$
  Inner 3
  Print "O2"; a; b; s$
  Local Integer a
  a = 7
  Inner 4
  Print "O3"; a; b; s$
End Sub
Outer
Print "M"; a; b; s$
Outer
"""),
    ("many locals and arrays", """Sub Many(d As Integer)
  Local Integer v1, v2, v3, v4, v5, v6, v7, v8, v9, v10
  Local Float f(20)
  Local t$(5) Length 10
  Local Integer i
  For i = 0 To 20 : f(i) = i * d / 2 : Next
  For i = 0 To 5 : t$(i) = Str$(i * d) : Next
  v1 = d : v10 = d * 2
  If d > 1 Then Many d - 1
  Print d; v1; v10; f(20); " "; t$(5)
End Sub
Many 8
Many 3
"""),
    ("strings and constants", """Function Rev$(s$)
  Local Integer i
  Local r$
  Const K = 2
  For i = Len(s$) To 1 Step -1 : r$ = r$ + Mid$(s$, i, 1) : Next
  Rev$ = r$ + Str$(K)
End Function
Print Rev$("abcdef"); " "; Rev$(Rev$("xy"))
"""),
    ("constants hide", """Const K = 10
Const N$ = "glob"
Sub Inner
  Print "I"; K; " "; N$
End Sub
Sub Outer
  Local Integer y
  y = K * 2 : Print "O1"; K; y
  Const K = 20
  Const N$ = "loc"
  y = K * 2 : Print "O2"; K; y; " "; N$
  Inner
  Print "O3"; K; " "; N$
End Sub
Function F(x As Integer) As Integer
  Const K = 3
  If x > 0 Then F = K + F(x - 1) Else F = K
End Function
Outer
Print "M"; K; " "; N$; F(2)
Outer
"""),
    ("sub name clash", """Sub Foo
End Sub
Sub T
  Local Integer foo
End Sub
T
"""),
    ("type clash", """Dim Integer aVal = 1
Sub T
  Print aVal
  aVal! = 2
End Sub
T
"""),
    ("parameter forms", """Type Pt
  x As Integer
  y As Float
End Type
Dim gp As Pt
Dim Integer ai(3), ib = 3, idd = 4
Dim Float fa = 1.5, fe = 2.5
Dim s$ = "abc"
Sub P1(a, b As Integer, c$, ByVal d As Integer, ByRef e As Float, f%(), g As Pt)
  Print a; b; " "; c$; d; e; Bound(f%()); g.x
  a = 99 : b = 98 : d = 97 : e = 96.5 : c$ = "zz" : f%(1) = 7 : g.x = 42
End Sub
gp.x = 11
P1 fa, ib, s$, idd, fe, ai(), gp
Print fa; ib; " "; s$; idd; fe; ai(1); gp.x
P1 fa + 1, 2.7, s$ + "!", 8, fe, ai(), gp
Print fa; ib; " "; s$; idd; fe; ai(1); gp.x
Sub P2(a, b, c)
  Print a; b; c
End Sub
P2 1
P2 1, , 3
Function Jn$(a$, ByVal n As Integer) As String
  Jn$ = a$ + Str$(n)
End Function
Function Sq(x As Float) As Float
  Sq = x * x
End Function
Function Fct(n As Integer) As Integer
  If n <= 1 Then Fct = 1 Else Fct = n * Fct(n - 1)
End Function
Print Jn$("x", 3.7); " "; Jn$(Jn$("y", 1), 2); Sq(3); Sq(ib); Fct(6)
Dim Integer k
For k = 1 To 3 : P2 k, k * 2, Sq(k) : Next
"""),
    ("missing first argument", """Sub P2(a, b, c)
  Print a; b; c
End Sub
P2 , 2
"""),
    ("byval array", """Dim Integer ai(3)
Sub Q(ByVal a%())
  Print Bound(a%())
End Sub
Q ai()
"""),
    ("byref expression", """Sub Q(ByRef a As Integer)
  Print a
End Sub
Q 1 + 2
"""),
    ("byref type", """Dim Float f = 2
Sub Q(ByRef a As Integer)
  Print a
End Sub
Q f
"""),
    ("structures", """Type Pt
  x As Integer
  y As Float
End Type
Dim gp As Pt
Sub Mv(k As Integer)
  Local q As Pt
  q.x = k : q.y = k / 4
  If k > 0 Then Mv k - 1
  gp.x = gp.x + q.x : gp.y = gp.y + q.y
End Sub
Mv 6
Print gp.x; gp.y
"""),
    ("gosub and interrupts", """Dim Integer t, g = 1, ticks, i
Sub S
  Local Integer g
  g = 50
  GoSub Lbl
End Sub
Sub Tk
  Local Integer q, w
  q = 3 : w = q * 2
  ticks = ticks + w - 5
End Sub
SetTick 2, Tk
For i = 1 To 400
  S
  GoSub Lbl
Next
SetTick 0, Tk
Print t; ticks > 0
End
Lbl:
  t = t + g
  Return
"""),
    ("error skip in a loop", """Dim Integer i, n
Sub E(k As Integer)
  Local Integer z
  z = k
  On Error Skip
  z = z \\ 0
  n = n + z
End Sub
For i = 1 To 500 : E i : Next
Print n
"""),
    ("the limit", """Sub Down(n As Integer)
  Local Integer a, b, c, d2, e, f, g, h, k, m
  If n Mod 5 = 0 Then Print n;
  Down n + 1
End Sub
Down 1
"""),
    ("after the limit", """Sub T(n As Integer)
  Local Integer z
  z = n * 2
  If n > 0 Then T n - 1
  Print z;
End Sub
T 5
Print
"""),
    ("context", """Dim Integer a = 5
Sub S
  Local Integer k = 9
  a = a + k
End Sub
S
Save Context Clear
Dim Integer a = 100
S
Print a
Load Context
S
Print a
"""),
    ("forty locals", """Sub Big
  Local Integer """ + LOCALS40 + """
  Local Integer i
  Timer = 0
  For i = 1 To 20000 : a1 = a1 + i : a40 = a40 + a1 Mod 3 : Next
  Print a1; a40
  Print "TIME "; Timer
End Sub
Big
"""),
    ("speed", """Dim Integer i, s
Sub W(a As Integer, b As Integer)
  Local Integer c, d, e, f
  c = a + b : d = c * 2 : e = d - a : f = e + c
  s = s + f
End Sub
Timer = 0
For i = 1 To 20000 : W i, 3 : Next
Print s
Print "TIME "; Timer
"""),
]

port, mode, fname = sys.argv[1], sys.argv[2], sys.argv[3]
b = pc3.PC3(port)
b.attention()


def run(src):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120)).replace("\r", "")
    lines = [l.rstrip() for l in out.split("\n") if l.strip() and l.strip() not in ("RUN", ">")]
    times = [l for l in lines if l.startswith("TIME ")]
    body = [l for l in lines if not l.startswith("TIME ")]
    return body, times


# expected from the manual rather than from a firmware (a constant defined in a
# sub is local to it and hides a global constant of the same name)
EXPECT = {"constants hide": ["O1 10 20", "O2 20 40 loc", "I 10 glob", "O3 20 loc", "M 10 glob 9",
                             "O1 10 20", "O2 20 40 loc", "I 10 glob", "O3 20 loc"],
          # the old code's error texts: error("A sub/fun has the same name: $") and
          # error("$ Different type already declared"), the name in capitals
          "sub name clash": ["[4] Local Integer foo", "Error : A sub/fun has the same name: FOO"],
          "type clash": [" 1", "[4] aVal! = 2", "Error : AVAL Different type already declared"]}

# Route B is RP2350-only: where OPTION COMPILE is refused, only the text run
modes = ("OFF",) if "Error" in b.cmd("OPTION COMPILE OFF", 10) else ("OFF", "ON")
got = {}
for name, src in PROGS:
    for m in modes:
        if len(modes) > 1:
            b.cmd("OPTION COMPILE " + m, 10)
        got[name + "/" + m] = run(src)
if len(modes) > 1:
    b.cmd("OPTION COMPILE OFF", 10)
b.close()

for k, (body, times) in got.items():
    want = EXPECT.get(k.split("/")[0])
    if want is not None and body != want:
        print("BAD  %-32s against the manual: %s" % (k, body))
if mode == "record":
    json.dump(got, open(fname, "w"), indent=1)
    for k, (body, times) in got.items():
        print("%-32s %s %s" % (k, " | ".join(body)[-90:], " ".join(t[5:].strip() for t in times)))
    print("RBF2 RECORDED")
else:
    ref = {}
    for f in reversed(fname.split(",")):  # several references: the first that has a program wins
        ref.update(json.load(open(f)))
    ok = True
    # a stack overflow's report names the stack and heap addresses, which move
    # from build to build; its depth is what must match
    addr = lambda ls: [re.sub(r"stack [0-9A-F]+, heap [0-9A-F]+", "stack *, heap *", l) for l in (ls or [])]
    for k, (body, times) in got.items():
        # a compiled run with no reference of its own (recorded where OPTION
        # COMPILE is refused) must print what the text run recorded
        rb, rt = ref.get(k, ref.get(k.split("/")[0] + "/OFF", (EXPECT.get(k.split("/")[0]), [])))
        good = addr(body) == addr(rb)
        ok = ok and good
        print("%-4s %-32s %s" % ("ok" if good else "BAD", k, " | ".join(body)[-80:]))
        if times:
            print("     time was %s now %s" % (rt[0][5:].strip() if rt else "?", times[0][5:].strip()))
        if not good:
            print("     was %s" % rb)
            print("     now %s" % body)
    print("RBF2", "PASS" if ok else "FAIL")
