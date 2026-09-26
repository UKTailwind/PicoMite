' pinint.bas - interrupt pins. Needs GP0 (driven as DOUT) jumpered to GP1.
'   INTH / INTL / INTB  five pulses: 5, 5 and 10 interrupts
'   STALE   an edge made before SETPIN INTH must not fire
'   IN-TICK a SETTICK handler toggles GP0, so the edges happen while an
'           interrupt runs; each rising one is served after IRETURN
'   IDLE    loop time with no pin armed and with an idle INTH pin
Dim hi%, lo%, bo%, t%, i%, a%, t1, t2
SetPin GP0, DOUT
Pin(GP0) = 0
Print "READY"
SetPin GP1, INTH, PH
For i% = 1 To 5 : Pin(GP0) = 1 : Pause 2 : Pin(GP0) = 0 : Pause 2 : Next
Print "INTH"; hi%
SetPin GP1, INTL, PL
For i% = 1 To 5 : Pin(GP0) = 1 : Pause 2 : Pin(GP0) = 0 : Pause 2 : Next
Print "INTL"; lo%
SetPin GP1, INTB, PB
For i% = 1 To 5 : Pin(GP0) = 1 : Pause 2 : Pin(GP0) = 0 : Pause 2 : Next
Print "INTB"; bo%
SetPin GP1, Off
Pin(GP0) = 1 : Pause 2
hi% = 0
SetPin GP1, INTH, PH
Pause 20
Print "STALE"; hi%
Pin(GP0) = 0 : Pause 2
hi% = 0
SetTick 50, TK, 1
Pause 420
SetTick 0, TK, 1
Print "IN-TICK"; hi%; t%
SetPin GP1, Off
Timer = 0
For i% = 1 To 100000
  a% = a% + 1
Next
t1 = Timer
SetPin GP1, INTH, PH
Timer = 0
For i% = 1 To 100000
  a% = a% + 1
Next
t2 = Timer
SetPin GP1, Off
SetPin GP0, Off
Print "IDLE"; t1; t2
End

Sub PH
  Inc hi%
End Sub
Sub PL
  Inc lo%
End Sub
Sub PB
  Inc bo%
End Sub
Sub TK
  Inc t%
  Pin(GP0) = 1 - Pin(GP0)
End Sub
