' Simple struct array parameter test
Option Base 0
Option Explicit

Type TPoint
  x As Integer
  y As Integer
End Type

Dim globalPts(3) As TPoint
Dim i%

' Initialize global array
For i% = 0 To 3
  globalPts(i%).x = i% * 10
  globalPts(i%).y = i% * 100
Next

Print "Before sub call:"
Print "globalPts(2).x = "; globalPts(2).x
Print "globalPts(2).y = "; globalPts(2).y

TestGlobalArray globalPts()

Print
Print "After sub call:"
Print "globalPts(1).x = "; globalPts(1).x

Sub TestGlobalArray pts() As TPoint
  Print
  Print "In sub (global array):"
  Print "pts(0).x = "; pts(0).x; " (expect 0)"
  Print "pts(1).x = "; pts(1).x; " (expect 10)"
  Print "pts(2).x = "; pts(2).x; " (expect 20)"
  Print "pts(2).y = "; pts(2).y; " (expect 200)"
  pts(1).x = 999
End Sub

Print
Print "=== Now testing with LOCAL array ==="

TestLocalArray

Sub TestLocalArray
  Local localPts(3) As TPoint
  Local j%
  
  ' Initialize local array
  For j% = 0 To 3
    localPts(j%).x = j% * 10
    localPts(j%).y = j% * 100
  Next
  
  Print "Before sub call:"
  Print "localPts(2).x = "; localPts(2).x
  
  TestLocalArrayParam localPts()
  
  Print "After sub call:"
  Print "localPts(1).x = "; localPts(1).x
End Sub

Sub TestLocalArrayParam pts() As TPoint
  Print
  Print "In sub (local array param):"
  Print "pts(0).x = "; pts(0).x; " (expect 0)"
  Print "pts(1).x = "; pts(1).x; " (expect 10)"  
  Print "pts(2).x = "; pts(2).x; " (expect 20)"
  pts(1).x = 888
End Sub

End
