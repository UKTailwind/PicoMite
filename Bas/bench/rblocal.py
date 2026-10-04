"""rblocal.py PORT - Route B P7a: LOCAL in the forms cmd_dim reads without a
value: a trailing comment, an AS type after a name, LENGTH n, arrays, and
mixes of them (item 7 of docs/Interpreter_RouteB_Coverage.html).  RBLocal
makes each name as cmd_dim does: its own AS type (with cmd_dim's errors),
then findvar on a copy of the name's text up to where cmd_dim's copy stops.
A structure type, and a value (P7b), stay cmd_dim's.  Every program runs
with OPTION COMPILE OFF, ON and SHADOW and must print the same, errors
included; where the SUBs' LOCALs should compile, the ON run's statements run
as text (RAN - CODE) must stay under the limit.  Then the OFF and ON time of
a SUB with such a LOCAL called 10,000 times.  Leaves OPTION COMPILE as it
found it."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
# (name, program, most statements run as text with OPTION COMPILE ON, or None)
PROGS = [
    ("a trailing comment", """Dim Integer k, t
Sub S(n%)
  Local c, lo, hi        ' the comment cmd_dim stops at
  c = n% : lo = c - 1 : hi = c + 1
  t = t + lo + hi
End Sub
For k = 1 To 100 : S k : Next
Print t
""", 6),
    ("AS after the names", """Dim Integer k
Dim Float t
Dim String r$
Sub S(n%)
  Local i As integer, f As Float, w As String
  i = n% + 0.6 : f = n% / 4 : w = Str$(i)
  t = t + i + f
  r$ = w
End Sub
For k = 1 To 100 : S k : Next
Print t; " "; r$
""", 6),
    ("LENGTH, and a String type before", """Dim Integer k, t
Sub S(n%)
  Local kb$ LENGTH 2
  Local String d$ Length 8, e$
  kb$ = Left$(Str$(n%), 2) : d$ = "abcdefgh" : e$ = d$ + d$
  t = t + Len(kb$) + Len(e$)
End Sub
For k = 1 To 100 : S k : Next
Print t
""", 6),
    ("arrays, with types and LENGTH", """Dim Integer k, t
Sub S(n%)
  Local rsun(3), rmoon(3)
  LOCAL INTEGER i, kk, b(5)
  Local String Sw(5) Length 16
  Local j As integer, tmatrix(3, 3)
  For i = 0 To 3 : rsun(i) = n% * i : rmoon(i) = rsun(i) + 1 : Next
  b(5) = n% : tmatrix(3, 3) = 2
  t = t + rsun(3) + rmoon(0) + b(5) + tmatrix(3, 3)
End Sub
For k = 1 To 100 : S k : Next
Print t
""", 12),
    ("a String array with LENGTH, used once", """Sub S(n%)
  Local String Sw(5) Length 16
  LOCAL INTEGER b(5)
  Sw(5) = "x" + Str$(n%)
  Print Len(Sw(5)); Bound(b())
  Sw(4) = String$(17, "y")
End Sub
S 7
""", None),
    ("AS INTEGER and LENGTH in one list", """Dim Integer k, t
Sub S(n%)
  local num AS integer, outP AS STRING length 60, unit AS INTEGER
  num = n% : outP = Space$(60) : unit = 3
  t = t + num * unit + Len(outP)
End Sub
For k = 1 To 100 : S k : Next
Print t
""", 6),
    ("OPTION DEFAULT INTEGER", """Option Default Integer
Dim k, t
Sub S(n)
  Local x, y ' integers
  x = n / 3 : y = x * 2
  t = t + x + y
End Sub
For k = 1 To 100 : S k : Next
Print t
""", 6),
    ("P7b values from parameters (TSCP chess)", """Dim Integer k, t
Function Q(a, b, d)
  Local alpha=a, beta=b, depth=d
  Local i, j, x, c, f, from, too, exitF=0
  x = alpha + beta * depth + exitF
  Q = x
