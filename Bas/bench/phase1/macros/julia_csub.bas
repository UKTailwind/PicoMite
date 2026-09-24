' julia_interp.bas - Phase 1 macro, the Julia pair: INTERPRETED half.
' plotjulia below is Bas/julia.bas's kernel, unchanged. Only the frame
' around it differs from Bas/julia.bas:
'  - the picture is a fixed 240x180, not MM.HRes x MM.VRes, so every board
'    and build does the same work (470,875 escape iterations, 43,200 pixels);
'  - the constants are integer ratios, so the doubles are exactly the ones a
'    PC computes and the checksum can be predicted (julia_expect.py);
'  - no SAVE IMAGE and no wait for a key; it ends with END;
'  - the checksum is read back from the screen after the timed region.
'    Nothing may be printed between MODE 2 and the read-back: the console
'    draws on the same screen.
' julia_csub.bas is this file with HALF$ = "csub", REPS% = 20 and
' plotjulia as a CSUB in the library (make_julia.py builds it).
Const PROFILE% = 0             ' 1 = run the PC sampler over the timed region
Const HALF$ = "csub"
Const REPS% = 20               ' renders timed
Const NITER% = 470875          ' escape iterations per render (julia_expect.py)
mhz = Val(MM.Info(CPUSPEED)) / 1000000
Print "BENCHSTART "; MM.Device$; " "; MM.Ver; " "; mhz; " MHz"
Mode 2
If MM.HRes < 240 Or MM.VRes < 180 Then Error "needs MODE 2 of at least 240x180 (a 640x480 display)"
CLS
map maximite
'Specify initial values
RealOffset = -130 / 100
ImaginOffset = -122 / 100
'------------------------------------------------*
'Set the Julia set constant [eg C = -1.2 + 0.8i]
CRealVal = -78 / 100
CImagVal = -20 / 100
'------------------------------------------------*
MAXIT = 80 'max iterations
PixelWidth = 240
PixelHeight = 180
GAP = PixelHeight / PixelWidth
SIZE = 5 / 2
XDelta = SIZE / PixelWidth
YDelta = (SIZE * GAP) / PixelHeight
Dim mp%(15)
For i% = 0 To 15 : mp%(i%) = map(i%) : Next i%
' the sampler's tables: the largest power of two up to 8192 entries (16
' bytes each) that leaves 20 KB of heap for OPTION PROFILING's counters.
' The PC3 needs 8192: the first board runs sampled 2,700-5,900 distinct PCs.
pcsn% = 8192
Do While pcsn% > 1024 And pcsn% * 16 + 20480 > MM.Info(HEAP)
  pcsn% = pcsn% \ 2
Loop
' the empty repetition loop, subtracted as bench_micro.bas does
t0 = Timer
For rep% = 1 To REPS%
Next rep%
bl = Timer - t0
Timer = 0
If PROFILE% Then Option profiling on, sample, pcsn%
For rep% = 1 To REPS%
  plotjulia PixelWidth, PixelHeight, XDelta, YDelta, RealOffset, ImaginOffset, CRealVal, CImagVal, MAXIT, mp%()
Next rep%
el = Timer
' read the picture back: Pixel() gives the fixed 16-colour table entry for
' COUNT Mod 16 (graphics/Draw.c:255), the same on every PICOMITEVGA build
' in MODE 2
ck% = 0
For py% = 0 To PixelHeight - 1
  For px% = 0 To PixelWidth - 1
    ck% = ck% + Pixel(px%, py%) * (1 + ((px% + 3 * py%) And 15))
  Next px%
Next py%
us = (el - bl) * 1000 / REPS%
' per render, per pixel, and the render time over the 470,875 escape
' iterations (so the last figure includes the per-pixel work as well)
Print "BENCH julia240x180_" + HALF$ + "_us_per_render "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
Print "BENCH julia240x180_" + HALF$ + "_us_per_pixel "; Str$(us / 43200, 0, 3); " "; Str$(us / 43200 * mhz, 0, 0)
Print "BENCH julia240x180_" + HALF$ + "_us_per_escape_iter "; Str$(us / NITER%, 0, 3); " "; Str$(us / NITER% * mhz, 0, 0)
Print "CHECK julia240x180 "; Str$(ck%)
Print "ELAPSED julia240x180_" + HALF$ + " "; Str$(el, 0, 3); " ms for "; Str$(REPS%); " render(s)"
Print "BENCHEND"
End
'
' w, h    picture size in pixels
' xd, yd  the step in the complex plane per pixel
' rOfs, iOfs   top-left corner of the view
' cRe, cIm     the Julia constant C
' mit          iteration limit
' mp%()        the sixteen colours, already resolved through MAP()
/*
' --- mmb2csub: plotjulia replaced by a CSUB; original follows
sub plotjulia w, h, xd, yd, rOfs, iOfs, cRe, cIm, mit, mp%()
Local X, Y, CX, CY, Zr, Zi, COUNT, new_Zr, new_Zi
'Loop processing - visit every pixel
For X = 0 To (w - 1)
  CX = X * xd + rOfs
  For Y = 0 To (h - 1)
    CY = Y * yd + iOfs
    Zr = CX
    Zi = CY
    COUNT = 0
'    Begin Iteration loop
    Do While (( COUNT <= mit ) And (( Zr * Zr + Zi * Zi ) < 4 ))
      new_Zr = Zr * Zr - Zi * Zi + cRe
      new_Zi = 2 * Zr * Zi + cIm
      Zr = new_Zr
      Zi = new_Zi
      COUNT = COUNT + 1
    Loop
    Pixel X,Y,mp%( COUNT Mod 16)
  Next Y
Next X
end sub
*/
