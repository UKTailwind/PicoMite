' bench_gfx.bas - Phase 1 graphics pairs: H4 (argument marshalling), E8, E9
' (docs/Interpreter_Performance_Plan.html 4.3/4.4). Same method as
' bench_micro.bas: time N passes of a FOR loop holding one statement,
' subtract the empty loop, print BENCH label us_per_stmt cycles_per_stmt.
' bench_gfx.py prints the SAME label for the same test, so the two pair by
' label. local.* labels are the SUB-on-LOCALs versions (Python: inside a def).
' Display: MODE 3 (640x480, 4-bit RGB121) on RP2350 builds, the same
' geometry and depth as MicroPython's RGB640_4; MODE 2 (320x240, 4-bit
' RGB121) on the RP2040, its only colour mode. Both draw with DrawPixel16 /
' DrawRectangle16 straight into the display buffer N, as framebuf draws into
' hdmi_fb: no FRAMEBUFFER CREATE / WRITE F.
' PROFILE% = 1 starts the PC sampler after Timer = 0; PSECT% picks one
' section (0 all, 1 H4, 2 E8, 3 E9). A profiled END report pages: run it
' with runbas_paged.py. Timed runs: PROFILE% = 0.
Option default float
Const PROFILE% = 0
Const PSECT% = 0
Const WANTMODE% = 3
Const N% = 20000
Const NE% = 2000
Const EA% = 400
Const PASSES% = 5
Dim i%, k%, px%, py%, a%, b%, x%, y%, u%, v%, s%, w%, c%, p%, q%, r%, m%
Dim wht%, gmode%, pcsn%, homemode%
Dim fx, fy, fc
Dim mhz, base_us, t0, e8b, e8t, e8a, npix
Dim geo$
' the E8 array test's 8 KB, taken before the sampler so that a profiled
' run sizes the sampler around it and the PC3 keeps it in SRAM
Dim xs%(511), ys%(511)
mhz = Val(MM.Info(CPUSPEED)) / 1000000
x% = 10 : y% = 20 : c% = 1 : u% = 17 : v% = 27 : w% = 1 : s% = 8
p% = 50 : q% = 50 : r% = 4 : m% = 1 : b% = 7
fx = 10 : fy = 20 : fc = 1
wht% = &HFFFFFF
npix = PASSES% * 4096
For k% = 0 To 511
  xs%(k%) = k% Mod 64
  ys%(k%) = k% \ 64
Next
' END keeps the current MODE, so put back the one the prompt had: MODE 1
' when the screen was 640 or more wide at RUN, else MODE 2 (no MM.INFO
' reads the mode itself)
homemode% = 1
If MM.HRes < 640 Then homemode% = 2
' results go to the serial console only, so no text is drawn or scrolled
' on the screen; END puts OPTION CONSOLE BOTH back
Option console serial
Print "BENCHSTART "; MM.Device$; " "; MM.Ver; " "; mhz; " MHz"
gmode% = 2
If Instr(MM.Device$, "RP2350") > 0 Then gmode% = WANTMODE%
On Error Skip 1
Mode gmode%
If MM.Errno Then
  gmode% = 2
  Mode 2
EndIf
geo$ = Str$(MM.HRes) + "x" + Str$(MM.VRes)
' sampler tables: 16 bytes an entry plus ~15 KB of counters; sized from
' the free heap, never by running it out (8192 on an RP2350 with PSRAM,
' else 4096, halved while it would leave under 20 KB)
pcsn% = 4096
If Instr(MM.Device$, "RP2350") > 0 And MM.Info(HEAP) > 400000 Then pcsn% = 8192
Do While pcsn% > 512 And pcsn% * 16 + 20000 > MM.Info(HEAP)
  pcsn% = pcsn% \ 2
Loop

Timer = 0
If PROFILE% Then Option profiling on, sample, pcsn%
' ---- controls: the empty loop every top-level test subtracts, and a%=b%
t0 = Timer
For i% = 1 To N%
Next
base_us = (Timer - t0) * 1000
Emit "ctl.loop.empty", base_us / N%

t0 = Timer
For i% = 1 To N%
  a% = b%
Next
Rep "ctl.assign", Timer - t0

If PSECT% = 0 Or PSECT% = 1 Then
  ' ---- H4: the same statement with literal and with variable arguments
  t0 = Timer
  For i% = 1 To N%
    Pixel 10,20,1
  Next
  Rep "H4.pixel.lit", Timer - t0

  t0 = Timer
  For i% = 1 To N%
    Pixel x%,y%,c%
  Next
  Rep "H4.pixel.var", Timer - t0

  t0 = Timer
  For i% = 1 To N%
    Pixel x%,20,1
  Next
  Rep "H4.pixel.var_x", Timer - t0

  t0 = Timer
  For i% = 1 To N%
    Pixel 10,20,c%
  Next
  Rep "H4.pixel.var_c", Timer - t0

  t0 = Timer
  For i% = 1 To N%
    Pixel fx,fy,fc
  Next
  Rep "H4.pixel.var_float", Timer - t0

  t0 = Timer
  For i% = 1 To N%
    Pixel 10,20
  Next
  Rep "H4.pixel.lit_2arg", Timer - t0

  ' lit_3digit and lit_offscreen have the same text length (9 characters
  ' of arguments), so their difference is the C write alone
  t0 = Timer
  For i% = 1 To N%
    Pixel 100,200,1
  Next
  Rep "H4.pixel.lit_3digit", Timer - t0

  t0 = Timer
  For i% = 1 To N%
    Pixel 999,999,1
  Next
  Rep "H4.pixel.lit_offscreen", Timer - t0

  t0 = Timer
  For i% = 1 To N%
    Line 10,20,17,27,1,1
  Next
  Rep "H4.line.lit", Timer - t0

  t0 = Timer
  For i% = 1 To N%
    Line 10,20,17,27
  Next
  Rep "H4.line.lit_4arg", Timer - t0

  t0 = Timer
  For i% = 1 To N%
    Line x%,y%,u%,v%,w%,c%
  Next
  Rep "H4.line.var", Timer - t0

  t0 = Timer
  For i% = 1 To N%
    Box 10,20,8,8,1,1
  Next
  Rep "H4.box.lit", Timer - t0

  t0 = Timer
  For i% = 1 To N%
    Box x%,y%,s%,s%,w%,c%
  Next
  Rep "H4.box.var", Timer - t0

  t0 = Timer
  For i% = 1 To N%
    Circle 50,50,4,1,1,1
  Next
  Rep "H4.circle.lit", Timer - t0

  t0 = Timer
  For i% = 1 To N%
    Circle p%,q%,r%,w%,m%,c%
  Next
  Rep "H4.circle.var", Timer - t0
EndIf

If PSECT% = 0 Or PSECT% = 2 Then
  ' ---- E8: PIXEL over a 64x64 region, per pixel
  t0 = Timer
  For i% = 1 To PASSES%
    For py% = 0 To 63
      For px% = 0 To 63
      Next
    Next
  Next
  e8b = (Timer - t0) * 1000
  Emit "E8.loop64.empty", e8b / npix

  t0 = Timer
  For i% = 1 To PASSES%
    For py% = 0 To 63
      For px% = 0 To 63
        Pixel px%,py%,wht%
      Next
    Next
  Next
  e8t = (Timer - t0) * 1000
  Emit "E8.pixel64.gross", e8t / npix
  e8t = e8t - e8b
  Emit "E8.pixel64.net", e8t / npix

  ' the C floor: one PIXEL statement draws 512 pixels from two arrays
  t0 = Timer
  For i% = 1 To EA%
    Pixel xs%(),ys%(),wht%
  Next
  e8a = ((Timer - t0) * 1000 - base_us * EA% / N%) / EA%
  Emit "E8.pixelarray512.stmt", e8a
  Emit "E8.pixelarray512.perpixel", e8a / 512
EndIf

If PSECT% = 0 Or PSECT% = 3 Then
  ' ---- E9: large shapes, where the time should be in C. Each has a small
  ' twin with the same argument text, so large minus small is the drawing
  t0 = Timer
  For i% = 1 To NE%
    CLS
  Next
  RepN "E9.cls." + geo$, Timer - t0, NE%

  t0 = Timer
  For i% = 1 To NE%
    Box 0,0,8,8,0,wht%,wht%
  Next
  RepN "E9.boxfill.8x8", Timer - t0, NE%

  t0 = Timer
  For i% = 1 To NE%
    Box 0,0,320,240,0,wht%,wht%
  Next
  RepN "E9.boxfill.320x240", Timer - t0, NE%

  t0 = Timer
  For i% = 1 To NE%
    Line 0,0,7,7,1,wht%
  Next
  RepN "E9.line.8x8", Timer - t0, NE%

  t0 = Timer
  For i% = 1 To NE%
    Line 0,0,319,239,1,wht%
  Next
  RepN "E9.line.320x240", Timer - t0, NE%

  t0 = Timer
  For i% = 1 To NE%
    Circle 160,120,4,0,1,wht%,wht%
  Next
  RepN "E9.circlefill.r4", Timer - t0, NE%

  t0 = Timer
  For i% = 1 To NE%
    Circle 160,120,100,0,1,wht%,wht%
  Next
  RepN "E9.circlefill.r100", Timer - t0, NE%

  If MM.HRes >= 640 And MM.VRes >= 480 Then
    t0 = Timer
    For i% = 1 To NE%
      Box 0,0,640,480,0,wht%,wht%
    Next
    RepN "E9.boxfill.640x480", Timer - t0, NE%

    t0 = Timer
    For i% = 1 To NE%
      Line 0,0,639,479,1,wht%
    Next
    RepN "E9.line.640x480", Timer - t0, NE%
  EndIf
EndIf

' ---- the same statements inside a SUB, on LOCALs (bench_gfx.py: in a def)
If PSECT% = 0 Or PSECT% = 1 Then InSubH4
If PSECT% = 0 Or PSECT% = 2 Then InSubE8

Mode homemode%
Print "BENCHEND"
End

Sub Emit lbl$, usv
  Print "BENCH "; lbl$; " "; Str$(usv, 0, 3); " "; Str$(usv * mhz, 0, 0)
End Sub

' N% passes at top level, against the top-level empty loop (tms = ms)
Sub Rep lbl$, tms
  Local us
  us = (tms * 1000 - base_us) / N%
  Emit lbl$, us
End Sub

' cnt% passes at top level: the empty loop scaled from N% to cnt% passes
Sub RepN lbl$, tms, cnt%
  Local us
  us = (tms * 1000 - base_us * cnt% / N%) / cnt%
  Emit lbl$, us
End Sub

' N% passes inside a SUB, against that SUB's own empty loop (bb, us)
Sub RepS lbl$, tms, bb
  Local us
  us = (tms * 1000 - bb) / N%
  Emit lbl$, us
End Sub

Sub InSubH4
  Local integer li, lx, ly, lc, lu, lv, lw, ls, lp, lq, lr, lm
  Local lbase, lt
  lx = 10 : ly = 20 : lc = 1 : lu = 17 : lv = 27 : lw = 1 : ls = 8
  lp = 50 : lq = 50 : lr = 4 : lm = 1
  lt = Timer
  For li = 1 To N%
  Next
  lbase = (Timer - lt) * 1000
  Emit "local.ctl.loop.empty", lbase / N%

  lt = Timer
  For li = 1 To N%
    Pixel 10,20,1
  Next
  RepS "local.H4.pixel.lit", Timer - lt, lbase

  lt = Timer
  For li = 1 To N%
    Pixel lx,ly,lc
  Next
  RepS "local.H4.pixel.var", Timer - lt, lbase

  lt = Timer
  For li = 1 To N%
    Line 10,20,17,27,1,1
  Next
  RepS "local.H4.line.lit", Timer - lt, lbase

  lt = Timer
  For li = 1 To N%
    Line lx,ly,lu,lv,lw,lc
  Next
  RepS "local.H4.line.var", Timer - lt, lbase

  lt = Timer
  For li = 1 To N%
    Box 10,20,8,8,1,1
  Next
  RepS "local.H4.box.lit", Timer - lt, lbase

  lt = Timer
  For li = 1 To N%
    Box lx,ly,ls,ls,lw,lc
  Next
  RepS "local.H4.box.var", Timer - lt, lbase

  lt = Timer
  For li = 1 To N%
    Circle 50,50,4,1,1,1
  Next
  RepS "local.H4.circle.lit", Timer - lt, lbase

  lt = Timer
  For li = 1 To N%
    Circle lp,lq,lr,lw,lm,lc
  Next
  RepS "local.H4.circle.var", Timer - lt, lbase
End Sub

Sub InSubE8
  Local integer li, lpx, lpy, lcw
  Local lt, lb8, lt8
  lcw = wht%
  lt = Timer
  For li = 1 To PASSES%
    For lpy = 0 To 63
      For lpx = 0 To 63
      Next
    Next
  Next
  lb8 = (Timer - lt) * 1000
  Emit "local.E8.loop64.empty", lb8 / npix

  lt = Timer
  For li = 1 To PASSES%
    For lpy = 0 To 63
      For lpx = 0 To 63
        Pixel lpx,lpy,lcw
      Next
    Next
  Next
  lt8 = (Timer - lt) * 1000
  Emit "local.E8.pixel64.gross", lt8 / npix
  lt8 = lt8 - lb8
  Emit "local.E8.pixel64.net", lt8 / npix
End Sub
