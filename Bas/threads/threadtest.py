"""threadtest.py PORT name [name ...] - run the thread tests on a PicoMite and compare each with its expected output.

The tests are on the board's A: in A:/tt (send them by ZMODEM: pm_ymodem/tools/ymodem_test/tt.py PORTNUM 921600
name.bas A:/tt/name.bas zmodem). The console is taken to be at 921600. name.picomite.expected is used where it exists
(the PicoMite's own: an error is "[#n:line] ..." then "Error : ...", where the CMM2 has "Error in thread n, line m: ...").

The tests are the Colour Maximite 2's (CMM2V1.5 78d4172, tests/threads), copied as they are; the *_p.bas ones are the
PicoMite's versions of some of them, sized for its memory or using its own commands and modes:
  tlocals_p  OPTION LOCAL VARIABLES 400 and THREAD SIZE 12, 48: four threads of the CMM2's default 48 locals, and its
             recursion six levels deep (about 830 bytes a level) - a PC3's local region is 256 slots
  tmany_p    THREAD SIZE 6, 8 (ten at once in a PC3's 108 KB of SRAM and 256 local slots); MM.INFO(HEAP) for the
             CMM2's HEAPI; a full round first, as each local slot range takes its variables' records once, for good
  tbind_p, tstop_p   THREAD SIZE for the PicoMite's stacks and local slots
  tlock_p, tintown_p OPTION CONSOLE SERIAL: on a PC3 PRINT also scrolls the HDMI screen, which upsets the timings
  tframe_p   FRAMEBUFFER CREATE and FRAMEBUFFER COPY F, N, B for the CMM2's PAGE COPY 0 TO 1, B (MODE 2)
  tmapset_p  MODE 2 and MODE 3, the PicoMite's modes with a colour map, for the CMM2's MODE 7,4 and MODE 1,8
  tnet_p     TEMPR() on GP0 for the CMM2's GP6 (needs the Wi-Fi joined; tnetown runs as it is)
  tpin_p     GP0 an output wired to GP1, the interrupt's pin, for the CMM2's pull-up register trick (COM11's PC3:
             "gp0 is free and you can read it on gp1 which is linked")
  tqueue_p   the busy thread's figure 10000 for the CMM2's 50000 (a PC3 with COMPILE OFF counts about 27500)
and twrite_p and tstack_p are the PicoMite's own: each thread's FRAMEBUFFER WRITE target (the plan's section 9), and
StackNeed and JSON$'s nesting cap (Stage P5). tcond is the CMM2's a2db2ec (THREAD WAIT inside THREAD HOLD).
stackm.bas and stackm2.bas measure the stack and need a measuring build (-DTHREAD_STACK_REPORT). demo_input.bas is
the manual's balls and clock while INPUT waits, in MODE 3, and demo_net.bas WEB NTP and an HTTPS GET in a thread
while the main program counts: things to watch, not tests.
"""
import difflib, os, re, sys, time
import serial

HERE = os.path.dirname(os.path.abspath(__file__))
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
port, names = sys.argv[1], sys.argv[2:]
s = serial.Serial(port, 921600, timeout=0.05)


def run(name, limit=40):
    """RUN the test; with name.in beside this script, its lines are typed (a key at a time, then Enter, as
    the CMM2's threadcheck.py does) at each prompt that ends in '?'."""
    keys = []
    inp = os.path.join(HERE, name + ".in")
    if os.path.exists(inp):
        keys = [l for l in open(inp, encoding="latin-1").read().replace("\r", "").split("\n") if l]
    s.read(65536)
    s.write(('RUN "A:/tt/%s.bas"\r' % name).encode())
    b = ""
    typed = 0
    end = time.time() + limit
    while time.time() < end:
        b += s.read(4096).decode("latin-1")
        c = ANSI.sub("", b).replace("\r", "")
        if re.search(r"\n> $", c):
            return c, True
        if keys and c.count("?") > typed and c.rstrip(" ").endswith("?"):
            time.sleep(0.2)
            for ch in keys.pop(0) + chr(13):   # (Enter after each line, as the CMM2's runner types it)
                s.write(ch.encode("latin-1"))
                time.sleep(0.05)
            typed = c.count("?")
    return ANSI.sub("", b).replace("\r", ""), False


def lines(path):
    t = open(path, encoding="latin-1").read().replace("\r", "").split("\n")
    t = [l.rstrip() for l in t]
    while t and t[-1] == "":
        t.pop()
    return t


fails = 0
for name in names:
    out, ok = run(name)
    got = [l.rstrip() for l in out.split("\n")[1:] if l.strip() != ">"]
    while got and got[-1] == "":
        got.pop()
    exp_path = os.path.join(HERE, name + ".picomite.expected")
    if not os.path.exists(exp_path):
        exp_path = os.path.join(HERE, name + ".expected")
    exp = lines(exp_path)
    if not ok:
        print("%-10s NO PROMPT after 40 s: %r" % (name, out[-300:]))
        fails += 1
        break
    if got == exp:
        print("%-10s PASS" % name)
    else:
        fails += 1
        print("%-10s DIFFERS" % name)
        for d in difflib.unified_diff(exp, got, "expected", "PicoMite", lineterm="", n=1):
            print("    " + d)
s.close()
sys.exit(1 if fails else 0)
