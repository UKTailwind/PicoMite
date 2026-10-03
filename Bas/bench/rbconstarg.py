"""rbconstarg.py PORT - Route B: a CONST named as a SUB or FUNCTION argument
goes by value, as DefinedSubFun passes it (it used to bind as a by-reference
target, which a CONST fails at run time, so the compiled call fell back on
every run).  The survey marks CONSTs; the bind checks the name really is one
(RB_BCONST) and falls back if not.  Every program runs with OPTION COMPILE
OFF, ON and SHADOW and must print the same, errors included; where the calls
should compile, the ON run's statements run as text (RAN - CODE) must stay
under the limit.  Leaves OPTION COMPILE as it found it."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
# (name, program, most statements run as text with OPTION COMPILE ON, or None)
PROGS = [
    ("SUB with CONST arguments", """Const DR = 260, HALF = 0.5
Dim Integer tot, k
Dim Float ft
Sub Bar(x As Integer, y As Integer)
  tot = tot + x + y
  x = 0
End Sub
Sub Fl(v)
  ft = ft + v
End Sub
For k = 1 To 100
  Bar DR, k
  Fl HALF
Next
Print tot; ft; DR
""", 20),
    ("FUNCTION with a CONST", """Const SC = 3
Function Mul%(a%, b%)
  Mul% = a% * b%
End Function
Dim Integer k, s
For k = 1 To 100 : s = s + Mul%(k, SC) : Next
Print s
""", 20),
    ("a CONST and its value together", """Const K = 7
Dim Integer t, i
Sub Two(a%, b%)
  t = t + a% * 10 + b%
End Sub
For i = 1 To 50 : Two K, K + i : Next
Print t
""", 20),
    ("float CONST to an integer parameter", """Const F = 2.75
Dim Integer t, i
Sub TakeI(n%)
  t = t + n%
End Sub
For i = 1 To 40 : TakeI F : Next
Print t
""", 20),
    ("BYREF needs a variable", """Const Q = 1
Sub R(ByRef n%)
  n% = 2
End Sub
R Q
Print "not reached"
""", None),
    ("a LOCAL hides the CONST", """Const W = 5
Sub Bump(n%)
  n% = n% + 1
End Sub
Sub Inner
  Local Integer W, k
  For k = 1 To 30 : Bump W : Next
  Print W
End Sub
Inner
Print W
""", 25),
    ("a SUB's own CONST", """Function Twice%(n%)
  Twice% = n% * 2
End Function
Sub Outer
  Const IN = 4
  Local Integer k, s
  For k = 1 To 40 : s = s + Twice%(IN) : Next
  Print s
End Sub
Outer
""", 25),
    ("not a CONST when it runs", """Dim Integer z, k
Sub Inc1(n)
  n = n + 1
End Sub
If z = 1 Then
  Const ZC = 3
EndIf
For k = 1 To 5 : Inc1 ZC : Next
Print ZC
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
        outs[mode] = run(src + STAT)
    same = outs["OFF"][0] == outs["ON"][0] == outs["SHADOW"][0]
    m = re.search(r"RAN (\d+) MISS \d+ CODE (\d+)", outs["ON"][1])
    text = int(m.group(1)) - int(m.group(2)) if m else -1
    good = same and (most_text is None or 0 <= text <= most_text)
    ok = ok and good
    print("%-4s %-28s text %-5d %s" % ("ok" if good else "BAD", name, text, " | ".join(outs["ON"][0])[-80:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s  [%s]" % (mode, outs[mode][0], outs[mode][1]))
b.cmd("OPTION COMPILE " + ("OFF" if was.startswith("OFF") else "ON"), 10)
print("RBCONSTARG", "PASS" if ok else "FAIL")
b.close()
