"""rbdraw.py PORT - Route B: TEXT, CIRCLE, TRIANGLE, ARC and RBOX through the
value splice (item 5 of docs/Interpreter_RouteB_Coverage.html), as BOX, LINE
and PIXEL already were: TEXT with string values, a bare-word justification
(LT) and a #font; CIRCLE behind OPTION LEGACY's guard; TRIANGLE's SAVE and
RESTORE, array forms and FUNCTION calls in the arguments stay text.  Every
program draws on a cleared screen and prints a checksum of the pixels it
drew (the others print errors), and runs with OPTION COMPILE OFF, ON and
SHADOW: all three must print the same, errors included; where the drawing
should compile, the ON run's statements run as text (RAN - CODE) before the
checksum must stay under the limit.  Then OFF and ON timings of TEXT and
CIRCLE loops.  Leaves OPTION COMPILE as it found it."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
# the drawn area's checksum (PIXEL( itself is not compiled here): the count is
# taken before it and printed after it, since the console's output is drawn on
# the screen too
CK = """zst$ = MM.Info(COMPILE)
Dim Integer ckx, cky, ck
For cky = 0 To 79 : For ckx = 0 To 159 : ck = ck + Pixel(ckx, cky) * (1 + ((ckx + 3 * cky) And 15)) : Next : Next
Print "CK"; ck
Print "STAT "; zst$
"""
# (name, program, most statements run as text with OPTION COMPILE ON before the STAT line, or None)
PROGS = [
    ("TEXT: strings and justification", """CLS
Dim Integer k
Dim s$ = "Abc"
Dim j$ = "CM"
For k = 1 To 20
  Text 2 + k, 2, "Hi" + Str$(k), "LT", 1, 1, RGB(WHITE)
  Text 80, 30, s$, j$, 1, 1, RGB(GREEN), RGB(BLUE)
  Text 60, 50, Left$(s$, 2), CT
  Text 120, 60, "Z", RB, #1, 2, RGB(RED)
Next
""", 8),
    ("TEXT: a String variable as justification", """CLS
Dim Integer k
Dim String just = "LB"
For k = 1 To 20
  Text 40, 40, "Q" + Chr$(64 + k), just
Next
""", 8),
    ("CIRCLE", """CLS
Dim Integer k
Dim Float r = 7.5
For k = 1 To 20
  Circle 40, 40, 5 + k Mod 9, 1, 1.25, RGB(CYAN), -1
  Circle 100.4, 30, r, 2, , RGB(YELLOW), RGB(MAGENTA)
  Circle 130, 60 - k, 3
Next
""", 8),
    ("TRIANGLE, ARC and RBOX", """CLS
Dim Integer k, c = RGB(WHITE)
For k = 1 To 10
  Triangle 10, 10, 60, 20 + k, 30, 70, c, -1
  Arc 110, 40, 10, , 30 * k, 200, RGB(GREEN)
  Arc 110, 40, 20, 25, 0, 90 + k, c
  RBox 70, 5, 40, 30, 6, RGB(RED), RGB(BLUE)
  RBox 5, 50, 40, 20
Next
""", 8),
    ("array TRIANGLE stays text", """CLS
Dim Integer xa(1) = (10, 50), ya(1) = (10, 60), xb(1) = (40, 90), yb(1) = (20, 30), xc(1) = (20, 70), yc(1) = (70, 75), cc(1) = (RGB(WHITE), RGB(RED))
Triangle xa(), ya(), xb(), yb(), xc(), yc(), cc(), cc()
""", None),
    ("a FUNCTION call in TEXT stays text", """CLS
Dim Integer k
Function Nm$(n)
  Nm$ = "N" + Str$(n)
End Function
For k = 1 To 5
  Text 10, 10 * k, Nm$(k)
Next
""", None),
    ("TEXT: invalid font", """Dim Integer k
For k = 1 To 3
  Text 10, 10, "x", "LT", 1 + 98 * (k = 3)
Next
""", None),
    ("TEXT: bad justification", """Text 10, 10, "x", "QQ"
""", None),
    ("CIRCLE under OPTION LEGACY", """Option Legacy On
On Error Skip 1
Circle 100, 100, 20
Print MM.ErrNo; " "; MM.ErrMsg$
Option Legacy Off
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
    body = [l for l in lines if not l.startswith("STAT ")]
    return body, (stat[0][5:] if stat else "")


for name, src, most_text in PROGS:
    outs = {}
    for mode in ("OFF", "ON", "SHADOW"):
        b.cmd("OPTION COMPILE " + mode, 10)
        outs[mode] = run(src + (CK if src.startswith("CLS") else STAT))
    same = outs["OFF"][0] == outs["ON"][0] == outs["SHADOW"][0]
    m = re.search(r"RAN (\d+) MISS \d+ CODE (\d+)", outs["ON"][1])
    text = int(m.group(1)) - int(m.group(2)) if m else -1
    good = same and (most_text is None or 0 <= text <= most_text)
    ok = ok and good
    print("%-4s %-40s text %-5d %s" % ("ok" if good else "BAD", name, text, " | ".join(outs["ON"][0])[-70:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s  [%s]" % (mode, outs[mode][0], outs[mode][1]))

TIMING = """CLS
Dim Integer k, n = 2000
Dim Float t0, tT, tC
t0 = Timer
For k = 1 To n
  Text 10, 10, "Score " + Str$(k), "LT", 1, 1, RGB(WHITE), 0
Next
tT = Timer - t0
t0 = Timer
For k = 1 To n
  Circle 60, 60, 4, 1, 1, RGB(GREEN), -1
Next
tC = Timer - t0
Print "T"; tT; tC
"""
times = {}
for mode in ("OFF", "ON"):
    b.cmd("OPTION COMPILE " + mode, 10)
    b.upload(TIMING, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120))
    m = re.search(r"T\s*([\d.]+)\s+([\d.]+)", out)
    times[mode] = (float(m.group(1)), float(m.group(2))) if m else None
if times["OFF"] and times["ON"]:
    (oT, oC), (nT, nC) = times["OFF"], times["ON"]
    good = nT < oT and nC < oC
    ok = ok and good
    print("%-4s timing, 2000 each: TEXT %.0f -> %.0f ms, CIRCLE %.0f -> %.0f ms (OFF -> ON)" % ("ok" if good else "BAD", oT, nT, oC, nC))
else:
    ok = False
    print("BAD  timing: no result", times)
b.cmd("OPTION COMPILE " + ("OFF" if was.startswith("OFF") else "ON"), 10)
print("RBDRAW", "PASS" if ok else "FAIL")
b.close()
