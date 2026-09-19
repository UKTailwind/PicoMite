' How expensive is a vertical flip of the finished frame?
' The invert potion turns the whole picture upside down, sprites included, so
' the only faithful way to do it here is to reverse the raster after the frame
' is composed.  There is no command for that, so it is 120 line swaps.
Option Console Serial
Option Explicit
Option Default None
Dim INTEGER y, t0, t1
Mode 2
FrameBuffer Create
FrameBuffer Create 2
FrameBuffer Write N
CLS
Box 10, 10, 100, 40, 1, RGB(WHITE), RGB(WHITE)
t0 = Timer
For y = 0 To 119
  Blit Read #1, 0, y, 320, 1
  Blit 0, 239 - y, 0, y, 320, 1
  Blit Write #1, 0, 239 - y
  Blit Close #1
Next y
t1 = Timer
Print "vertical flip of a 320x240 frame: "; Str$(t1 - t0); " ms"
FrameBuffer Close
Option Console Both
End
