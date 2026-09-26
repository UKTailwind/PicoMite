' ticktime.bas - loop time with no interrupt armed, with a SETTICK armed that
' never fires in the loop, and with a 1 ms SETTICK firing an empty handler
Dim a%, i%, t1, t2, t3, n%
Timer = 0
For i% = 1 To 300000
  a% = a% + 1
Next
t1 = Timer
SetTick 60000, TickH, 1
Timer = 0
For i% = 1 To 300000
  a% = a% + 1
Next
t2 = Timer
SetTick 1, TickF, 1
Timer = 0
For i% = 1 To 300000
  a% = a% + 1
Next
t3 = Timer
SetTick 0, TickF, 1
Print "UNARMED"; t1; " ARMED"; t2; " FIRING"; t3; " FIRED"; n%
End

Sub TickH
End Sub

Sub TickF
  Inc n%
End Sub
