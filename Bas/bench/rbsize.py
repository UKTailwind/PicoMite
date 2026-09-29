"""rbsize.py PORT PROG [PROG ...] - how big each program's compiled stream is.

For every program: OPTION COMPILE ON, RUN it (the stream is compiled at RUN),
break into it a few seconds later, then read the stream's header from PSRAM
(PSRAMstream on a PC3: 8 MB of PSRAM, 144 KB slots) with PEEK and the
program's size from MEMORY.  The stream takes codeoff + codelen bytes: the
header page, the statement map, its bucket index, and the records.  A
program given as NAME=PATH is RUN from PATH.  Leaves OPTION COMPILE OFF."""
import sys, os, re, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STREAM = 0x11000000 + 0x600000 + 0x60000 + 5 * 144 * 1024  # PSRAMstream with 6 MB of heap PSRAM and 144 KB slots
MAGIC = 0x31304252

b = pc3.PC3(sys.argv[1])
b.attention()


def word(off):
    out = b.cmd("Print Peek(Word &H%X)" % (STREAM + off), 10)
    m = re.search(r"-?\d+", out)
    return int(m.group(0)) & 0xFFFFFFFF if m else None


print("%-10s %8s %10s %10s %8s %s" % ("program", "stmts", "stream B", "program K", "ratio", "status"))
for arg in sys.argv[2:]:
    name, path = arg.split("=", 1) if "=" in arg else (os.path.basename(arg), arg)
    b.cmd("OPTION COMPILE ON", 10)
    b.drain(0.2)
    b.send_line('RUN "%s"' % path)
    t0 = time.time()
    while time.time() - t0 < 6:
        b._read()
    b.s.write(b"\x03")
    time.sleep(1)
    b.s.write(b"\x03")
    try:
        b.wait_prompt(15)
    except Exception:
        b.s.write(b"\r")
        b.wait_prompt(15)
    b.cmd("CLS", 10)
    status = " ".join(b.cmd("Print MM.Info(COMPILE)", 10).split())[:60]
    if word(0) != MAGIC:
        print("%-10s no stream header (%s)" % (name, status))
        continue
    stmts, codeoff, codelen = word(20), word(24), word(28)
    mem = b.cmd("Memory", 20)
    m = re.search(r"Program:\s*\n?\s*(\d+)K", mem.replace("\r", ""))
    prog = int(m.group(1)) if m else 0
    size = codeoff + codelen
    print("%-10s %8d %10d %10d %8.2f %s" % (name, stmts, size, prog, size / 1024 / prog if prog else 0, status))
b.cmd("OPTION COMPILE OFF", 10)
b.close()
