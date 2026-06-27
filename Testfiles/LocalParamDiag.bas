' Diagnostic test for LOCAL variable parameter passing
Option Base 0
Option Explicit

Type TPoint
  x As Integer
  y As Integer
End Type

' Test 1: Simple struct - LOCAL vs GLOBAL
Print "=== Test 1: LOCAL vs GLOBAL struct ==="
Dim gPt As TPoint
gPt.x = 50
gPt.y = 75
Print "Global before: gPt.x="; gPt.x; " gPt.y="; gPt.y
TestStructGlobal gPt
Print "Global after:  gPt.x="; gPt.x; " gPt.y="; gPt.y

Print
TestLocalStruct

Sub TestStructGlobal p As TPoint
  Print "In sub (global): p.x="; p.x; " p.y="; p.y
  p.x = 999
  p.y = 888
End Sub

Sub TestLocalStruct
  Local pt As TPoint
  pt.x = 50
  pt.y = 75
  Print "Local before: pt.x="; pt.x; " pt.y="; pt.y
  TestStructLocal pt
  Print "Local after:  pt.x="; pt.x; " pt.y="; pt.y
End Sub

Sub TestStructLocal p As TPoint
  Print "In sub (local): p.x="; p.x; " p.y="; p.y
  p.x = 999
  p.y = 888
End Sub

' Test 2: Integer array - LOCAL vs GLOBAL
Print
Print "=== Test 2: LOCAL vs GLOBAL integer array ==="
Dim gArr%(5)
Dim i%
For i% = 0 To 5
  gArr%(i%) = i% * 10
Next
Print "Global arr: 0="; gArr%(0); " 1="; gArr%(1); " 2="; gArr%(2); " 3="; gArr%(3)
TestArrGlobal gArr%()

Print
TestLocalArr

Sub TestArrGlobal a%()
  Print "In sub (global): 0="; a%(0); " 1="; a%(1); " 2="; a%(2); " 3="; a%(3)
End Sub

Sub TestLocalArr
  Local arr%(5)
  Local i%
  For i% = 0 To 5
    arr%(i%) = i% * 10
  Next
  Print "Local arr: 0="; arr%(0); " 1="; arr%(1); " 2="; arr%(2); " 3="; arr%(3)
  TestArrLocal arr%()
End Sub

Sub TestArrLocal a%()
  Print "In sub (local): 0="; a%(0); " 1="; a%(1); " 2="; a%(2); " 3="; a%(3)
End Sub

End
