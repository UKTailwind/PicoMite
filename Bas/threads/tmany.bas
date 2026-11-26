' tmany.bas - threads, Stage 1: ten threads started and ended fifty times. Their memory and their numbers come back.
' Then what THREAD refuses.
Option Explicit
Dim Integer tests, pass, i, r, c, h0, h1, i0, i1, v0

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Brief
  Local Integer k
  Local s$ = "thread" + Str$(MM.Info(Thread))
  For k = 1 To 3 : Thread Yield : Next
  If s$ = "thread" + Str$(MM.Info(Thread)) Then Inc c
End Sub

Sub Idle
  Do : Thread Yield : Loop
End Sub

Thread Start 1, Brief                 ' once first, so that what a first use takes for good is not counted
Do While c < 1 : Thread Yield : Loop
Thread Yield
c = 0
h0 = MM.Info(Heap) : i0 = MM.Info(HeapI) : v0 = MM.Info(VarCnt)
For r = 1 To 50
  For i = 1 To 10 : Thread Start i, Brief : Next
  Do While c < 10 * r : Thread Yield : Loop
Next
Thread Yield
h1 = MM.Info(Heap) : i1 = MM.Info(HeapI)
Check "500 threads ran to their end", c, 500
Check "the heap is as it was", h1 - h0, 0
Check "and the on-chip heap", i1 - i0, 0
Check "no variable left behind", MM.Info(VarCnt) - v0, 0

Thread Start 5, Idle
On Error Skip
Thread Start 5, Idle
Check "a number in use is refused", MM.ErrMsg$ = "Thread 5 is running", 1
On Error Skip
Thread Start 7, NoSuchSub
Check "a name that is not a SUB is refused", MM.ErrMsg$ = "NOSUCHSUB is not a SUB", 1
On Error Skip
Thread Exit
Check "THREAD EXIT in the main program is refused", MM.ErrMsg$ = "Not in a thread", 1
On Error Skip
Erase tests
Check "ERASE is refused while a thread is running", MM.ErrMsg$ = "Not allowed while a thread is running", 1
Print "tmany:"; pass; " of"; tests; " passed"
