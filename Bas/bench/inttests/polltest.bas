' polltest.bas - sources without their own signal (ON KEY, PID, INTERRUPT)
' still fire, alongside a SETTICK. The driver types xyz while it runs.
Dim t%, p%, c%, keys$, prm!(13)
SetTick 20, TT, 1
On Key KK
prm!(0) = 1 : prm!(8) = 0.05 : prm!(4) = -100 : prm!(5) = 100 : prm!(6) = -100 : prm!(7) = 100
Math Pid Init 1, prm!(), PP
Interrupt CC
Print "READY"
Timer = 0
Math Pid Start 1
Do While Timer < 2000
  If Timer > 1000 And c% = 0 Then Interrupt
Loop
Math Pid Stop 1
On Key 0
Interrupt 0
SetTick 0, TT, 1
Print "POLL tick"; t%; " pid"; p%; " csub"; c%; " keys "; keys$
End

Sub TT
  Inc t%
End Sub
Sub KK
  keys$ = keys$ + Inkey$
End Sub
Sub PP
  Local o! = Math(Pid 1, 0, 0)
  Inc p%
End Sub
Sub CC
  Inc c%
End Sub
