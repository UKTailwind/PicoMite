"""symtime.py PORT NAME... - run programs from A:/g saved as text and with symbols, same firmware.

For each name: OPTION SYMBOLS OFF, LOAD, RUN; then OPTION SYMBOLS ON, LOAD, RUN.  Prints
both outputs' last lines side by side (the timing programs print their time)."""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

port = sys.argv[1]
plain = "--plain" in sys.argv  # firmware without OPTION SYMBOLS: just LOAD and RUN
names = [a for a in sys.argv[2:] if not a.startswith("--")]
b = pc3.PC3(port)
b.attention()
for name in names:
    res = {}
    for mode in (("OFF", "ON") if not plain else ("OFF", "ON")):
        if not plain:
            b.cmd("OPTION SYMBOLS " + mode, 5)
        b.cmd('LOAD "A:/g/%s.bas"' % name, 60)
        out = b.run(300)
        lines = [l.rstrip() for l in out.split("\n")[1:] if l.strip() not in ("", ">")]
        res[mode] = lines[-4:]
    print("%-10s OFF: %s" % (name, " | ".join(res["OFF"])))
    print("%-10s ON:  %s" % ("", " | ".join(res["ON"])))
if not plain:
    b.cmd("OPTION SYMBOLS ON", 5)
b.close()
