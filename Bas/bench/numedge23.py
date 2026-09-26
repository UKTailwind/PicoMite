"""numedge23.py PORT - numeric edge cases (bug report items 23, 25/26 option A,
and 27).

23  ABS and SGN asked evaluate() for an integer, which converts a float
    through FloatToInt64: "Number too large" for anything from 2^63 up.
25  FloatToStr knew only +INF: -INF printed garbage on the RP2350 and hung the
    RP2040.  -INF and NaN now print as -INF and NAN.
26  NaN reached the C conversion and became 9223372036854775807 in an integer;
    FloatToInt64 (and FloatToInt32) now refuse it, and 2^63 itself, and INT
    and FIX (plain casts) now go through FloatToInt64 too.
27  An integer power ran one multiplication per unit of the exponent, with no
    CTRL-C; exponentiation by squaring gives the same wrapped results.
(Before the fix, the -INF line hangs an RP2040: do not run this on an old build.)"""
import sys, os, re, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

CASES = [
    ("?ABS(-1e19); ABS(-1.5); ABS(-7)", " 1e+19 1.5 7"),
    ("?SGN(1e300); SGN(-1e300); SGN(0); SGN(-3)", " 1-1 0-1"),
    ('?ABS("x")', "Error : Expected a number"),
    ("?1e308-(-1e308)", " INF"),
    ("?-1e308-1e308", "-INF"),
    ("x=1e308-(-1e308) : y=x-x : ?y", ("NAN", " INF")),   # the RP2040's soft float gives INF for INF-INF
    ("z%=y", "Error : Number too large"),
    ("?INT(y)", "Error : Number too large"),
    ("?FIX(-1e30)", "Error : Number too large"),
    ("?INT(-7.5); FIX(-7.5); INT(7.5); FIX(7.5)", "-8-7 7 7"),
    ("z%=9.223372036854775808e18", "Error : Number too large"),
    ("?2^10; 7^0; 0^0; 3^40; (-2)^63", " 1024 1 1-6289078614652622815-9223372036854775808"),
    ("t=Timer : q%=1^4000000000 : ?q%; Timer-t < 100", " 1 1"),
]

b = pc3.PC3(sys.argv[1])
b.attention()
b.cmd("New", 10)
ok = True
for cmd, want in CASES:
    got = pc3.ANSI.sub("", b.cmd(cmd, 20)).strip("\r\n")
    good = got in want if isinstance(want, tuple) else got == want
    ok = ok and good
    print("%-4s %-46s %r" % ("ok" if good else "BAD", cmd, got))
print("NUMEDGE23", "PASS" if ok else "FAIL")
b.close()
