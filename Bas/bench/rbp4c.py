"""rbp4c.py PORT - Route B P4c: SELECT CASE on a number, CASE and END SELECT
compiled (docs/Interpreter_RouteB_Design.html).  Every program runs with OPTION
COMPILE OFF, ON and SHADOW and must print the same, errors included.  Leaves
OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("values", """Dim Integer i, a, b, c, d
For i = 0 To 20
  Select Case i
    Case 1, 3, 5
      a = a + 1
    Case 7 To 10
      b = b + 1
    Case Is > 15
      c = c + i
    Case Else
      d = d + 1
  End Select
Next
Print a; b; c; d
""", True),
    ("float selector", """Dim Float x
Dim Integer i, n1, n2, n3
For i = 0 To 40
  x = i / 4
  Select Case x
    Case 2.5
      n1 = n1 + 1
    Case 3 To 4.25
      n2 = n2 + 1
    Case < 1, >= 9
      n3 = n3 + 1
  End Select
Next
Print n1; n2; n3
""", True),
    ("int selector float case", """Dim Integer i, h
For i = 0 To 6
  Select Case i
    Case 2.6
      h = h + 100
    Case 4.4
      h = h + 10
    Case Else
      h = h + 1
  End Select
Next
Print h
""", True),
    ("nested", """Dim Integer i, j, s
For i = 1 To 3
  For j = 1 To 3
    Select Case i
      Case 1
        Select Case j
          Case 1 : s = s + 1
          Case 2 : s = s + 10
          Case Else : s = s + 100
        End Select
      Case 2
        s = s + 1000
      Case Else
        s = s + 10000
    End Select
  Next
Next
Print s
""", True),
    ("expressions", """Dim Integer i, t, k = 3
For i = 0 To 12
  Select Case i Mod 5
    Case k - 1, k + 1
      t = t + 1
    Case Is <> 0
      t = t + 10
  End Select
Next
Print t
""", True),
    ("in a sub", """Dim Integer total
Sub Pick(v As Integer)
  Local Integer r
  Select Case v
    Case 1 To 3 : r = 1
    Case 4, 5 : r = 2
    Case Else : r = 3
  End Select
  total = total + r
End Sub
Dim Integer i
For i = 0 To 8 : Pick i : Next
Print total
""", True),
    ("string selector", """Dim s$, n
Dim Integer i
For i = 1 To 3
  s$ = Mid$("abc", i, 1)
  Select Case s$
    Case "a" : n = n + 1
    Case "b" To "c" : n = n + 10
  End Select
Next
Print n
""", None),
    ("case error", """Dim Integer i = 2, z = 0
Select Case i
  Case 1
    Print "one"
  Case 5 \\ z
    Print "never"
End Select
""", None),
    ("no match", """Dim Integer i = 9, c
Select Case i
  Case 1 : c = 1
  Case 2 : c = 2
End Select
Print c
""", None),
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
    print("%-4s %-22s CODE %-6d %s" % ("ok" if good else "BAD", name, code, " | ".join(outs["ON"][0])[-90:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s" % (mode, outs[mode][0]))
b.cmd("OPTION COMPILE OFF", 10)
print("RBP4C", "PASS" if ok else "FAIL")
b.close()
