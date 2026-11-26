' targs.bas - threads, Stage 3: arguments for a thread's SUB. Each is evaluated when THREAD START runs and handed
' over by value, exact: integers, floats, strings with any bytes in them. A global array is handed over as itself.
Option Explicit
Dim Integer tests, pass, i, n, bad, hits(4), seen(4), arr(10)
Dim Float f1 = 1 / 3, f2 = 1e300, f3 = -2.5e-300, f4 = Pi, f5 = 0.1, f6 = 123456789.123456789
Dim s1$ = "plain", s2$ = "a " + Chr$(34) + "quoted" + Chr$(34) + " word", s3$ = "", s4$, s5$, r$
Dim Float fr(6)

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Ints(a%, b%, c%, d%)
  If a% <> 42 Then Inc bad
  If b% <> -7 Then Inc bad
  If c% <> &H7FFFFFFFFFFFFFFF Then Inc bad
  If d% <> &H8000000000000000 Then Inc bad
  Inc n
End Sub

Sub Floats(a!, b!, c!, d!, e!, f!)
  fr(1) = a! : fr(2) = b! : fr(3) = c! : fr(4) = d! : fr(5) = e! : fr(6) = f!
  Inc n
End Sub

Sub Strs(a$, b$, c$, d$, e$)
  If a$ <> s1$ Then Inc bad
  If b$ <> s2$ Then Inc bad
  If c$ <> s3$ Or Len(c$) <> 0 Then Inc bad
  If d$ <> s4$ Or Len(d$) <> 255 Then Inc bad
  If e$ <> s5$ Or Len(e$) <> 5 Then Inc bad
  Inc n
End Sub

Sub Mixed(a As Float, b As Integer, c$)
  ' an integer for a float, a float for an integer: converted as in any call
  If a <> 3 Or b <> 3 Or c$ <> "x3" Then Inc bad
  Inc n
End Sub

Sub Change(a%, b$)
  a% = a% + 1000                        ' by value: the variables they came from stay as they are
  b$ = b$ + " changed"
  Inc n
End Sub

Sub Numbered(k%)
  Pause 10
  seen(MM.Info(Thread)) = k%
End Sub

Sub Fill(a%(), v%)
  Local Integer j
  For j = 0 To 10 : a%(j) = v% + j : Next
End Sub

Function Twice%(x%)
  Twice% = x% * 2
End Function

Sub Maker                               ' a SUB's own array may not go to a thread: it is gone when the SUB ends
  Local Integer mine(5)
  On Error Skip
  Thread Start 4, Fill, mine(), 1
  r$ = MM.ErrMsg$
End Sub

For i = 1 To 255 : s4$ = s4$ + Chr$(i) : Next
s5$ = "a" + Chr$(0) + Chr$(34) + Chr$(0) + "b"

Thread Start 1, Ints, 42, 3 - 10, &H7FFFFFFFFFFFFFFF, &H8000000000000000
Thread Wait 1
Check "four integers, the largest and the smallest among them", bad, 0
Thread Start 1, Floats, f1, f2, f3, f4, f5, f6
Thread Wait 1
bad = (fr(1) <> f1) + (fr(2) <> f2) + (fr(3) <> f3) + (fr(4) <> f4) + (fr(5) <> f5) + (fr(6) <> f6)
Check "six floats, each bit for bit", bad, 0
For i = 1 To 40
  f1 = Rnd * 10 ^ (Int(Rnd * 600) - 300) : f2 = -Rnd : f3 = Rnd * 1e-300 : f4 = Sqr(i) : f5 = 1 / i : f6 = i * 1e15 + 0.5
  Thread Start 1, Floats, f1, f2, f3, f4, f5, f6
  Thread Wait 1
  bad = bad + (fr(1) <> f1) + (fr(2) <> f2) + (fr(3) <> f3) + (fr(4) <> f4) + (fr(5) <> f5) + (fr(6) <> f6)
Next
Check "240 more, of every size", bad, 0
Thread Start 1, Strs, s1$, s2$, s3$, s4$, s5$
Thread Wait 1
Check "five strings: quotes, every byte from 1 to 255, zero bytes, empty", bad, 0
Thread Start 1, Mixed, 3, 3.2, "x" + Str$(3)
Thread Wait 1
Check "an integer for a float, a float for an integer, an expression", bad, 0
Check "every SUB ran", n, 44

i = 5 : r$ = "as it was"
Thread Start 1, Change, i, r$
Thread Wait 1
Check "by value: the integer it came from is unchanged", i, 5
Check "and the string", r$ = "as it was", 1

For i = 1 To 4 : Thread Start i, Numbered, Twice%(i) * 10 : Next
For i = 1 To 4 : Thread Wait i : Next
Check "evaluated when THREAD START runs, each thread its own", seen(1) * 1000 + seen(2) * 100 + seen(3) * 10 + seen(4) / 10, 20000 + 4000 + 600 + 8

Thread Start 1, Fill, arr(), 100
Thread Wait 1
Check "a global array is the thread's to fill", arr(0) + arr(10), 210
Maker
Check "a SUB's own array is refused", r$ = "Only a global array can be passed to a thread", 1
Check "and no thread was started", MM.Info(Thread 4), 0
Print "targs:"; pass; " of"; tests; " passed"
