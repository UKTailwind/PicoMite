"""rbstamp.py PORT - Route B P1: OPTION COMPILE, where the stream lives, the
stamp, the records and the executor (docs/Interpreter_RouteB_Design.html).
Route B is RP2350-only: on an RP2040 this reports SKIP.

P1a: OPTION COMPILE ON|OFF|SHADOW.  With PSRAM the stream has a region of its
own, so every RAM slot stays the user's: a compiled program's stream outlives
RAM SAVE 4 and RAM ERASE 4.  Without PSRAM the stream is in flash slot 2, which
the slot commands refuse while OPTION COMPILE is on, and only then.
P1b: RUN compiles when the program (or library) has changed since the stream
was written, and reuses the stream when it has not; a program saved without
symbols runs as text.  MM.INFO(COMPILE) says which happened.
P1c: one record per statement; a comment line or a label alone gets a NOP
(its line's head only).
P1d: the statements run from the stream - MM.INFO(COMPILE) counts them (RAN) -
and TRACE and ON ERROR SKIP print what the text loop prints.
Leaves OPTION COMPILE OFF and OPTION SYMBOLS ON."""
import sys, os, re, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

b = pc3.PC3(sys.argv[1])
b.attention()
ok = True


def check(name, got, good):
    global ok
    ok = ok and good
    print("%-4s %-34s %s" % ("ok" if good else "BAD", name, got))


def cmd(line):
    return pc3.ANSI.sub("", b.cmd(line, 20)).strip()


def run_status(src):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(60))
    m = re.search(r"STAT (.*)", out)
    return m.group(1).strip() if m else out.strip()[-120:]


r = cmd("OPTION COMPILE OFF")
if "rror" in r:
    print("RBSTAMP SKIP: no OPTION COMPILE here (Route B is RP2350-only):", r)
    sys.exit(0)
r = cmd("?MM.INFO(PSRAM SIZE)")
ram = bool(re.fullmatch(r"\s*\d+", r)) and int(r) > 0  # an error or 0: no PSRAM
print("stream:", "its own PSRAM region" if ram else "flash slot 2")

# P1a
check("MM.INFO(COMPILE) off", cmd("?MM.INFO(COMPILE)"), cmd("?MM.INFO(COMPILE)") == "OFF")
check("bad mode", cmd("OPTION COMPILE BOGUS"), "syntax" in cmd("OPTION COMPILE BOGUS").lower())
check("SHADOW accepted", cmd("OPTION COMPILE SHADOW") or "(no reply)", cmd("OPTION COMPILE SHADOW") == "")
cmd("OPTION COMPILE ON")
if ram:
    for c in ("RAM LOAD 4", "FLASH LOAD 2"):
        r = cmd(c)
        check(c + " not refused", r, "holds the compiled program" not in r)
else:
    for sub in ("SAVE", "LOAD", "ERASE", "RUN"):
        r = cmd("FLASH %s 2" % sub)
        check("FLASH %s 2 refused" % sub, r, "holds the compiled program" in r)
    r = cmd("FLASH LOAD 1")
    check("FLASH LOAD 1 not refused", r, "holds the compiled program" not in r)
    cmd("OPTION COMPILE OFF")
    r = cmd("FLASH LOAD 2")
    check("OFF: FLASH LOAD 2 not refused", r, "holds the compiled program" not in r)
cmd("OPTION COMPILE OFF")

