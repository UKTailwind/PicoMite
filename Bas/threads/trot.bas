' trot.bas - threads (issue #15): the turns go round in number order whatever is picked out of turn. A fast SETTICK
' in thread 1 is handed to it from the busy threads 2 and 3; the rotation used to restart after thread 1 each time, so
' thread 3 and the main program got few turns or none (the main program's PAUSE never ended). Each SETTICK period:
' the main program's PAUSE 500 ends on time, and the three busy threads each get a fair share.
Option Explicit
Dim Integer c(3), halt, period, i, lo, hi, tests, pass
Dim Float t0, t1

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Ticker
  SetTick period, Tk, 1                ' the routine runs in thread 1
  Count
  SetTick 0, 0, 1
End Sub

Sub Tk
End Sub

Sub Count
  Local Integer me = MM.Info(Thread)
  Do While halt = 0 : Inc c(me) : Loop
End Sub

period = 1
halt = 0 : For i = 1 To 3 : c(i) = 0 : Next
Thread Start 1, Ticker
Thread Start 2, Count
Thread Start 3, Count
t0 = Timer
Pause 500
t1 = Timer - t0
halt = 1 : Thread Wait 1 : Thread Wait 2 : Thread Wait 3
lo = Min(c(1), c(2), c(3)) : hi = Max(c(1), c(2), c(3))
Check "SETTICK 1 in thread 1: the main program's PAUSE 500 ends on time", t1 >= 500 And t1 < 520, 1
Check "  and each busy thread has at least a third of the busiest one's count", lo * 3 >= hi, 1
period = 2
halt = 0 : For i = 1 To 3 : c(i) = 0 : Next
Thread Start 1, Ticker
Thread Start 2, Count
Thread Start 3, Count
t0 = Timer
Pause 500
t1 = Timer - t0
halt = 1 : Thread Wait 1 : Thread Wait 2 : Thread Wait 3
lo = Min(c(1), c(2), c(3)) : hi = Max(c(1), c(2), c(3))
Check "SETTICK 2 in thread 1: the main program's PAUSE 500 ends on time", t1 >= 500 And t1 < 520, 1
Check "  and each busy thread has at least a third of the busiest one's count", lo * 3 >= hi, 1
period = 50
halt = 0 : For i = 1 To 3 : c(i) = 0 : Next
Thread Start 1, Ticker
Thread Start 2, Count
Thread Start 3, Count
t0 = Timer
Pause 500
t1 = Timer - t0
halt = 1 : Thread Wait 1 : Thread Wait 2 : Thread Wait 3
lo = Min(c(1), c(2), c(3)) : hi = Max(c(1), c(2), c(3))
Check "SETTICK 50 in thread 1: the main program's PAUSE 500 ends on time", t1 >= 500 And t1 < 520, 1
Check "  and each busy thread has at least a third of the busiest one's count", lo * 3 >= hi, 1
Print "trot:"; pass; " of"; tests; " passed"