End Function
For k = 1 To 100 : t = t + Q(k, 2, 3) : Next
Print t
""", 6),
    ("P7b a TIMER value, then a name (walkr)", """Dim Integer k, t
Sub S
  Local Float tt = Timer, ut
  If tt >= 0 And ut = 0 Then t = t + 1
End Sub
For k = 1 To 100 : S : Next
Print t
""", 6),
    ("P7b types: AS, &H, a comment, LENGTH, strings", """Dim Integer k, t
Dim String r$
Sub S(n%)
  Local i As Integer = n% + 0.6, xx% = &HFF
  Local factor = 0.25            ' geometric acceleration factor
  Local integer j=1, col, bg_hit, hit
  Local s1$ LENGTH 5 = "ab" + Str$(n% Mod 10)
  Local String d$ = "x"
  t = t + i + xx% + factor * 4 + j + col + Len(s1$)
  r$ = s1$ + d$
End Sub
For k = 1 To 100 : S k : Next
Print t; " "; r$
""", 6),
    ("P7b a string for a number", """Sub S
  Local x = "a"
End Sub
S
""", None),
    ("P7b a number for a string", """Sub S
  Local s1$ = 5
End Sub
S
""", None),
    ("P7b a value naming the LOCAL's own: text", """Dim Integer k, t
Sub S(n%)
  Local a = n%, b = a + 1
  t = t + a + b
End Sub
For k = 1 To 100 : S k : Next
Print t
""", None),
    ("P7b a FUNCTION call in a value: text", """Dim Integer k, t
Function F(v)
  F = v * 2
End Function
Sub S(n%)
  Local v = F(n%)
  t = t + v
End Sub
For k = 1 To 100 : S k : Next
Print t
""", None),
    ("P7b an array's list: text", """Dim Integer k, t
Sub S(n%)
  Local a(2) = (n%, 2, 3)
  t = t + a(0) + a(2)
End Sub
For k = 1 To 100 : S k : Next
Print t
""", None),
    ("P7c STATIC as walkr's timing SUB has it", """Dim Integer k
Sub T(t0, rep)
  Static Float dt(9)
  Static Integer p = 0, first = 1, n = 0
  If rep Then Print n; p; first; Math(Sum dt()) : Exit Sub
  dt(p) = t0 : Inc p, 1 : If p >= 10 Then p = 0
  Inc n, 1
End Sub
For k = 1 To 100 : T k, 0 : Next
T 0, 1
""", 12),
    ("P7c STATIC: AS, LENGTH with a value, two SUBs", """Dim Integer k
Sub A(v%)
  Static count As Integer
  Static s$ LENGTH 3, t$ = "ab", u$ LENGTH 4 = "cd" ' (LENGTH with a value failed in cmd_dim until P7)
  Inc count, v%
  If v% = 0 Then Print "A"; count; s$; t$; u$
End Sub
Sub B(v%)
  Static count As Integer = 1000
  Inc count, v%
  If v% = 0 Then Print "B"; count
End Sub
For k = 1 To 100 : A k : B 1 : Next
A 0 : B 0
""", 10),
    ("P7c STATIC in a recursive SUB", """Sub R(d%)
  Static Integer calls
  Inc calls
  If d% > 0 Then R d% - 1
  If d% = 6 Then Print calls
End Sub
R 6
R 6
""", 8),
    ("P7c an AS type is its own name's only (STATIC kept it until P7)", """Dim Integer k
Sub C(v%)
  Static m = 5, w As Integer, z
  z = z + v% / 2
  If v% = 0 Then Print m; w; z
End Sub
For k = 1 To 10 : C k : Next
C 0
""", None),
    ("P7c STATIC outside a SUB", """Static z As Integer
Print z
""", None),
    ("P7c STATIC twice in one call", """Sub S
  Local Integer i
  For i = 1 To 2
    Static Integer q
  Next
End Sub
S
""", None),
    ("P7c STATIC string too long", """Sub Sx
  Static s$ LENGTH 3 = "ab"
  s$ = s$ + "c"
