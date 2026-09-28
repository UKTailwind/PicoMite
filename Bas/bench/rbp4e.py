"""rbp4e.py PORT - Route B P4e: ELSEIF arms, ELSE / ELSEIF / END IF reached
from a block, statements that do nothing (cmd_null), and CONTINUE FOR
compiled (docs/Interpreter_RouteB_Design.html).  Every program runs with
OPTION COMPILE OFF, ON and SHADOW and must print the same, errors and TRACE
included; a line starting TIME is left out of the comparison and shown.
Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("elseif chain", """Dim Integer i, a, b, c, d
For i = 0 To 20
  If i < 3 Then
    a = a + 1
  ElseIf i < 7 Then
    b = b + 1
  ElseIf i = 10 Or i = 12 Then
    c = c + i
  Else
    d = d + 1
  EndIf
Next
Print a; b; c; d
""", True),
    ("elseif no else", """Dim Integer i, n
For i = 0 To 9
  If i = 1 Then
    n = n + 1
  ElseIf i = 5 Then
    n = n + 10
  End If
Next
Print n
""", True),
    ("else if two words", """Dim Integer i, n
For i = 0 To 9
  If i < 2 Then
    n = n + 1
  Else If i < 4 Then
    n = n + 10
  Else
    n = n + 100
  End If
Next
Print n
""", True),
    ("nested", """Dim Integer i, j, n
For i = 0 To 5
  For j = 0 To 5
    If i = j Then
      n = n + 1
    ElseIf i < j Then
      If j - i = 1 Then
        n = n + 10
      ElseIf j - i = 2 Then
        n = n + 100
      Else
        n = n + 1000
      EndIf
    Else
      n = n + 10000
    EndIf
  Next
Next
Print n
""", True),
    ("float and string arms", """Dim Float x
Dim s$
Dim Integer i, n
For i = 0 To 8
  x = i / 3
  s$ = Mid$("abcdefghi", i + 1, 1)
  If x > 2.5 Then
    n = n + 1
  ElseIf s$ = "c" Then
    n = n + 10
  ElseIf x Then
    n = n + 100
  EndIf
Next
Print n
""", True),
    ("elseif error line", """Dim Integer i = 5, z = 0
If i = 1 Then
  Print "one"
ElseIf i \\ z = 1 Then
  Print "never"
EndIf
""", None),
    ("line after the chain", """Dim Integer i = 5, z = 0
If i = 1 Then
  Print "one"
ElseIf i = 2 Then
  Print "two"
Else : Print "else"; i \\ z
EndIf
""", None),
    ("in a sub, a call", """Function Sq(n As Integer) As Integer
  Sq = n * n
End Function
Sub Pick(v As Integer)
  Local Integer r
  If v = 0 Then
    r = 1
  ElseIf Sq(v) = 16 Then
    r = 2
  Else
    r = 3
  EndIf
  Print r;
End Sub
Dim Integer i
For i = 0 To 5 : Pick i : Next
Print
""", True),
    ("else junk", """Dim Integer i = 1
If i = 1 Then
  Print "one"
Else i = 2
  Print "two"
EndIf
""", None),
    ("continue for", """Dim Integer kh(38), i, n, t
For i = 0 To 38 : kh(i) = i * 37 Mod 256 : Next
Sub Proc
  Local Integer i, k
  For i = &H26 To 0 Step -1
    k = kh(i)
    If (k And 128) = 0 Then Continue For
    t = t + k
  Next i
End Sub
For n = 1 To 50 : Proc : Next
Print t
""", True),
    ("continue next two", """Dim Integer i, j, s
For i = 1 To 4
  For j = 1 To 4
    If j = 2 Then Continue For
    s = s + i * 10 + j
  Next j, i
Print s; i; j
""", True),
    ("continue float loop", """Dim Float x, t
Dim Integer n
For x = 0 To 2 Step 0.25
  If x = 1 Then Continue For
  t = t + x
  n = n + 1
Next
Print t; n; x
""", True),
    ("continue last pass", """Dim Integer i, s
For i = 1 To 3
  s = s + i
  Continue For
  s = s + 100
Next
Print s; i
""", True),
    ("continue no for", """Print "go"
Continue For
""", None),
    ("continue do stays text", """Dim Integer i, n
Do While i < 10
  i = i + 1
  If i Mod 2 Then Continue Do
  n = n + i
Loop
Print n
""", True),
    ("cmd_null", """Dim Integer i, n
For i = 1 To 5
  Data 1, 2, 3
  Randomize
  Select Case i
    Case 2 : n = n + 100
  End Select
  n = n + i
Next
Print n
""", True),
    ("trace", """Dim Integer i, n
Trace On
For i = 0 To 3
  If i = 0 Then
    n = n + 1
  ElseIf i = 2 Then
    n = n + 10
  Else
    n = n + 100
  EndIf
Next
Trace Off
Print n
""", None),
    ("speed", """Dim Integer i, n, k
Timer = 0
For i = 1 To 20000
  k = i And 7
  If k = 0 Then
    n = n + 1
  ElseIf k = 1 Then
    n = n + 2
  ElseIf k = 2 Then
    n = n + 3
  Else
    n = n + 4
  EndIf
Next
Print n
Print "TIME "; Timer
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
print("RBP4E", "PASS" if ok else "FAIL")
b.close()
