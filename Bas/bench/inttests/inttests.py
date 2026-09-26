"""inttests.py PORT OUT [TEST.bas ...] - put and run the interrupt tests; OUT
collects the result lines. The .bas files sit next to this script; with no
TEST names it runs the ones that need no wiring (ticktime, ticktest, polltest,
onkeyk, pidfirst). Also here:
  comloop.bas, pinint.bas  need GP0 jumpered to GP1
  scanclear.bas            the per-statement scan stops once PID/INTERRUPT are off
  wavtone.bas              needs audio (the PC3)
  oneshot.bas              RP2350 register addresses
  tickneg.bas              SETTICK must refuse a negative period
Compare the output with an earlier build's: each test prints fixed labels."""
import sys, os, time
sys.path.insert(0, r"D:/Dropbox/PicoMite/PicoMite/Bas/elite_tools")
import pc3
HERE = os.path.dirname(os.path.abspath(__file__))
port, outf = sys.argv[1], sys.argv[2]
b = pc3.PC3(port); b.attention()
b.cmd('Chdir "A:/"', 10)  # goldens.py leaves the board in A:/g
lines = []

def put(name):
    """Send a test only if A: lacks it or has a different size."""
    data = open(os.path.join(HERE, name), "rb").read()
    size = b.cmd('Print MM.Info(FILESIZE "A:/%s")' % name, 10).split()[-2:]
    if str((len(data) + 127) // 128 * 128) in size:  # XMODEM pads to 128-byte blocks
        return
    b.xmodem_send(name, data)
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
