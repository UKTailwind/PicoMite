' terror.bas - threads, Stage 1: ON ERROR SKIP, MM.ERRNO and MM.ERRMSG$ are each thread's own. A thread's skipped
' error is still its own after the other threads have run and raised theirs, and its count of statements to skip is
' not used up by their statements.
Option Explicit
Dim Integer tests, pass, i, done, good(3), late(3)

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Careful
  Local Integer me = MM.Info(Thread), k
  For k = 1 To 5
    On Error Skip
    Error "from thread" + Str$(me) + " pass" + Str$(k)
    Thread Yield
    If MM.Errno <> 0 And MM.ErrMsg$ = "from thread" + Str$(me) + " pass" + Str$(k) Then Inc good(me)
    Thread Yield
  Next
  On Error Skip 2
  Thread Yield
  Error "skipped after a yield"
  If MM.ErrMsg$ = "skipped after a yield" Then late(me) = 1
  On Error Clear
  Thread Yield
  If MM.Errno = 0 And MM.ErrMsg$ = "" Then late(me) = late(me) + 1
  Inc done
End Sub

For i = 1 To 3 : Thread Start i, Careful : Next
Careful
Do While done < 4 : Thread Yield : Loop
For i = 0 To 3
  Check "thread" + Str$(i) + ": five skipped errors, each still its own after the others ran", good(i), 5
  Check "thread" + Str$(i) + ": a skip count kept across a yield, and ON ERROR CLEAR", late(i), 2
Next
Print "terror:"; pass; " of"; tests; " passed"
