' texitint.bas - threads (issue #14): THREAD EXIT, or THREAD STOP of its own thread, in an interrupt routine is refused
' ("Invalid in an interrupt routine"): it used to end the thread with the routine still marked as running, so no switch
' and no interrupt came again for the rest of the run. The thread ends by a flag its routine sets; afterwards the main
' program's interrupts run, another thread runs and THREAD WAIT waits. A routine may still stop another thread.
Option Explicit
Dim Integer tests, pass, n, beats, done, quit, spins
Dim e1$, e2$
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

Sub Worker
  SetTick 20, Tick, 1                  ' its routine runs in this thread
  Do While quit = 0 : Pause 5 : Loop
  SetTick 0, 0, 1
End Sub

Sub Tick
  Inc n
  If n = 3 Then
    On Error Skip
    Thread Exit
    e1$ = MM.ErrMsg$
    On Error Skip
    Thread Stop MM.Info(Thread)
    e2$ = MM.ErrMsg$
    quit = 1                            ' the way to end its thread: a flag the thread's loop tests
  EndIf
End Sub

Sub Worker2
  done = 1
End Sub

Sub Spinner
  Do : Inc spins : Loop
End Sub

Sub Beat
  Inc beats
  If beats = 5 Then Thread Stop 3       ' a routine stopping another thread is allowed
End Sub

Thread Start 1, Worker
Thread Wait 1
Check "THREAD EXIT in an interrupt routine is refused", e1$ = "Invalid in an interrupt routine", 1
Check "and THREAD STOP of the routine's own thread", e2$ = "Invalid in an interrupt routine", 1
Check "the thread ended by its flag", MM.Info(Thread 1), 0
Thread Start 3, Spinner
SetTick 10, Beat, 2
Thread Start 2, Worker2
t0 = Timer
Pause 200
SetTick 0, 0, 2
Check "afterwards the main program's interrupt runs", beats >= 15, 1
Check "another thread runs", done, 1
Check "PAUSE lets the others run", spins > 1000, 1
Check "a routine stopped another thread", MM.Info(Thread 3), 0
Thread Wait 2
Check "and THREAD WAIT waits", MM.Info(Thread 2), 0
Print "texitint:"; pass; " of"; tests; " passed"
