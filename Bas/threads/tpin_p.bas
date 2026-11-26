' tpin_p.bas - threads: a pin interrupt (SETPIN INTB) with threads running (the PicoMite's version of tpin.bas). GP0
' is an output wired to GP1, the interrupt's pin (on COM11's PC3, Peter: "gp0 is free and you can read it on gp1
' which is linked"), where the CMM2 moves its pin's level with the pull-up and pull-down register.
' A thread moves the level every 5 ms. The routine runs in the main program each time: with every thread busy, with
' the main program in a PAUSE, and with every thread in a PAUSE.
Option Explicit
Dim Integer tests, pass, busy, edges, wrong, level, bad, spins(2)

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub PinInt
  Inc edges
  If MM.Info(Thread) <> 0 Then Inc wrong
  If Pin(GP1) <> level Then Inc bad
End Sub

Sub Mover
  Local Integer k
  For k = 1 To 20
    level = k And 1
    Pin(GP0) = level
    Pause 5
  Next
End Sub

Sub Spin
  Local Integer me = MM.Info(Thread)
  Do While busy
    Inc spins(me)
  Loop
End Sub

Sub Nap
  Do While busy
    Pause 7
  Loop
End Sub

Sub Report(what$)
  Check what$ + ": 20 edges, 20 interrupts", edges, 20
  Check what$ + ": every one in the main program, the pin at its new level", wrong + bad, 0
  edges = 0
End Sub

SetPin GP0, DOUT
SetPin GP1, DIN
Pin(GP0) = 1 : Pause 2
If Pin(GP1) <> 1 Then Print "GP1 does not follow GP0: are they linked?" : End
Pin(GP0) = 0 : Pause 2
If Pin(GP1) <> 0 Then Print "GP1 does not follow GP0: are they linked?" : End
SetPin GP1, INTB, PinInt

busy = 1
Thread Start 1, Spin
Thread Start 2, Spin
Thread Start 3, Mover
Do While MM.Info(Thread 3) : Loop
Report "every thread busy"

Thread Start 3, Mover
Do While MM.Info(Thread 3) : Pause 50 : Loop
Report "main program in PAUSE, two threads busy"
Check "the busy threads ran", spins(1) > 1000 And spins(2) > 1000, 1

busy = 0
Do While MM.Info(Thread 1) Or MM.Info(Thread 2) : Loop
busy = 1
Thread Start 1, Nap
Thread Start 2, Nap
Thread Start 3, Mover
Do While MM.Info(Thread 3) : Pause 50 : Loop
Report "every thread in PAUSE"
busy = 0
SetPin GP1, OFF
SetPin GP0, OFF
Print "tpin:"; pass; " of"; tests; " passed"
