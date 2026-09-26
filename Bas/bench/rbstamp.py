"""rbstamp.py PORT - Route B P1a/P1b: OPTION COMPILE, the stream slot guard
and the stamp (docs/Interpreter_RouteB_Design.html).

P1a: OPTION COMPILE ON|OFF|SHADOW; while it is on, the slot that holds the
stream (RAM slot 4 with PSRAM, flash slot 2 without) is refused to the slot
commands, and only then.
P1b: RUN compiles when the program (or library) has changed since the stream
was written, and reuses the stream when it has not; a program saved without
symbols runs as text.  MM.INFO(COMPILE) says which happened.
P1c: one record per statement - comments get none.
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


r = cmd("?MM.INFO(PSRAM SIZE)")
ram = bool(re.fullmatch(r"\s*\d+", r)) and int(r) > 0  # an error or 0: no PSRAM
slotcmd = ("RAM", 4) if ram else ("FLASH", 2)
print("stream slot:", "%s %d" % slotcmd)

# P1a
cmd("OPTION COMPILE OFF")
check("MM.INFO(COMPILE) off", cmd("?MM.INFO(COMPILE)"), cmd("?MM.INFO(COMPILE)") == "OFF")
check("bad mode", cmd("OPTION COMPILE BOGUS"), "syntax" in cmd("OPTION COMPILE BOGUS").lower())
check("SHADOW accepted", cmd("OPTION COMPILE SHADOW") or "(no reply)", cmd("OPTION COMPILE SHADOW") == "")
cmd("OPTION COMPILE ON")
for sub in ("SAVE", "LOAD", "ERASE", "RUN"):
    r = cmd("%s %s %d" % (slotcmd[0], sub, slotcmd[1]))
    check("%s %s %d refused" % (slotcmd[0], sub, slotcmd[1]), r, "holds the compiled program" in r)
other = 1
r = cmd("%s LOAD %d" % (slotcmd[0], other))
check("%s LOAD %d not refused" % (slotcmd[0], other), r, "holds the compiled program" not in r)
cmd("OPTION COMPILE OFF")
r = cmd("%s LOAD %d" % slotcmd)
check("OFF: %s LOAD %d not refused" % slotcmd, r, "holds the compiled program" not in r)

# P1b
cmd("OPTION SYMBOLS ON")
cmd("OPTION COMPILE ON")
prog1 = 'a = 1\nPrint "STAT "; MM.Info(COMPILE)\n'
prog2 = 'a = 2\nPrint "STAT "; MM.Info(COMPILE)\n'
s1 = run_status(prog1)
n1 = [int(x) for x in re.findall(r"\d+", s1)] or [0, 0]
check("first RUN compiles", s1, s1.startswith("COMPILED"))
check("2 statements", s1, s1.endswith("STMTS 2"))
b.drain(0.1)
b.send_line("RUN")
s2 = re.search(r"STAT (.*)", pc3.ANSI.sub("", b.wait_prompt(30)))
s2 = s2.group(1).strip() if s2 else "?"
n2 = [int(x) for x in re.findall(r"\d+", s2)] or [0, 0]
check("same program: reused", s2, n2[0] == n1[0] and n2[1] == n1[1] + 1)
s3 = run_status(prog2)
n3 = [int(x) for x in re.findall(r"\d+", s3)] or [0, 0]
check("changed program: compiled", s3, n3[0] == n2[0] + 1)
prog3 = """' a comment line: no record
x = 1 : y = 2 ' a comment in a statement
Lbl: z = 3
10 Print "STAT "; MM.Info(COMPILE)
"""
s35 = run_status(prog3)
check("comments, label, line number: 4", s35, s35.endswith("STMTS 4"))
cmd("OPTION SYMBOLS OFF")
s4 = run_status(prog1)
check("saved as text: runs as text", s4, s4 == "TEXT: saved without symbols")
cmd("OPTION SYMBOLS ON")
cmd("OPTION COMPILE OFF")
s5 = run_status(prog1)
check("COMPILE OFF", s5, s5 == "OFF")

print("RBSTAMP", "PASS" if ok else "FAIL")
b.close()
