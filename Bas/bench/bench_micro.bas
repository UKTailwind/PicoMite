' bench_micro.bas - Phase 0 micro suite (Interpreter_Performance_Plan.html 4.2 0c)
' Each test times N passes of a FOR loop holding one statement, subtracts
' the empty loop, and prints: BENCH name us_per_stmt cycles_per_stmt
' Top-level tests use globals; the Sub tests use LOCALs, as the anchor does.
Option default float
Const N% = 20000
Const K1 = 1.5
Dim a%, b%, s%, y%, i%, arr%(1024), arr2%(32, 32)
Dim fx, fy
Dim st$, t$, u$
Dim abcdefghijklmnopqrstuvwxyz1234%
Dim mhz, base_us, t0
mhz = Val(MM.Info(CPUSPEED)) / 1000000
t$ = "Hello world" : u$ = "abc" : fx = 1.25 : b% = 7
Print "BENCHSTART "; MM.Device$; " "; MM.Ver; " "; mhz; " MHz"

' ---- empty loop (the baseline every top-level test subtracts)
t0 = Timer
For i% = 1 To N%
Next
base_us = (Timer - t0) * 1000
Print "BENCH empty_for_loop "; Str$(base_us / N%, 0, 3); " "; Str$(base_us / N% * mhz, 0, 0)

t0 = Timer
For i% = 1 To N%
  a% = b%
Next
Rep "a%=b% (1-char names)", Timer - t0

t0 = Timer
For i% = 1 To N%
  abcdefghijklmnopqrstuvwxyz1234% = abcdefghijklmnopqrstuvwxyz1234%
Next
Rep "30-char name = 30-char name", Timer - t0

t0 = Timer
For i% = 1 To N%
  a% = a% + 1
Next
Rep "a%=a%+1", Timer - t0

t0 = Timer
For i% = 1 To N%
  a% = a% + 1 ' a forty character trailing comment......
Next
Rep "a%=a%+1 + 40-char comment", Timer - t0

t0 = Timer
For i% = 1 To N%
  a% = a% + 1                                        
Next
Rep "a%=a%+1 + 40 trailing spaces", Timer - t0

t0 = Timer
For i% = 1 To N%
  s% = (s% + i%) And 65535
Next
Rep "s%=(s%+i%) AND 65535", Timer - t0

t0 = Timer
For i% = 1 To N%
  fx = fx * 1.000001 + 0.5
Next
Rep "fx=fx*1.000001+0.5", Timer - t0

t0 = Timer
For i% = 1 To N%
  fy = fx
Next
Rep "fy=fx (float)", Timer - t0

t0 = Timer
For i% = 1 To N%
  fy = 1.23456789
Next
Rep "fy=1.23456789 (literal)", Timer - t0

t0 = Timer
For i% = 1 To N%
  fy = K1
Next
Rep "fy=K1 (CONST)", Timer - t0

t0 = Timer
For i% = 1 To N%
  y% = arr%(i% And 1023)
Next
Rep "y%=arr%(i% AND 1023)", Timer - t0

t0 = Timer
For i% = 1 To N%
  arr%(i% And 1023) = y%
Next
Rep "arr%(i% AND 1023)=y%", Timer - t0

t0 = Timer
For i% = 1 To N%
  y% = arr2%(i% And 31, 7)
Next
Rep "y%=arr2%(i% AND 31,7)", Timer - t0

t0 = Timer
For i% = 1 To N%
  fy = Abs(fx)
Next
Rep "fy=ABS(fx)", Timer - t0

t0 = Timer
For i% = 1 To N%
  fy = Abs(Abs(Abs(Abs(Abs(fx)))))
Next
Rep "fy=ABS nested 5 deep", Timer - t0

t0 = Timer
For i% = 1 To N%
  fy = Sin(fx)
Next
Rep "fy=SIN(fx)", Timer - t0

t0 = Timer
For i% = 1 To N%
  st$ = Str$(i%)
Next
Rep "st$=STR$(i%)", Timer - t0

t0 = Timer
For i% = 1 To N%
  st$ = Left$(t$, 3)
Next
Rep "st$=LEFT$(t$,3)", Timer - t0

t0 = Timer
For i% = 1 To N%
  st$ = t$ + u$
Next
Rep "st$=t$+u$", Timer - t0

t0 = Timer
For i% = 1 To N%
  If i% And 1 Then a% = 1 Else a% = 2
