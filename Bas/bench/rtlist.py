"""rtlist.py PORT [DIR] - LIST round trip for symbols, run on the board.

For every .bas file in DIR (default A:/g): LOAD it with OPTION SYMBOLS OFF and
SAVE it as A:/rt/<name>.off, then LOAD it with OPTION SYMBOLS ON and SAVE it as
A:/rt/<name>.on.  SAVE writes what LIST prints, so the two files must be
identical.  A small BASIC program then compares every pair on the board and
prints the ones that differ.  Also prints the "Saved nnn bytes" of both loads.
"""
import sys, os, re, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

port = sys.argv[1]
d = sys.argv[2] if len(sys.argv) > 2 else "A:/g"
b = pc3.PC3(port)
b.attention()
b.cmd('On Error Skip: Mkdir "A:/rt"', 10)
listing = b.cmd('f$=Dir$("%s/*.bas",FILE):Do While f$<>"":Print f$:f$=Dir$():Loop' % d, 60)
names = [l.strip() for l in listing.split("\n") if l.strip().lower().endswith(".bas")]
print(len(names), "programs")
sizes = []
for n in names:
    base = n[:-4]
    row = [base]
    for mode, ext in (("OFF", "off"), ("ON", "on")):
        b.cmd("OPTION SYMBOLS " + mode, 5)
        out = b.cmd('LOAD "%s/%s"' % (d, n), 60)
        m = re.search(r"Saved\s+(\d+)\s+bytes", out)
        err = "Error" in out
        row.append(m.group(1) if m else ("ERR" if err else "?"))
        out2 = b.cmd('SAVE "A:/rt/%s.%s"' % (base, ext), 60)
        if "Error" in out or "Error" in out2:
            row.append("[%s] %s" % (mode, (out + out2).strip().replace("\n", " ")[-120:]))
    sizes.append(row)
    print(" ".join(row))
b.cmd("OPTION SYMBOLS ON", 5)
cmp_prog = r'''
Dim f$, a$, b$, n%, bad%
f$ = Dir$("A:/rt/*.off", FILE)
Do While f$ <> ""
  n% = n% + 1
  Open "A:/rt/" + f$ For Input As #1
  Open "A:/rt/" + Left$(f$, Len(f$) - 4) + ".on" For Input As #2
  Do While Not Eof(#1)
    Line Input #1, a$
    If Eof(#2) Then Print "SHORT "; f$: bad% = bad% + 1: Exit Do
    Line Input #2, b$
    If a$ <> b$ Then Print "DIFF "; f$: Print " off: "; a$: Print " on:  "; b$: bad% = bad% + 1: Exit Do
  Loop
  If Not Eof(#2) And a$ = b$ Then Print "LONG "; f$: bad% = bad% + 1
  Close #1: Close #2
  f$ = Dir$()
Loop
Print "compared"; n%; " pairs,"; bad%; " differ"
'''
saved, _ = b.upload(cmp_prog, timeout=60)
print(b.run(900))
b.close()
