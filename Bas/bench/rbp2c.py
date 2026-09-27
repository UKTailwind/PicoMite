"""rbp2c.py PORT - Route B P2c: compiled IF (docs/Interpreter_RouteB_Design.html).

A multi-line IF whose next arm is ELSE or ENDIF compiles its condition and
its jump; a single-line IF compiles when its THEN (and ELSE) parts are
assignments.  Every program runs with OPTION COMPILE OFF, ON and SHADOW and
must print the same; SHADOW also checks each condition's truth against the
text evaluator.  What stays text: ELSEIF chains, IF ... GOTO, THEN GOTO,
THEN linenumber, THEN with a command.  Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("multi-line", """Dim Integer i, n, c, d
Dim Float x
For i = 1 To 20
  x = i / 3
  If i Mod 3 = 0 Then
    c = c + 1
  Else
    d = d + i
  EndIf
  If x > 2.5 Then
    n = n + 1
  End If
  If i And 1 Then ' odd
    n = n + 10
  EndIf
Next
Print c; d; n
""" + STAT, lambda n: n > 0),
    ("nested", """Dim Integer i, j, a, b
For i = 1 To 6
  For j = 1 To 6
    If i < j Then
      If (i + j) Mod 2 Then
        a = a + 1
      Else
        b = b + 1
      EndIf
    EndIf
  Next
Next
Print a; b
""" + STAT, lambda n: n > 0),
    ("single-line", """Dim Integer i, a, b, c
Dim Float f
For i = 1 To 20
  If i > 10 Then a = a + 1
  If i Mod 2 = 0 Then b = b + 1 Else c = c + 1
  If i > 15 Then f = f + 0.5 : a = a + 100
  If i < 0 Then a = -1 : b = -1
Next
Print a; b; c; f
""" + STAT, lambda n: n > 0),
    ("float cond", """Dim Integer i, a
Dim Float x
For i = 1 To 10
  x = (i - 5) * 0.1
  If x Then a = a + 1
  If Not x Then a = a + 100
Next
Print a
""" + STAT, lambda n: n > 0),
    # THEN and ELSE parts that are commands: records of their own (RB_PART),
    # reached only by cmd_if's jump; a true IF with an ELSE must not fall
    # into its ELSE part's record
    ("then parts", """Dim Integer i, a, b
For i = 1 To 6
  If i > 3 Then Print "T"; i;
  If i Mod 2 Then Print "o"; Else Print "e";
  If i = 2 Then If a = 0 Then a = 5 Else a = 6
  If i = 4 Then Inc b, 10 Else Inc b
  If i = 5 Then S i
Next
Print
Print a; b
Sub S x
  Print "S"; x;
End Sub
""" + STAT, None),
    ("text stays", """Dim Integer i, a, b
For i = 1 To 10
  If i = 1 Then
    a = a + 1
  ElseIf i = 2 Then
    a = a + 2
  Else
    a = a + 3
  EndIf
  If i = 5 Then GoTo skip
  b = b + 1
skip:
  If i = 7 Then Print "seven";
Next
Print a; b
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
    print("%-4s %-12s CODE %-5d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-100:]))
    if not same:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))

src = """Dim Integer i, a, b
Dim Float x = 1.5
t = Timer
For i = 1 To 100000
  If i And 1 Then
    a = a + 1
  Else
    b = b + 1
  EndIf
  If x > 1 Then a = a + 2
Next
Print Timer - t
""" + STAT
for mode in ("OFF", "ON"):
    b.cmd("OPTION COMPILE " + mode, 10)
    body, stat = run(src)
    print("time %-3s %s  %s" % (mode, body[-1] if body else "?", stat))
b.cmd("OPTION COMPILE OFF", 10)
print("RBP2C", "PASS" if ok else "FAIL")
b.close()
