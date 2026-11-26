' pngsum.bas - LOAD PNG's pixels, as a checksum for each PNG on B: (the first 128 x 128 of the screen in MODE 4, full
' colour), to compare one decoder with another: the same figures before and after a change mean the same pictures.
' MODE 1 back at the end. No .expected: the figures are compared between two runs.
Option Explicit
Dim Integer x, y, h
Dim f$, e$
Mode 4
f$ = Dir$("B:/*.png", FILE)
Do While f$ <> ""
  CLS
  On Error Skip
  Load PNG "B:/" + f$
  e$ = MM.ErrMsg$
  h = 0
  For y = 0 To 127
    For x = 0 To 127
      h = (h * 31 + Pixel(x, y)) And &HFFFFFFFF
    Next
  Next
  Print @(0, 200) "";
  Print f$; " "; Hex$(h); " "; e$
  f$ = Dir$()
Loop
Mode 1
