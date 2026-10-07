"""make_perfguide.py - write perfguide.bas and perfguide_off.bas.

perfguide.bas times the constructs that docs/MMBasic_Performance_Guide.md
compares.  Every test runs three times and reports its best time; a line

    R <id> <passes> <best ms>

per test, so us per pass = best ms * 1000 / passes.  perfguide_off.bas is the
same program with OPTION SYMBOLS OFF as its first statement.

Two kinds of test:
  body   - lines run inside the standard loop "For i = 1 To np ... Next"
  custom - lines that time themselves between t0 = Timer and dt = Timer - t0
"""
import os

N = 20000      # passes for most tests
NL = 5000      # passes for the LINE tests (each draws 91 pixels)

T = []         # (id, kind, title, passes, lines)


def body(tid, title, lines, passes=N):
    T.append((tid, "body", title, passes, lines))


def custom(tid, title, lines, passes=N):
    T.append((tid, "custom", title, passes, lines))


# ---- the empty loop, for reference
body("0", "empty FOR loop (the loop's own cost)", [])

# ---- 1 statements per line
body("1a", "three assignments, one per line", ["a = b", "b = c", "c = d"])
body("1b", "three assignments, one line", ["a = b : b = c : c = d"])

# ---- 2 loop forms (20000 passes each)
custom("2a", "FOR float / NEXT", ["For i = 1 To 20000", "Next"])
custom("2b", "FOR integer / NEXT", ["For ii% = 1 To 20000", "Next"])
custom("2c", "DO / INC / LOOP UNTIL i >= 20000", ["i = 0", "Do", "  Inc i", "Loop Until i >= 20000"])
custom("2d", "DO / INC / LOOP UNTIL i >= n (a variable)", ["i = 0", "Do", "  Inc i", "Loop Until i >= n"])
custom("2e", "DO WHILE i < 20000 / INC / LOOP", ["i = 0", "Do While i < 20000", "  Inc i", "Loop"])
custom("2f", "DO / i = i + 1 / LOOP UNTIL", ["i = 0", "Do", "  i = i + 1", "Loop Until i >= 20000"])
custom("2g", "integer DO WHILE / INC / LOOP", ["ii% = 0", "Do While ii% < 20000", "  Inc ii%", "Loop"])
custom("2h", "GOTO loop (IF ... THEN GOTO)", ["i = 0", "Lp2h:", "Inc i", "If i < 20000 Then GoTo Lp2h"])

# ---- 3 NEXT with or without the variable (100 x 200 = 20000 inner passes)
custom("3a", "nested, NEXT : NEXT", ["For j = 1 To 100", "  For k = 1 To 200", "  Next", "Next"])
custom("3b", "nested, NEXT k : NEXT j", ["For j = 1 To 100", "  For k = 1 To 200", "  Next k", "Next j"])
custom("3c", "nested, NEXT k, j", ["For j = 1 To 100", "  For k = 1 To 200", "Next k, j"])

# ---- 4 the limit of a FOR and the condition of a DO
custom("4a", "FOR limit a variable", ["For i = 1 To n", "Next"])
custom("4b", "FOR limit a1 + b1 + c1 + Sqr(x1)", ["For i = 1 To a1 + b1 + c1 + Sqr(x1)", "Next"])
custom("4c", "DO WHILE i < a1 + b1 + c1 + Sqr(x1)", ["i = 0", "Do While i < a1 + b1 + c1 + Sqr(x1)", "  Inc i", "Loop"])
custom("4d", "the same limit worked out once before the DO", ["m = a1 + b1 + c1 + Sqr(x1)", "i = 0", "Do While i < m", "  Inc i", "Loop"])

