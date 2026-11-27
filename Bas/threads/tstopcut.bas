' tstopcut.bas - threads (#167, the PicoMite's addition to the CMM2's fix): an interrupt routine handed to its thread
' out of turn stops the thread whose turn it cut. That thread's record is freed at once, so the rest of its turn must
' go to nobody (ThreadStop clears GiveBack): the next switch must not pick the freed record. Thread 2 is the only busy
' thread, so it is the one each tick cuts. Afterwards the main program's PAUSE ends, the ticks go on and thread 3 runs.
Option Explicit
Dim Integer n, c2, c3, halt, tests, pass, stopped
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
  SetTick 1, Tk, 1                     ' its routine is handed to thread 1 from whichever thread is running
  Do While halt = 0 : Pause 1 : Loop
  SetTick 0, 0, 1
End Sub

Sub Tk
  Local s$
  Inc n
  If n = 20 Then
    Thread Stop 2                      ' the thread whose turn this routine cut
    s$ = String$(200, "x")             ' (memory taken at once, which may reuse the freed record)
    stopped = 1
  EndIf
End Sub

Sub Busy2
  Do While halt = 0 : Inc c2 : Loop
End Sub

Sub Busy3
  Do While stopped = 0 : Pause 1 : Loop    ' asleep until thread 2 has been stopped
  Do While halt = 0 : Inc c3 : Loop
End Sub

Thread Start 1, Ticker
Thread Start 2, Busy2
Thread Start 3, Busy3
t0 = Timer
Pause 300
t1 = Timer - t0
halt = 1
Thread Wait 1
Thread Wait 3
Check "a routine stopped the thread whose turn it cut", stopped And MM.Info(Thread 2) = 0, 1
Check "the main program's PAUSE 300 ended on time", t1 >= 300 And t1 < 320, 1
Check "the ticks went on", n >= 100, 1
Check "and thread 3 ran afterwards", c3 > 1000, 1
Print "tstopcut:"; pass; " of"; tests; " passed"
