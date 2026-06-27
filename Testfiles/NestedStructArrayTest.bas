' Test nested struct array parameter passing
Option Base 0
Option Explicit

Type TPoint
  x As Integer
  y As Integer
End Type

Type TRect
  topLeft As TPoint
  bottomRight As TPoint
  name As String Length 20
End Type

Print "=== Testing nested struct array parameter ==="

TestNestedArray

Sub TestNestedArray
  Local rects(3) As TRect
  Local i%
  
  ' Initialize
  For i% = 0 To 3
    rects(i%).topLeft.x = i% * 10
    rects(i%).topLeft.y = i% * 20
    rects(i%).bottomRight.x = i% * 10 + 100
    rects(i%).bottomRight.y = i% * 20 + 100
    rects(i%).name = "Rect" + Str$(i%)
  Next
  
  Print "Before sub call:"
  Print "rects(0).topLeft.x = "; rects(0).topLeft.x; " (expect 0)"
  Print "rects(1).topLeft.x = "; rects(1).topLeft.x; " (expect 10)"
  Print "rects(2).topLeft.x = "; rects(2).topLeft.x; " (expect 20)"
  Print "rects(2).name = '"; rects(2).name; "' (expect 'Rect 2')"
  
  TestRectArrayParam rects()
  
  Print
  Print "After sub call:"
  Print "rects(1).topLeft.x = "; rects(1).topLeft.x; " (expect 555)"
End Sub

Sub TestRectArrayParam r() As TRect
  Print
  Print "In sub:"
  Print "r(0).topLeft.x = "; r(0).topLeft.x; " (expect 0)"
  Print "r(1).topLeft.x = "; r(1).topLeft.x; " (expect 10)"
  Print "r(2).topLeft.x = "; r(2).topLeft.x; " (expect 20)"
  Print "r(2).name = '"; r(2).name; "' (expect 'Rect 2')"
  r(1).topLeft.x = 555
End Sub

End
