' Simple test for struct array member evaluation
Option Base 0
Option Explicit

Type TBox
  x As Integer
  y As Integer
  w As Integer
  h As Integer
End Type

Dim boxes(2) As TBox
Dim i%

boxes(0).x = 10  : boxes(0).y = 10  : boxes(0).w = 100 : boxes(0).h = 50
boxes(1).x = 120 : boxes(1).y = 20  : boxes(1).w = 80  : boxes(1).h = 60
boxes(2).x = 50  : boxes(2).y = 80  : boxes(2).w = 150 : boxes(2).h = 40

Print "Direct access with literal index:"
Print "boxes(0).w = "; boxes(0).w
Print "boxes(1).w = "; boxes(1).w
Print "boxes(2).w = "; boxes(2).w

Print
Print "Access with variable index:"
For i% = 0 To 2
  Print "boxes("; i%; ").x = "; boxes(i%).x
  Print "boxes("; i%; ").y = "; boxes(i%).y
  Print "boxes("; i%; ").w = "; boxes(i%).w
  Print "boxes("; i%; ").h = "; boxes(i%).h
  Print
Next i%

Print "Testing multiple args on one line:"
For i% = 0 To 2
  Print boxes(i%).x; boxes(i%).y; boxes(i%).w; boxes(i%).h
Next

End
