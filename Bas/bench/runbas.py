"""runbas.py PORT DEVPATH OUTFILE - LOAD a program from the board's drive, RUN it, save the output."""
import sys, re, time
sys.path.insert(0, r"D:/Dropbox/PicoMite/PicoMite/Bas/elite_tools")
import pc3
port, dev, outf = sys.argv[1:4]
b = pc3.PC3(port); b.attention()
print(b.cmd('LOAD "%s"' % dev, 60).strip()[-200:])
b.drain(0.1); b.send_line("RUN")
out = pc3.ANSI.sub("", b.wait_prompt(600))
open(outf, "w", encoding="utf-8").write(out)
print(out[-6000:])
b.close()
