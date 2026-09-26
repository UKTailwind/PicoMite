"""static5.py PORT - STATIC names its hidden global after the SUB that runs it
(bug report item 5).

The name used to come from CurrentSubFunName, which every SUB entry set and
nothing restored, and inside an interrupt SUB from CurrentInterruptName:
- a STATIC run after a call to another SUB attached to that SUB's static;
- a SUB with a STATIC, called from an interrupt SUB as well as normally, had a
  second static for the interrupt calls."""
import sys, os, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

SRC = """Sub SubB
  Static n
  n = n + 100
End Sub
Sub SubA
  SubB
  Static n
  n = n + 1
  Print "SubA n="; n
End Sub
Sub Helper
  Static n
  n = n + 1
  Print "Helper n="; n
End Sub
Sub TickSub
  Helper
  Static t
  t = t + 1
  SetTick 0, TickSub
  done = 1
End Sub
Function Twice(x)
  Static calls
  calls = calls + 1
  Twice = 2 * x
End Function
Sub UsesFun
  Static m
  m = m + Twice(5)
  Print "UsesFun m="; m
End Sub
SubA : SubA : SubA
Helper
SetTick 10, TickSub
Do While done = 0 : Loop
Helper
UsesFun : UsesFun
"""
WANT = ["SubA n= 1", "SubA n= 2", "SubA n= 3", "Helper n= 1", "Helper n= 2", "Helper n= 3",
        "UsesFun m= 10", "UsesFun m= 20"]

b = pc3.PC3(sys.argv[1])
b.attention()
print("upload:", b.upload(SRC, 30))
out = pc3.ANSI.sub("", b.run(30))
print(out)
got = [l.strip() for l in out.splitlines() if l.strip() and l.strip() not in ("RUN", ">")]
print("STATIC5", "PASS" if got == WANT else "FAIL (want %s)" % WANT)
b.close()
