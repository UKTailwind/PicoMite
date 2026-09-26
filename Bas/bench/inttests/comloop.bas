' comloop.bas - COM1 interrupt at a buffer level. Needs GP0 (COM1 TX) jumpered
' to GP1 (COM1 RX). The level is 10.
'   A  9 bytes: below the level, no interrupt
'   B  one more: the interrupt fires once and the handler takes all 10
'   C  25 bytes at once: fires until fewer than 10 are left; 25 in all
'   D  12 bytes, handler reads one byte per call: it is called again while
'      10 or more are waiting, so 3 calls and 9 bytes left
Dim n%, onebyte%, got$
SetPin GP1, GP0, COM1
Open "COM1:115200,256,ComInt,10" As #5
Print "READY"
Print #5, "123456789";
Pause 50
Print "A"; n%; Loc(#5)
Print #5, "0";
Pause 50
Print "B"; n%; Len(got$); Loc(#5)
n% = 0 : got$ = ""
Print #5, String$(25, "x");
Pause 50
Print "C"; n% > 0; Len(got$) + Loc(#5); Loc(#5) < 10
got$ = Input$(255, #5)
n% = 0 : got$ = "" : onebyte% = 1
Print #5, String$(12, "y");
Pause 100
Print "D"; n%; Len(got$); Loc(#5)
Close #5
SetPin GP0, Off : SetPin GP1, Off
End

Sub ComInt
  Inc n%
  If onebyte% = 0 Then
    got$ = got$ + Input$(255, #5)
  Else
    got$ = got$ + Input$(1, #5)
  EndIf
End Sub
