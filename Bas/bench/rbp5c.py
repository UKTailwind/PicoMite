"""rbp5c.py PORT - Route B P5c: INC, and the string functions through the value
splice (docs/Interpreter_RouteB_Design.html).  Every program runs with OPTION
COMPILE OFF, ON and SHADOW and must print the same, errors included.  A
program with a count needs at least that many compiled statements (CODE) in
its compiled run: its loops run statements that compile only with P5c.  A
line starting TIME is left out of the comparison and shown.  Leaves OPTION
COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("inc scalars", """Dim Integer i, n = 5
Dim Float f = 1.5
Dim s$ = "ab"
For i = 1 To 100
  Inc n
  Inc n, 3
  Inc n, 2.5
  Inc f
  Inc f, i
  Inc f, 0.25
Next
Print n; f
Inc s$, "cd" : Inc s$, s$ : Print s$; Len(s$)
""", 400),
    ("inc elements", """Dim Integer a(5), b(2, 3), i, j
Dim Float c(4)
For i = 0 To 5
  Inc a(i), i * 2
  Inc a(i)
  For j = 0 To 3 : Inc b(i Mod 3, j), i + j : Next
Next
For i = 0 To 4 : Inc c(i), i / 3 : Inc c(i) : Next
Print a(0); a(3); a(5); b(0, 0); b(2, 3); c(1); c(4)
""", 40),
    ("inc in a sub", """Sub Tst(n As Integer)
  Local Integer k, s
  Local t$
  For k = 1 To n : Inc s, k : Inc t$, Chr$(64 + k) : Next
  Print s; " "; t$
End Sub
Tst 5 : Tst 10
""", 20),
    ("inc wraps and overflows", """Dim Integer n = 9223372036854775807
Dim Float f = 1e308
Inc n
Print n
Inc f, 1e308
Print f
""", None),
    ("inc errors: const", """Const K = 3
Inc K
""", None),
    ("inc errors: string no value", """Dim s$ = "a"
Inc s$
""", None),
    ("inc errors: too long", """Dim s$ Length 5 = "abc"
Inc s$, "de"
Print s$
Inc s$, "f"
""", None),
    ("inc errors: type", """Dim Integer n
Inc n, "a"
""", None),
    ("len asc chr", """Dim a$ = "Hello", i, n
For i = 1 To 50
  n = n + Len(a$) + Asc(a$) + Len(Chr$(65 + i Mod 26))
Next
Print n; Asc(""); Len(""); Chr$(66); Asc(Chr$(200))
""", 90),
    ("chr error", """Print Chr$(256)
""", None),
    ("mid left right", """Dim a$ = "abcdefgh", b$, i
For i = 1 To 30
  b$ = Mid$(a$, 1 + i Mod 8) + Mid$(a$, 2, 3) + Left$(a$, i Mod 10) + Right$(a$, i Mod 4)
Next
Print b$; " "; Mid$(a$, 9); "|"; Mid$(a$, 8, 5); "|"; Left$(a$, 0); "|"; Right$(a$, 20)
Print Left$(Mid$(a$, 2), 3); Right$(Left$(a$, 5), 2)
""", 30),
    ("mid error", """Dim a$ = "abc"
Print Mid$(a$, 0)
""", None),
    ("instr", """Dim a$ = "the cat sat on the mat", n, i
For i = 1 To 30
  n = n + Instr(a$, "at") + Instr(i Mod 20 + 1, a$, "the") + Instr(a$, "dog")
Next
Print n; Instr(5, a$, "t"); Instr(a$, "")
""", 30),
    ("str space trim", """Dim x = 3.14159, i, s$
For i = 1 To 20
  s$ = Str$(i) + Str$(x * i, 4, 2) + Space$(i Mod 3) + "|" + Trim$("  ab  ") + Trim$("xxhixx", "x")
Next
Print s$; Str$(-7); Str$(1234567, 12); Str$(2.5, 5, 3, "0")
""", 20),
    ("case", """Dim a$ = "MiXeD 123", b$, i
For i = 1 To 20 : b$ = UCase$(a$) + LCase$(a$) : Next
Print b$; UCase$(""); LCase$("ABC")
""", 20),
    ("hex oct bin", """Dim i, s$
For i = 1 To 20 : s$ = Hex$(i * 4099) + Oct$(i) + Bin$(i, 8) + Hex$(-1) : Next
Print s$; Hex$(255, 4); Bin$(5)
""", 20),
    ("in conditions", """Dim a$ = "hello", n, i
For i = 1 To 40
  If Len(a$) > 3 And Left$(a$, 1) = "h" Then Inc n
  If Mid$(a$, i Mod 5 + 1, 1) = "l" Then Inc n, 10
Next
Do While Len(a$) < 12 : a$ = a$ + "!" : Loop
Print n; a$
""", 80),
    ("select on a string function", """Dim a$ = "abcabc", n, i
For i = 1 To 6
  Select Case Mid$(a$, i, 1)
    Case "a" : Inc n
    Case "b" : Inc n, 10
    Case Else : Inc n, 100
  End Select
Next
Print n
""", 10),
    ("a function argument stays text", """Function Two$(s$)
  Two$ = s$ + s$
End Function
Dim a$ = "xy"
Print Len(Two$(a$)); Left$(Two$(a$), 3)
""", None),
    ("speed", """Dim a$ = "The quick brown fox", i, n, b$
Timer = 0
For i = 1 To 5000
  n = n + Len(a$) + Instr(a$, "fox")
  b$ = Left$(a$, 3) + Mid$(a$, 5, 5)
  Inc n, Asc(Right$(a$, 1))
Next
Print n; " "; b$
Print "TIME "; Timer
""", 15000),
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
    good = same and (want_code is None or code >= want_code)
    ok = ok and good
    print("%-4s %-30s CODE %-6d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-90:]))
    if outs["OFF"][2]:
        print("     time OFF %s  ON %s" % (outs["OFF"][2][0][5:].strip(), outs["ON"][2][0][5:].strip() if outs["ON"][2] else "?"))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))
b.cmd("OPTION COMPILE OFF", 10)
print("RBP5C", "PASS" if ok else "FAIL")
b.close()
