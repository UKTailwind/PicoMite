"""elsespace.py PORT - IF and ELSE whose command token has two or more spaces after it.

tokenise drops one space after a command and keeps the rest, which skipspace
then steps over, so cmdline - sizeof(CommandToken) is not the token there.
cmd_if, cmd_else and the compiler's ELSE looked their IF-table entries up at
that address: the lookup missed, cmd_if and cmd_else fell back to scanning
the block for its ELSE/ENDIF, and the compiler left the ELSE as text.  Each
case below skips a 40-line block 2,000 times, so a missed lookup shows as a
much longer time than the same case with one space.  Runs OPTION COMPILE OFF
and ON (RP2350) and leaves the option as it found it."""
import re, sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

BLOCK = "\n".join(["    a = a - 1"] * 40)
SRC = """' elsespace.py: IF and ELSE with extra spaces after the token
Option Default Integer
Dim i, n, a, t1, t2, t3, t4
n = 2000
Timer = 0
For i = 1 To n
  If i > 0 Then
    a = a + 1
  Else ' one space
%(B)s
  EndIf
Next
t1 = Timer
Timer = 0
For i = 1 To n
  If i > 0 Then
    a = a + 1
  Else   ' three spaces
%(B)s
  EndIf
Next
t2 = Timer
Timer = 0
For i = 1 To n
  If i < 0 Then
%(B)s
  Else
    a = a + 1
  EndIf
Next
t3 = Timer
Timer = 0
For i = 1 To n
  If   i < 0 Then
%(B)s
  Else
    a = a + 1
  EndIf
Next
t4 = Timer
Print "A="; a
Print "T"; t1; t2; t3; t4
End
""" % {"B": BLOCK}


def run(b, mode):
    if mode:
        out = b.cmd("OPTION COMPILE " + mode, 10)
        if "rror" in out:
            return None
    out = pc3.ANSI.sub("", b.run(60))
    info = b.cmd("PRINT MM.INFO(COMPILE)", 10).strip()
    a = re.search(r"A=\s*(-?\d+)", out)
    t = re.search(r"T\s*([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)", out)
    if not a or not t:
        print(out[-600:])
        return dict(ok=False)
    return dict(ok=a.group(1) == "8000", a=a.group(1), t=[float(x) for x in t.groups()], info=info)


b = pc3.PC3(sys.argv[1])
b.attention()
was = b.cmd("PRINT MM.INFO(COMPILE)", 10).strip()
print("upload:", b.upload(SRC, 60))
fail = 0
for mode in ("OFF", "ON"):
    r = run(b, mode)
    if r is None:
        print(mode, ": no OPTION COMPILE on this build")
        continue
    if not r["ok"]:
        print(mode, "FAIL: wrong result", r.get("a"))
        fail = 1
        continue
    t1, t2, t3, t4 = r["t"]
    else_ok = t2 < 1.5 * t1 + 5
    if_ok = t4 < 1.5 * t3 + 5
    print("%-3s ELSE one space %7.1f ms, three %7.1f ms %s | IF one space %7.1f ms, three %7.1f ms %s | %s" % (
        mode, t1, t2, "ok" if else_ok else "SLOW", t3, t4, "ok" if if_ok else "SLOW", r["info"]))
    fail |= not (else_ok and if_ok)
if not was.startswith("OFF") and "rror" not in was:
    b.cmd("OPTION COMPILE ON", 10)
elif was.startswith("OFF"):
    b.cmd("OPTION COMPILE OFF", 10)
b.close()
print("FAIL" if fail else "PASS")
