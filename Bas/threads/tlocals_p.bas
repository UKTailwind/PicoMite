' tlocals.bas - threads, Stage 1: the same SUBs and FUNCTIONs running in four threads at once, with a yield wherever
' another thread could disturb them. Each thread's locals, parameters, arrays, strings and recursion are its own.
Option Local Variables 400 ' (the PicoMite: four threads of LOCALS 48 in the local region; a PC3's is 192)
Option Explicit
Thread Size 12, 48 ' (the CMM2's default LOCALS; 12 KB: its recursion goes 6 levels deep, about 830 bytes each)
Dim Integer tests, pass, i, done, base
Dim Integer sums(4), strok(4), arrok(4), facts(4), refv(4), depthok(4)

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Function Fact%(n%)
  Local Integer mine = n% * 7
  Thread Yield
  If n% <= 1 Then Fact% = 1 Else Fact% = n% * Fact%(n% - 1)
  If mine <> n% * 7 Then Fact% = -1000000
End Function

Sub AddTo(v%, by%)
  Local Integer j
  For j = 1 To 5
    v% = v% + by%
    Thread Yield
  Next
End Sub

Sub Deep(me%, n%)
  Local Integer here = me% * 1000 + n%, x(3)
  Local w$ = Str$(here)
  x(1) = here
  Thread Yield
  If n% > 0 Then Deep me%, n% - 1
  If here <> me% * 1000 + n% Or x(1) <> here Or w$ <> Str$(here) Then
    depthok(me%) = -100
  Else
    Inc depthok(me%)
  EndIf
End Sub

Sub Worker
  Local Integer me = MM.Info(Thread), k, total, bad, a(10)
  Local s$ = "T" + Str$(me), t$ Length 20
  Local Float f = me * 1.5
  For k = 1 To 10
    a(k) = me * 100 + k
    total = total + a(k)
    t$ = t$ + Chr$(64 + me)
    Thread Yield
  Next
  sums(me) = total
  strok(me) = (s$ = "T" + Str$(me)) And (t$ = String$(10, 64 + me)) And (f = me * 1.5)
  For k = 1 To 10
    If a(k) <> me * 100 + k Then bad = 1
    Thread Yield
  Next
  arrok(me) = Not bad
  facts(me) = Fact%(me + 2)
  AddTo refv(me), me
  Deep me, 4
  Inc done
End Sub

base = MM.Info(VarCnt)
For i = 1 To 4 : Thread Start i, Worker : Next
Do While done < 4 : Thread Yield : Loop
Thread Yield
For i = 1 To 4
  Check "thread" + Str$(i) + ": its locals summed", sums(i), 1000 * i + 55
  Check "thread" + Str$(i) + ": its strings and its float", strok(i), 1
  Check "thread" + Str$(i) + ": its array", arrok(i), 1
  Check "thread" + Str$(i) + ": a FUNCTION recursing with a yield at each level", facts(i), Choice(i = 1, 6, Choice(i = 2, 24, Choice(i = 3, 120, 720)))
  Check "thread" + Str$(i) + ": a parameter by reference", refv(i), 5 * i
  Check "thread" + Str$(i) + ": a SUB recursing, each level's locals", depthok(i), 5
Next
Check "the main program is thread 0 again", MM.Info(Thread), 0
Check "no variable left behind", MM.Info(VarCnt) - base, 0
Print "tlocals:"; pass; " of"; tests; " passed"
