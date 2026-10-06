"""rbcallname.py PORT record|check FILE - CALL name$ and CALL() (a SUB or
FUNCTION called by a name worked out at run time, BASIC's function pointer).
Every program runs with OPTION COMPILE OFF and ON (and SHADOW where compiling
is offered); "record" saves what a firmware printed, "check" compares another
firmware with it, so a change to the interpreted CALL is checked as well as
the compiled one.  A line starting TIME is left out of the comparison and
shown.  Leaves OPTION COMPILE OFF."""
import sys, os, json, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

NAMES = ["FollowerExtend", "FollowerRetract", "IsFollowerRetracted", "MandrelExtend", "IsMandrelExtended",
         "MandrelRetract", "ColletClose", "IsColletClosed", "ColletOpen", "IsColletOpen", "ClampPdieClose",
         "IsClampPdieClosed", "ClampPdieOpen", "IsClampPdieOpen", "ClampOpen", "IsClampOpen", "ClampClose",
         "IsClampClosed", "PdieOpen", "IsPdieOpen", "PdieClose", "IsPdieClosed", "PdieShort", "IsPdieShort",
         "IsSeqStartSignal", "BendDieReturn", "BendDieReturnOff", "HighPressure", "HighPressureOff"]
TABLE = "\n".join(
    ["Dim Integer param1, param2, i, k", "Dim String SequenceTable(28)"] +
    ['SequenceTable(%d) = "%s"' % (i, n) for i, n in enumerate(NAMES)] +
    ["param1 = 3 : param2 = 1000",
     "For k = 1 To 7 : For i = 0 To 28 : Call SequenceTable(i), param1, param2 : Next : Next",
     "Print param1; param2",
     "Timer = 0",
     "For k = 1 To 100 : For i = 0 To 28 : Call SequenceTable(i), param1, param2 : Next : Next",
     'Print "TIME"; Timer',
     "Print param1; param2"] +
    ["Sub %s(a As Integer, b As Integer)\n%sEnd Sub" % (n, "  Inc a\n" if i == 0 else ("  b = b - 1\n" if i == 28 else ""))
     for i, n in enumerate(NAMES)]) + "\n"

PROGS = [
    ("table dispatch", TABLE),
    ("argument kinds", """Dim Integer a = 3, k(4) = (10, 20, 30, 40, 50)
Dim Float f = 1.5, g(2) = (0.5, 1.5, 2.5)
Dim String s$ = "ab", t$(1) = ("cd", "ef")
Const C = 7
Sub Bump(x As Integer, y As Float, z As String)
  x = x + 1 : y = y * 2 : z = z + "!"
End Sub
Sub Show(x As Integer, y As Float, z As String)
  Print x; y; " "; z
End Sub
Sub Arr(v() As Integer, w() As Float)
  v(1) = v(1) + 1 : w(2) = w(2) * 10
End Sub
Function Twice(v As Integer) As Integer
  Twice = v * 2
End Function
Call "Bump", a, f, s$
Call "Show", a, f, s$
Call "Bump", k(2), g(1), t$(1)
Call "Show", k(2), g(1), t$(1)
Call "Show", C, C / 2, "const"
Call "Show", a * 10 + 1, f / 3, s$ + t$(0)
Call "Show", Twice(a), Twice(k(0)) / 4, Left$(s$, 1)
Call "Arr", k(), g()
Print k(1); g(2)
Call "Show", 1.9, 2, "float to integer"
""" ),
    ("byval and byref", """Dim Integer a = 1, b = 2
Dim Float f = 3
Sub BV(ByVal x As Integer, ByRef y As Integer)
  x = x + 100 : y = y + 100
End Sub
Sub BR(ByRef y As Float)
  y = y + 0.25
End Sub
Call "BV", a, b
Print a; b
Call "BR", f
Print f
Call "BR", a
""" ),
    ("locals static recursion", """Dim Integer n = 0
Dim String who$ = "Down"
Sub Down(d As Integer)
  Local Integer m = d * 2
  Static Integer calls
  calls = calls + 1
  If d > 0 Then Call who$, d - 1
  n = n + m
  If d = 0 Then Print "calls"; calls
End Sub
Call who$, 5
Print n
Call who$, 3
Print n
""" ),
    ("call function", """Dim Integer a = 4
Dim Float f = 2.5
Dim String fn$(2) = ("AddI", "Half", "Rev$")
Function AddI(x As Integer, y As Integer) As Integer
  AddI = x + y
End Function
Function Half(v As Float) As Float
  Half = v / 2
End Function
Function Rev$(s As String)
  Local Integer i
  Rev$ = ""
  For i = Len(s) To 1 Step -1 : Rev$ = Rev$ + Mid$(s, i, 1) : Next
End Function
Print Call(fn$(0), a, 6); Call(fn$(1), f); " "; Call(fn$(2), "abc")
Print Call("AddI", Call("AddI", 1, 2), Call("AddI", 3, 4))
Dim Integer t = Call("addi", a, a) + 1
Print t
""" ),
    ("names", """Sub MixedCase(x As Integer)
  Print "mixed"; x
End Sub
Dim String nm$ = "mixedcase"
Call nm$, 1
Call "MIXEDCASE", 2
Call UCase$(nm$), 3
Call Left$("MixedCaseX", 9), 4
""" ),
    ("unknown name", """Sub A1(x As Integer)
End Sub
Print "go"
Call "A2", 1
""" ),
    ("too many arguments", """Sub A1(x As Integer)
End Sub
Print "go"
Call "A1", 1, 2
""" ),
    ("byref type", """Dim Float f = 1
Sub A1(ByRef x As Integer)
End Sub
Print "go"
Call "A1", f
""" ),
    ("a function called as a sub", """Function F1(x As Integer) As Integer
  F1 = x
End Function
Print "go"
Call "F1", 1
""" ),
    ("a sub called as a function", """Sub S1(x As Integer)
End Sub
Print "go"
Print Call("S1", 1)
""" ),
    ("error inside", """Dim Integer z = 0
Sub Div(x As Integer)
  Print x \\ z
End Sub
Print "go"
Call "Div", 5
""" ),
]

