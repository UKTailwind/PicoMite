"""g3run.py PORT m0|m33 [OUTFILE] - AUTOSAVE g3_<cpu>.bas into the board and RUN it."""
import sys, os
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "elite_tools"))
import pc3

port, cpu = sys.argv[1], sys.argv[2]
outf = sys.argv[3] if len(sys.argv) > 3 else os.path.join(HERE, "g3_%s_out.txt" % cpu)
src = open(os.path.join(HERE, "g3_%s.bas" % cpu)).read()
b = pc3.PC3(port)
b.attention()
print("upload:", b.upload(src, 60))
out = pc3.ANSI.sub("", b.run(600))
open(outf, "w", encoding="utf-8").write(out)
print(out)
b.close()
