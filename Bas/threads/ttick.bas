' ttick.bas - threads, Stage 2: a SETTICK interrupt every 5 ms while two threads are busy. The routine runs in the
' main program, whichever thread was running when the tick came, and on time: with the main program busy, in a
' PAUSE, and with every thread in a PAUSE.
Option Explicit
Dim Integer tests, pass, busy, ticks, wrong, late, phase, got(3), spins(2)
Dim Float t0

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Tick
  Inc ticks
  If MM.Info(Thread) <> 0 Then Inc wrong
  If Timer - t0 - ticks * 5 > 3 Then Inc late
End Sub

Sub Spin
  Local Integer me = MM.Info(Thread)
  Do While busy
    Inc spins(me)
  Loop
End Sub

Sub Nap
  Do While busy
    Pause 20
  Loop
End Sub

Sub Report(what$)
  Inc phase
  Check what$ + ": 60 ticks in 300 ms, one either way", ticks >= 59 And ticks <= 61, 1
  Check what$ + ": every one in the main program", wrong, 0
  Check what$ + ": none more than 3 ms late", late, 0
End Sub

busy = 1
Thread Start 1, Spin
Thread Start 2, Spin
t0 = Timer : ticks = 0
SetTick 5, Tick
Do While Timer - t0 < 302 : Loop
SetTick 0, 0
Report "main program busy, two threads busy"

t0 = Timer : ticks = 0 : wrong = 0 : late = 0
SetTick 5, Tick
Pause 302
SetTick 0, 0
Report "main program in PAUSE, two threads busy"
Check "the busy threads ran", spins(1) > 1000 And spins(2) > 1000, 1

busy = 0
Do While MM.Info(Thread 1) Or MM.Info(Thread 2) : Loop
busy = 1
Thread Start 1, Nap
Thread Start 2, Nap
t0 = Timer : ticks = 0 : wrong = 0 : late = 0
SetTick 5, Tick
Pause 302
SetTick 0, 0
Report "every thread in PAUSE"
busy = 0
Print "ttick:"; pass; " of"; tests; " passed"
