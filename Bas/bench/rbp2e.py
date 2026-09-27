"""rbp2e.py PORT - Route B P2e: a library saved with symbols (docs/Interpreter_RouteB_Design.html).

LIBRARY LOAD saves the library's names as library symbols with a table of its
own, after the 0xFFFFFFFF marker and ahead of the CSUB and font records.  The
library here shares names with the program (in other spellings), has locals
that hide program globals, a label, recursion, a call back into the program,
and more than 63 names, so both short and long symbols are used; libcsub.bas
and libfont.bas (Testfiles) add a CSUB and a font after it.  Every program
runs with OPTION COMPILE OFF, ON and SHADOW and must print the same.

  RAM    LIBRARY LOAD ..., RAM; RAM LIST 5 spells the names; the slot has a table
  flash  LIBRARY LOAD ..., O; LIBRARY LIST ALL spells them; survives CPU RESTART
  merge  LIBRARY SAVE puts a program on top of it (the old table is kept)
  disk   LIBRARY DISK SAVE, DELETE, DISK LOAD, CPU RESTART

Needs a board with no flash library (it deletes the one it makes) and PSRAM.
Leaves OPTION COMPILE OFF."""
import sys, os, re, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

PORT = sys.argv[1]
TESTFILES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Testfiles")

# ---- the library -------------------------------------------------------------
W = ["w%02d" % i for i in range(1, 81)]
LIB = """' p2elib.bas - the library rbp2e.py loads (generated)
Dim Integer LibCount, shared_g = 5
Dim Float LibF = 1.5
""" + "".join("Dim Integer " + ", ".join(W[i:i + 20]) + "\n" for i in range(0, 80, 20)) + """Function LibFact(n As Integer) As Integer
  If n <= 1 Then LibFact = 1 Else LibFact = n * LibFact(n - 1)
End Function
Sub LibShadow(x As Integer)
  Local Integer total, i
  For i = 1 To x : total = total + i : Next
  LibCount = LibCount + total
End Sub
Function LibSumW() As Integer
  Local Integer s
""" + "".join("  %s = %d\n" % (w, i + 1) for i, w in enumerate(W)) + \
    "".join("  s = s + " + " + ".join(W[i:i + 10]) + "\n" for i in range(0, 80, 10)) + """  LibSumW = s
End Function
Sub LibJump(n As Integer)
  If n > 2 Then GoTo big
  LibCount = LibCount + 1
  Exit Sub
big:
  LibCount = LibCount + 100
End Sub
Function LibUsesProg() As Integer
  LibUsesProg = ProgHelper(3) + PROGVAL
End Function
Function LibReadShared() As Integer
  LibReadShared = shared_g
End Function
Function LibScale(v As Float) As Float
  LibScale = v * LibF
End Function
Dim Integer LibI, LibN, LibAcc
Sub LibLoop(n As Integer)
  LibN = n
  LibAcc = 0
  For LibI = 1 To LibN
    LibAcc = LibAcc + LibI * 3
    If LibAcc > 100000 Then LibAcc = LibAcc - 100000
  Next
End Sub
"""
NAMES = ["LibCount", "shared_g", "LibF", "w01", "w63", "w64", "w80", "LibFact", "LibShadow", "total",
         "LibSumW", "LibJump", "big", "LibUsesProg", "ProgHelper", "PROGVAL", "LibReadShared", "LibScale",
         "LibLoop", "LibAcc"]

FILES = '"A:/p2elib.bas", "A:/libcsub.bas", "A:/libfont.bas"'


def prog(load, merged=False):
    """The test program: load is its first line (or None when the library is in flash already)."""
    src = (load + "\n" if load else "") + """Option EXPLICIT
Option CONSOLE SERIAL
Dim Integer fails, tests, PROGVAL = 7, total = 99, i, a, b, s, m
Check "recursion", LibFact(10), 3628800
LibShadow 10
Check "a local hides a global", LIBCOUNT, 55
Check "the global it hid", total, 99
Check "80 names", LibSumW(), 3240
LibJump 1 : LibJump 5
Check "a label", libcount, 156
Check "calls back into the program", LibUsesProg(), 16
Check "one name, two spellings", Shared_G, 5
Shared_G = 11
Check "the library sees the write", LibReadShared(), 11
For i = 1 To 1000 : LibCount = LibCount + 1 : Next
Check "a loop over a library global", LibCount, 1156
Check "a float global", LibScale(4) * 10, 60
LibLoop 1000
Check "a loop in the library", LibAcc, 1500
a = 1234 : b = 5678
CHECKSUM a, b, s, m
Print "  CHECKSUM"; s; m
Check "the CSUB ran", (s <> 0) Or (m <> 0), 1
FONT 8
Print "  FONT 8 width"; MM.INFO(FONTWIDTH)
FONT 1
""" + ("""Check "the merged program", NewF(5), 21
""" if merged else "") + """Print "P2ELIB:" + Str$(tests - fails) + " of" + Str$(tests)
If fails = 0 Then Print "PASS" Else Print "FAIL"
Option CONSOLE BOTH
End
Function ProgHelper(q As Integer) As Integer
  ProgHelper = q * q
End Function
Sub Check(what$, got As integer, want As integer)
  tests = tests + 1
  If got = want Then
    Print "ok:   " + what$ + " =" + Str$(got)
  Else
    Print "FAIL: " + what$ + " got" + Str$(got) + " want" + Str$(want)
    fails = fails + 1
  EndIf
End Sub
"""
    return src


