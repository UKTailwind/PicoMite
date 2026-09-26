' ticktest.bas - SETTICK behaviour: four ticks, two with the same period
' (due on the same millisecond, so both must run), PAUSE/RESUME, ticks during
' a PAUSE command, and a slow handler that makes other ticks overdue
Dim a%, b%, c%, d%, e%, i%
SetTick 10, TA, 1
SetTick 10, TB, 2
SetTick 25, TC, 3
SetTick 100, TD, 4
Timer = 0
Do While Timer < 1000 : Loop
Print "P1"; a%; b%; c%; d%
SetTick Pause, TC, 3
i% = c%
Timer = 0
Do While Timer < 500 : Loop
Print "P2 paused"; c% - i%
SetTick Resume, TC, 3
a% = 0 : b% = 0 : c% = 0 : d% = 0
Pause 1000
Print "P3 in pause"; a%; b%; c%; d%
SetTick 0, TC, 3
SetTick 0, TD, 4
SetTick 200, TE, 3
a% = 0 : b% = 0 : e% = 0
Timer = 0
Do While Timer < 1000 : Loop
Print "P4 slow"; a%; b%; e%
SetTick 0, TA, 1 : SetTick 0, TB, 2 : SetTick 0, TE, 3
Print "DONE"
End

Sub TA
  Inc a%
End Sub
Sub TB
  Inc b%
End Sub
Sub TC
  Inc c%
End Sub
Sub TD
  Inc d%
End Sub
Sub TE
  ' takes 50 ms, so the 10 ms ticks are overdue when it returns
  Inc e%
  Pause 50
End Sub
