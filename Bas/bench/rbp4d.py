"""rbp4d.py PORT - Route B P4d: simple strings (string scalars and literals,
+ and the comparisons, LET to a string) compiled
(docs/Interpreter_RouteB_Design.html).  Every program runs with OPTION
COMPILE OFF, ON and SHADOW and must print the same, errors included; a line
starting TIME is left out of the comparison and shown.  Leaves OPTION
COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
PROGS = [
    ("assign and compare", """Dim a$, b$, c$
Dim Integer i, n
For i = 1 To 20
  a$ = "ab"
  b$ = a$ + "cd"
  c$ = b$ + a$
  If c$ = "abcdab" Then n = n + 1
  If a$ < b$ Then n = n + 10
  If b$ > c$ Then n = n + 100
  If a$ <> "" And b$ >= "abcd" Then n = n + 1000
  If c$ <= b$ Then n = n + 10000
Next
Print n; " "; a$; " "; b$; " "; c$
""", True),
    ("empty and case", """Dim p$, q$
Dim Integer i, k
For i = 0 To 5
  p$ = q$
  q$ = q$ + "x"
  If p$ = "" Then k = k + 1
  If q$ > p$ Then k = k + 2
  If "A" < "a" Then k = k + 4
  If p$ + "x" = q$ Then k = k + 8
Next
Print k; Len(q$); " "; q$
""", True),
    ("declared string", """Dim nm As String, Integer i
For i = 1 To 5
  nm = nm + "z"
  If nm >= "zzz" Then nm = nm + "."
Next
Print nm
""", True),
    ("option default string", """Option Default String
Dim Integer i
Dim t
For i = 1 To 4
  t = t + "q"
Next
Print t
""", True),
    ("locals and byref", """Dim g$ = "start", Integer i
Sub AddTo(s$, x$)
  Local t$, u As String
  t$ = x$ + "."
  u = t$ + "!"
  s$ = s$ + u
End Sub
For i = 1 To 4 : AddTo g$, "k" : Next
Print g$
""", True),
    ("length limit", """Dim s$ Length 5, Integer i
For i = 1 To 10
  s$ = s$ + "ab"
  Print s$
Next
""", None),
    ("byref length", """Dim q$ Length 3
Sub Grow(s$)
  s$ = s$ + "x"
End Sub
Dim Integer i
For i = 1 To 5 : Grow q$ : Print q$ : Next
""", None),
    ("concat too long", """Dim a$, Integer i
a$ = "0123456789"
For i = 1 To 30
  a$ = a$ + a$
  Print Len(a$)
Next
""", None),
    ("number into string", """Dim a$, Integer n = 1
a$ = "x"
Print "go"
a$ = n
""", None),
    ("string into number", """Dim a$ = "5", Integer n
n = 2
Print "go"
n = a$
""", None),
    ("string plus number", """Dim a$ = "5", b$
Print "go"
b$ = a$ + 1
""", None),
    ("invalid operator", """Dim a$ = "5", b$ = "6", Integer n
Print "go"
n = a$ * b$
""", None),
    ("const", """Const K$ = "ok"
Dim r$, Integer i
For i = 1 To 3 : r$ = r$ + K$ : Next
Print r$
""", True),
    ("const assign", """Const K$ = "ok"
Print K$
K$ = "no"
""", None),
    ("single-line if else", """Dim a$, b$, Integer i
For i = 1 To 6
  If i Mod 2 Then a$ = a$ + "o" Else a$ = a$ + "e"
  If a$ = "oeo" Then b$ = "hit" Else b$ = b$ + "."
Next
Print a$; " "; b$
""", True),
    ("escape", """Option Escape
Dim a$, Integer i
For i = 1 To 2 : a$ = a$ + "x\\ty" : Next
Print Len(a$)
""", None),
    ("functions stay text", """Dim a$ = "hello", b$, Integer i
For i = 1 To 3
  b$ = Left$(a$, i) + "-" + Mid$(a$, i, 1)
  Print b$
Next
""", None),
    ("do while", """Dim w$, Integer n
Do While w$ <> "xxxx"
  w$ = w$ + "x"
  n = n + 1
Loop
Print n; " "; w$
""", True),
    ("long literal", """Dim a$, Integer i
For i = 1 To 2
  a$ = "0123456789012345678901234567890123456789012345678901234567890123456789"
Next
Print Len(a$)
""", None),
    ("brackets", """Dim a$ = "x", b$, Integer i, n
For i = 1 To 3
  b$ = (a$ + "y") + ("z")
  If (b$ = "xyz") Then n = n + 1
Next
Print n; " "; b$
""", True),
    ("speed", """Dim a$, b$, Integer i, n
Timer = 0
For i = 1 To 20000
  a$ = "abc"
  b$ = a$ + "def"
  If b$ = "abcdef" Then n = n + 1
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
print("RBP4D", "PASS" if ok else "FAIL")
b.close()
