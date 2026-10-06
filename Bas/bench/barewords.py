"""barewords.py PORT [--compile] - unquoted words in statements, at the prompt and in a program.

From V7 a saved program stores every name as a symbol (core/Symbols.h).  Most
commands are handed their statement with the symbols spelt out again, but the
ones on the symbol-aware lists (SymAwareNames / SymAwareFunNames in Symbols.c)
see the symbols, so any of them that reads a bare word's letters itself breaks
in a program while working at the prompt - which is how TEXT's justification
(TEXT x, y, s$, CM) broke in V7.0.00b1.

SINGLE: every statement form of a symbol-aware command or function (from the
manual) that carries an unquoted word, one line each.  Each runs at the prompt
(text, as in 6.03) and as a one-line program (symbols); the two must print the
same.  MULTI: forms that need several lines, checked against their expected
output.  --compile also runs the programs with OPTION COMPILE ON (RP2350).
Leaves OPTION COMPILE OFF.
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

SINGLE = [
    'Text 10, 10, "x", CM',
    'Text 10, 20, "x", RB, 1, 1',
    'Text 10, 30, "x", LTV',
    'Box 10, 10, 20, 20, 1, RGB(red), RGB(blue)',
    'RBox 10, 10, 20, 20, 3, RGB(red), RGB(blue)',
    'Circle 50, 50, 10, 1, 1, RGB(red), RGB(blue)',
    'Arc 50, 50, 10, 20, 0, 90, RGB(red)',
    'Triangle 0, 0, 10, 0, 5, 5, RGB(red), RGB(blue)',
    'Triangle Save #1, 0, 0, 10, 0, 5, 5 : Triangle Restore #1',
    'Pixel 5, 5, RGB(white)',
    'Line 0, 0, 50, 50, 1, RGB(green)',
    'Line AA 0, 0, 50, 50, 1, RGB(green)',
    'Dim ly(9) : Line Plot ly()',
    'Dim gx(9), gy(9) : Line Graph gx(), gy(), RGB(red)',
    'Dim cma%(9), cmb%(9) : Colour Map cma%(), cmb%()',
    'Colour RGB(white), RGB(black)',
    'Color RGB(white), RGB(black)',
    'CLS RGB(black)',
    'Font 1, 1',
    'Font #1',
    'Blit Read #1, 0, 0, 8, 8 : Blit Write #1, 10, 10 : Blit Close #1',
    'Blit 0, 0, 10, 10, 8, 8',
    'FrameBuffer Create : Blit FrameBuffer N, F, 0, 0, 0, 0, 8, 8 : Blit FrameBuffer F, N, 0, 0, 0, 0, 8, 8 : FrameBuffer Close',
    'Sprite Read #1, 0, 0, 8, 8 : Sprite Show #1, 20, 20, 1 : Sprite Hide #1 : Sprite Close #1',
    'Sprite Hide All',
    'Sprite Close All',
    'Sprite NoInterrupt',
    'Print Bin2str$(INT64, 5, BIG) = Bin2str$(INT64, 5, BIG)',
    'Print Str2bin(INT64, Bin2str$(INT64, 5, BIG), BIG)',
    'Print Str2bin(DOUBLE, Bin2str$(DOUBLE, 1.5))',
    'Print Str2bin(UINT8, Bin2str$(UINT8, 200))',
    'Print Trim$("  ab  ", " ", R) + "|"',
    'Print Trim$("  ab  ", " ", L) + "|"',
    'Print Trim$("  ab  ", " ", B) + "|"',
    'Print @(10, 10) "x"',
    'Dim Integer di1(3) : Dim di2 As Float : Dim di3$ Length 10 : Dim String di4 Length 5 = "ab" : Print di4; di2',
    'Const K1 = 5 : Print K1',
    'For i = 1 To 3 Step 1 : Next i : Print i',
    'Do While 0 : Loop : Print "w"',
    'Do : Loop Until 1 : Print "u"',
    'i = 1 : Inc i, 2 : Print i',
    'Print Max(3, 7); Min(3, 7); LCase$("AB"); UCase$("ab"); Left$("abc", 2)',
    # commands and functions NOT on the symbol-aware lists: their statement is
    # spelt out before they run, so these show the spelling-out path
    'SetPin gp2, DOUT : Pin(gp2) = 1 : Print Pin(gp2) : SetPin gp2, OFF',
    'Option Angle Degrees : Print Int(Sin(90)) : Option Angle Radians',
    'WS2812 B, gp7, 1, RGB(red)',
    'Print MM.Info$(PLATFORM) = MM.Info$(PLATFORM); MM.Info(FONTWIDTH) > 0',
    'Dim ma(2) : Math Set 3, ma() : Print Math(SUM ma())',
    'Play Stop',
]

MULTI = [
    ("select", """Dim Integer v