# ---- 5 literals, constants, variables
body("5a", "a = b + 10", ["a = b + 10"])
body("5b", "a = b + K10 (CONST)", ["a = b + K10"])
body("5c", "a = b + v10 (variable)", ["a = b + v10"])
body("5d", "a = b * 3.14159265", ["a = b * 3.14159265"])
body("5e", "a = b * KPI (CONST)", ["a = b * KPI"])
body("5f", "a = b * vpi (variable)", ["a = b * vpi"])
body("5g", "a = b * Pi", ["a = b * Pi"])
body("5h", "a = b * 1.5E-3", ["a = b * 1.5E-3"])
body("5i", "Line 10, 10, 100, 100", ["Line 10, 10, 100, 100"], NL)
body("5j", "Line lx1, ly1, lx2, ly2 (variables)", ["Line lx1, ly1, lx2, ly2"], NL)
body("5k", "Line KX1, KY1, KX2, KY2 (CONSTs)", ["Line KX1, KY1, KX2, KY2"], NL)
body("5l", "Line ix1%, iy1%, ix2%, iy2% (integer variables)", ["Line ix1%, iy1%, ix2%, iy2%"], NL)

# ---- 6 calls
body("6a", "a = b + c written in place", ["a = b + c"])
body("6b", "Sub0 (no parameters, empty)", ["Sub0"])
body("6c", "AddIt b, c", ["AddIt b, c"])
body("6d", "Call \"AddIt\", b, c", ["Call \"AddIt\", b, c"])
body("6e", "Call f$, b, c", ["Call f$, b, c"])
body("6f", "a = FAdd(b, c)", ["a = FAdd(b, c)"])
body("6g", "a = Call(\"FAdd\", b, c)", ["a = Call(\"FAdd\", b, c)"])
body("6h", "GoSub AddG", ["GoSub AddG"])
body("6i", "SubLoc (three LOCALs, otherwise empty)", ["SubLoc"])

# ---- 7 name length
body("7a", "a = b + c", ["a = b + c"])
body("7b", "16-17 letter names", ["TheFirstVariable = TheSecondVariable + TheThirdVariable"])
body("7c", "30 letter names", ["ThisIsAVeryLongVariableNameNo1 = ThisIsAVeryLongVariableNameNo2 + ThisIsAVeryLongVariableNameNo3"])
body("7d", "AddTwoNumbersAndStoreTheResult b, c", ["AddTwoNumbersAndStoreTheResult b, c"])
body("7e", "a = RGB(rr, gg, bb)", ["a = RGB(rr, gg, bb)"])
body("7f", "RGB( with 26-28 letter names", ["a = RGB(TheRedComponentOfTheColour, TheGreenComponentOfTheColour, TheBlueComponentOfTheColour)"])

# ---- 8 integers and floats
body("8a", "a = b + c (float)", ["a = b + c"])
body("8b", "ia% = ib% + ic%", ["ia% = ib% + ic%"])
body("8c", "a = b * c", ["a = b * c"])
body("8d", "ia% = ib% * ic%", ["ia% = ib% * ic%"])
body("8e", "a = b / c", ["a = b / c"])
body("8f", "ia% = ib% \\ ic%", ["ia% = ib% \\ ic%"])
body("8g", "a = b ^ 2", ["a = b ^ 2"])
body("8h", "a = b * b", ["a = b * b"])
body("8i", "a = Sqr(b)", ["a = Sqr(b)"])
body("8j", "a = Sin(b)", ["a = Sin(b)"])

# ---- 9 locals and globals in a SUB (one call, n passes inside it)
custom("9a", "loop in a SUB on global variables", ["LoopGlobal"])
custom("9b", "loop in a SUB on LOCAL variables", ["LoopLocal"])
custom("9c", "loop in a SUB on LOCAL INTEGERs", ["LoopLocalInt"])

# ---- 10 arrays
body("10a", "a = x(5) + y(5)", ["a = x(5) + y(5)"])
body("10b", "a = x(q) + y(q)", ["a = x(q) + y(q)"])
body("10c", "a = m2(5, 5) + m2(6, 6)", ["a = m2(5, 5) + m2(6, 6)"])

# ---- 11 comments
body("11a", "a = b", ["a = b"])
body("11b", "a = b with a trailing ' comment", ["a = b ' a trailing comment of about forty chars"])
body("11c", "a ' comment line and a = b", ["' a comment line of about forty characters", "a = b"])
body("11d", "a REM line and a = b", ["Rem a comment line of about forty characters", "a = b"])

