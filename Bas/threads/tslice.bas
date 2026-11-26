' tslice.bas - threads, Stage 2 (docs/Threads_Plan.md): four loops that never yield, in three threads and the main
' program. The 1 ms time slice shares the processor between them, and the work the four do together is what one
' does alone in the same time, less the switches.
Option Explicit
Dim Integer tests, pass, i, go, lo, hi, total, alone, n(5)

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Count
  Local Integer me = MM.Info(Thread)
  Do While go
    Inc n(me)
  Loop
End Sub

Sub Stopper
  Pause 400
  go = 0
End Sub

go = 1
Thread Start 5, Stopper
Count                                 ' alone, for 400 ms
alone = n(0)
Do While MM.Info(Thread 5) : Loop
n(0) = 0
go = 1
For i = 1 To 3 : Thread Start i, Count : Next
Thread Start 5, Stopper
Count                                 ' with three more, for 400 ms
Do While MM.Info(Thread 1) Or MM.Info(Thread 2) Or MM.Info(Thread 3) : Loop
lo = n(0) : hi = n(0)
For i = 0 To 3
  If n(i) < lo Then lo = n(i)
  If n(i) > hi Then hi = n(i)
  total = total + n(i)
Next
Check "every loop ran", lo > 0, 1
Check "the four had the same share, within a tenth", lo * 10 >= hi * 9, 1
Check "together they did what one does alone, within a tenth", total * 10 >= alone * 9 And total * 10 <= alone * 11, 1
Check "the threads have ended", MM.Info(Thread 1) + MM.Info(Thread 2) + MM.Info(Thread 3) + MM.Info(Thread 5), 0
Print "tslice:"; pass; " of"; tests; " passed"
