"""rbprof.py PORT FILE.bas OUT.txt [OFF|ON|SHADOW] - run a program under
OPTION PROFILING ON, SAMPLE with OPTION COMPILE set as asked, and keep what
it prints (the [PCS] lines come at END) for pcs_report.py OUT.txt ELF.
The profiling option goes in as the program's first line, as the sample
tables live on the heap that RUN clears.  The report pages on the display
console, so every PRESS ANY KEY is answered."""
import sys, os, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

port, src, outf = sys.argv[1], sys.argv[2], sys.argv[3]
mode = sys.argv[4] if len(sys.argv) > 4 else "ON"
b = pc3.PC3(port)
b.attention()
b.cmd("OPTION COMPILE " + mode, 10)
b.upload("Option Profiling On, Sample\n" + open(src, encoding="latin-1").read(), 30)
b.drain(0.1)
b.send_line("RUN")
buf, t0, last = "", time.time(), time.time()
while time.time() - t0 < 600:
    got = b._read()
    if got:
        buf += got
        last = time.time()
    tail = pc3.ANSI.sub("", buf[-120:])
    if "ANY KEY" in tail.upper():
        buf += "\n"
        b.s.write(b" ")
        time.sleep(0.2)
        continue
    if tail.rstrip().endswith(">") and time.time() - last > 0.5:
        break
    time.sleep(0.05)
out = pc3.ANSI.sub("", buf).replace("\r", "")
open(outf, "w", encoding="utf-8").write(out)
b.cmd("OPTION COMPILE OFF", 10)
b.close()
print("%d lines, %d [PCS]" % (out.count("\n"), out.count("[PCS]")))
