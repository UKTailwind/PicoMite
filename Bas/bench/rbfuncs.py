"""rbfuncs.py PORT - Route B: built-in functions through the function splice
(item 8 of docs/Interpreter_RouteB_Coverage.html): ASIN, ACOS, ATAN2, CINT,
MAX, MIN, PIXEL( and KEYDOWN(; the functions with no argument (TIMER, INKEY$,
POS, DATE$, TIME$); MM.HRES, MM.ERRNO and the other integer ~(letter) forms;
PEEK's INT8 (BYTE), WORD, SHORT, INTEGER, FLOAT and VAR forms; MATH(RAND).
VAL(, MM.INFO(, PEEK(VARADDR and a FUNCTION call in the arguments stay text.
Every program runs with OPTION COMPILE OFF, ON and SHADOW and must print the
same, errors included; where the statements should compile, the ON run's
statements run as text (RAN - CODE) must stay under the limit.  Then OFF and
ON timings of a PIXEL( checksum and a TIMER loop.  Leaves OPTION COMPILE as
it found it."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
# (name, program, most statements run as text with OPTION COMPILE ON, or None)
PROGS = [
    ("ASIN ACOS ATAN2 CINT MAX MIN", """Dim Integer k, ci
Dim Float s, t
For k = -10 To 10
  s = s + ASin(k / 10) + ACos(k / 20) + Atan2(k, 3) + Atan2(1.5, k + 0.5)
  ci = ci + Cint(k / 3 + 0.4) + Max(k, 2, -k) + Min(k * 1.5, 3)
  t = t + Max(k, 0) - Min(-k, 0.5) + Max(k)
Next
Print s; ci; t
""", 6),
    ("the same in degrees", """Option Angle Degrees
Dim Integer k
Dim Float s
For k = -10 To 10
  s = s + ASin(k / 10) + ACos(k / 20) + Atan2(k, 3)
Next
Print s
Option Angle Radians
""", 6),
    ("ASIN out of range", """Dim Integer k
Dim Float s
For k = 1 To 5
  s = s + ASin(k / 4)
Next
""", None),
    ("TIMER INKEY$ POS DATE$ TIME$", """Dim Integer k, bad, n, p
Dim Float t0, t
t0 = Timer
For k = 1 To 200
  t = Timer
  If t < t0 Then bad = bad + 1
  t0 = t
  If Inkey$ <> "" Then n = n + 1
Next
Print "ab";
p = Pos
Print
Print bad; n; p; Len(Date$); Len(Time$); Timer >= t0
""", 8),
    ("MM.HRES MM.VRES MM.FONTHEIGHT MM.ERRNO MM.ERRLINE", """Dim Integer k, a, e, l
Dim Float x
For k = 1 To 20
  a = a + MM.HRes + MM.VRes * 2 + MM.FontHeight
Next
On Error Skip 1
x = 1 / 0
e = MM.Errno
l = MM.ErrLine
Print a; e; l > 0
""", 8),
    ("KEYDOWN", """Dim Integer k, n
For k = 0 To 7
  n = n + KeyDown(k)
Next
Print n
""", 4),
    ("PEEK forms", """Dim Integer a = &H12345678, b(3) = (1, 2, 3, 4), k, s, ad, fa
Dim Float f = 1.5, g
ad = Peek(VarAddr a)
fa = Peek(VarAddr f)
For k = 0 To 7
  s = s + Peek(Byte ad + (k And 3)) + Peek(Var b(), k * 8) + Peek(Short ad + 2 * (k And 1)) + Peek(Var a, k And 3)
  s = s + Peek(Word ad) + Peek(INTEGER ad)
  g = g + Peek(Float fa)
Next
Print s; g
""", 6),
    ("MATH(RAND)", """Dim Integer k, bad
Dim Float r
For k = 1 To 200
  r = Math(Rand)
  If r < 0 Or r >= 1 Then bad = bad + 1
Next
Print bad
""", 4),
    ("PIXEL( checksum", """CLS
Dim Integer x, y, ck
Box 10, 10, 30, 20, 1, RGB(RED), RGB(BLUE)
Circle 60, 30, 12, 2, 1, RGB(GREEN)
For y = 0 To 49 : For x = 0 To 99 : ck = ck + Pixel(x, y) * (1 + ((x + 3 * y) And 15)) : Next : Next
st$ = MM.Info(COMPILE)
Print "CK"; ck
If Instr(st$, "RAN") Then Print "STAT "; Mid$(st$, Instr(st$, "RAN"))
""", None),
    ("VAL, MM.INFO(, PEEK(VARADDR stay text", """Dim Integer k, a, n
Dim Float v
For k = 1 To 5
  v = v + Val("1" + Str$(k))
  n = n + MM.Info(FONTHEIGHT)
  a = a + (Peek(VarAddr k) <> 0)
Next
Print v; n; a
""", None),
    ("a FUNCTION call as an argument stays text", """Dim Integer k
Dim Float s
Function Half(x)
  Half = x / 2
End Function
For k = 1 To 5
  s = s + ASin(Half(k / 5))
Next
Print s
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
        outs[mode] = run(src if "STAT" in src else src + STAT)
    same = outs["OFF"][0] == outs["ON"][0] == outs["SHADOW"][0]
    m = re.search(r"RAN (\d+) MISS \d+ CODE (\d+)", outs["ON"][1])
    text = int(m.group(1)) - int(m.group(2)) if m else -1
    good = same and (most_text is None or 0 <= text <= most_text)
    if name == "PIXEL( checksum":
        # the checksum loop's 5,000 PIXEL( statements compile: under 20 run as text
        m0 = re.search(r"RAN (\d+) MISS \d+ CODE (\d+)", outs["ON"][1])
        good = good and bool(m0) and int(m0.group(1)) - int(m0.group(2)) < 20
        text = int(m0.group(1)) - int(m0.group(2)) if m0 else -1
    ok = ok and good
    print("%-4s %-46s text %-5d %s" % ("ok" if good else "BAD", name, text, " | ".join(outs["ON"][0])[-60:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s  [%s]" % (mode, outs[mode][0], outs[mode][1]))

TIMING = """CLS
Dim Integer x, y, ck, k
Dim Float t0, tP, tT, t
Box 0, 0, 100, 100, 1, RGB(RED), RGB(BLUE)
t0 = Timer
For y = 0 To 99 : For x = 0 To 99 : ck = ck + Pixel(x, y) : Next : Next
tP = Timer - t0
t0 = Timer
For k = 1 To 10000
  t = Timer
Next
tT = Timer - t0
Print "T"; tP; tT
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
    (oP, oT), (nP, nT) = times["OFF"], times["ON"]
    good = nP < oP and nT < oT
    ok = ok and good
    print("%-4s timing: 10,000 PIXEL( %.0f -> %.0f ms, 10,000 t = TIMER %.0f -> %.0f ms (OFF -> ON)" % ("ok" if good else "BAD", oP, nP, oT, nT))
else:
    ok = False
    print("BAD  timing: no result", times)
b.cmd("OPTION COMPILE " + ("OFF" if was.startswith("OFF") else "ON"), 10)
print("RBFUNCS", "PASS" if ok else "FAIL")
b.close()
