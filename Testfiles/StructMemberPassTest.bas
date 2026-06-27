' Test: passing struct parameter member to another sub
Option Base 0
Option Explicit

Type TPoint
  x As Integer
  y As Integer
End Type

Print "=== Testing struct param member passed to another sub ==="

Dim gPt As TPoint
gPt.x = 50
gPt.y = 75

' Test with global struct
Print "Calling TestStruct with global..."
TestStruct gPt

' Test with local struct
Print
Print "Calling wrapper that uses local..."
TestWithLocal

Sub TestStruct p As TPoint
  Print "Direct access: p.x="; p.x; " p.y="; p.y
  
  ' Now pass p.x and p.y to another sub
  Print "Passing p.x to PrintInt..."
  PrintInt p.x, "p.x"
  
  Print "Passing p.y to PrintInt..."
  PrintInt p.y, "p.y"
End Sub

Sub PrintInt val%, name$
  Print "  PrintInt received "; name$; " = "; val%
End Sub

Sub TestWithLocal
  Local pt As TPoint
  pt.x = 50
  pt.y = 75
  Print "Local direct: pt.x="; pt.x; " pt.y="; pt.y
  TestStruct pt
End Sub

End
