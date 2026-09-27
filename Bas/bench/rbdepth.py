"""rbdepth.py PORT - the deepest a recursive FUNCTION can go before "Stack
overflow", with OPTION COMPILE OFF and ON (Route B must not make it
shallower).  The recursion is the text path's own case: a single-line IF
whose ELSE calls the FUNCTION again.  Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

SRC = """Function D(n As Integer) As Integer
  If n <= 0 Then D = 0 Else D = 1 + D(n - 1)
End Function
Sub S(n As Integer)
  If n > 0 Then S n - 1
End Sub
Dim Integer k, r, best, bests
On Error Ignore
For k = 1 To 60
  r = D(k)
  If MM.ErrNo <> 0 Then Exit For
  best = k
Next
On Error Clear
On Error Ignore
For k = 1 To 200
  S k
  If MM.ErrNo <> 0 Then Exit For
  bests = k
Next
On Error Abort
Print "DEPTH"; best; bests
"""
b = pc3.PC3(sys.argv[1])
b.attention()
for mode in ("OFF", "ON"):
    b.cmd("OPTION COMPILE " + mode, 10)
    b.upload(SRC, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120)).replace("\r", "")
    m = re.search(r"DEPTH *(\d+) +(\d+)", out)
    print("%-3s FUNCTION depth %s, SUB depth %s" % (mode, m.group(1) if m else "?", m.group(2) if m else out[-80:]))
b.cmd("OPTION COMPILE OFF", 10)
b.close()
