"""rbp4b.py PORT - Route B P4b: EXIT FOR, EXIT DO, END SUB / EXIT SUB / RETURN
and END FUNCTION / EXIT FUNCTION compiled, and a single-line IF whose THEN part
is not a LET (docs/Interpreter_RouteB_Design.html).  Every program runs with
OPTION COMPILE OFF, ON and SHADOW and must print the same, errors included.
Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("exit for do", """Dim Integer i, j, s, n
For i = 1 To 10
  For j = 1 To 10
    If j > i Then Exit For
    s = s + j
  Next
  If s > 100 Then Exit For
Next
Print s; i; j
Do
  n = n + 1
  If n >= 7 Then Exit Do
Loop
Do While 1
  n = n + 2
  If n > 20 Then Exit
Loop
Print n
""", True),
    ("sub exits", """Dim Integer t
Sub A(n%)
  If n% < 0 Then Exit Sub
  t = t + n%
End Sub
Sub B
  Local Integer k
  For k = 1 To 5
    A k - 3
  Next
End Sub
B : B : Print t
""", True),
    ("function exits", """Function F%(n%)
  F% = -1
  If n% < 0 Then Exit Function
  F% = n% * 2
End Function
Dim Integer i, s
For i = -3 To 3 : s = s + F%(i) : Next
Print s
""", True),
    ("gosub return", """Dim Integer i, s
For i = 1 To 5
  GoSub Add
Next
Print s
Print "STAT "; MM.Info(COMPILE)
End
Add:
  s = s + i
  Return
""", True),
    ("then parts", """Dim Integer i, c, d
Sub Bump
  d = d + 1
End Sub
For i = 1 To 10
  If i Mod 2 = 0 Then Bump
  If i > 3 Then If i < 7 Then c = c + 1
Next
Print c; d
i = 0
Do
  i = i + 1
  If i = 4 Then Exit Do
Loop
Print i
""", True),
    ("else not compiled", """Dim Integer i, a, b
Sub Pa
  a = a + 1
End Sub
For i = 1 To 6
  If i Mod 3 = 0 Then Pa Else b = b + 1
Next
Print a; b
""", None),
    ("exit errors", """Dim Integer i
On Error Skip
Exit For
Print MM.ErrMsg$
On Error Skip
Exit Do
Print MM.ErrMsg$
On Error Skip
Return
Print MM.ErrMsg$
Sub S
  Exit Sub
End Sub
S : Print "ok"
""", None),
    ("nested returns", """Dim Integer d
Function Deep(n As Integer) As Integer
  If n = 0 Then
    Deep = 0
    Exit Function
  EndIf
  Deep = 1 + Deep(n - 1)
End Function
Print Deep(8)
""", True),
]

b = pc3.PC3(sys.argv[1])
b.attention()
ok = True


def run(src):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120)).replace("\r", "")
    lines = [l for l in out.split("\n") if l.strip() and l.strip() not in ("RUN", ">")]
    stat = [l for l in lines if l.startswith("STAT ")]
    body = [l for l in lines if not l.startswith("STAT ")]
    return body, (stat[0][5:] if stat else "")


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
    print("%-4s %-18s CODE %-6d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-90:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))
b.cmd("OPTION COMPILE OFF", 10)
print("RBP4B", "PASS" if ok else "FAIL")
b.close()
