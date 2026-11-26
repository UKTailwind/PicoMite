' tpause.bas - threads, Stage 2: PAUSE in three threads and the main program at once. Each pause is its own, the
' pauses run side by side, and a thread's pause is on time while another thread is busy.
Option Explicit
Dim Integer tests, pass, i, done, busy, good(3), spins
Dim Float t0, all

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Sleeper
  Local Integer me = MM.Info(Thread), k
  Local Float t
  For k = 1 To 4
    t = Timer
    Pause 60 + me * 10
    t = Timer - t
    If t >= 60 + me * 10 And t < 60 + me * 10 + 4 Then Inc good(me)
  Next
  Inc done
End Sub

Sub Spin
  Do While busy
    Inc spins
  Loop
End Sub

t0 = Timer
For i = 1 To 3 : Thread Start i, Sleeper : Next
Sleeper
Do While done < 4 : Pause 1 : Loop
all = Timer - t0
For i = 0 To 3
  Check "thread" + Str$(i) + ": four pauses of" + Str$(60 + i * 10) + " ms, each within 4 ms", good(i), 4
Next
Check "side by side: the longest four, not all sixteen", all >= 360 And all < 420, 1

done = 0
For i = 0 To 3 : good(i) = 0 : Next
busy = 1
Thread Start 4, Spin
For i = 1 To 3 : Thread Start i, Sleeper : Next
Sleeper
Do While done < 4 : Pause 1 : Loop
busy = 0
For i = 0 To 3
  Check "thread" + Str$(i) + ": the same with a busy thread running", good(i), 4
Next
Check "the busy thread ran meanwhile", spins > 1000, 1
Print "tpause:"; pass; " of"; tests; " passed"
