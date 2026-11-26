' tpack_r.bas - tpack.bas for the RP2040 (its default LOCALS 16, a 192-slot local region): at most 8 threads (Peter,
' 26 Nov, after the RP2040's tests), their local slots packed with no rounding. Eight threads of the default LOCALS
' each hold 15 locals at once while the main program holds 10; a ninth number is refused. The main program's own
' locals are its, under the threads' ranges.
Option Explicit
Dim Integer tests, pass, i, c, ok8

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Twenty
  Local Integer a1, a2, a3, a4, a5, a6, a7, a8, a9, a10
  Local Integer b1, b2, b3, b4
  Local Integer me = MM.Info(Thread)
  a1 = me : b4 = me * 2
  Do While c < 8 : Thread Yield : Loop                       ' every thread holds its locals until all have started
  If a1 = me And b4 = me * 2 Then Inc ok8
End Sub

Sub Main10
  Local Integer m1, m2, m3, m4, m5, m6, m7, m8, m9, m10
  m1 = 111 : m10 = 999
  For i = 1 To 8 : Thread Start i, Twenty : Inc c : Next
  For i = 1 To 8 : Thread Wait i : Next              ' (every thread's count in: not only the last's)
  Check "eight threads of LOCALS 16 kept their locals", ok8, 8
  Check "the main program's locals are its own", m1 + m10, 1110
End Sub

Main10
On Error Skip
Thread Start 9, Twenty
Check "a ninth is refused", MM.ErrMsg$ = "9 is invalid (valid is 1 to 8)", 1
Print "tpack:"; pass; " of"; tests; " passed"
