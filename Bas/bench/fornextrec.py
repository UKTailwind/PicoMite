"""fornextrec.py PORT - FOR...NEXT in a recursive SUB or FUNCTION.  Every level
of the recursion has its loop's NEXT at the same place, and NEXT finds its
loop by that place.  When a loop ended, NEXT also closed any other loop with
the same NEXT (for NEXT j, i) - including the caller's, whose variable it
stepped and whose body it ran in the callee, losing a loop each time: a
recursive search (TSCP chess's quiesce) then hit "Cannot find a matching FOR"
or an EXIT FOR left the wrong loop.  Each program prints the same with
OPTION COMPILE OFF, ON and SHADOW, and what a host model of it says.  Leaves
OPTION COMPILE as it found it."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'


def count(n):
    """F(n) = 1 + sum over i = 1..2 of F(n - 1) for n > 0, else 1."""
    return 1 if n == 0 else 1 + 2 * count(n - 1)


def visits(n):
    """the loop body runs twice per level that recurses: 2 + 4 + ... ."""
    return 0 if n == 0 else 2 + 2 * visits(n - 1)


# (name, program, the expected output)
PROGS = [
    ("FUNCTION, loop around the call", """Dim Integer v
Function F(n)
  Local i, s
  s = 1
  If n = 0 Then F = 1 : Exit Function
  For i = 1 To 2
    s = s + F(n - 1)
    Inc v
  Next i
  F = s
End Function
Print F(6); v
""", " %d %d" % (count(6), visits(6))),
    ("SUB with NEXT and no variable", """Dim Integer v
Sub S(n)
  Local i
  If n = 0 Then Exit Sub
  For i = 1 To 2
    S n - 1
    Inc v
  Next
End Sub
S 6
Print v
""", " %d" % visits(6)),
    ("EXIT FOR at a deeper level", """Dim Integer v, w
Function Q(n)
  Local i, x
  Q = 0
  If n = 0 Then Exit Function
  For i = 1 To 3
    x = Q(n - 1)
    Inc v
    If i = 2 Then Exit For
  Next i
  Inc w
  Q = i
End Function
Print Q(5); v; w
""", None),
    ("NEXT j, i still closes both", """Dim Integer i, j, t
For i = 1 To 3
  For j = 1 To 4
    t = t + i * j
Next j, i
Print t; i; j
""", " 60 4 5"),
]

b = pc3.PC3(sys.argv[1])
b.attention()
was = b.cmd("PRINT MM.INFO(COMPILE)", 10).strip()
ok = True


def run(src):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120)).replace("\r", "")
    lines = [l for l in out.split("\n") if l.strip() and l.strip() not in ("RUN", ">")]
    stat = [l for l in lines if l.startswith("STAT ")]
    return [l for l in lines if not l.startswith("STAT ")], (stat[0][5:] if stat else "")


# EXIT FOR at i = 2 each level: Q(n) visits its body twice and returns 2
def qmodel(n):
    if n == 0:
        return 0, 0, 0
    v = w = 0
    for i in (1, 2):
        _, dv, dw = qmodel(n - 1)
        v += dv + 1
        w += dw
    return 2, v, w + 1


r, v, w = qmodel(5)
PROGS[2] = (PROGS[2][0], PROGS[2][1], " %d %d %d" % (r, v, w))
for name, src, want in PROGS:
    outs = {}
    for mode in ("OFF", "ON", "SHADOW"):
        b.cmd("OPTION COMPILE " + mode, 10)
        outs[mode] = run(src + STAT)
    same = outs["OFF"][0] == outs["ON"][0] == outs["SHADOW"][0]
    good = same and outs["OFF"][0] == [want]
    ok = ok and good
    print("%-4s %-34s %s" % ("ok" if good else "BAD", name, " | ".join(outs["ON"][0])[-70:]))
    if not good:
        print("     want   %r" % want)
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s  [%s]" % (mode, outs[mode][0], outs[mode][1]))
b.cmd("OPTION COMPILE " + ("OFF" if was.startswith("OFF") else "ON"), 10)
print("FORNEXTREC", "PASS" if ok else "FAIL")
b.close()