End Sub
Dim Integer k
For k = 1 To 3 : Sx : Next
""", None),
    ("Type specified twice", """Sub S
  Local Integer x As Float
End Sub
S
""", None),
    ("an unknown type", """Sub S
  Local x As Bogus
End Sub
S
""", None),
    ("LOCAL outside a SUB", """Local q As Integer
Print q
""", None),
    ("declared twice", """Dim Integer k
Sub S
  Local Integer w
  Local w As Float
End Sub
S
""", None),
    ("too long for LENGTH", """Sub S
  Local kb$ LENGTH 2
  kb$ = "abc"
End Sub
S
""", None),
    ("a structure type stays cmd_dim's", """Type Pt
  x As Integer
  y As Integer
End Type
Dim Integer k, t
Sub S(n%)
  Local p As Pt
  p.x = n% : p.y = 2 * n%
  t = t + p.x + p.y
End Sub
For k = 1 To 10 : S k : Next
Print t
""", None),
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


for name, src, most_text in PROGS:
    outs = {}
    for mode in ("OFF", "ON", "SHADOW"):
        b.cmd("OPTION COMPILE " + mode, 10)
        outs[mode] = run(src + STAT)
    same = outs["OFF"][0] == outs["ON"][0] == outs["SHADOW"][0]
    m = re.search(r"RAN (\d+) MISS \d+ CODE (\d+)", outs["ON"][1])
    text = int(m.group(1)) - int(m.group(2)) if m else -1
    good = same and (most_text is None or 0 <= text <= most_text)
    ok = ok and good
    print("%-4s %-36s text %-5d %s" % ("ok" if good else "BAD", name, text, " | ".join(outs["ON"][0])[-70:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s  [%s]" % (mode, outs[mode][0], outs[mode][1]))
TIMING = """Dim Integer k, n = 10000
Dim Float t0, tL
Sub S(v%)
  Local i As integer, f As Float, a(3) ' a comment
  i = v%
End Sub
t0 = Timer
For k = 1 To n : S k : Next
tL = Timer - t0
Print "T"; tL
"""
times = {}
for mode in ("OFF", "ON"):
    b.cmd("OPTION COMPILE " + mode, 10)
    b.upload(TIMING, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120))
    m = re.search(r"T\s*([\d.]+)", out)
    times[mode] = float(m.group(1)) if m else None
if times["OFF"] and times["ON"]:
    good = times["ON"] < times["OFF"]
    ok = ok and good
    print("%-4s timing: 10,000 calls of a SUB opening with LOCAL i As integer, f As Float, a(3): %.0f -> %.0f ms (OFF -> ON)" % (
        "ok" if good else "BAD", times["OFF"], times["ON"]))
else:
    ok = False
    print("BAD  timing: no result", times)
TIMING2 = """Dim Integer k, n = 10000
Dim Float t0, tS
Sub Tick(t)
  Static Float dt(9)
  Static Integer p = 0, first = 1, c = 0
  Local Float tt = Timer, ut
  dt(p) = t : Inc p : If p >= 10 Then p = 0
End Sub
t0 = Timer
For k = 1 To n : Tick k : Next
tS = Timer - t0
Print "T"; tS
"""
times = {}
for mode in ("OFF", "ON"):
    b.cmd("OPTION COMPILE " + mode, 10)
    b.upload(TIMING2, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120))
    m = re.search(r"T\s*([\d.]+)", out)
    times[mode] = float(m.group(1)) if m else None
if times["OFF"] and times["ON"]:
    good = times["ON"] < times["OFF"]
    ok = ok and good
    print("%-4s timing: 10,000 calls of walkr's timing SUB (two STATICs, a LOCAL with a value): %.0f -> %.0f ms (OFF -> ON)" % (
        "ok" if good else "BAD", times["OFF"], times["ON"]))
else:
    ok = False
    print("BAD  timing 2: no result", times)
b.cmd("OPTION COMPILE " + ("OFF" if was.startswith("OFF") else "ON"), 10)
print("RBLOCAL", "PASS" if ok else "FAIL")
b.close()