b = pc3.PC3(PORT)
b.attention()
ok = True
reference = None


def put(name, data):
    """Send a file only if A: lacks it or has a different size."""
    size = b.cmd('Print MM.Info(FILESIZE "A:/%s")' % name, 10).split()[-2:]
    if str((len(data) + 127) // 128 * 128) in size:  # XMODEM pads to 128-byte blocks
        return
    b.xmodem_send(name, data)
    b.drain(0.3)


def result(out):
    out = pc3.ANSI.sub("", out).replace("\r", "")
    return [l for l in out.split("\n") if re.match(r"(ok:|FAIL|P2ELIB|PASS|  CHECKSUM|  FONT|Error)", l)]


def check(name, src, modes=("OFF", "ON", "SHADOW")):
    """Run src in each mode; every run must PASS and print what the first run printed."""
    global ok, reference
    for mode in modes:
        b.cmd("OPTION COMPILE " + mode, 10)
        b.upload(src, 30)
        b.drain(0.1)
        body = result(b.run(120))
        stat = b.cmd('Print MM.Info(COMPILE)', 10).strip()
        good = "PASS" in body and (reference is None or body == reference)
        if reference is None and good:
            reference = body
        ok = ok and good
        print("%-4s %-7s %-6s %s | %s" % ("ok" if good else "BAD", name, mode, (body[-2:] if body else "(nothing)"), stat[-60:]))
        if not good:
            print("\n".join("     " + l for l in body))


def spelled(name, listing):
    """Every name the library uses appears in the listing, spelt as in the source."""
    global ok
    missing = [n for n in NAMES if not re.search(r"\b%s\b" % re.escape(n), listing)]
    good = not missing and "?" not in re.sub(r'"[^"]*"', "", listing)
    ok = ok and good
    print("%-4s %-7s listing spells every name%s" % ("ok" if good else "BAD", name, "" if good else ": missing " + " ".join(missing)))


def table(name, slot):
    """The image in flash/RAM slot 'slot' carries a symbol table after its text."""
    global ok
    b.upload("""Dim Integer base = MM.Info(FLASH ADDRESS %d), f = -1, q
For q = base To base + 131072 Step 4
  If Peek(WORD q) = &H314D5953 Then f = q - base : Exit For
Next
Print "TABLE"; f
""" % slot, 30)
    out = b.run(30)
    m = re.search(r"TABLE\s*(-?\d+)", out)
    good = m is not None and int(m.group(1)) > 0
    ok = ok and good
    print("%-4s %-7s symbol table at %s" % ("ok" if good else "BAD", name, m.group(1) if m else out[-60:]))


def restart():
    global b
    b.send_line("CPU RESTART")
    time.sleep(1)
    b.close()
    time.sleep(10)
    b = pc3.PC3(PORT)
    b.attention()


def cmd(line, timeout=30):
    out = pc3.ANSI.sub("", b.cmd(line, timeout)).replace("\r", "")
    print("     %s -> %s" % (line, out.strip().replace("\n", " | ")[-100:]))
    return out


# the board must have no flash library: this test makes one and deletes it
if "available" not in b.cmd("Flash List", 10).split("\n")[-1]:
    sys.exit("slot 3 is in use: this test needs a board without a flash library")
put("p2elib.bas", LIB.encode("latin-1"))
for f in ("libcsub.bas", "libfont.bas"):
    put(f, open(os.path.join(TESTFILES, f), "rb").read())

# RAM
check("RAM", prog("LIBRARY LOAD %s, RAM" % FILES))
spelled("RAM", cmd("RAM LIST 5, ALL", 30))
table("RAM", 8)

# flash
check("flash", prog("LIBRARY LOAD %s, O" % FILES))
spelled("flash", cmd("LIBRARY LIST ALL", 30))
table("flash", 3)
restart()
cmd("Flash List", 10)
check("restart", prog(None), ("ON",))

# the old LIBRARY SAVE puts a program on top
cmd("NEW", 10)
b.upload("""Function NewF(x As Integer) As Integer
  NewF = x * 3 + LibFact(3)
End Function
""", 30)
cmd("LIBRARY SAVE", 60)
reference = None
check("merge", prog(None, merged=True))
spelled("merge", cmd("LIBRARY LIST ALL", 30))
table("merge", 3)

# LIBRARY DISK SAVE and DISK LOAD
cmd('LIBRARY DISK SAVE "A:/p2e.lib"', 30)
cmd("LIBRARY DELETE", 30)
cmd('LIBRARY DISK LOAD "A:/p2e.lib"', 30)
check("disk", prog(None, merged=True), ("ON",))
restart()
cmd("Flash List", 10)
check("disk+rs", prog(None, merged=True), ("SHADOW",))

cmd("LIBRARY DELETE", 30)
cmd("NEW", 10)
b.cmd("OPTION COMPILE OFF", 10)
print("RBP2E", "PASS" if ok else "FAIL")
b.close()
