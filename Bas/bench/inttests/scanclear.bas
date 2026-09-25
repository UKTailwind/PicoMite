' scanclear.bas - sources without a signal of their own (here PID and
' INTERRUPT) make every statement run the interrupt scan while they are armed;
' once they are stopped the loop must be as fast as with nothing armed.
'   SCAN  nothing armed, PID running, after PID STOP, after INTERRUPT 0
Dim a%, i%, p%, t0, t1, t2, t3, prm!(13)
prm!(0) = 1 : prm!(8) = 0.5 : prm!(4) = -100 : prm!(5) = 100 : prm!(6) = -100 : prm!(7) = 100
Timer = 0
For i% = 1 To 100000
  a% = a% + 1
Next
t0 = Timer
Math Pid Init 1, prm!(), PP
Math Pid Start 1
Timer = 0
For i% = 1 To 100000
  a% = a% + 1
Next
t1 = Timer
Math Pid Stop 1
Timer = 0
For i% = 1 To 100000
  a% = a% + 1
Next
t2 = Timer
Interrupt CC
Interrupt 0
Timer = 0
For i% = 1 To 100000
  a% = a% + 1
Next
t3 = Timer
Print "SCAN"; t0; t1; t2; t3
End

Sub PP
  Local o! = Math(Pid 1, 0, 0)
  Inc p%
End Sub
Sub CC
End Sub