b = pc3.PC3(sys.argv[1])
b.attention()
mode_arg, fname = sys.argv[2], sys.argv[3]


def run(src):
    b.upload(src, 60)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(180)).replace("\r", "")
    lines = [l.rstrip() for l in out.split("\n") if l.strip() and l.strip() not in ("RUN", ">")]
    times = [l for l in lines if l.startswith("TIME")]
    body = [l for l in lines if not l.startswith("TIME")]
    return body, times


modes = ("OFF",) if "Error" in b.cmd("OPTION COMPILE OFF", 10) else ("OFF", "ON", "SHADOW")
got = {}
for name, src in PROGS:
    for m in modes:
        if len(modes) > 1:
            b.cmd("OPTION COMPILE " + m, 10)
        got[name + "/" + m] = run(src)
if len(modes) > 1:
    b.cmd("OPTION COMPILE OFF", 10)
b.close()

ok = True
for name, src in PROGS:
    outs = [got[name + "/" + m][0] for m in modes]
    if any(o != outs[0] for o in outs):
        ok = False
        print("BAD  %-28s modes differ" % name)
        for m in modes:
            print("     %-6s %s" % (m, got[name + "/" + m][0]))
if mode_arg == "record":
    json.dump(got, open(fname, "w"), indent=1)
    for k, (body, times) in got.items():
        print("%-36s %s %s" % (k, " | ".join(body)[-80:], " ".join(t[4:].strip() for t in times)))
    print("RBCALLNAME RECORDED" if ok else "RBCALLNAME RECORDED, MODES DIFFER")
else:
    ref = json.load(open(fname))
    for k, (body, times) in got.items():
        rb, rt = ref.get(k, ref.get(k.split("/")[0] + "/OFF", ([], [])))
        good = body == rb
        ok = ok and good
        print("%-4s %-36s %s" % ("ok" if good else "BAD", k, " | ".join(body)[-70:]))
        if times:
            print("     time was %s now %s" % (" ".join(t[4:].strip() for t in rt), " ".join(t[4:].strip() for t in times)))
        if not good:
            print("     was %s" % rb)
            print("     now %s" % body)
    print("RBCALLNAME", "PASS" if ok else "FAIL")
