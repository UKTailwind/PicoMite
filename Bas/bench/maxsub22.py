"""maxsub22.py PORT - MAX/MIN beyond the single-precision range (bug report
item 22) and 64-bit array subscripts (item 24).

MAX and MIN started from -FLT_MAX / FLT_MAX, so MAX(-1e300, -2e300) gave
-3.4e38; they now start from their first argument.  An integer subscript was
cut to 32 bits before the bounds check, so with DIM a(3), a(4294967297) read
a(1); a subscript that does not fit in 32 bits is now "Index out of bounds"."""
import sys, os, re, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

CASES = [
    ("?MAX(-1e300, -2e300)", "-1e+300"),
    ("?MIN(1e300, 2e300)", " 1e+300"),
    ("?MAX(3, 1, 2); MIN(3, 1, 2); MAX(-7); MIN(5)", " 3 1-7 5"),
    ("Dim a(3) : a(1) = 11 : ?a(1)", " 11"),
    ("?a(4294967297)", "Error : Index out of bounds"),
    ("?a(-4294967295)", "Error : Index out of bounds"),
    ("?a(2^31)", "Error : Index out of bounds"),
]

b = pc3.PC3(sys.argv[1])
b.attention()
b.cmd("New", 10)
ok = True
for cmd, want in CASES:
    got = pc3.ANSI.sub("", b.cmd(cmd, 10)).strip("\r\n")
    good = got.strip() == want.strip()
    ok = ok and good
    print("%-4s %-46s %s" % ("ok" if good else "BAD", cmd, got))
print("MAXSUB22", "PASS" if ok else "FAIL")
b.close()