For v = 1 To 4
  Select Case v
    Case Is < 2
      Print "lt";
    Case 2 To 3
      Print "in";
    Case Else
      Print "else";
  End Select
Next
Print
""", "ltininelse"),
    ("types", """Dim Integer a1(2) = (1, 2, 3)
Dim f As Float = 1.5
Dim s$ Length 8 = "abc"
Dim String t Length 4 = "xy"
Sub S1(x As Integer, y As Float, z As String)
  Local Integer m = 2
  Local n As Float
  Static Integer c
  c = c + 1
  Print x * m; y; z; c
End Sub
S1 a1(2), f, s$ + t
S1 1, 2, t
""", " 6 1.5abcxy 1\n 2 2xy 2"),
    ("call", """Dim Integer a = 3, k(2) = (1, 2, 3)
Dim Float f = 1.5
Dim s$ = "ab", n$(1) = ("Bump", "Show")
Sub Bump(x As Integer, y As Float)
  x = x + 1 : y = y * 2
End Sub
Sub Show(x As Integer, y As Float, z As String)
  Print x; y; z
End Sub
Function Sum(x As Integer, y As Integer) As Integer
  Sum = x + y
End Function
Call n$(0), a, f
Call "Show", a, f, s$ + "c"
Call n$(1), k(2), f / 3, s$
Print Call("Sum", a, k(1) * 10)
""", " 4 3abc\n 3 1ab\n 24"),
    ("functions", """Function Twice(v As Integer) As Integer
  Twice = v * 2
End Function
Function Half(v) As Float
  Half = v / 2
End Function
Print Twice(4); Half(5)
""", " 8 2.5"),
    ("labels", """GoSub there
GoTo done
there:
Print "sub";
Return
done:
Print "end"
""", "subend"),
    ("exit", """Dim Integer i, j
For i = 1 To 5
  If i = 2 Then Continue For
  If i = 4 Then Exit For
  Print i;
Next
Do
  j = j + 1
  If j = 3 Then Exit Do
Loop
Print j
""", " 1 3 3"),
    ("struct", """Type Pt
  x As Integer
  y As Float
  n As String Length 5
End Type
Dim p As Pt
p.x = 3 : p.y = 1.5 : p.n = "ab"
Print p.x; p.y; p.n
""", " 3 1.5ab"),
]


def clean(out):
    out = pc3.ANSI.sub("", out).replace("\r", "")
    return [l.rstrip() for l in out.split("\n") if l.strip() and l.strip() not in ("RUN", ">")]


b = pc3.PC3(sys.argv[1])
b.attention()
modes = ["OFF", "ON"] if "--compile" in sys.argv else [None]
bad = 0
struct_ok = True
for mode in modes:
    if mode:
        b.cmd("OPTION COMPILE " + mode, 10)
    print("== OPTION COMPILE %s" % (mode or "(not set)"))
    for line in SINGLE:
        b.cmd("Clear", 10)
        # the prompt echoes the line (wrapped at the screen width): take only
        # what follows the marker it prints first
        out = clean(b.cmd('Print "@@" : ' + line, 20))
        prompt = out[out.index("@@") + 1:] if "@@" in out else out
        b.upload(line + "\n", 30)
        b.drain(0.1)
        prog = clean(b.run(30))
        # a program's error names its line: "[1] <line>" then "Error : ..."
        prog = [l for l in prog if not (l.startswith("[") and "]" in l[:6])]
        same = prompt == prog
        bad += not same
        print("%-4s %-60s %s" % ("ok" if same else "BAD", line[:60], " | ".join(prog)[:60]))
        if not same:
            print("     prompt:  %s" % " | ".join(prompt))
            print("     program: %s" % " | ".join(prog))
    for name, src, want in MULTI:
        b.upload(src, 30)
        b.drain(0.1)
        got = "\n".join(clean(b.run(30)))
        if name == "struct" and ("Unknown command" in got or "Invalid" in got) and "Type" in got:
            print("--   %-60s (no structures on this build)" % name)
            continue
        good = got == want
        bad += not good
        print("%-4s %-60s %s" % ("ok" if good else "BAD", name, got.replace("\n", " / ")[:60]))
        if not good:
            print("     want: %s" % want.replace("\n", " / "))
if "--compile" in sys.argv:
    b.cmd("OPTION COMPILE OFF", 10)
print("BAREWORDS", "PASS" if bad == 0 else "FAIL (%d)" % bad)
b.close()
