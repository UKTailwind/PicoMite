' tkey.bas - threads, Stage 2: ON KEY while two threads are busy. tkey.in types the keys at the prompts ending in
' "?". The routine runs in the main program whichever thread found the key: with the main program busy, then in a
' PAUSE, then with the key found from inside a FUNCTION a thread is in.
Option Explicit
Dim Integer tests, pass, busy, wrong, spins(3)
Dim k$, got$
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

Sub KeyIn
  If MM.Info(Thread) <> 0 Then Inc wrong
  Do
    k$ = Inkey$
    If k$ = "" Then Exit Do
    If k$ > " " Then got$ = got$ + k$
  Loop
End Sub

Sub Spin
  Local Integer me = MM.Info(Thread)
  Do While busy
    Inc spins(me)
  Loop
End Sub

Function Inner%(x%)
  Local Integer j
  For j = 1 To 50 : Next
  Inner% = x% + 1
End Function

Sub SpinFun
  Local Integer me = MM.Info(Thread)
  Do While busy
    spins(me) = Inner%(spins(me))
  Loop
End Sub

busy = 1
Thread Start 1, Spin
Thread Start 2, Spin
On Key KeyIn
Print "keys?";
t0 = Timer
Do While Right$(got$, 1) <> "." And Timer - t0 < 8000 : Loop
Print
Check "main program busy: the keys, in order", got$ = "abc.", 1

got$ = ""
Print "more?";
t0 = Timer
Do While Right$(got$, 1) <> "." And Timer - t0 < 8000 : Pause 20 : Loop
Print
Check "main program in PAUSE: the keys, in order", got$ = "pqrs.", 1

Thread Start 3, SpinFun
got$ = ""
Print "last?";
t0 = Timer
Do While Right$(got$, 1) <> "." And Timer - t0 < 8000 : Pause 20 : Loop
Print
On Key 0
Check "with a thread inside a FUNCTION: the keys, in order", got$ = "xyz.", 1
Check "every key's routine ran in the main program", wrong, 0
Check "the threads ran meanwhile", spins(1) > 1000 And spins(2) > 1000 And spins(3) > 100, 1
busy = 0
Print "tkey:"; pass; " of"; tests; " passed"
