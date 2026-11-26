' stackm.bas - threads, Stage P5: how much of its stack a thread uses, for a SUB and a FUNCTION level and for the
' commands that go deepest. Needs a measuring build (cmake ... -DTHREAD_STACK_REPORT), which prints
' "[stack] thread n used x of y" as each thread's stack is freed. Each candidate runs alone in thread 1 with SIZE 32;
' its name is printed first, so each report line follows its name. Run it with OPTION COMPILE OFF and ON. Uses B:'s
' test.jpg, ghost.png, pd_rgba.png and bd_24.bmp, and A:/tt for a file. The last candidate is an error that stops the
' program, so it ends at that error. No .expected: the figures are the build's.
Option Explicit
Dim Integer d, r, k, arr(200), js(64)
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

Sub C_PrintSer
  Option Console Serial
  Print Str$(Sin(1.2) * Cos(0.3) + Sqr(2)); Mid$(UCase$("hello world"), 2, 3); Format$(3.14159, "%8.3f")
  Option Console Both
End Sub

Sub C_TLS
  Local Integer rx(512)
  Local cr$ = Chr$(13) + Chr$(10)
  On Error Skip 3
  Web Open Tls Client "httpbin.org", 443, 15000
  Web Tcp Client Request "GET /get HTTP/1.1" + cr$ + "Host: httpbin.org" + cr$ + "Connection: close" + cr$ + cr$, rx(), 10000
  Web Close Tcp Client
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

Sub C_FileB
  Open "B:/stk.txt" For Output As #1
  Print #1, "hello"
  Close #1
  Open "B:/stk.txt" For Input As #1
  Line Input #1, a$
  Close #1
  Kill "B:/stk.txt"
End Sub

Sub C_DirB
  a$ = Dir$("B:/*.png", FILE)
  Do While a$ <> ""
    a$ = Dir$()
  Loop
End Sub

Sub C_PNG
  Load PNG "B:/ghost.png"
End Sub

Sub C_PNGa
  Load PNG "B:/pd_rgba.png"
End Sub

Sub C_JPG
  Load JPG "B:/test.jpg"
End Sub

Sub C_BMP
  Load BMP "B:/bd_24.bmp"
End Sub

Sub C_JSON
  LongString Clear js()
  LongString Append js(), "{" + Chr$(34) + "a" + Chr$(34) + ":{" + Chr$(34) + "b" + Chr$(34) + ":[1,2,{" + Chr$(34) + "c" + Chr$(34) + ":" + Chr$(34) + "deep" + Chr$(34) + "}]}}"
  a$ = Json$(js(), "a.b[2].c")
End Sub

Sub C_Sort
  For k = 0 To 200 : arr(k) = (k * 7919) Mod 1009 : Next
  Sort arr()
End Sub

Sub C_Fill
  Box 300, 200, 60, 60, 1, RGB(white)
  Fill 320, 220, RGB(red)
End Sub

Sub C_Circle
  Circle 100, 100, 50, 1, 1, RGB(red), RGB(blue)
  Text 10, 10, "Measured", "LT", 4
End Sub

Sub C_Save
  Save Image "B:/stk.bmp", 0, 0, 32, 32
  Kill "B:/stk.bmp"
End Sub

Sub C_ErrStop
  Error "measured, and the program stops"
End Sub

Sub C_String
  s$ = ""
  For k = 1 To 30 : s$ = s$ + Str$(k) + " " : Next
  a$ = Right$(s$, 10) + Left$(s$, 10) + Hex$(k) + Bin$(k)
End Sub

Print "nothing"; : Thread Start 1 Size 32, Nothing : Thread Wait 1
For d = 0 To 6 Step 2
  Print "SUB depth"; d; : Thread Start 1 Size 32, S_Sub : Thread Wait 1
Next
For d = 0 To 6 Step 2
  Print "FUNCTION depth"; d; : Thread Start 1 Size 32, S_Fun : Thread Wait 1
Next
Print "PRINT"; : Thread Start 1 Size 32, C_Print : Thread Wait 1
Print "PRINT, serial only"; : Thread Start 1 Size 32, C_PrintSer : Thread Wait 1
Print "ERROR skipped"; : Thread Start 1 Size 32, C_ErrSkip : Thread Wait 1
Print "EVAL"; : Thread Start 1 Size 32, C_Eval : Thread Wait 1
Print "EXECUTE"; : Thread Start 1 Size 32, C_Exec : Thread Wait 1
Print "file on A:"; : Thread Start 1 Size 32, C_FileA : Thread Wait 1
Print "file on B:"; : Thread Start 1 Size 32, C_FileB : Thread Wait 1
Print "DIR$ on B:"; : Thread Start 1 Size 32, C_DirB : Thread Wait 1
Print "LOAD PNG"; : Thread Start 1 Size 32, C_PNG : Thread Wait 1
Print "LOAD PNG RGBA"; : Thread Start 1 Size 32, C_PNGa : Thread Wait 1
Print "LOAD JPG"; : Thread Start 1 Size 32, C_JPG : Thread Wait 1
Print "LOAD BMP"; : Thread Start 1 Size 32, C_BMP : Thread Wait 1
Print "JSON$"; : Thread Start 1 Size 32, C_JSON : Thread Wait 1
Print "SORT"; : Thread Start 1 Size 32, C_Sort : Thread Wait 1
Print "FILL"; : Thread Start 1 Size 32, C_Fill : Thread Wait 1
Print "CIRCLE, TEXT"; : Thread Start 1 Size 32, C_Circle : Thread Wait 1
Print "SAVE IMAGE"; : Thread Start 1 Size 32, C_Save : Thread Wait 1
Print "strings"; : Thread Start 1 Size 32, C_String : Thread Wait 1
Print "TLS request"; : Thread Start 1 Size 32, C_TLS : Thread Wait 1
Print "ERROR, the program stops"; : Thread Start 1 Size 32, C_ErrStop : Thread Wait 1
