"""symsizes.py PORT FILE.bas... - program sizes saved as text and with symbols.

Uploads each file with AUTOSAVE (N, no crunch) twice, with OPTION SYMBOLS OFF and ON,
and prints the "Saved nnn bytes" of each (None if the save failed, eg too big)."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

port = sys.argv[1]
b = pc3.PC3(port)
b.attention()
print("%-28s %10s %10s %8s" % ("program", "text", "symbols", "change"))
for path in sys.argv[2:]:
    src = open(path, encoding="latin-1").read()
    got = {}
    for mode in ("OFF", "ON"):
        b.cmd("OPTION SYMBOLS " + mode, 5)
        try:
            saved, n = b.upload(src, timeout=300)
        except TimeoutError as e:
            saved = None
            b.attention()
        got[mode] = saved
    a, c = got["OFF"], got["ON"]
    ch = ("%+d" % (c - a)) if (a and c) else "-"
    print("%-28s %10s %10s %8s" % (os.path.basename(path), a, c, ch), flush=True)
b.cmd("OPTION SYMBOLS ON", 5)
b.cmd("NEW", 5)
b.close()
