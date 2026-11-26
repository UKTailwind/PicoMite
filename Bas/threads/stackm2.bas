' stackm2.bas - threads, Stage P5: the candidates for StackNeed, measured as stackm.bas does (a measuring build,
' cmake ... -DTHREAD_STACK_REPORT; each in thread 1 with SIZE 32, its name first). Every PNG and JPG on B:, SAVE PNG,
' JSON$ nested 5, 10 and 20 deep, and PLAY MODFILE's start. No .expected: the figures are the build's.
Option Explicit
Dim Integer d, js(256)
Dim a$, f$

Sub P_Load
  On Error Skip
  Load PNG f$
  If MM.Errno Then Print " (refused)";
End Sub

Sub J_Load
  Load JPG f$
End Sub

Sub C_SavePNG
  Save PNG "B:/stk.png", 0, 0, 64, 64
  Kill "B:/stk.png"
End Sub

Sub C_JSON
  Local Integer k
  LongString Clear js()
  For k = 1 To d : LongString Append js(), "{" + Chr$(34) + "a" + Chr$(34) + ":" : Next
  LongString Append js(), "1"
  For k = 1 To d : LongString Append js(), "}" : Next
  Local p$ = "a"
  For k = 2 To d : p$ = p$ + ".a" : Next
  a$ = Json$(js(), p$)
End Sub

Sub C_Mod
  Play ModFile "B:/game.mod"
  Pause 200
  Play Stop
End Sub

Sub C_Fill2
  Fill 100, 100, RGB(green)
End Sub

f$ = Dir$("B:/*.png", FILE)
Do While f$ <> ""
  f$ = "B:/" + f$
  Print "LOAD PNG "; f$; : Thread Start 1 Size 32, P_Load : Thread Wait 1
  f$ = Dir$()
Loop
For d = 1 To 2
  f$ = Choice(d = 1, "B:/test.jpg", "B:/holiday.jpg")
  Print "LOAD JPG "; f$; : Thread Start 1 Size 32, J_Load : Thread Wait 1
Next
Print "SAVE PNG"; : Thread Start 1 Size 32, C_SavePNG : Thread Wait 1
For d = 5 To 20 Step 5
  Print "JSON$ nested"; d; : Thread Start 1 Size 32, C_JSON : Thread Wait 1
Next
CLS
Print "FILL the screen"; : Thread Start 1 Size 32, C_Fill2 : Thread Wait 1
Print "PLAY MODFILE"; : Thread Start 1 Size 32, C_Mod : Thread Wait 1
CLS
Print "done"
