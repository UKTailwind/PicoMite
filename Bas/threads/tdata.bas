' tdata.bas - threads, Stage 1: READ's place in the DATA is each thread's own, and so is the line EXECUTE runs: a
' line that waits in a FUNCTION while another thread's EXECUTE is tokenised and run goes on unharmed.
Option Explicit
Dim Integer tests, pass, i, done, v, s, sums(2), ex(2)

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Reader
  Local Integer me = MM.Info(Thread), k, v, s
  If me = 2 Then Restore second
  For k = 1 To 4
    Read v
    s = s + v
    Thread Yield
  Next
  sums(me) = s
  Inc done
End Sub

Function Bump%(t%)
  Thread Yield
  Bump% = t% * 100
End Function

Sub Exec
  Local Integer me = MM.Info(Thread), k
  For k = 1 To 4
    Execute "ex(" + Str$(me) + ") = ex(" + Str$(me) + ") + " + Str$(k) + " + Bump%(" + Str$(me) + ")"
    Thread Yield
  Next
  Inc done
End Sub

Data 1, 2, 3, 4, 5, 6, 7, 8
second:
Data 10, 20, 30, 40

Read v : s = v
Thread Start 1, Reader
Thread Start 2, Reader
Do While done < 2 : Thread Yield : Loop
Read v : s = s + v
Check "thread 1 read from the start", sums(1), 10
Check "thread 2 read from its RESTORE", sums(2), 100
Check "the main program's place was not moved", s, 3

done = 0
Thread Start 1, Exec
Thread Start 2, Exec
Exec
Do While done < 3 : Thread Yield : Loop
For i = 0 To 2
  Check "thread" + Str$(i) + ": four EXECUTEs, each waiting in a FUNCTION", ex(i), 10 + 400 * i
Next
Print "tdata:"; pass; " of"; tests; " passed"
