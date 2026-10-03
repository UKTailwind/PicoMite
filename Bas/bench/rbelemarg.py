"""rbelemarg.py PORT - Route B: an array element alone as a SUB or FUNCTION
argument, compiled (RP_ELEM): its address goes on the VM's stack (RC_ADEL) and
RBMakeParams binds the parameter to it as DefinedSubFun binds it to findvar's
pointer.  Every program runs with OPTION COMPILE OFF, ON and SHADOW and must
print the same, errors included; where the calls should compile, the ON run's
statements run as text (RAN - CODE) must stay under the limit.  Leaves
OPTION COMPILE as it found it."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
# (name, program, most statements run as text with OPTION COMPILE ON, or None)
PROGS = [
    ("sub byref element", """Dim Integer a(10), i
Sub Inc1(n%)
  n% = n% + 1
End Sub
For i = 1 To 100
  Inc1 a(i Mod 10)
Next
Print a(0); a(5); a(9)
""", 20),
    ("function elements", """Dim Integer v(3), i
Function Add8%(a%, b%, c%)
  Add8% = (a% + b% + c%) And 255
End Function
For i = 1 To 100
  v(i Mod 3) = Add8%(v(i Mod 3), i, 1)
Next
Print v(0); v(1); v(2)
""", 20),
    ("function changes it", """Dim Integer c(2), k, t
Function Take%(n%)
  n% = n% + 1
  Take% = n% * 10
End Function
For k = 1 To 50 : t = Take%(c(1)) + Take%(c(2)) : Next
Print c(1); c(2); t
""", 20),
    ("types differ: a copy", """Dim Integer a(3), k
Dim Float f(3)
Sub Twice(x!)
  x! = x! * 2
End Sub
Sub TwiceI(x%)
  x% = x% * 2
End Sub
a(1) = 5 : f(1) = 2.5 : f(2) = 1.75
For k = 1 To 30
  Twice a(1)
  TwiceI f(2)
  If k <= 3 Then Twice f(1)
Next
Print a(1); f(1); f(2)
""", 40),
    ("2-D, OPTION BASE 1", """Option Base 1
Dim Float m(3, 4)
Dim Integer r, c
Sub SetIt(z!, v!)
  z! = v!
End Sub
Dim Integer z
For z = 1 To 10
  For r = 1 To 3
    For c = 1 To 4
      SetIt m(r, c), r * 10 + c + z
    Next
  Next
Next
Print m(1, 1); m(2, 3); m(3, 4)
""", 30),
    ("array parameter's element", """Dim Integer q(5)
Sub Bump(n%)
  n% = n% + 3
End Sub
Sub Walk(arr%())
  Local Integer k
  For k = 0 To 5
    Bump arr%(k)
  Next
End Sub
Dim Integer z
For z = 1 To 10 : Walk q() : Next
Print q(0); q(5)
""", 25),
    ("local array", """Sub Bump2(n%, v%)
  n% = n% + v% * 2
End Sub
Sub L
  Local Integer w(4), k
  For k = 0 To 4
    Bump2 w(k), k
  Next
  Print w(0); w(4)
End Sub
Dim Integer z
For z = 1 To 12 : L : Next
""", 40),
    ("untyped float parameter", """Dim Float gg(2)
Dim Integer hh(2), k
Function G(x)
  x = x + 0.5
  G = x
End Function
For k = 1 To 40 : gg(1) = G(gg(1)) + G(hh(1)) : gg(1) = gg(1) / 4 : Next
Print gg(1); hh(1)
""", 20),
    ("recursion", """Dim Integer d(10)
Dim Integer z
Sub FillDn(slot%, n%)
  slot% = n% + z
  If n% > 0 Then FillDn d(n% - 1), n% - 1
End Sub
For z = 1 To 5 : FillDn d(9), 9 : Next
Print d(0); d(5); d(9)
""", 25),
    ("byref", """Dim Integer a(2)
Dim Float f(2)
Sub R(ByRef n%)
  n% = 7
End Sub
R a(1) : Print a(1)
On Error Skip
R f(1)
Print MM.ErrMsg$
""", None),
    ("byval stays a copy", """Dim Integer a(2), k
Sub V(ByVal n%)
  n% = 9
End Sub
For k = 1 To 3 : V a(1) : Next
Print a(1)
""", None),
    ("string element", """Dim String s$(2)
Sub T(z$)
  z$ = z$ + "x"
End Sub
T s$(1) : T s$(1)
Print s$(1)
""", None),
    ("bounds error", """Dim Integer a(3)
Sub S(n%)
  n% = 1
End Sub
S a(2)
Print "a(2) ok"
S a(4)
Print "not reached"
""", None),
    ("dimensions error", """Dim Integer a(3)
Sub S(n%)
  n% = 1
End Sub
S a(1, 1)
""", None),
    ("negative index error", """Dim Integer a(3)
Function F%(n%)
  F% = n%
End Function
Dim Integer x
x = F%(a(-1))
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
print("RBELEMARG", "PASS" if ok else "FAIL")
b.close()
