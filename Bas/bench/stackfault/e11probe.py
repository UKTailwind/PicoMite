"""e11probe.py PORT [TRIALS] - does the XIP cache clean in save_psram_settings()
(misc/FileIO.c) hit RP2350-E11?

E11: a clean by set/way wrongly rewrites the tag of the line it cleans with the
address bits of the maintenance write.  save_psram_settings() cleans with
maintenance offsets 0..16K, so a dirty PSRAM line would come back as flash
0x10000000-0x10003FFF (way 0 at +set, way 1 at 0x2000+set).  The SDK's
xip_cache_clean_all() uses offsets at the top of the XIP window for this reason.

Each trial evicts sets 0x100-0x13F, POKEs 64 bytes of PSRAM that land in those
sets (dirty lines), does one kind of clean, then PEEKs both flash aliases and
the PSRAM words back.  Flash 0x10000100 (vector table end) and 0x10002100 (boot
banner code in main) are never run after boot, so a wrong line there is safe.
  control  nothing
  ws2812   WS2812 (disable_interrupts_pico -> save_psram_settings)
  low      clean by set/way at offsets 0x100.. and 0x2100.. (the firmware's way)
  sdk      clean by set/way at 0x3FFC000 + the same (xip_cache_clean_all's way)
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "elite_tools"))
import pc3

PROG = """Dim Integer k, f0(15), f1(15)
Const PSA = &H1165E100
Sub Evict
  Local Integer j, n, t
  For j = 0 To 7
    For n = 0 To 15
      t = Peek(WORD &H10004100 + &H2000 * j + 4 * n)
    Next
  Next
End Sub
Sub Trial(mode As Integer, nm$)
  Local Integer n, b0, b1, bp
  Evict
  For n = 0 To 15
    Poke WORD PSA + 4 * n, &H5A000000 + n
  Next
  Select Case mode
    Case 0
      Pause 1
    Case 1
      WS2812 B, gp7, 1, RGB(0, 0, 0)
    Case 2
      For n = 0 To 7
        Poke BYTE &H18000100 + 8 * n + 1, 0
        Poke BYTE &H18002100 + 8 * n + 1, 0
      Next
    Case 3
      For n = 0 To 7
        Poke BYTE &H1BFFC100 + 8 * n + 1, 0
        Poke BYTE &H1BFFE100 + 8 * n + 1, 0
      Next
  End Select
  For n = 0 To 15
    If Peek(WORD &H10000100 + 4 * n) <> f0(n) Then b0 = b0 + 1
    If Peek(WORD &H10002100 + 4 * n) <> f1(n) Then b1 = b1 + 1
    If Peek(WORD PSA + 4 * n) <> &H5A000000 + n Then bp = bp + 1
  Next
  Print "E11 "; nm$; " way0"; b0; " way1"; b1; " psram"; bp
  Evict
End Sub
Evict
For k = 0 To 15
  f0(k) = Peek(WORD &H10000100 + 4 * k)
  f1(k) = Peek(WORD &H10002100 + 4 * k)
Next
For k = 1 To TRIALS
  Trial 0, "control"
  Trial 3, "sdk"
  Trial 2, "low"
  Trial 1, "ws2812"
Next
Print "E11 done"
"""

port = sys.argv[1]
trials = int(sys.argv[2]) if len(sys.argv) > 2 else 3
b = pc3.PC3(port)
b.attention()
b.upload(PROG.replace("TRIALS", str(trials)), 30)
b.drain(0.1)
out = pc3.ANSI.sub("", b.run(120)).replace("\r", "")
for l in out.split("\n"):
    if l.strip() and l.strip() not in ("RUN", ">"):
        print(l)
b.close()
