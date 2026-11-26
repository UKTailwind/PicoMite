' tpriolist.bas - threads: the list after Ctrl-C or an error shows a priority that is not 1, and the turn a lock's
' holder has while a thread of a higher priority waits for its lock. (THREAD LIST shows the same, with each stack.)
Option Explicit
Dim Integer go = 1, held

Sub Holder
  Thread Lock 1
  held = 1
  Do While go
    Pause 20
  Loop
  Thread Unlock 1
End Sub

Sub Waiter
  Thread Lock 1
  Thread Unlock 1
End Sub

Sub Idle
  Do While go
    Pause 20
  Loop
End Sub

Thread Priority 0, 2
Thread Start 1, Holder
Do While held = 0 : Pause 1 : Loop
Thread Start 2 Priority 7, Waiter
Thread Start 3 Priority 3, Idle
Thread Start 4, Idle
Pause 50
Error "the list"