# ---- 12 IF and SELECT CASE
body("12a", "single-line IF b < c THEN a = 1", ["If b < c Then a = 1"])
body("12b", "multi-line IF / a = 1 / ENDIF", ["If b < c Then", "  a = 1", "EndIf"])
chain = ["If v8 = 1 Then", "  a = 1"]
for k in range(2, 9):
    chain += ["ElseIf v8 = %d Then" % k, "  a = %d" % k]
chain += ["EndIf"]
body("12c", "IF / 7 x ELSEIF, the last one true", chain)
sel = ["Select Case v8"]
for k in range(1, 9):
    sel += ["  Case %d" % k, "    a = %d" % k]
sel += ["End Select"]
body("12d", "SELECT CASE, 8 cases, the last one true", sel)

# ---- 13 strings
body("13a", "s$ = t$ + u$", ["s$ = t$ + u$"])
body("13b", "s$ = Str$(b)", ["s$ = Str$(b)"])
body("13c", "s$ = Left$(t$, 3)", ["s$ = Left$(t$, 3)"])

# ---- 14 a loop or a MATH command (20 x 1000 elements = 20000)
custom("14a", "sum 1000 elements in a FOR loop", ["For k = 1 To 20", "  tot = 0", "  For j = 0 To 999", "    tot = tot + arr(j)", "  Next", "Next"])
custom("14b", "the same with Math(SUM arr())", ["For k = 1 To 20", "  tot = Math(SUM arr())", "Next"])
custom("14c", "set 1000 elements in a FOR loop", ["For k = 1 To 20", "  For j = 0 To 999", "    arr(j) = 1", "  Next", "Next"])
custom("14d", "the same with Math Set 1, arr()", ["For k = 1 To 20", "  Math Set 1, arr()", "Next"])
custom("14e", "draw 1000 pixels in a FOR loop", ["For k = 1 To 2", "  For j = 0 To 999", "    Pixel px%(j), py%(j)", "  Next", "Next"], 2000)
custom("14f", "the same with Pixel px%(), py%()", ["For k = 1 To 2", "  Pixel px%(), py%()", "Next"], 2000)

# ---- 15 INC
body("15a", "Inc a", ["Inc a"])
body("15b", "a = a + 1", ["a = a + 1"])
body("15c", "Inc ia%", ["Inc ia%"])
body("15d", "ia% = ia% + 1", ["ia% = ia% + 1"])

# ---- 16 spaces
body("16a", "a=b+c (no spaces)", ["a=b+c"])

# ---- 17 work out a repeated part once
body("17a", "a = Sin(b) * c + Sin(b) * d", ["a = Sin(b) * c + Sin(b) * d"])
body("17b", "sx = Sin(b) : a = sx * c + sx * d", ["sx = Sin(b) : a = sx * c + sx * d"])


HEAD = """' perfguide.bas - times the constructs compared in MMBasic_Performance_Guide.
' Each test runs three times; R <id> <passes> <best ms> per test, so the
' time per pass in microseconds is best ms * 1000 / passes.
' Generated by make_perfguide.py: edit that, not this.
"""

