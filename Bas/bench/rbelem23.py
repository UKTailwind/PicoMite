"""rbelem23.py PORT - Route B V3: two- and three-dimensional array elements,
which RBElementAt takes straight through (docs/Interpreter_RouteB_VM_Review.html).
Every program runs with OPTION COMPILE OFF, ON and SHADOW and must print the
same, errors included: values and stores at every corner, OPTION BASE 1, a
dimension whose upper bound is 0, float indices, an element by reference,
each index out of bounds in turn, below the base, and the wrong count of
indices both ways.  Then the time of a 2-D and a 3-D read.  Leaves OPTION
COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("2-D every element", """Dim Integer m(3, 4), i, j, s
For i = 0 To 3 : For j = 0 To 4 : m(i, j) = i * 10 + j : Next : Next
For i = 0 To 3 : For j = 0 To 4 : s = s * 3 + m(i, j) : s = s Mod 1000003 : Next : Next
Print s; m(0, 0); m(3, 4); m(3, 0); m(0, 4)
""", True),
    ("3-D every element", """Dim Float c(2, 3, 4)
Dim Integer i, j, k
Dim Float s
For i = 0 To 2 : For j = 0 To 3 : For k = 0 To 4 : c(i, j, k) = i * 100 + j * 10 + k + 0.5 : Next : Next : Next
For i = 0 To 2 : For j = 0 To 3 : For k = 0 To 4 : s = s * 1.5 + c(i, j, k) : s = s - Int(s / 1000) * 1000 : Next : Next : Next
Print s; c(0, 0, 0); c(2, 3, 4); c(2, 0, 4); c(0, 3, 0)
""", True),
    ("base 1", """Option Base 1
Dim Integer m(3, 4), c(2, 3, 2), i, j, k, s
For i = 1 To 3 : For j = 1 To 4 : m(i, j) = i * 10 + j : Next : Next
For i = 1 To 2 : For j = 1 To 3 : For k = 1 To 2 : c(i, j, k) = m(i + 1, j) * 100 + k : Next : Next : Next
For i = 1 To 2 : s = s + c(i, 3, 2) + m(3, 4) + c(2, 1, 1) : Next
Print s; m(1, 1); c(1, 1, 1); c(2, 3, 2)
""", True),
    ("upper bound 0", """Dim Integer m(0, 3), c(2, 0, 1), i, s
For i = 0 To 3 : m(0, i) = i + 1 : Next
c(2, 0, 1) = 7 : c(0, 0, 0) = 5
Print m(0, 0); m(0, 3); c(2, 0, 1); c(0, 0, 0); c(1, 0, 1)
""", True),
    ("float indices", """Dim Integer m(5, 5), i, j
For i = 0 To 5 : For j = 0 To 5 : m(i, j) = i * 10 + j : Next : Next
Dim Float x = 1.6, y = 2.4
Print m(x, y); m(y, x); m(2.5, 3.5); m(x + y, 0)
m(x, y) = 99 : Print m(2, 2)
""", True),
    ("stores and INC", """Dim Integer m(4, 4), c(2, 2, 2), i, j
For i = 0 To 4 : For j = 0 To 4 : m(i, j) = 1 : Next : Next
For i = 0 To 4 : m(i, i) = m(i, i) + i : Inc m(i, 4 - i), 10 : Next
For i = 0 To 2 : c(i, i, i) = m(i, i) * 2 : Inc c(i, 0, 2), 3 : Next
Print m(0, 0); m(2, 2); m(4, 0); m(0, 4); m(2, 1); c(1, 1, 1); c(2, 0, 2); c(0, 1, 0)
""", True),
    ("element by reference", """Dim Integer m(3, 3), c(1, 2, 3), i
Sub Bump(v As Integer)
  v = v + 5
End Sub
For i = 0 To 3 : Bump m(i, 3 - i) : Next
Bump c(1, 2, 3) : Bump c(1, 2, 3)
Print m(0, 3); m(3, 0); m(1, 1); c(1, 2, 3)
""", True),
    ("five dimensions", """Dim Integer p(1, 2, 1, 2, 1), a, b, c, d, e, s
For a = 0 To 1 : For b = 0 To 2 : For c = 0 To 1 : For d = 0 To 2 : For e = 0 To 1
p(a, b, c, d, e) = a + b * 2 + c * 7 + d * 13 + e * 29
Next : Next : Next : Next : Next
For a = 0 To 1 : For b = 0 To 2 : For d = 0 To 2 : s = s + p(a, b, 1, d, a) : Next : Next : Next
Print s; p(1, 2, 1, 2, 1)
""", True),
    ("2-D first out of bounds", """Dim Integer m(3, 4), i = 4
Print m(i, 1)
""", None),
    ("2-D second out of bounds", """Dim Integer m(3, 4), i = 5
Print m(1, i)
""", None),
    ("2-D write out of bounds", """Dim Integer m(3, 4), i = 5
m(2, i) = 1
""", None),
    ("3-D third out of bounds", """Dim Integer c(2, 2, 2), i = 3
Print c(1, 1, i)
""", None),
    ("3-D below base", """Dim Integer c(2, 2, 2), i = -1
Print c(1, i, 1)
""", None),
    ("base 1, index 0", """Option Base 1
Dim Integer m(3, 4), i = 0
Print m(1, i)
""", None),
    ("2-D given one index", """Dim Integer m(3, 4), i = 1
Print m(i)
""", None),
    ("2-D given three", """Dim Integer m(3, 4), i = 1
Print m(i, i, i)
""", None),
    ("1-D given two", """Dim Integer a(5), i = 1
Print a(i, i)
""", None),
    ("3-D given two", """Dim Integer c(2, 2, 2), i = 1
Print c(i, i)
""", None),
]

TIMED = """Dim Integer i, j = 9, k = 5, x, n = 20000
Dim Integer b(7, 15), c(3, 7, 15)
Dim Float t
t = Timer
For i = 1 To n : x = b(k, j) : Next
Print "T2"; Timer - t
t = Timer
For i = 1 To n : x = c(1, k, j) : Next
Print "T3"; Timer - t
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
    print("%-4s %-26s CODE %-6d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-80:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))

for mode in ("OFF", "ON"):
    b.cmd("OPTION COMPILE " + mode, 10)
    body, stat = run(TIMED)
    print("     time %-4s %s" % (mode, " ".join(body)))
b.cmd("OPTION COMPILE OFF", 10)
print("RBELEM23", "PASS" if ok else "FAIL")
b.close()
