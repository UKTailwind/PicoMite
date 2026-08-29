' ArrayFuncTest.bas - array-returning functions  FUNCTION f() AS INTEGER(n)  (Phase 2)
' Requires Phase 1 (whole-array assignment). Runs on every build variant.
Option Explicit
Option Base 0

Dim tests% = 0, fails% = 0

Sub report chk%, msg$
  tests% = tests% + 1
  If chk% Then
    Print "PASS  "; msg$
  Else
    fails% = fails% + 1
    Print "FAIL  "; msg$
  EndIf
End Sub

Sub reperr msg$
  report MM.ERRNO <> 0, msg$ + "  [" + MM.ERRMSG$ + "]"
  On Error Clear
End Sub

' ---------- F01: basic integer array return -------------------------
Function seqsq(n%) As Integer(5)
  Local i%
  For i% = 0 To 5
    seqsq(i%) = n% + i% * i%
  Next
End Function

Dim b%(5)
b%() = seqsq(100)
report b%(0) = 100 And b%(3) = 109 And b%(5) = 125, "F01a integer array function"
b%() = seqsq(7)
report b%(0) = 7 And b%(5) = 32, "F01b second call gives a fresh result"

' ---------- F02: float, multi-dim return, reshape --------------------
Function halves() As Float(2, 1)
  halves(0,0) = 0.5 : halves(1,0) = 1.5 : halves(2,0) = 2.5
  halves(0,1) = 3.5 : halves(1,1) = 4.5 : halves(2,1) = 5.5
End Function

Dim ff!(5)
ff!() = halves()
report ff!(0) = 0.5 And ff!(5) = 5.5, "F02  float 3x2 return reshaped into 6"

' ---------- F03: build in LOCAL, whole-array self assign -------------
Function fib(dummy%) As Integer(9)
  Local r%(9), i%
  r%(0) = 0 : r%(1) = 1
  For i% = 2 To 9 : r%(i%) = r%(i% - 1) + r%(i% - 2) : Next
  fib() = r%()
End Function

Dim fb%(9)
fb%() = fib(0)
report fb%(8) = 21 And fb%(9) = 34, "F03  LOCAL build + fib() = r%() self assign"

' ---------- F04: recursive array function -----------------------------
Function rdepth(n%) As Integer(2)
  Local t%(2)
  If n% > 0 Then
    t%() = rdepth(n% - 1)
    t%(0) = t%(0) + 1
  EndIf
  t%(1) = 42
  rdepth() = t%()
End Function

Dim rd%(2)
rd%() = rdepth(5)
report rd%(0) = 5 And rd%(1) = 42, "F04  recursive array function"

' ---------- F05: expression contexts rejected --------------------------
' NOTE: ON ERROR SKIP n counts statements executed INSIDE a called
' function's body too, so it cannot cover errors raised after a function
' call - use ON ERROR IGNORE / ABORT around these instead.
Dim x%
On Error Ignore
x% = seqsq(1)
On Error Abort
reperr "F05a scalar = array function rejected"

On Error Ignore
Print seqsq(1)
On Error Abort
reperr "F05b PRINT array function rejected"

On Error Ignore
x% = seqsq(1) + 1
On Error Abort
reperr "F05c array function in arithmetic rejected"

On Error Ignore
x% = Call("seqsq", 1)
On Error Abort
reperr "F05d CALL() of array function rejected"

' ---------- F06: mismatches vs destination -----------------------------
Dim small%(3)
On Error Ignore
small%() = seqsq(1)
On Error Abort
reperr "F06a count mismatch rejected"

On Error Ignore
ff!() = seqsq(1)
On Error Abort
reperr "F06b float() = integer function rejected"

' ---------- F07: scalar function as array source rejected ---------------
Function sca%(n%)
  sca% = n%
End Function

On Error Ignore
b%() = sca%(5)
On Error Abort
reperr "F07  scalar function as array source rejected"

' ---------- F08: illegal return types rejected at call ------------------
Function bads() As String(3)
  bads(0) = "x"
End Function

Dim s3$(3)
On Error Ignore
s3$() = bads()
On Error Abort
reperr "F08  AS STRING(n) rejected"

' ---------- F09: scalar functions still work (regression) ---------------
Function scb!(n%)
  scb! = n% / 2
End Function
report sca%(9) = 9 And scb!(9) = 4.5, "F09  scalar functions unchanged"

' ---------- summary ------------------------------------------------------
Print
Print "----------------------------------------"
Print tests%; " tests, "; fails%; " failed";
If fails% = 0 Then Print "  *** ALL PASS ***" Else Print "  *** FAILURES ***"
