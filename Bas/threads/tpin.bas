' tpin.bas - threads, Stage 2: a pin interrupt (SETPIN INTB) with threads running. No wire is needed: the pin's level
' is moved by changing its pull-down to a pull-up in the port's register (GPIOC's PUPDR; pin 7 is PC1), which leaves the
' interrupt as SETPIN set it. Nothing may drive pin 7 (GP6): a wire to a pin that is not an output does no harm.
' A thread moves the level every 5 ms. The routine runs in the main program each time: with every thread busy, with
' the main program in a PAUSE, and with every thread in a PAUSE.
Option Explicit
Const PUPDR = &H5802080C
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

Sub SetLevel(v%)
  Poke Word PUPDR, (Peek(Word PUPDR) And &HFFFFFFF3) Or Choice(v%, 4, 8)
End Sub

Sub PinInt
  Inc edges
  If MM.Info(Thread) <> 0 Then Inc wrong
  If Pin(7) <> level Then Inc bad
End Sub

Sub Mover
  Local Integer k
  For k = 1 To 20
    level = k And 1
    SetLevel level
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

SetPin 7, DIN, PULLDOWN
SetLevel 1 : Pause 2
If Pin(7) <> 1 Then Print "pin 7 does not follow its pull-up: something drives it" : End
SetLevel 0 : Pause 2
SetPin 7, INTB, PinInt, PULLDOWN

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
SetPin 7, OFF
Print "tpin:"; pass; " of"; tests; " passed"
