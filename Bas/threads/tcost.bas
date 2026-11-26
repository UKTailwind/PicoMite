' tcost.bas - threads: what they cost on this PicoMite, for the Threads User Manual. A switch (THREAD YIELD to a thread
' and back: two switches and the other thread's loop, the empty loop's time taken off), a THREAD START and its end, and
' the heap a thread takes (its stack and its record) at the defaults and at the smallest. No .expected: the figures
' are the build's and the clock's.
Option Explicit
Dim Integer i, n = 10000, h0, h1, go
Dim Float t0, tl

Sub Other
  Do
    Thread Yield
  Loop
End Sub

Sub Nothing
End Sub

Sub Idle
  Do While go : Pause 5 : Loop
End Sub

t0 = Timer : For i = 1 To n : Next : tl = Timer - t0
t0 = Timer : For i = 1 To n : Thread Yield : Next
Print "THREAD YIELD with no thread:"; Str$((Timer - t0 - tl) * 1000 / n, 0, 2); " us"
Thread Start 1, Other
t0 = Timer : For i = 1 To n : Thread Yield : Next
Print "a yield to a thread and back:"; Str$((Timer - t0 - tl) * 1000 / n, 0, 2); " us"
Thread Stop 1
t0 = Timer
For i = 1 To 500 : Thread Start 1, Nothing : Thread Wait 1 : Next
Print "THREAD START and its end:"; Str$((Timer - t0) * 1000 / 500, 0, 0); " us"
go = 1 : Thread Start 2, Idle                        ' (the main program's record first, kept while a thread runs)
h0 = MM.Info(Heap)
Thread Start 1, Idle
h1 = MM.Info(Heap)
Print "a thread of the defaults takes"; h0 - h1; " bytes"
go = 0 : Thread Wait 1 : Thread Wait 2
go = 1 : Thread Start 2, Idle
h0 = MM.Info(Heap)
Thread Start 1 Size 6 Locals 8 Nest 4, Idle
h1 = MM.Info(Heap)
Print "a thread of SIZE 6 LOCALS 8 NEST 4 takes"; h0 - h1; " bytes"
go = 0 : Thread Wait 1 : Thread Wait 2