Next
Rep "IF i% AND 1 THEN a%=1 ELSE a%=2", Timer - t0

t0 = Timer
For i% = 1 To N%
  If i% And 1 Then
    a% = 1
  Else
    a% = 2
  EndIf
Next
Rep "multi-line IF/ELSE/ENDIF (per pass)", Timer - t0

t0 = Timer
For i% = 1 To N%
  Select Case 1
    Case 1
      a% = 1
    Case 2
      a% = 2
    Case 3
      a% = 3
    Case 4
      a% = 4
    Case 5
      a% = 5
    Case 6
      a% = 6
    Case 7
      a% = 7
    Case 8
      a% = 8
  End Select
Next
Rep "SELECT 8 arms, first matches", Timer - t0

t0 = Timer
For i% = 1 To N%
  Select Case 8
    Case 1
      a% = 1
    Case 2
      a% = 2
    Case 3
      a% = 3
    Case 4
      a% = 4
    Case 5
      a% = 5
    Case 6
      a% = 6
    Case 7
      a% = 7
    Case 8
      a% = 8
  End Select
Next
Rep "SELECT 8 arms, last matches", Timer - t0

t0 = Timer
For i% = 1 To N%
  NoArgs
Next
Rep "call SUB with no arguments", Timer - t0

t0 = Timer
For i% = 1 To N%
  y% = Add2%(i%, 3)
Next
Rep "y%=FUNCTION f%(a%,b%)", Timer - t0

t0 = Timer
For i% = 1 To N%
  SixParams i%, 2, 3, 4, 5, 6
Next
Rep "call SUB, 6 params + 4 LOCALs", Timer - t0

t0 = Timer
For i% = 1 To N% \ 10
  LocalArrays
Next
Rep10 "call SUB creating 3 LOCAL arrays", Timer - t0

t0 = Timer
For i% = 1 To N%
  GoSub gsb
Next
Rep "GOSUB/RETURN", Timer - t0

' ---- the same kinds of statement inside a SUB, on LOCALs, as the anchor runs
InSub

Print "BENCHEND"
End

gsb:
Return

Sub Rep lbl$, secs
  Local us
  us = (secs * 1000 - base_us) / N%
  Print "BENCH "; lbl$; " "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
End Sub

Sub Rep10 lbl$, secs
  Local us
  us = (secs * 1000 - base_us / 10) / (N% \ 10)
  Print "BENCH "; lbl$; " "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
End Sub

Sub NoArgs
End Sub

Function Add2%(p%, q%)
  Add2% = p% + q%
End Function

Sub SixParams p1%, p2%, p3%, p4%, p5%, p6%
  Local integer l1, l2, l3, l4
  l1 = p1%
End Sub

Sub LocalArrays
  Local float la(3), lb(3), lc(3)
  la(1) = 1
End Sub

Sub InSub
  Local integer li, la, lb
  Local float lx, ly
  Local lbase, lt
  lx = 1.25 : lb = 7
  lt = Timer
  For li = 1 To N%
  Next
  lbase = (Timer - lt) * 1000
  Print "BENCH [sub] empty_for_loop "; Str$(lbase / N%, 0, 3); " "; Str$(lbase / N% * mhz, 0, 0)

  lt = Timer
  For li = 1 To N%
    la = lb
  Next
  RepS "[sub] la=lb (LOCALs)", Timer - lt, lbase

  lt = Timer
  For li = 1 To N%
    la = b%
  Next
  RepS "[sub] la=b% (global read from a SUB)", Timer - lt, lbase

  lt = Timer
  For li = 1 To N%
    ly = K1
  Next
  RepS "[sub] ly=K1 (CONST read from a SUB)", Timer - lt, lbase

  lt = Timer
  For li = 1 To N%
    ly = lx * 1.000001 + 0.5
  Next
  RepS "[sub] ly=lx*1.000001+0.5", Timer - lt, lbase

  lt = Timer
  For li = 1 To N%
    ly = Sin(lx)
  Next
  RepS "[sub] ly=SIN(lx)", Timer - lt, lbase

  lt = Timer
  For li = 1 To N%
    la = arr%(li And 1023)
  Next
  RepS "[sub] la=arr%(li AND 1023) (global array)", Timer - lt, lbase
End Sub

Sub RepS lbl$, secs, bb
  Local us
  us = (secs * 1000 - bb) / N%
  Print "BENCH "; lbl$; " "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
End Sub
