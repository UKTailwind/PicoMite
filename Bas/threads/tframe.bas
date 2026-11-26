' tframe.bas - threads, Stage 4: the waits for a frame's start. Two threads that each wait for every frame both get
' every frame while a third is busy, and the main program's PAGE COPY ...,B keeps the frame rate too.
Option Explicit
Dim Integer tests, pass, busy, go, i, spins, f(2)
Dim Float t0, t1, one

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Frames
  Local Integer me = MM.Info(Thread)
  Do While go
    FrameBuffer Wait
    Inc f(me)
  Loop
End Sub

Sub Spin
  Do While busy
    Inc spins
  Loop
End Sub

t0 = Timer
For i = 1 To 30 : FrameBuffer Wait : Next
one = (Timer - t0) / 30                 ' a frame's time, measured with no thread
Check "a frame is between 10 and 25 ms", one > 10 And one < 25, 1

busy = 1 : go = 1
Thread Start 3, Spin
Thread Start 1, Frames
Thread Start 2, Frames
Pause one * 30
go = 0
Thread Wait 1
Thread Wait 2
Check "thread 1 had every frame of thirty", f(1) >= 29 And f(1) <= 31, 1
Check "and thread 2", f(2) >= 29 And f(2) <= 31, 1
Check "the busy thread ran meanwhile", spins > 10000, 1

spins = 0
t0 = Timer
For i = 1 To 30 : Page Copy 0 To 1, B : Next
t1 = Timer - t0
Check "the main program's PAGE COPY ...,B: thirty copies in thirty frames", t1 > one * 29 And t1 < one * 31.5, 1
Check "the busy thread ran meanwhile", spins > 5000, 1
busy = 0
Print "tframe:"; pass; " of"; tests; " passed"
