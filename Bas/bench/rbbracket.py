"""rbbracket.py PORT - Route B: a SUB call written with its arguments in
brackets, Foo(a, b) or Foo(), compiles as the same call without them.
DefinedSubFun's makeargs takes the bracket for the list's own and ends the
list at its close, ignoring anything after it: those forms stay text.  Every
program runs with OPTION COMPILE OFF, ON and SHADOW and must print the same,
errors included; where the calls should compile, the ON run's statements run
as text (RAN - CODE) must stay under the limit.  Leaves OPTION COMPILE as it
found it."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
# (name, program, most statements run as text with OPTION COMPILE ON, or None)
PROGS = [
    ("variables and values", """Dim Integer a, b, k
Sub Add2(x%, y%)
  x% = x% + y%
End Sub
For k = 1 To 100
  Add2(a, k)
  Add2 (b, 2)
Next
Print a; b
""", 20),
    ("no arguments", """Dim Integer n, k
Sub Tick()
  n = n + 1
End Sub
Sub Tock
  n = n + 10
End Sub
For k = 1 To 100
  Tick()
  Tock()
Next
Print n
""", 20),
    ("elements and a CONST", """Const BASE = 1000
Dim Integer v(5), k
Sub Put(slot%, val%)
  slot% = slot% + val%
End Sub
For k = 1 To 100
  Put(v(k Mod 6), BASE)
Next
Print v(0); v(5)
""", 20),
    ("a comment after the list", """Dim Integer a, k
Sub One(x%)
  x% = x% + 1
End Sub
For k = 1 To 100
  One(a) ' counts
Next
Print a
""", 20),
    ("text after the list stays text", """Dim Integer a, k
Sub P(x%)
  a = a + x%
End Sub
For k = 1 To 5
  P (k) * 2
Next
Print a
""", None),
    ("a missing argument", """Dim Integer a
Sub Two(x%, y%)
  a = x% + y%
End Sub
Two(5)
Print a
""", None),
    ("nested brackets and strings", """Dim Integer a, k
Dim Integer m(3, 3)
Sub Acc(x%, y%)
  a = a + x% * y%
End Sub
For k = 1 To 50
  Acc((k + 1) * 2, m(1, (k Mod 3)) + Len(",)"))
Next
Print a
""", 20),
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
print("RBBRACKET", "PASS" if ok else "FAIL")
b.close()