# P1b
cmd("OPTION SYMBOLS ON")
cmd("OPTION COMPILE ON")
prog1 = 'a = 1\nPrint "STAT "; MM.Info(COMPILE)\n'
prog2 = 'a = 2\nPrint "STAT "; MM.Info(COMPILE)\n'
s1 = run_status(prog1)
n1 = [int(x) for x in re.findall(r"\d+", s1)] or [0, 0]
check("first RUN compiles", s1, s1.startswith("COMPILED"))
check("2 statements", s1, " STMTS 2 " in s1)
b.drain(0.1)
b.send_line("RUN")
s2 = re.search(r"STAT (.*)", pc3.ANSI.sub("", b.wait_prompt(30)))
s2 = s2.group(1).strip() if s2 else "?"
n2 = [int(x) for x in re.findall(r"\d+", s2)] or [0, 0]
check("same program: reused", s2, n2[0] == n1[0] and n2[1] == n1[1] + 1)
s3 = run_status(prog2)
n3 = [int(x) for x in re.findall(r"\d+", s3)] or [0, 0]
check("changed program: compiled", s3, n3[0] == n2[0] + 1)
if ram:  # the stream is in no RAM slot: saving over slot 4 and erasing it leave it alone
    r = cmd("RAM SAVE 4") + cmd("RAM ERASE 4")
    b.drain(0.1)
    b.send_line("RUN")
    s36 = re.search(r"STAT (.*)", pc3.ANSI.sub("", b.wait_prompt(30)))
    s36 = s36.group(1).strip() if s36 else "?"
    n36 = [int(x) for x in re.findall(r"\d+", s36)] or [0, 0]
    check("RAM SAVE/ERASE 4: stream kept", (r or "(no reply)") + " | " + s36,
          "rror" not in r and n36[0] == n3[0] and n36[1] == n3[1] + 1)
prog3 = """' a comment line: no record
x = 1 : y = 2 ' a comment in a statement
Lbl: z = 3
10 Print "STAT "; MM.Info(COMPILE)
"""
s35 = run_status(prog3)
check("comments, label, line number: 5", s35, " STMTS 5 " in s35)

# P1d: RUN hands over at once, so every statement runs from the stream - the
# loop body 100 times, the NEXT 100 times, and the Print that reads the count
prog4 = """a = 0
For i = 1 To 100
  a = a + 1
Next
Print "STAT "; MM.Info(COMPILE)
"""
s6 = run_status(prog4)
m = re.search(r"RAN (\d+)", s6)
check("loop runs from the stream", s6, bool(m) and int(m.group(1)) == 203)
# a GOTO, a GOSUB, a SUB, a FUNCTION and an IF...THEN on one line (the IF
# runs the GOTO after THEN itself): 21 statements
prog5 = """n = 0
Again: n = n + 1
If n < 5 Then GoTo Again
GoSub G1
S1 n
x = F1(n)
Print "STAT "; x; " "; MM.Info(COMPILE)
End
G1: n = n * 2
Return
Sub S1 v
  n = v + 1
End Sub
Function F1(v)
  F1 = v * 3
End Function
"""
s7 = run_status(prog5)
m = re.search(r"^(\d+) .*RAN (\d+)", s7)
check("jumps and calls", s7, bool(m) and m.group(1) == "33" and int(m.group(2)) == 21)
# TRACE prints a comment line's number too; ON ERROR SKIP resumes after the
# failing statement.  The output must match the text loop's exactly.
prog6 = """' header
Trace On
For i = 1 To 2
  ' in the loop
  a = i
Next
L1:
On Error Skip 2
b = 1 / 0 : c = 2
Print "E"; MM.ErrNo; b; c
Trace Off
Print "STAT "; MM.Info(COMPILE)
"""
outs = []
for mode in ("OFF", "ON"):
    cmd("OPTION COMPILE " + mode)
    b.upload(prog6, 30)
    b.drain(0.1)
    o = pc3.ANSI.sub("", b.run(60))
    outs.append(o[:o.find("STAT")].strip() if "STAT" in o else o.strip())
    last = o
check("TRACE, ON ERROR SKIP as text", outs[1].replace("\r\n", " | ")[-60:], outs[0] == outs[1] and "RAN" in last and "[" in outs[0])
if outs[0] != outs[1]:
    print("  text:  ", repr(outs[0]))
    print("  stream:", repr(outs[1]))
cmd("OPTION SYMBOLS OFF")
s4 = run_status(prog1)
check("saved as text: runs as text", s4, s4 == "TEXT: saved without symbols")
cmd("OPTION SYMBOLS ON")
cmd("OPTION COMPILE OFF")
s5 = run_status(prog1)
check("COMPILE OFF", s5, s5 == "OFF")

print("RBSTAMP", "PASS" if ok else "FAIL")
b.close()
