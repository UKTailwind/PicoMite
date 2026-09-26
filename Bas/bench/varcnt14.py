"""varcnt14.py PORT - MM.INFO(VARCNT) after ERASE and after a SUB's locals are
freed (bug report item 14).

The manual: "Returns the number of variables in use".  The total was worked
out only when a variable was created, so ERASE and a returning SUB left it
too high.  Erasing one array must lower it by 1, two LOCALs raise it by 2, and
returning from the SUB must bring it back: "VC -1 2 0"."""
import sys, os, re, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

SRC = """Dim a(10), n0, n1, n2, n3
n0 = MM.Info(VARCNT)
Erase a
n1 = MM.Info(VARCNT)
S
n3 = MM.Info(VARCNT)
Print "VC"; n1 - n0; n2 - n1; n3 - n1
Sub S
  Local x, y
  n2 = MM.Info(VARCNT)
End Sub
"""

b = pc3.PC3(sys.argv[1])
b.attention()
b.upload(SRC, 30)
b.drain(0.1)
out = pc3.ANSI.sub("", b.run(30))
line = [l.strip() for l in out.splitlines() if l.strip().startswith("VC")]
print(line[0] if line else out[-300:])
print("VARCNT14", "PASS" if line and re.findall(r"-?\d+", line[0]) == ["-1", "2", "0"] else "FAIL")
b.close()
