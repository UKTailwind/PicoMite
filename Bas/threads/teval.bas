' teval.bas - threads, Stage 4: EVAL() in a thread while the main program is part-way through an EXECUTE line. Both
' call FUNCTIONs that let the other run. EVAL tokenises its string in the token buffer, where the main program's
' EXECUTE line is: it now gives the buffer back before the expression runs.
Option Explicit
Dim Integer tests, pass, go, i, v, r, bad, badmain, runs

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Function Slow%(n%)
  Local Integer k
  For k = 1 To n% : Thread Yield : Next
  Slow% = n% * 2
End Function

Function Twice%(x%)
  Thread Yield
  Twice% = x% * 2
End Function

Sub Evaller
  Do While go
    v = Eval("Twice%(21) + 0")
    If v <> 42 Then Inc bad
    Inc runs
  Loop
End Sub

go = 1
Thread Start 1, Evaller
For i = 1 To 20
  Execute "r = Slow%(5) + 1"
  If r <> 11 Then Inc badmain
Next
go = 0
Thread Wait 1
Check "EXECUTE in the main program: every line right", badmain, 0
Check "EVAL in the thread: every value right", bad, 0
Check "the thread ran", runs > 20, 1
Print "teval:"; pass; " of"; tests; " passed"
