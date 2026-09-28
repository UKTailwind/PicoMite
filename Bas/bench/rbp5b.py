"""rbp5b.py PORT - Route B P5b: BOX, LINE and PIXEL through the value splice
(docs/Interpreter_RouteB_Design.html).  Every program runs with OPTION
COMPILE OFF, ON and SHADOW and must print the same, errors included; what
it draws is read back with the Pixel() function and summed.  A line
starting TIME is left out of the comparison and shown.  Leaves OPTION
COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
SUM = """Function Area(x0 As Integer, y0 As Integer, x1 As Integer, y1 As Integer) As Integer
  Local Integer x, y, s
  For y = y0 To y1 : For x = x0 To x1
    If Pixel(x, y) <> 0 Then s = s + (x * 7 + y * 13 + (Pixel(x, y) And 255)) Mod 1000
  Next : Next
  Area = s
End Function
"""
PROGS = [
    ("box forms", SUM + """CLS
Dim Integer i, x, y
Dim Float f = 2.6
For i = 0 To 5
  x = i * 12 : y = i * 5
  Box x, y, 10, 6
  Box x, y + 40, 10, 6, 2, RGB(255, 0, 0)
  Box x + f, y + 80, 10 - f, 6, , RGB(0, 255, 0), RGB(0, 0, 255)
  Box x, y + 120, -8, -5, 1, , RGB(255, 255, 0)
Next
Print Area(0, 0, 90, 170)
""", True),
    ("line forms", SUM + """CLS
Dim Integer i, x, y
For i = 0 To 6
  x = i * 10 : y = i * 7
  Line x, y, x + 30, y + 20
  Line x, y + 60, x + 25, y + 50, 3, RGB(255, 0, 255)
  Line x, y + 100, , , 1, RGB(0, 255, 255)
  Line x + 0.4, y + 130, x + 20.6, y + 131, -2
Next
Print Area(0, 0, 100, 180)
""", True),
    ("pixel", SUM + """CLS
Dim Integer i
For i = 0 To 40
  Pixel i, i \\ 2
  Pixel i + 50, i, RGB(128, 64, 32)
  Pixel i * 1.5, 60 + i / 3, i * 5000
Next
Print Area(0, 0, 100, 80)
""", True),
    ("box colour error", """CLS
Dim Integer i, c = RGB(0, 0, 255)
For i = 1 To 3
  Box 10, 10, 20, 20, 1, c
  Print "ok"; i
  c = c * 1000
Next
""", None),
    ("line width error", """CLS
Dim Integer i, w = 50
For i = 1 To 3
  Line 0, 0, 30, 30, w
  Print "ok"; i
  w = w * 3
Next
""", None),
    ("argument count", """CLS
Dim Integer x = 5
Print "go"
Box x, x, x
""", None),
    ("legacy", SUM + """CLS
Dim Integer i
Option Legacy On
For i = 0 To 3
  Line (i * 5, 0)-(i * 5 + 20, 30), RGB(255, 255, 255)
  Pixel(i, 40) = RGB(255, 255, 255)
Next
Option Legacy Off
For i = 0 To 3
  Line i * 5, 50, i * 5 + 20, 80
Next
Print Area(0, 0, 50, 90)
""", None),
    ("modern under legacy", """CLS
Dim Integer i, z = 0
Option Legacy On
Print "go"
Line 1 \\ z, 0, 10, 10
""", None),
    ("line aa and arrays", SUM + """CLS
Dim Integer i, xs(3) = (10, 20, 30, 40), ys(3) = (5, 25, 5, 25), ws(3) = (5, 5, 5, 5)
For i = 0 To 2
  Line Aa 0, i * 10, 60, i * 10 + 30
  Box xs(), ys(), ws(), ws()
Next
Print Area(0, 0, 70, 70)
""", None),
    ("function argument", SUM + """CLS
Dim Integer n, i
Function Nx(v As Integer) As Integer
  n = n + 1
  Nx = v * 3
End Function
For i = 0 To 4
  Box Nx(i), 10, 5, 5
Next
Print n; Area(0, 0, 40, 20)
""", None),
    ("speed", """CLS
Dim Integer i, x, y
Timer = 0
For i = 1 To 3000
  x = i Mod 200 : y = (i \\ 200) * 4
  Box x, y, 3, 3, 1, RGB(0, 128, 255)
  Line x, y + 100, x + 5, y + 104
Next
Print "done"
Print "TIME "; Timer
""", True),
]

b = pc3.PC3(sys.argv[1])
b.attention()
ok = True


def run(src):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(180)).replace("\r", "")
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
print("RBP5B", "PASS" if ok else "FAIL")
b.close()
