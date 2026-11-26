' tloops.bas - threads, Stage 1: FOR, DO, SELECT CASE, multi-line IF and GOSUB in two threads and the main program at
' once, with a yield between the statements. Each thread's loops are its own.
Option Explicit
Dim Integer tests, pass, i, done
Dim Integer r1(2), r2(2), r3(2), r4(2), r5(2), gs(2), gi(2)

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Loops
  Local Integer me = MM.Info(Thread), i, j, n, s, t
  For i = 1 To 6
    Thread Yield
    For j = i To 1 Step -1
      Thread Yield
      n = n + j
    Next j
    Thread Yield
  Next i
  r1(me) = n

  i = 0
  Do While i < 10
    Inc i
    Thread Yield
    If i = 4 Then Continue Do
    If i = 9 Then Exit Do
    s = s + i
    Thread Yield
  Loop
  r2(me) = s

  s = 0 : i = 0
  Do
    Inc i
    Thread Yield
    Select Case i Mod 3
      Case 0
        s = s + 100
        Thread Yield
      Case 1
        Thread Yield
        s = s + 1
      Case Else
        s = s + 10
    End Select
    Thread Yield
  Loop Until i = 9
  r3(me) = s

  For i = 1 To 8
    If i < 3 Then
      Thread Yield
      t = t + 1
    ElseIf i < 6 Then
      Thread Yield
      t = t + 10
    Else
      t = t + 100
      Thread Yield
    EndIf
  Next
  r4(me) = t

  For i = 1 To 5                      ' (a GOSUB's routine is a level of its own: it does not see the SUB's locals)
    gi(me) = i
    GoSub addit
    Thread Yield
  Next
  r5(me) = gs(me)
  Inc done
  Exit Sub
addit:
  Thread Yield
  gs(MM.Info(Thread)) = gs(MM.Info(Thread)) + gi(MM.Info(Thread))
  Return
End Sub

Thread Start 1, Loops
Thread Start 2, Loops
Loops
Do While done < 3 : Thread Yield : Loop
For i = 0 To 2
  Check "thread" + Str$(i) + ": nested FOR", r1(i), 56
  Check "thread" + Str$(i) + ": DO WHILE with CONTINUE DO and EXIT DO", r2(i), 32
  Check "thread" + Str$(i) + ": DO ... LOOP UNTIL with SELECT CASE", r3(i), 333
  Check "thread" + Str$(i) + ": IF, ELSEIF, ELSE", r4(i), 332
  Check "thread" + Str$(i) + ": GOSUB and RETURN", r5(i), 15
Next
Print "tloops:"; pass; " of"; tests; " passed"
