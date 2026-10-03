"""rbdotted.py PORT - Route B: dotted names that are not structure members
(srv.pCurr, R.running) compile when the program and its library define no
TYPE; findvar binds them like any other name then.  With a TYPE they stay
text (findvar may take them for member paths).  Every program runs with
OPTION COMPILE OFF, ON and SHADOW and must print the same, errors included;
where the statements should compile, the ON run's statements run as text
(RAN - CODE) must stay under the limit.  Leaves OPTION COMPILE as it found it."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
# (name, program, most statements run as text with OPTION COMPILE ON, or None)
PROGS = [
    ("dotted scalars and arrays", """Dim Integer R.running = 1, g.n
Dim Float srv.pCurr(17), ggn.dtMov = 0.5
Dim Integer k
For k = 1 To 200
  g.n = g.n + 1
  srv.pCurr(k Mod 18) = srv.pCurr(k Mod 18) + ggn.dtMov
  If g.n > 150 Then R.running = 2
Next
Print g.n; R.running; srv.pCurr(0); srv.pCurr(17)
""", 20),
    ("SUB, LOCAL, by reference", """Dim Integer cnt.total
Sub Add.It(n.val%)
  Local Integer t.tmp
  t.tmp = n.val% * 2
  cnt.total = cnt.total + t.tmp
  n.val% = n.val% + 1
End Sub
Dim Integer a.b, k
For k = 1 To 100
  Add.It a.b
Next
Print a.b; cnt.total
""", 25),
    ("FOR variable and INC", """Dim Integer i.x, s.um, d.o
For i.x = 1 To 300
  Inc s.um, i.x
Next
Do While d.o < 200
  Inc d.o
Loop
Print s.um; d.o
""", 20),
    ("FUNCTION with dotted names", """Function M.sq(v.in)
  M.sq = v.in * v.in
End Function
Dim Float r.res
Dim Integer k
For k = 1 To 100 : r.res = r.res + M.sq(k) : Next
Print r.res
""", 20),
    ("a name and its dotted twin", """Dim Integer a, a.b, a.b.c
Dim Integer k
For k = 1 To 100
  a = a + 1 : a.b = a.b + 2 : a.b.c = a.b.c + a
Next
Print a; a.b; a.b.c
""", 20),
    ("strings", """Dim String s.out$, w.word$ = "ab"
Dim Integer k
For k = 1 To 30
  s.out$ = s.out$ + Left$(w.word$, 1)
Next
Print Len(s.out$); s.out$
""", 20),
    ("OPTION EXPLICIT error", """Option Explicit
Dim Integer d.one
d.one = 5
d.two = d.one + 1
Print d.two
""", None),
    ("with a TYPE: text, members work", """Type pt
  x As Integer
  y As Integer
End Type
Dim p As pt
Dim Integer q.n, k
For k = 1 To 50
  p.x = p.x + 1
  q.n = q.n + 2
Next
Print p.x; q.n
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
print("RBDOTTED", "PASS" if ok else "FAIL")
b.close()
