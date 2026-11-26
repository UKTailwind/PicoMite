"""threadtest.py PORT name [name ...] - run the thread tests on a PicoMite and compare each with its expected output.

The tests are on the board's A: in A:/tt (send them by ZMODEM: pm_ymodem/tools/ymodem_test/tt.py PORTNUM 921600
name.bas A:/tt/name.bas zmodem). The console is taken to be at 921600. name.picomite.expected is used where it exists
(the PicoMite's own: an error is "[#n:line] ..." then "Error : ...", where the CMM2 has "Error in thread n, line m: ...").

The tests are the Colour Maximite 2's (CMM2V1.5 78d4172, tests/threads), copied as they are; the *_p.bas ones are the
PicoMite's versions of two of them, sized for its memory:
  tlocals_p  OPTION LOCAL VARIABLES 400 and THREAD SIZE 12, 48: four threads of the CMM2's default 48 locals, and its
             recursion six levels deep (about 830 bytes a level) - a PC3's local region is 192 slots
  tmany_p    THREAD SIZE 6, 8 (ten at once in a PC3's 108 KB of SRAM and 192 local slots); MM.INFO(HEAP) for the
             CMM2's HEAPI; a full round first, as each local slot range takes its variables' records once, for good
"""
import difflib, os, re, sys, time
import serial

HERE = os.path.dirname(os.path.abspath(__file__))
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
port, names = sys.argv[1], sys.argv[2:]
s = serial.Serial(port, 921600, timeout=0.05)


def run(name, limit=40):
    """RUN the test; with name.in beside this script, its lines are typed (a key at a time, as the CMM2's
    threadcheck.py does) at each prompt that ends in '?'."""
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
            for ch in keys.pop(0):
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