DECL = """Option Console Serial
Dim Float a, b, c, d, i, j, k, m, n, q, t0, dt, best, rep, tot, sx
Dim Float v10, vpi, a1, b1, c1, x1, lx1, ly1, lx2, ly2
Dim Float TheFirstVariable, TheSecondVariable, TheThirdVariable
Dim Float ThisIsAVeryLongVariableNameNo1, ThisIsAVeryLongVariableNameNo2, ThisIsAVeryLongVariableNameNo3
Dim Integer rr, gg, bb, TheRedComponentOfTheColour, TheGreenComponentOfTheColour, TheBlueComponentOfTheColour
Dim ii%, ia%, ib%, ic%, ix1%, iy1%, ix2%, iy2%
Dim Integer v8
Dim Float arr(999), x(9), y(9), m2(9, 9)
Dim px%(999), py%(999)
Dim s$, t$, u$, f$
Const K10 = 10, KPI = 3.14159265
Const KX1 = 10, KY1 = 10, KX2 = 100, KY2 = 100
n = 20000
b = 2 : c = 3 : d = 4
v10 = 10 : vpi = 3.14159265
a1 = 5000 : b1 = 5000 : c1 = 9900 : x1 = 10000
lx1 = 10 : ly1 = 10 : lx2 = 100 : ly2 = 100
ix1% = 10 : iy1% = 10 : ix2% = 100 : iy2% = 100
TheSecondVariable = 2 : TheThirdVariable = 3
ThisIsAVeryLongVariableNameNo2 = 2 : ThisIsAVeryLongVariableNameNo3 = 3
rr = 10 : gg = 20 : bb = 30
TheRedComponentOfTheColour = 10 : TheGreenComponentOfTheColour = 20 : TheBlueComponentOfTheColour = 30
ib% = 7 : ic% = 3 : v8 = 8 : q = 5
t$ = "Hello" : u$ = "World" : f$ = "AddIt"
For j = 0 To 999 : px%(j) = 200 + (j Mod 100) : py%(j) = 20 + j \\ 100 : Next
Print "H "; MM.Device$; " "; MM.Ver; " "; MM.Info(CPUSPEED)
"""

TAIL = """Print "DONE"
Option Console Both
End

Sub Report(id$, ms, passes)
  Print "R "; id$; " "; Str$(passes); " "; Str$(ms, 0, 4)
End Sub

Sub Sub0
End Sub

Sub SubLoc
  Local la, lb, lc
End Sub

Sub AddIt(p1, p2)
  a = p1 + p2
End Sub

Sub AddTwoNumbersAndStoreTheResult(p1, p2)
  a = p1 + p2
End Sub

Function FAdd(p1, p2)
  FAdd = p1 + p2
End Function

Sub LoopGlobal
  For i = 1 To n
    a = b + c
  Next
End Sub

Sub LoopLocal
  Local li, la, lb, lc
  lb = 2 : lc = 3
  For li = 1 To n
    la = lb + lc
  Next
End Sub

Sub LoopLocalInt
  Local Integer li, la, lb, lc
  lb = 2 : lc = 3
  For li = 1 To n
    la = lb + lc
  Next
End Sub

AddG:
  a = b + c
  Return
"""


def test_block(tid, kind, title, passes, lines):
    out = ["' %s: %s" % (tid, title), "best = 1E9", "For rep = 1 To 3"]
    if kind == "body":
        out += ["  t0 = Timer", "  For i = 1 To %d" % passes]
        out += ["    " + l for l in lines]
        out += ["  Next"]
    else:
        pre = []
        rest = list(lines)
        # statements that set a loop up go before the clock starts
        while rest and rest[0] in ("i = 0", "ii% = 0"):
            pre.append(rest.pop(0))
        out += ["  " + l for l in pre]
        out += ["  t0 = Timer"]
        out += [("  " + l) if not l.endswith(":") else l for l in rest]
    out += ["  dt = Timer - t0", "  If dt < best Then best = dt", "Next rep",
            "Report \"%s\", best, %d" % (tid, passes), ""]
    return out


def program(symbols_off):
    lines = HEAD.splitlines()
    if symbols_off:
        lines += ["Option Symbols Off"]
    lines += DECL.splitlines() + [""]
    for t in T:
        lines += test_block(*t)
    lines += TAIL.splitlines()
    return "\r\n".join(lines) + "\r\n"


def titles():
    return {t[0]: (t[2], t[1], t[3]) for t in T}


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    for name, off in (("perfguide.bas", False), ("perfguide_off.bas", True)):
        data = program(off)
        with open(os.path.join(here, name), "w", newline="") as f:
            f.write(data)
        print(name, len(data), "bytes", data.count("\n"), "lines", len(T), "tests")
