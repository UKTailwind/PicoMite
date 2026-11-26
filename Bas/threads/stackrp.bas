' stackrp.bas - threads, Stage P5 on the RP2040: stackm.bas's candidates that a PicoMite without a display or an SD card
' has (JSON$ is measured by stackm2.bas: PICOMIN has none), and INPUT and LINE INPUT in a thread (the runner types "abc, 5" and "a line" at their prompts). Needs a measuring
' build (cmake ... -DTHREAD_STACK_REPORT); each in thread 1 with SIZE 32, its name first. No .expected.
Option Explicit
Dim Integer d, r, k, arr(200)
Dim a$, s$

Sub Nothing
End Sub

Sub SubRec(n%)
  If n% > 0 Then SubRec n% - 1
End Sub

Function FunRec%(n%)
  If n% > 0 Then FunRec% = FunRec%(n% - 1) + 1 Else FunRec% = 0
End Function

Sub S_Sub
  SubRec d
End Sub

Sub S_Fun
  r = FunRec%(d)
End Sub

Sub C_Print
  Print Str$(Sin(1.2) * Cos(0.3) + Sqr(2)); Mid$(UCase$("hello world"), 2, 3); Format$(3.14159, "%8.3f")
End Sub

Sub C_ErrSkip
  On Error Skip
  Error "measured"
End Sub

Sub C_Eval
  r = Eval("1 + 2 * 3 + Len(Str$(Pi))")
End Sub

Sub C_Exec
  Execute "a$ = Str$(Pi)"
End Sub

Sub C_FileA
  Open "A:/tt/stk.txt" For Output As #1
  Print #1, "hello"
  Close #1
  Open "A:/tt/stk.txt" For Input As #1
  Line Input #1, a$
  Close #1
  Kill "A:/tt/stk.txt"
End Sub

Sub C_DirA
  a$ = Dir$("A:/tt/*.bas", FILE)
  Do While a$ <> ""
    a$ = Dir$()
  Loop
End Sub

Sub C_Sort
  For k = 0 To 200 : arr(k) = (k * 7919) Mod 1009 : Next
  Sort arr()
End Sub

Sub C_String
  s$ = ""
  For k = 1 To 30 : s$ = s$ + Str$(k) + " " : Next
  a$ = Right$(s$, 10) + Left$(s$, 10) + Hex$(k) + Bin$(k)
End Sub

Sub C_Input
  Local w$, v%
  Input "type two"; w$, v%
End Sub

Sub C_LineInput
  Local l$
  Line Input "type a line? ", l$
End Sub

Sub C_ErrStop
  Error "measured, and the program stops"
End Sub

Print "nothing"; : Thread Start 1 Size 32, Nothing : Thread Wait 1
For d = 0 To 4 Step 2
  Print "SUB depth"; d; : Thread Start 1 Size 32, S_Sub : Thread Wait 1
Next
For d = 0 To 4 Step 2
  Print "FUNCTION depth"; d; : Thread Start 1 Size 32, S_Fun : Thread Wait 1
Next
Print "PRINT"; : Thread Start 1 Size 32, C_Print : Thread Wait 1
Print "ERROR skipped"; : Thread Start 1 Size 32, C_ErrSkip : Thread Wait 1
Print "EVAL"; : Thread Start 1 Size 32, C_Eval : Thread Wait 1
Print "EXECUTE"; : Thread Start 1 Size 32, C_Exec : Thread Wait 1
Print "file on A:"; : Thread Start 1 Size 32, C_FileA : Thread Wait 1
Print "DIR$ on A:"; : Thread Start 1 Size 32, C_DirA : Thread Wait 1
Print "SORT"; : Thread Start 1 Size 32, C_Sort : Thread Wait 1
Print "strings"; : Thread Start 1 Size 32, C_String : Thread Wait 1
Print "INPUT"; : Thread Start 1 Size 32, C_Input : Thread Wait 1
Print "LINE INPUT"; : Thread Start 1 Size 32, C_LineInput : Thread Wait 1
Print "ERROR, the program stops"; : Thread Start 1 Size 32, C_ErrStop : Thread Wait 1
