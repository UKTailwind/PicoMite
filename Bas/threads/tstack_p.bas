' tstack_p.bas - threads, Stage P5: a command that goes deeper than a thread's guard tests for its room first
' (StackNeed: LOAD PNG), and JSON$'s nesting is capped by the stack's room, in a thread and in the main program. Too
' small a SIZE is "Not enough stack (thread n needs SIZE k)", and SIZE k is then enough. Uses B:/ghost.png.
Option Explicit
Dim Integer tests, pass, js(1024), k, need
Dim msg$, a$, p$

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Png
  On Error Skip
  Load PNG "B:/ghost.png"
  msg$ = MM.ErrMsg$
End Sub

Function Deeper%(n%)
  If n% > 0 Then
    Deeper% = Deeper%(n% - 1)
  Else
    Png
    Deeper% = 0
  EndIf
End Function

Sub PngDeep
  k = Deeper%(2)
End Sub

Sub MakeJSON(d%)                        ' d% arrays, one inside the next, around a 1
  Local Integer j
  LongString Clear js()
  For j = 1 To d% : LongString Append js(), "[" : Next
  LongString Append js(), "1"
  For j = 1 To d% : LongString Append js(), "]" : Next
  p$ = ""
  If d% <= 50 Then
    For j = 1 To d% : p$ = p$ + "[0]" : Next
  EndIf
End Sub

Sub JsonIn
  On Error Skip
  a$ = Json$(js(), p$)
  msg$ = MM.ErrMsg$
End Sub

Function NeedOf%(m$)                    ' the k of "... needs SIZE k)"
  NeedOf% = Val(Mid$(m$, Instr(m$, "SIZE ") + 5))
End Function

msg$ = "x" : Thread Start 1 Size 6, Png : Thread Wait 1
Check "LOAD PNG in a thread of SIZE 6: not enough stack, before it starts", Instr(msg$, "Not enough stack (thread 1 needs SIZE ") = 1, 1
need = NeedOf%(msg$)
msg$ = "x" : Thread Start 1 Size need, Png : Thread Wait 1
Check "and with the SIZE it names it loads", msg$ = "", 1
msg$ = "x" : Thread Start 1, Png : Thread Wait 1
Check "LOAD PNG in a thread of the default SIZE", msg$ = "", 1
msg$ = "x" : Thread Start 1, PngDeep : Thread Wait 1
Check "two FUNCTIONs deep in the default SIZE: not enough stack", Instr(msg$, "Not enough stack (thread 1 needs SIZE ") = 1, 1
need = NeedOf%(msg$)
Check "the SIZE it names is more than the default", need > 8, 1
msg$ = "x" : Thread Start 1 Size need, PngDeep : Thread Wait 1
Check "and with it the deep LOAD PNG works", msg$ = "", 1

MakeJSON 10
msg$ = "x" : Thread Start 1, JsonIn : Thread Wait 1
Check "JSON$ ten deep in a thread", a$ = "1" And msg$ = "", 1
MakeJSON 200
msg$ = "x" : Thread Start 1, JsonIn : Thread Wait 1
Check "two hundred deep in a thread: the cap, an error", Instr(msg$, "JSON nested more than ") > 0, 1
MakeJSON 10
JsonIn
Check "ten deep in the main program", a$ = "1" And msg$ = "", 1
MakeJSON 500
JsonIn
Check "five hundred deep in the main program: the cap, an error", Instr(msg$, "JSON nested more than ") > 0, 1
CLS
Print "tstack:"; pass; " of"; tests; " passed"
