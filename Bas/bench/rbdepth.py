"""rbdepth.py PORT - the deepest a recursive FUNCTION (and SUB) can go before
"Stack overflow" stops the program, with OPTION COMPILE OFF and ON (Route B
must not make it shallower, and must stop it: a recursion through compiled
calls never reaches the text evaluator's check).  The recursion is a
single-line IF whose ELSE calls the FUNCTION again.  Each depth is printed
before it is tried, and the error ends the run, so no ON ERROR is in force (it
would send every statement to the text path).  Leaves OPTION COMPILE OFF."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

FUN = """Function D(n As Integer) As Integer
  If n <= 0 Then D = 0 Else D = 1 + D(n - 1)
End Function
Dim Integer k, r
For k = 1 To 80
  Print "K"; k
  r = D(k)
Next
"""
SUB = """Sub S(n As Integer)
  If n > 0 Then S n - 1
End Sub
Dim Integer k
For k = 1 To 200
  Print "K"; k
  S k
Next
"""
b = pc3.PC3(sys.argv[1])
b.attention()


def depth(src):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120)).replace("\r", "")
    ks = re.findall(r"K *(\d+)", out)
    err = [l for l in out.split("\n") if "rror" in l]
    last = int(ks[-1]) if ks else 0
    return (last - 1 if err else last), (err[0].strip() if err else "no error")


for mode in ("OFF", "ON"):
    b.cmd("OPTION COMPILE " + mode, 10)
    fd, fe = depth(FUN)
    sd, se = depth(SUB)
    print("%-3s FUNCTION depth %d (%s), SUB depth %d (%s)" % (mode, fd, fe[:60], sd, se[:40]))
b.cmd("OPTION COMPILE OFF", 10)
b.close()
