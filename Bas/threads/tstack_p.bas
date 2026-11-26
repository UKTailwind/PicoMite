' tstack_p.bas - threads, Stage P5: LOAD PNG in a thread of the smallest SIZE and two FUNCTIONs deep in the default
' (its decoder's Huffman tables in its heap block: it goes about 1.5 KB below its command, inside the guard), and
' JSON$'s nesting capped by the stack's room, in a thread and in the main program. Uses B:/ghost.png.
Option Explicit
Dim Integer tests, pass, js(1024), k
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

msg$ = "x" : Thread Start 1 Size 6, Png : Thread Wait 1
Check "LOAD PNG in a thread of the smallest SIZE (its decoder's tables are in its heap block)", msg$ = "", 1
msg$ = "x" : Thread Start 1, PngDeep : Thread Wait 1
Check "and two FUNCTIONs deep in the default SIZE", msg$ = "", 1

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
