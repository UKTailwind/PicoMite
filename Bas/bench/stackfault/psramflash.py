"""psramflash.py PORT [ROUNDS] - with no XIP cache clean in disable_interrupts_pico()
(misc/FileIO.c), is PSRAM data still safe across flash writes, and low flash
still right after the timing commands?

A flash erase or program flushes the XIP cache, which throws away PSRAM writes
still sitting in it.  The SDK's flash_range_erase, flash_range_program and
flash_do_cmd clean the cache first (at RP2350-E11-safe offsets); this checks
that nothing else is needed.  Each step POKEs 2 KB of PSRAM (dirty lines, a new
pattern each time), does one operation, then PEEKs the 2 KB back and compares
flash 0x10000100/0x10002100 (never run after boot) with what it held at the
start, which a clean at the wrong offsets would change (see e11probe.py).
  file     OPEN/PRINT/CLOSE a file on A: (littlefs erase, program, sync)
  kill     KILL it (a littlefs commit)
  flashid  MM.INFO(FLASH SIZE) (flash_do_cmd)
  ws2812   WS2812 on gp7 (interrupts off, no flash)
  bitstr   BITSTREAM on gp2
With --control a last step, discard, invalidates the 2 KB's cache lines without
cleaning them (what a flush does with no clean before it): it must show PSRAM
words wrong, or this test could not see the loss.  The board is restarted after
it, as other dirty PSRAM lines in those sets are lost too.
Prints PSF <op> <psram words wrong of 512> <flash words wrong of 32>.
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "elite_tools"))
import pc3

PROG = """Dim Integer k, r, op, f0(15), f1(15), d(9)
Dim Integer c(11)
Const PSA = &H1165C000
Dim nm$(5) = ("file", "kill", "flashid", "ws2812", "bitstr", "discard")
For k = 0 To 15
  f0(k) = Peek(WORD &H10000100 + 4 * k)
  f1(k) = Peek(WORD &H10002100 + 4 * k)
Next
For k = 0 To 9 : d(k) = 250 : Next
For r = 1 To ROUNDS
  For op = 0 To 4
    DoStep op, r * 16 + op
  Next
Next
If CONTROL Then DoStep 5, 999
Print "PSF done"
Sub DoStep(op As Integer, tag As Integer)
  Local Integer n, bp, bf, pv
  pv = &H3C000000 + tag * &H1000
  For n = 0 To 511
    Poke WORD PSA + 4 * n, pv + n
  Next
  Select Case op
    Case 0
      Open "A:/psf.txt" For Output As #1
      Print #1, "psramflash "; tag
      Close #1
    Case 1
      Kill "A:/psf.txt"
    Case 2
      n = MM.Info(FLASH SIZE)
    Case 3
      WS2812 B, gp7, 12, c()
    Case 4
      BITSTREAM gp2, 10, d()
    Case 5
      For n = 0 To 255
        Poke BYTE &H1BFFC000 + 8 * n, 0
        Poke BYTE &H1BFFE000 + 8 * n, 0
      Next
  End Select
  For n = 0 To 511
    If Peek(WORD PSA + 4 * n) <> pv + n Then bp = bp + 1
  Next
  For n = 0 To 15
    If Peek(WORD &H10000100 + 4 * n) <> f0(n) Then bf = bf + 1
    If Peek(WORD &H10002100 + 4 * n) <> f1(n) Then bf = bf + 1
  Next
  Print "PSF "; nm$(op); bp; bf
End Sub
"""

port = sys.argv[1]
rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 5
b = pc3.PC3(port)
b.attention()
control = "--control" in sys.argv
b.upload(PROG.replace("ROUNDS", str(rounds)).replace("CONTROL", "1" if control else "0"), 30)
b.drain(0.1)
out = pc3.ANSI.sub("", b.run(300)).replace("\r", "")
bad = 0
for l in out.split("\n"):
    if l.strip() and l.strip() not in ("RUN", ">"):
        print(l)
        f = l.split()
        if f[:1] == ["PSF"] and len(f) == 4:
            if f[1] == "discard":
                print("CONTROL", "sees the loss" if f[2] != "0" else "DOES NOT see the loss")
                bad += f[2] == "0"
            elif f[2] != "0" or f[3] != "0":
                bad += 1
print("PSRAMFLASH", "PASS" if bad == 0 and "PSF done" in out else "FAIL")
if control:
    b.send_line("CPU RESTART")
b.close()
