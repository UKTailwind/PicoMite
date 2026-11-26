' tstop.bas - threads, Stage 2: THREAD PAUSE, RESUME and STOP, MM.INFO(THREAD n), THREAD SLICE and THREAD SIZE. A
' thread is stopped in a loop, inside a FUNCTION and in a PAUSE; what it held comes back.
Option Explicit
Dim Integer tests, pass, v, h0, i0, v0, h1, i1, deep, c(4)

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Spin
  Local Integer me = MM.Info(Thread), a(20)
  Local s$ = "held by thread" + Str$(me)
  Do
    Inc c(me)
  Loop
End Sub

Function Slow%(x%)
  Local q$ = "inside the function"
  Do
    Inc c(MM.Info(Thread))
  Loop
End Function

Sub InFun
  Local Integer r
  Local s$ = "calls " + Str$(Slow%(1))
End Sub

Sub Napper
  Local s$ = "asleep"
  Pause 100000
End Sub

Function Down%(n%)
  Local Integer k = n%
  If n% > 0 Then Down% = Down%(n% - 1) + 1 Else Down% = 0
End Function

Sub GoDeep
  deep = Down%(12)
End Sub

Thread Start 1, Spin                  ' once first, so that what a first use of each SUB takes for good is not counted
Thread Start 2, InFun
Thread Start 3, Napper
Pause 20
Thread Stop 1 : Thread Stop 2 : Thread Stop 3
c(1) = 0 : c(2) = 0
h0 = MM.Info(Heap) : i0 = MM.Info(Heap) : v0 = MM.Info(VarCnt)

Thread Start 1, Spin
Thread Start 2, InFun
Thread Start 3, Napper
Pause 30
Check "a running thread is 1", MM.Info(Thread 1), 1
Check "a thread in PAUSE is 2", MM.Info(Thread 3), 2
Check "a number not in use is 0", MM.Info(Thread 8), 0 ' (the PicoMite: 8 is its last)
Check "the main program is thread 0, and 1", MM.Info(Thread 0) + MM.Info(Thread), 1
Thread Pause 1
v = c(1)
Pause 30
Check "paused: it does not run", c(1) - v, 0
Check "paused is 3", MM.Info(Thread 1), 3
Thread Resume 1
Pause 30
Check "resumed: it runs again", c(1) > v, 1
Thread Stop 1
v = c(1)
Pause 20
Check "stopped in a loop: it does not run, and is 0", c(1) - v + MM.Info(Thread 1), 0
Check "the thread inside a FUNCTION had run", c(2) > 0, 1
Thread Stop 2
v = c(2)
Pause 20
Check "stopped inside a FUNCTION", c(2) - v + MM.Info(Thread 2), 0
Thread Stop 3
Check "stopped in a PAUSE", MM.Info(Thread 3), 0
Thread Stop 4                         ' not running: nothing
h1 = MM.Info(Heap) : i1 = MM.Info(Heap)       ' (read first: a SUB's call has memory of its own in use)
Check "the heap is as it was", h1 - h0, 0
Check "and the on-chip heap", i1 - i0, 0
Check "no variable left behind", MM.Info(VarCnt) - v0, 0
On Error Skip
Thread Pause 4
Check "THREAD PAUSE of a thread not running is refused", MM.ErrMsg$ = "Thread 4 is not running", 1

Thread Size 24, 64, 16                ' twelve FUNCTIONs deep needs more than the usual stack (the PicoMite: 24 KB, about 1.2 KB a level with COMPILE OFF; NEST 16)
Thread Start 1, GoDeep
Do While MM.Info(Thread 1) : Loop
Check "THREAD SIZE: a FUNCTION twelve deep in a thread", deep, 12
Thread Slice 10
Thread Start 1, Spin
Thread Start 2, Spin
v = Timer
Do While Timer - v < 200 : Loop
Thread Stop 1 : Thread Stop 2
Check "THREAD SLICE 10: both threads ran", c(1) > 0 And c(2) > 0, 1
Print "tstop:"; pass; " of"; tests; " passed"
