"""rbblockcmt.py PORT - Route B: a /* ... */ block is comment.  cmd_comment
steps from the /* to the statement after the first */, so the compiler gives
every statement from the /* to the */ a map entry and no record, as it does a
comment line: a loop with a block in it stays compiled (walkr-robot's main
loop ran 149,461 text /* statements, three a pass), and the surveys do not
read what is in a block (a commented-out END SUB or LOCAL).  tokenise keeps
the lines between the /* and the */ as plain text, so only a */ that starts a
statement ends a block, and a /* inside one is text.  An unclosed /* keeps
its record: cmd_comment raises the error.  Every
program runs with OPTION COMPILE OFF, ON and SHADOW and must print the same,
errors included; where the loop should compile, the ON run's statements run
as text (RAN - CODE) must stay under the limit.  Then a timing: a loop with a
block in it must take no more than 3% longer compiled than without.  Leaves
OPTION COMPILE as it found it."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
# (name, program, most statements run as text with OPTION COMPILE ON, or None)
PROGS = [
    ("blocks in a loop", """Dim Integer i, a, b
For i = 1 To 200
  a = a + 1
  /*
  a = a + 1000
  Print "never"
  */
  b = b + 2
  /* (the */ on this line is text: the block ends at the next line's)
  */
Next
Print a; b
""", 6),
    ("a block after a statement, a statement after */", """Dim Integer i, a, b
For i = 1 To 100
  a = a + 1 : /*
  a = a + 1000
  */ : b = b + 3
Next
Print a; b
""", 6),
    ("a block in a SUB with END SUB and LOCAL in it", """Dim Integer t, i
Sub S(n%)
  Local Integer k
  /*
  Local Float k
  End Sub
  */
  k = n% * 2
  t = t + k
End Sub
For i = 1 To 100 : S i : Next
Print t
""", 8),
    ("an unclosed /*", """Dim Integer a
a = 1
/*
a = 2
Print a
""", None),
    ("a /* inside a block is text", """Dim Integer a
a = 1
/*
/*
*/
Print a
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
    return [l for l in lines if not l.startswith("STAT ")], (stat[0][5:] if stat else "")


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
    print("%-4s %-48s text %-5d %s" % ("ok" if good else "BAD", name, text, " | ".join(outs["ON"][0])[-60:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s  [%s]" % (mode, outs[mode][0], outs[mode][1]))
TIMING = """Dim Integer i, a, b, n = 100000
Dim Float t0, tA, tB
t0 = Timer
For i = 1 To n
  a = a + 1
  b = b + 1
Next
tA = Timer - t0
t0 = Timer
For i = 1 To n
  a = a + 1
  /*
  b = b + 1000
  */
  b = b + 1
Next
tB = Timer - t0
Print "T"; tA; tB
"""
if "rror" not in b.cmd("OPTION COMPILE ON", 10):
    b.upload(TIMING, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120))
    m = re.search(r"T\s*([\d.]+)\s+([\d.]+)", out)
    if m:
        tA, tB = float(m.group(1)), float(m.group(2))
        good = tB <= tA * 1.03 + 2
        ok = ok and good
        print("%-4s timing, compiled: no block %.0f ms, a block %.0f ms" % ("ok" if good else "BAD", tA, tB))
    else:
        ok = False
        print("BAD  timing: no result", out[-300:])
b.cmd("OPTION COMPILE " + ("OFF" if was.startswith("OFF") else "ON"), 10)
print("RBBLOCKCMT", "PASS" if ok else "FAIL")
b.close()
