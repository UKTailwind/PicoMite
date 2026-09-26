"""label8.py PORT - GOTO/GOSUB/RESTORE to a name that is also a SUB (bug report
item 8, RP2350).

SUB/FUNCTION names and labels share one table on the RP2350, and findlabel
returned whichever entry matched, so a SUB's small table index was used as a
program address: the program stopped silently.  A label may share a SUB's name
(as on the RP2040), and a jump to a name that is only a SUB must say "Cannot find
label"."""
import sys, os, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

CASES = [
    ("label and SUB share a name",
     "Sub Here\n  Print \"in SUB\"\nEnd Sub\nGosub Here\nPrint \"after\"\nHere\nRestore Here\nRead v\nPrint \"read\"; v\nEnd\nHere:\nPrint \"at label\"\nReturn\nData 42\n",
     ["at label", "after", "in SUB", "read 42"]),
    ("GOTO a name that is only a SUB",
     "Sub Foo\nEnd Sub\nPrint \"before\"\nGoto Foo\nPrint \"after\"\n",
     ["before", "Error : Cannot find label"]),
]

b = pc3.PC3(sys.argv[1])
b.attention()
ok = True
for name, src, want in CASES:
    b.upload(src, 30)
    b.drain(0.1)
    b.send_line("RUN")
    time.sleep(3)
    out = pc3.ANSI.sub("", b._read())
    got = [l.strip() for l in out.splitlines() if l.strip() and l.strip() not in ("RUN", ">") and not l.startswith("[")]
    print(name, got)
    ok = ok and got == want
print("LABEL8", "PASS" if ok else "FAIL")
b.close()
