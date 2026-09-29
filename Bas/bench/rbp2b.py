"""rbp2b.py PORT - Route B P2b: compiled numeric expressions on the right of
LET (docs/Interpreter_RouteB_Design.html).

Every program runs with OPTION COMPILE OFF, ON and SHADOW and must print the
same; SHADOW also checks each compiled right-hand side against the text
evaluator bit for bit.  Covered: + - * / \\ MOD ^, the comparisons, AND OR
XOR << >>, unary - + NOT INV, brackets and precedence, integer and float
promotion, errors raised inside compiled code, and what stays text (integer
^, functions, strings, arrays).  Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
class _Loop:  # LOOP % body: the body five times (a '%' template would trip on i%)
    def __mod__(self, body):
        return "For i% = 1 To 5\n" + body + "\nNext\n"


LOOP = _Loop()
PROGS = [
    ("arith", """a% = 7 : b% = 3 : x = 2.5 : y = -1.25
""" + LOOP % """  r1 = a% + b% * 2
  r2% = a% * b% - 4
  r3 = x * y + a%
  r4 = (a% + x) / b%
  r5% = a% \\ b%
  r6% = a% Mod b%
  r7 = 2 + 3 * 4 - 8 / 4 / 2
  r8 = x * 3.7 - y / 0.3
  r9% = x * 3.7
  r10 = 1 / 3 + 2 / 3
  r11 = -x + -a% - +b%
  r12% = -a% \\ 2""" + """Print r1; r2%; r3; r4; r5%; r6%; r7; r8; r9%; r10; r11; r12%
""" + STAT, lambda n: n > 0),
    ("compare logic", """a% = 7 : b% = 3 : x = 2.5 : y = -1.25
""" + LOOP % """  c1% = a% < b%
  c2% = x >= y
  c3% = (a% = 7)
  c4% = a% <> b%
  c5% = x <= 2.5
  c6% = y > -2
  c7% = (a% And 5) Or (b% Xor 1)
  c8% = a% << 2
  c9% = a% >> 1
  c10% = Not a%
  c11 = Not x
  c12% = Inv b%
  c13% = Inv x
  c14% = x > 1 And y < 0""" + """Print c1%; c2%; c3%; c4%; c5%; c6%; c7%; c8%; c9%; c10%; c11; c12%; c13%; c14%
""" + STAT, lambda n: n > 0),
    ("power", """x = 2.5
a% = 3
""" + LOOP % """  p1 = x ^ 2
  p2 = 2 ^ 0.5
  p3 = -2.0 ^ 2
  p4 = a% ^ 2
  p5 = 2 ^ -1""" + """Print p1; p2; p3; p4; p5
""" + STAT, lambda n: n > 0),
    ("text stays", """a% = 3 : x = 0.5 : s$ = "ab"
""" + LOOP % """  f1 = Max(x, 1) + a%
  f2 = Val(s$) * 2
  f3 = Cint(x + 1) * 2""" + """Print f1; f2; f3
""" + STAT, lambda n: n == 5),  # only the five NEXTs (P3c); functions still not compiled (arrays compile since P4a, SIN since P5a, LEN and ASC since P5c)
    ("div zero", """a% = 5 : b% = 0
For i% = 1 To 3
  c = a% / b%
Next
""" + STAT, None),
    ("idiv zero", """a% = 5 : b% = 0
For i% = 1 To 3
  c% = a% \\ b% + 1
Next
""" + STAT, None),
    ("overflow", """x = 1E200
For i% = 1 To 3
  y = x * x + 1
Next
""" + STAT, None),
    ("to int", """x = 1E30
For i% = 1 To 3
  c% = x + 1
Next
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
    print("%-4s %-13s CODE %-5d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-100:]))
    if not same:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))

src = """a% = 7 : b% = 3 : x = 2.5 : y = -1.25
t = Timer
For i% = 1 To 100000
  r = a% * b% + x * y - 3
  s% = (a% + b%) \\ 2 + i% Mod 7
Next
Print Timer - t
""" + STAT
for mode in ("OFF", "ON"):
    b.cmd("OPTION COMPILE " + mode, 10)
    body, stat = run(src)
    print("time %-3s %s  %s" % (mode, body[-1] if body else "?", stat))
b.cmd("OPTION COMPILE OFF", 10)
print("RBP2B", "PASS" if ok else "FAIL")
b.close()
