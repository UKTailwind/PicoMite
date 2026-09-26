"""tknbuf10.py PORT - a line whose tokenised form does not fit in tknbuf (bug
report item 10).

tknbuf is 256 bytes and tokenise() never checked its output position, so a
line that tokenises to more ran on over the variables after it.  A program line
of short assignments does that (every implied LET adds a 2-byte token): it
restarted the RP2040 VGA board.  At the prompt every statement that starts with
an operator gets the CALC token and MM.ANSWER, so "+1:" repeated 80 times hung
the same board.  Both must now stop with "Line is too long" and leave the board
running, and a long line that does fit must still work."""
import sys, os, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3


def settle(b, secs):
    time.sleep(secs)
    return pc3.ANSI.sub("", b.drain(0.5))


def alive(b):
    """The board still answers at the prompt and did not restart."""
    b.send_line("?1+1")
    out = settle(b, 1.5)
    return " 2" in out


b = pc3.PC3(sys.argv[1])
b.attention()
ok = True

# 1: a program line of 62 implied LETs, about 374 bytes tokenised
big = ":".join("a=%d" % (i % 10) for i in range(62))
b.log.clear()
b.upload("Print \"start\"\n" + big + "\nPrint \"end\"\n", 30)
out = pc3.ANSI.sub("", "".join(b.log))
good = "Line is too long" in out and "FAULT" not in out and "MMBasic" not in out
good = good and alive(b)
print("program line, %d chars -> %s" % (len(big), "Line is too long" if good else "FAIL\n" + out[-400:]))
ok = ok and good

# 2: a long program line that does fit still runs
fits = ":".join("v%d=%d" % (i, i) for i in range(1, 21)) + ":Print \"sum\";" + "+".join("v%d" % i for i in range(1, 21))
b.upload(fits + "\n", 30)
b.drain(0.1)
b.send_line("RUN")
out = settle(b, 2)
good = "sum 210" in out
print("program line, %d chars that fits -> %s" % (len(fits), "runs" if good else "FAIL\n" + out[-400:]))
ok = ok and good

# 3: the prompt, implied CALC on every statement, about 1190 bytes tokenised
line = "+1:" * 80
b.send_line(line)
out = settle(b, 3)
good = "Line is too long" in out and "FAULT" not in out and alive(b)
print("prompt, %d chars of +1: -> %s" % (len(line), "Line is too long" if good else "FAIL\n" + out[-400:]))
ok = ok and good

print("TKNBUF10", "PASS" if ok else "FAIL")
b.close()
