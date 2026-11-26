' tbind.bas - threads, Stage 2: the compiler's bind caches when threads are switched by the clock. Four threads and the
' main program run the same FUNCTION to different depths for two seconds, switched every millisecond wherever they
' happen to be and by a yield at some of the depths, and each checks every result and every local.
' (A compiled statement keeps the addresses its variables were found at, with a stamp that says when. After a thread
' switch the stamp made at one depth could come out the same as one made before it at another depth, by this thread
' or another, and the statement then used that depth's variables: two runs in eight of tlocals with OPTION COMPILE ON,
' and nearly every run of this, 25 Nov 2026. A switch now makes every older stamp stale: BindsStale, Src/Thread.c.)
Option Explicit
Dim Integer tests, pass, go, i, bad(5), runs(5)

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Function Sum%(n%, k%)
  Local Integer a = n% * k%, b = k%
  If (n% + k%) And 1 Then Thread Yield
  If n% > 0 Then Sum% = Sum%(n% - 1, k%) + n% Else Sum% = 0
  If a <> n% * k% Or b <> k% Then Sum% = -1000000
End Function

Sub Work
  Local Integer me = MM.Info(Thread), d, want, got
  Do While go
    For d = 1 To 2 + me
      want = d * (d + 1) \ 2
      got = Sum%(d, me + 1)
      If got <> want Then Inc bad(me)
    Next
    Inc runs(me)
  Loop
End Sub

Sub Stopper
  Pause 2000
  go = 0
End Sub

go = 1
For i = 1 To 4 : Thread Start i, Work : Next
Thread Start 5, Stopper
Work
Do While MM.Info(Thread 1) Or MM.Info(Thread 2) Or MM.Info(Thread 3) Or MM.Info(Thread 4) : Loop
For i = 0 To 4
  Check "thread" + Str$(i) + ": every sum and every local right", bad(i), 0
  Check "thread" + Str$(i) + ": it ran", runs(i) > 10, 1
Next
Print "tbind:"; pass; " of"; tests; " passed"
