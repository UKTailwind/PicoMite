' tickneg.bas - SETTICK must reject a negative period; a normal tick must still run
Dim n% = 0
SetTick 100, T1
Timer = 0
Do While Timer < 1050 : Loop
SetTick 0, T1
Print "TICKS"; n%
SetTick -1, T1
Print "NOT REJECTED"
End

Sub T1
  Inc n%
End Sub
