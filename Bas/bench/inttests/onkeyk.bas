' onkeyk.bas - ON KEY k on the serial console
' The driver sends A A B A: each A should call KeyA and be swallowed,
' the B should reach INKEY$
Dim n% = 0, k$ = "", a$
On Key 65, KeyA
Print "READY"
Timer = 0
Do While Timer < 6000
  a$ = Inkey$
  If a$ <> "" Then k$ = k$ + a$
Loop
On Key 65, 0
Print "ONKEYK"; n%; " BUF="; k$
End

Sub KeyA
  Inc n%
End Sub
