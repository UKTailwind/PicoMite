"""rbnopchain.py PORT - Route B: comment-only, empty and label-only lines
inside compiled code.  Each had a NOP record, at which the chain of compiled
statements stopped and went back through RunStream; now such a line has no
record, only a map entry pointing at the next statement, so the chain runs
straight past it.  Every program runs with OPTION COMPILE OFF, ON and SHADOW
and must print the same, errors included (an error after a comment line,
MM.ERRLINE), except that TRACE LIST, compiled, leaves the comment and empty
lines out.  Then a timing: a two-statement loop with a comment line, an
empty line or three comment lines in it must take no more than 3% longer
compiled than the same loop without them (with a record each they cost
+18% and +30%).  Leaves OPTION COMPILE as it found it."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
# (name, program, most statements run as text with OPTION COMPILE ON, or None)
PROGS = [
    ("loop with comments", """Dim Integer i, a, b
For i = 1 To 200
  a = a + 1
  ' a comment

  ' and another
  b = b + 2
Next
Print a; b
""", 20),
    ("error after a comment line", """Dim Integer i, v(5)
For i = 1 To 10
  ' the next line fails when i is 6
  v(i) = i
Next
""", None),
    ("MM.ERRLINE after comments", """Dim Integer i, v(3)
On Error Skip 1
For i = 1 To 4
  ' a comment
  v(i) = i
Next
Print MM.Errno; MM.ErrLine
""", None),
    ("TRACE LIST", """Dim Integer i, s
For i = 1 To 3
  s = s + i
  ' a comment line

  s = s * 2
Next
Trace List 6
Print
""", None),
    ("comment as the loop's first line", """Dim Integer k, n
Do
  ' first line of the body
  Inc n
  k = k + 1
Loop Until k >= 100
Print n
""", 20),
    ("comments in a SUB body", """Dim Integer t, i
Sub S(n%)
  ' a comment
  t = t + n%

  ' another
End Sub
For i = 1 To 100 : S i : Next
Print t
""", 25),
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
    if name == "TRACE LIST":
        # compiled, a comment or empty line has no record, so it is not traced: the
        # compiled list holds no such line and its last entry is TRACE LIST's own line,
        # as the text list's is (TRACE prints CountLines' number, the file line + off)
        nums = re.findall(r"\[(\d+)\]", " ".join(outs["OFF"][0]))
        off = int(nums[-1]) - 8 if nums else 0 # (Trace List is file line 8)
        nolines = {str(i + 1 + off) for i, l in enumerate(src.split("\n")) if not l.strip() or l.strip().startswith("'")}
        onums = re.findall(r"\[(\d+)\]", " ".join(outs["ON"][0]))
        same = (outs["ON"][0] == outs["SHADOW"][0] and len(onums) == len(nums) and not (set(onums) & nolines)
                and set(nums) & nolines and onums[-1] == nums[-1])
    m = re.search(r"RAN (\d+) MISS \d+ CODE (\d+)", outs["ON"][1])
    text = int(m.group(1)) - int(m.group(2)) if m else -1
    good = same and (most_text is None or 0 <= text <= most_text)
    ok = ok and good
    print("%-4s %-28s text %-5d %s" % ("ok" if good else "BAD", name, text, " | ".join(outs["ON"][0])[-80:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s  [%s]" % (mode, outs[mode][0], outs[mode][1]))
TIMING = """Dim Integer i, a, b, n = 100000
Dim Float t0, tA, tB, tC, tD
t0 = Timer
For i = 1 To n
  a = a + 1
  b = b + 1
Next
tA = Timer - t0
t0 = Timer
For i = 1 To n
  a = a + 1
  ' a comment
  b = b + 1
Next
tB = Timer - t0
t0 = Timer
For i = 1 To n
  a = a + 1

  b = b + 1
Next
tC = Timer - t0
t0 = Timer
For i = 1 To n
  a = a + 1
  ' one
  ' two
  ' three
  b = b + 1
Next
tD = Timer - t0
Print "T"; tA; tB; tC; tD
"""
if "rror" not in b.cmd("OPTION COMPILE ON", 10):
    b.upload(TIMING, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120))
    m = re.search(r"T\s*([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)", out)
    if m:
        tA, tB, tC, tD = (float(x) for x in m.groups())
        good = max(tB, tC, tD) <= tA * 1.03 + 2
        ok = ok and good
        print("%-4s timing, compiled: none %.0f ms, comment %.0f, empty %.0f, three comments %.0f" % ("ok" if good else "BAD", tA, tB, tC, tD))
    else:
        ok = False
        print("BAD  timing: no result", out[-300:])
b.cmd("OPTION COMPILE " + ("OFF" if was.startswith("OFF") else "ON"), 10)
print("RBNOPCHAIN", "PASS" if ok else "FAIL")
b.close()
