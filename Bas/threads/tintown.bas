' tintown.bas - threads, Stage 4b (docs/Threads_Plan.md): an interrupt routine runs in the thread that set it up.
' tintown.in types a line at the prompt ending in "?". A thread sets up a SETTICK and waits for a flag its routine
' sets; the routine runs in that thread, with its colour, while the main program sits in INPUT. A routine set up in a
' thread that has ended runs in the main program; a paused thread's interrupts wait for it; a pin interrupt and a
' SETTICK set up by the main program still run in the main program.
Option Explicit
Dim Integer tests, pass, ticks, where, wrongcol, mainticks, mainwhere, k, held
Dim a$
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
  where = MM.Info(Thread)
  If MM.Info(FColour) <> RGB(cyan) And where = 2 Then Inc wrongcol
  If ticks = 10 Then Flag(3) = 1
End Sub

Sub MainTick
  Inc mainticks
  mainwhere = MM.Info(Thread)
End Sub

Sub Owner
  Colour RGB(cyan)
  SetTick 20, Tick, 1
  Thread Wait Flag 3                    ' its own interrupt wakes it
  Do : Pause 100 : Loop                 ' and stays, so that its interrupts are its own
End Sub

Sub Brief
  SetTick 10, Tick, 2
End Sub

Flag(3) = 0
Thread Start 2, Owner
SetTick 15, MainTick, 3
t0 = Timer
Input "the main program waits for a line"; a$
Print
Check "a thread's SETTICK ran while the main program was in INPUT", ticks >= 10, 1
Check "in the thread that set it up", where, 2
Check "with that thread's colour", wrongcol, 0
Check "the main program's own SETTICK waited for its INPUT to end", mainticks <= 1, 1
Pause 100
Check "and runs in the main program", mainwhere = 0 And mainticks >= 3, 1

Thread Pause 2
k = ticks
Pause 120
held = ticks - k
Thread Resume 2
Pause 100
Check "a paused thread's interrupt waits for it", held <= 1, 1
Check "and runs when it is resumed", ticks > k + 1, 1
Thread Stop 2
SetTick 0, 0, 1
SetTick 0, 0, 3

ticks = 0 : where = -1
Thread Start 4, Brief
Thread Wait 4                           ' the thread that set it up has ended
Pause 60
SetTick 0, 0, 2
Check "a routine whose thread has ended runs in the main program", ticks >= 3 And where = 0, 1
Print "tintown:"; pass; " of"; tests; " passed"
