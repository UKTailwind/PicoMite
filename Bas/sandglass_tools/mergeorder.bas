' Which framebuffer is on top in FRAMEBUFFER MERGE?
' F is filled white.  2 is filled with the transparent slot, with a red box in
' the middle.  If 2 is the overlay the box reads red and the rest reads white.
Option Console Serial
Option Explicit
Option Default None
Dim INTEGER TRANSP, a, b
TRANSP = 11
Mode 2
FrameBuffer Create
FrameBuffer Create 2
FrameBuffer Write F
CLS RGB(WHITE)
FrameBuffer Write 2
CLS Map(TRANSP)
Box 100, 80, 80, 60, 1, RGB(RED), RGB(RED)
FrameBuffer Merge TRANSP, B
FrameBuffer Write N
a = Pixel(140, 110)
b = Pixel(20, 20)
Print "inside the box  "; Hex$(a); "  (red is "; Hex$(RGB(RED)); ")"
Print "outside the box "; Hex$(b); "  (white is "; Hex$(RGB(WHITE)); ")"
If a = RGB(RED) Then
  Print "=> framebuffer 2 is the OVERLAY, F is the background"
ElseIf a = RGB(WHITE) Then
  Print "=> framebuffer F is the OVERLAY, 2 is the background"
Else
  Print "=> neither: "; Hex$(a)
End If
FrameBuffer Close
Option Console Both
End
