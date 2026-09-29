"""rbp2a.py PORT - Route B P2a: compiled LET g = literal and LET g = g2 for
global integer and float scalars (docs/Interpreter_RouteB_Design.html).

Every program runs with OPTION COMPILE OFF, ON and SHADOW and must print the
same.  Compiled statements are counted by MM.INFO(COMPILE)'s CODE: a
variable's first two assignments run through the fallback (findvar makes the
variable the first time and binds its symbol the second, when it finds it);
later ones run compiled.  The survey types unsuffixed names from DIM/LOCAL/
STATIC declarations, CONST literals and OPTION DEFAULT (text order), so those
compile too.  A CONST target, a LOCAL of the same name and ON ERROR SKIP run
the fallback.  Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("scalars", """a% = 5
b! = 2.5
c = 1.25
d% = a%
e = a%
f% = b!
g = 123456789012345
h% = 1.5E3
k! = 7
For i% = 1 To 1000
  x = c
  y% = a%
  z% = b!
  w = 0.1
Next
Print a%; b!; c; d%; e; f%; g; h%; k!; x; y%; z%; w
""" + STAT, lambda n: n == 4 * 999 + 1000),  # the four LETs from the second pass (bound as made, P5d), and every NEXT (P3c)
    ("const", """Const K = 5
For i% = 1 To 3
  j = K
Next
Print j
K = 6
""" + STAT, None),
    ("default int", """Option Default Integer
a = 1.7
For i = 1 To 10
  b = a
Next
Print a; b
""" + STAT, lambda n: n == 9 + 10),  # b = a from the second pass (bound as made, P5d), and the NEXTs (P3c)
    ("dim as", """Dim n As Integer
For i% = 1 To 10
  n = 2.5
Next
Print n
""" + STAT, lambda n: n >= 8),
    ("dim integer", """Option Default None
Dim Integer p, q = 4
Dim Float r
Dim t As Float, u%
For i% = 1 To 10
  p = q + 1
  r = p * 0.5
  t = r + u%
Next
Print p; q; r; t
""" + STAT, lambda n: n >= 24),
    ("const read", """Const K = 3, F = 1.5
Dim Integer z
Dim y As Float
For i% = 1 To 10
  z = K * 2 + F
  y = F / K
Next
Print z; y
""" + STAT, lambda n: n >= 16),
    ("local", """a = 1
S
Print a
Sub S
  Local a
  For i% = 1 To 10
    a = 7
  Next
  Print a
End Sub
""" + STAT, None),
    # the second a% = b! runs compiled, and its FloatToInt64 raises the error
    ("too large", """b! = 1
For i% = 1 To 3
  a% = b!
  b! = 1E30
Next
""" + STAT, None),
    ("error skip", """b! = 1E30
a% = 1
For i% = 1 To 3
  On Error Skip 1
  a% = b!
Next
Print a%; MM.ErrNo
""" + STAT, None),
]

b = pc3.PC3(sys.argv[1])
b.attention()
ok = True


def run(src):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(60)).replace("\r", "")
    lines = [l for l in out.split("\n") if l.strip() and l.strip() not in ("RUN", ">")]
    stat = [l for l in lines if l.startswith("STAT ")]
    body = [l for l in lines if not l.startswith("STAT ")]
    return body, (stat[0][5:] if stat else "")


for name, src, want_code in PROGS:
    outs = {}
    for mode in ("OFF", "ON", "SHADOW"):
        b.cmd("OPTION COMPILE " + mode, 10)
        outs[mode] = run(src)
    same = outs["OFF"][0] == outs["ON"][0] == outs["SHADOW"][0]
    m = re.search(r"CODE (\d+)", outs["ON"][1])
    code = int(m.group(1)) if m else -1
    good = same and (want_code is None or want_code(code))
    ok = ok and good
    print("%-4s %-12s CODE %-6d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-90:]))
    if not same:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))

# speed: a compiled LET against its text
src = """t = Timer
For i% = 1 To 100000
  x = c
  y% = a%
Next
Print Timer - t
""" + STAT
for mode in ("OFF", "ON"):
    b.cmd("OPTION COMPILE " + mode, 10)
    body, stat = run(src)
    print("time %-3s %s  %s" % (mode, body[-1] if body else "?", stat))
b.cmd("OPTION COMPILE OFF", 10)
print("RBP2A", "PASS" if ok else "FAIL")
b.close()
