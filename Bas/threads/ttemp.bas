' ttemp.bas - threads, Stage 1: string expressions that call FUNCTIONs which yield, in three threads and the main
' program at once. The strings a statement has made so far are its thread's: another thread's statements, each of
' which frees its own temporary memory, leave them alone.
Option Explicit
Dim Integer tests, pass, i, done, good(3)

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Function Tag$(a$)
  Local b$ = "<" + a$ + ">"
  Thread Yield
  Tag$ = b$ + Str$(MM.Info(Thread))
End Function

Sub Strings
  Local Integer me = MM.Info(Thread), k
  Local r$, want$
  For k = 1 To 8
    r$ = "a" + Tag$("x" + Str$(k)) + "-" + Tag$(Tag$("in")) + Mid$("0123456789", k, 2) + UCase$(Tag$("lc"))
    want$ = "a<x" + Str$(k) + ">" + Str$(me) + "-<<in>" + Str$(me) + ">" + Str$(me) + Mid$("0123456789", k, 2) + "<LC>" + Str$(me)
    If r$ = want$ Then Inc good(me)
    Thread Yield
  Next
  Inc done
End Sub

For i = 1 To 3 : Thread Start i, Strings : Next
Strings
Do While done < 4 : Thread Yield : Loop
For i = 0 To 3
  Check "thread" + Str$(i) + ": eight strings built across yields", good(i), 8
Next
Print "ttemp:"; pass; " of"; tests; " passed"
