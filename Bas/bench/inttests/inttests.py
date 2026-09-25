"""inttests.py PORT OUT [TEST.bas ...] - put and run the round-1 tests; OUT collects the result lines.
Assumes the .bas files sit next to this script."""
import sys, os, time
sys.path.insert(0, r"D:/Dropbox/PicoMite/PicoMite/Bas/elite_tools")
import pc3
HERE = os.path.dirname(os.path.abspath(__file__))
port, outf = sys.argv[1], sys.argv[2]
b = pc3.PC3(port); b.attention()
lines = []

def put(name):
    b.xmodem_send(name, open(os.path.join(HERE, name), "rb").read())
    b.drain(0.3)

def run(name, keys=None, timeout=120):
    print(b.cmd('LOAD "A:/%s"' % name, 30).strip()[-80:])
    b.drain(0.1); b.send_line("RUN")
    buf = ""
    if keys:
        t0 = time.time()
        while "READY" not in buf and time.time() - t0 < 10:
            buf += b._read()
        if "READY" not in buf:  # the program stopped before it was ready
            out = pc3.ANSI.sub("", buf + b.drain(0.5))
            print(out)
            lines.append("### " + name + "\n" + out)
            return
        for ch in keys:
            time.sleep(0.2)
            b.s.write(ch.encode())
    out = pc3.ANSI.sub("", buf + b.wait_prompt(timeout))
    print(out)
    lines.append("### " + name + "\n" + out)

KEYS = {"polltest.bas": "xyz", "onkeyk.bas": "AABA"}
tests = sys.argv[3:] or ["ticktime.bas", "ticktest.bas", "polltest.bas", "onkeyk.bas", "pidfirst.bas"]
for n in tests:
    put(n)
for n in tests:
    run(n, keys=KEYS.get(n))
open(outf, "w", encoding="utf-8").write("\n".join(lines))
b.close()
