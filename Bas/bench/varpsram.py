"""varpsram.py PORT - variables' records when arrays have filled the SRAM heap
(RP2350 with PSRAM).  RUN takes the records of the globals a program declares
(DIM, CONST, STATIC) before anything else, and a record chunk the program
needs after that comes from PSRAM when the SRAM heap is full.  Before this,
the records came from the SRAM heap only, as each chunk of 32 filled, and a
program whose arrays had taken the SRAM heap stopped at its 33rd, 65th...
variable with "Not enough memory for 2048 bytes" (Prince of Pico, 192
variables, 6 MB of PSRAM free).

The program fills the SRAM heap with 16 KB and then 2 KB arrays, then makes
100 globals the count does not see (no DIM) and calls a SUB with 40 LOCALs
(the local stack's second chunk), so that both kinds of record go to PSRAM
(VARADDR of the last of each is checked to be in PSRAM).  Then SAVE CONTEXT
CLEAR, 100 new globals that use the same records again, LOAD CONTEXT: the
saved values must be back, from the context's copy of the chunks in PSRAM
(the new globals' records are checked to be in PSRAM, where the old ones were).
Runs with OPTION COMPILE OFF and ON; both must print the expected lines.
Leaves OPTION COMPILE as it found it."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3


def chunks(items, n):
    return [items[i:i + n] for i in range(0, len(items), n)]


def summed(var, names):  # var = the sum of names, in lines MMBasic takes (255 characters)
    return ["%s = 0" % var] + ["%s = %s + %s" % (var, var, " + ".join(c)) for c in chunks(names, 20)]


src = ["' varpsram: records in PSRAM"]
src += ["Dim Integer " + ", ".join("a%d(1999)" % i for i in c) for c in chunks(range(30), 10)]  # 30 x 16 KB: more than any SRAM heap
src += ["Dim Integer " + ", ".join("b%d(255)" % i for i in c) for c in chunks(range(30), 10)]   # 30 x 2 KB: what is left of it
src.append("a0(5) = 123 : a29(5) = 456 : b29(3) = 789")
V = ["v%d" % k for k in range(1, 101)]
src += [" : ".join("v%d = %d" % (k, k) for k in c) for c in chunks(range(1, 101), 10)]  # made without DIM
src += summed("sv", V)
src += ['Print "G"; sv', 'Print "PS"; (Peek(VarAddr v100) >= &H11000000)', "Sub Deep(n)"]
L = ["l%d" % k for k in range(1, 41)]
src += ["  Local " + ", ".join(L)]
src += ["  " + " : ".join("l%d = n + %d" % (k, k - 1) for k in c) for c in chunks(range(1, 41), 10)]
src += ["  " + x for x in summed("sl", L)]
src += ['  Print "L"; sl', '  Print "LPS"; (Peek(VarAddr l40) >= &H11000000)', "End Sub", "Deep 5", "Save Context Clear"]
W = ["w%d" % k for k in range(1, 101)]
src += [" : ".join("w%d = %d" % (k, 2 * k) for k in c) for c in chunks(range(1, 101), 10)]  # the same records again
src += summed("sw", W) + ['Print "W"; sw', 'Print "WPS"; (Peek(VarAddr w100) >= &H11000000)', "Load Context"]
src += summed("sr", V) + ['Print "R"; sr; v100; a0(5); a29(5); b29(3); w1']
assert max(len(x) for x in src) < 250, max(len(x) for x in src)
PROG = "\n".join(src) + "\n"
WANT = ["G 5050", "PS 1", "L 980", "LPS 1", "W 10100", "WPS 1", "R 5050  100  123  456  789  0"]

b = pc3.PC3(sys.argv[1])
b.attention()
was = b.cmd("PRINT MM.INFO(COMPILE)", 10).strip()
ok = True
for mode in ("OFF", "ON"):
    b.cmd("OPTION COMPILE " + mode, 10)
    b.upload(PROG, 60)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120)).replace("\r", "")
    lines = [re.sub(r"\s+", " ", l.strip()) for l in out.split("\n") if l.strip() and l.strip() not in ("RUN", ">")]
    want = [re.sub(r"\s+", " ", w) for w in WANT]
    good = lines == want
    ok = ok and good
    print("%-4s COMPILE %-3s %s" % ("ok" if good else "BAD", mode, " | ".join(lines)[-160:]))
b.cmd("OPTION COMPILE " + ("OFF" if was.startswith("OFF") else "ON"), 10)
print("VARPSRAM", "PASS" if ok else "FAIL")
b.close()
sys.exit(0 if ok else 1)
