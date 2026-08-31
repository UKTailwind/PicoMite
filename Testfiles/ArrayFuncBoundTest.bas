' ArrayFuncBoundTest.bas - array functions whose return dimensions are taken
' from their arguments, and whose result is filled by an array-taking command.
' Conformance test for all build variants (uses OPTION BASE 1).
Option Explicit
Option Base 1

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

' The inverse of whatever size matrix it is given: the return dimensions
' come from the parameter and the result is filled by MATH, not by
' element writes.
Function INVR(N()) As Float(Bound(N(),1), Bound(N(),2))
  Math M_Inverse N(), INVR()
End Function

' A ramp 1..n: the return dimension comes from a scalar parameter.
Function Ramp(n%) As Integer(n%)
  Local k%
  For k% = 1 To n% : Ramp(k%) = k% : Next k%
End Function

' ---------- B01: 3x3 inverse, dims from BOUND of the parameter ------------
Dim A(3,3) = (4,5,6,2,8,5,7,2,1)
Dim C(3,3), Ident(3,3)
Dim rr%, cc%, ok%
C() = INVR(A())
Math M_Mult A(), C(), Ident()
ok% = 1
For rr% = 1 To 3
  For cc% = 1 To 3
    If Abs(Ident(rr%,cc%) - (rr% = cc%)) > 1e-9 Then ok% = 0
  Next cc%
Next rr%
report ok%, "B01  3x3 inverse filled by MATH into the result (A*inv(A) = I)"

' ---------- B02: the same function returns a 2x2 for a 2x2 argument -------
Dim B(2,2) = (2,0,0,4)
Dim D(2,2)
D() = INVR(B())
ok% = Abs(D(1,1) - 0.5) < 1e-9 And Abs(D(2,2) - 0.25) < 1e-9
ok% = ok% And Abs(D(1,2)) < 1e-9 And Abs(D(2,1)) < 1e-9
report ok%, "B02  return dims follow the argument (2x2 through the same function)"

' ---------- B03: dimension from a scalar parameter -------------------------
Dim r5%(5), r8%(8)
r5%() = Ramp(5)
r8%() = Ramp(8)
report r5%(1) = 1 And r5%(5) = 5 And r8%(8) = 8, "B03  return dim from a scalar parameter (5 then 8)"

' ---------- B04: a size mismatch is still rejected -------------------------
Dim E(2,2)
On Error Ignore
E() = INVR(A())
On Error Abort
report MM.ErrNo <> 0, "B04  3x3 result into a 2x2 rejected  [" + MM.ErrMsg$ + "]"
On Error Clear

' ---------- B05: wrong scalar-derived size rejected too --------------------
On Error Ignore
r5%() = Ramp(6)
On Error Abort
report MM.ErrNo <> 0, "B05  Ramp(6) into a 5 element array rejected  [" + MM.ErrMsg$ + "]"
On Error Clear

' ---------- summary ----------
Print
Print "----------------------------------------"
Print tests%; " tests, "; fails%; " failed";
If fails% = 0 Then Print "  *** ALL PASS ***" Else Print "  *** FAILURES ***"
