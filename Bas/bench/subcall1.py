"""subcall1.py PORT - a one-letter SUB called alone on a line (bug report item 29).

ExecuteProgram's pre-scan used to step two bytes past the start of every
statement, as if it were a command token.  A call to a one-letter SUB with no
arguments is one byte long, so the return address landed a line too far and the
line after the call was skipped.  makeargs had the same assumption after THEN
and ELSE, so IF ... THEN Q (with more program after it) failed with "Expected
closing bracket"."""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

SRC = """Sub Q
  Inc n
End Sub
Sub QQ
  Inc n
End Sub
Q
Print "after Q"
QQ
Print "after QQ"
Q : Print "same line"
If n > 0 Then Q
Print "after IF Q"
For i = 1 To 2
  Q
Next
If n = 0 Then Print "no" Else Q
Print "after ELSE Q"
Print "n="; n
"""
WANT = ["after Q", "after QQ", "same line", "after IF Q", "after ELSE Q", "n= 7"]

b = pc3.PC3(sys.argv[1])
b.attention()
print("upload:", b.upload(SRC, 30))
out = pc3.ANSI.sub("", b.run(30))
print(out)
got = [l.strip() for l in out.splitlines() if l.strip() and l.strip() not in ("RUN", ">")]
print("SUBCALL1", "PASS" if got == WANT else "FAIL (want %s)" % WANT)
b.close()
