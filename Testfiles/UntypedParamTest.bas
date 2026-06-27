' Test: struct param member passed to untyped parameter
Option Base 0
Option Explicit

Type TPoint
  x As Integer
  y As Integer
End Type

Print "=== Testing struct param member to untyped param ==="

Dim gPt As TPoint
gPt.x = 50
gPt.y = 75

TestStruct gPt

Print
Print "With local struct:"
TestWithLocal

Sub TestStruct p As TPoint
  Print "Direct: p.x="; p.x; " p.y="; p.y
  
  ' Pass to typed parameter
  Print "To typed param:"
  PrintTyped p.x, "p.x"
  PrintTyped p.y, "p.y"
  
  ' Pass to UNtyped parameter  
  Print "To UNtyped param:"
  PrintUntyped p.x, "p.x"
  PrintUntyped p.y, "p.y"
End Sub

Sub PrintTyped val%, name$
  Print "  Typed: "; name$; " = "; val%
End Sub

Sub PrintUntyped val, name$
  Print "  Untyped: "; name$; " = "; val
End Sub

Sub TestWithLocal
  Local pt As TPoint
  pt.x = 50
  pt.y = 75
  TestStruct pt
End Sub

End
