"""rbp4f.py PORT - Route B P4f: LOCAL compiled (docs/Interpreter_RouteB_Design.html).
Every program runs with OPTION COMPILE OFF, ON and SHADOW and must print the
same, errors included; a line starting TIME is left out of the comparison
and shown.  Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("forms", """Dim Integer n, i
Sub P(v As Integer)
  Local Integer i, j
  Local Float x
  Local String s
  Local t$, u%, w!
  Local As Integer k
  i = v : j = v * 2 : x = v / 4 : s = "s" + Str$(v) : t$ = "t" : u% = v + 1 : w! = v * 1.5 : k = 7
  n = n + i + j + Int(x * 4) + Len(s) + Len(t$) + u% + Int(w!) + k
End Sub
For i = 1 To 20 : P i : Next
Print n
""", True),
    ("default integer", """Option Default Integer
Dim n, i
Sub Q
  Local a, b, c
  a = 1 : b = 2 : c = a + b
  n = n + c
End Sub
For i = 1 To 10 : Q : Next
Print n
""", True),
    ("default none no type", """Option Default None
Dim Integer i
Sub R
  Local a
  a = 1
End Sub
Print "go"
For i = 1 To 2 : R : Next
""", None),
    ("outside a sub", """Dim Integer i
Print "go"
Local Integer z
""", None),
    ("declared twice", """Dim Integer i
Sub D
  Local Integer a
  Local Integer a
End Sub
Print "go"
For i = 1 To 2 : D : Next
""", None),
    ("sub name", """Dim Integer i
Sub Foo
End Sub
Sub G
  Local Integer Foo
End Sub
Print "go"
For i = 1 To 2 : G : Next
""", None),
    ("hides a global", """Dim Integer a = 5, i, t
Sub H
  Local Integer a
  a = a + 1
  t = t + a
End Sub
For i = 1 To 5 : H : Next
Print a; t
""", True),
    ("text forms", """Dim Integer n, i
Sub T(v As Integer)
  Local s$ Length 3
  Local Integer arr(3)
  Local Integer z = v * 2
  Local Integer q ' a comment
  s$ = "ab" : arr(2) = v : q = 1
  n = n + z + arr(2) + Len(s$) + q
End Sub
For i = 1 To 10 : T i : Next
Print n
""", True),
    ("recursion", """Function F(n As Integer) As Integer
  Local Integer t, u
  t = n : u = 1
  If n > 1 Then u = F(n - 1)
  F = t * u
End Function
Dim Integer i, s
For i = 1 To 8 : s = s + F(i) : Next
Print s
""", True),
    ("speed", """Dim Integer i, n
Sub S3
  Local Integer a, b, c
  a = 1 : b = 2 : c = 3
  n = n + a + b + c
End Sub
Timer = 0
For i = 1 To 5000 : S3 : Next
Print n
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
print("RBP4F", "PASS" if ok else "FAIL")
b.close()
