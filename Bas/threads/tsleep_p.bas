' tsleep_p.bas - threads: CPU SLEEP while the program has threads is a PAUSE, as the CMM2's (Peter, 26 Nov: "make the
' Picomite consistent with the cmm2"): the other threads run meanwhile, and the sleeping thread's interrupts run as in
' a PAUSE. Without threads it is the wait it always was.
Option Explicit
Dim Integer tests, pass, busy, spins, ticks
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

Sub Spin
  Do While busy
    Inc spins
  Loop
End Sub

Sub Tick
  Inc ticks
End Sub

Sub Sleeper
  CPU Sleep 0.2
End Sub

busy = 1
Thread Start 1, Spin
SetTick 20, Tick
t0 = Timer
CPU Sleep 0.3
t1 = Timer - t0
SetTick 0, 0
Check "CPU SLEEP 0.3 in the main program with a thread: 300 ms", t1 >= 299 And t1 < 320, 1
Check "the thread ran meanwhile", spins > 1000, 1
Check "its SETTICK ran during it, as in a PAUSE", ticks >= 12, 1
spins = 0
t0 = Timer
Thread Start 2, Sleeper
Thread Wait 2
t1 = Timer - t0
Check "CPU SLEEP 0.2 in a thread: 200 ms", t1 >= 199 And t1 < 220, 1
Check "and the other threads ran", spins > 1000, 1
busy = 0
Thread Wait 1
Print "tsleep:"; pass; " of"; tests; " passed"
