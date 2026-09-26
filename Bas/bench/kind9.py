"""kind9.py PORT - a FUNCTION called as a statement, or a SUB used in an
expression (bug report item 9).

The RP2350's FindSubFun ignored which kind it was asked for, so the routine
ran part-way and then failed inside itself with a misleading error; the RP2040
reports the mistake at the call.  Both must now stop at the call without
running any of the routine."""
import sys, os, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

CASES = [
    ("FUNCTION as a statement",
     "Function Dbl(a)\n  Print \"in Dbl\"; a\n  Dbl = a * 2\nEnd Function\nDbl 3\nPrint \"after\"\n",
     "[5] Dbl 3"),
    ("SUB in an expression",
     "Sub Sh(a)\n  Print \"in Sh\"; a\nEnd Sub\nx = Sh(3)\nPrint \"x=\"; x\n",
     "[4] x = Sh(3)"),
    ("right kinds still work",
     "Function Dbl(a)\n  Dbl = a * 2\nEnd Function\nSub Sh(a)\n  Print \"in Sh\"; a\nEnd Sub\nSh Dbl(4)\nPrint \"after\"\n",
     None),
]

b = pc3.PC3(sys.argv[1])
b.attention()
ok = True
for name, src, errline in CASES:
    b.upload(src, 30)
    b.drain(0.1)
    b.send_line("RUN")
    time.sleep(3)
    out = pc3.ANSI.sub("", b._read())
    lines = [l.strip() for l in out.splitlines() if l.strip() and l.strip() not in ("RUN", ">")]
    print("%-24s %s" % (name, " | ".join(lines)))
    if errline:
        good = bool(lines) and lines[0] == errline and not any(l.startswith("in ") for l in lines)
    else:
        good = lines == ["in Sh 8", "after"]
    ok = ok and good
print("KIND9", "PASS" if ok else "FAIL")
b.close()
