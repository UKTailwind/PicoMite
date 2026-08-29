' ArrayCopyStructTest.bas - whole-array assignment with structures (Phase 1)
' Requires a build with structure support (RP2350 variants).
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

Type TPoint
  x As Integer
  y As Integer
End Type

Type TReading
  temp As Float
  values(9) As Float
End Type

' ---------- S01: struct array copy via LET ---------------------------
Dim pa(3) As TPoint, pb(3) As TPoint
Dim i%
For i% = 0 To 3
  pa(i%).x = i% * 10 : pa(i%).y = i% * 100
Next
pb() = pa()
report pb(2).x = 20 And pb(3).y = 300, "S01a struct array copy"
pa(2).x = 999
report pb(2).x = 20, "S01b copy is by value"

' ---------- S02: mismatches rejected ----------------------------------
Dim ra(3) As TReading
On Error Skip 1
pb() = ra()
reperr "S02a different structure types rejected"

Dim pshort(1) As TPoint
On Error Skip 1
pshort() = pa()
reperr "S02b struct array count mismatch rejected"

Dim n%(3)
On Error Skip 1
n%() = pa()
reperr "S02c integer() = struct() rejected"

' ---------- S03: struct member arrays rejected ------------------------
Dim f!(9)
For i% = 0 To 9 : f!(i%) = i% : Next
On Error Skip 1
ra(0).values() = f!()
reperr "S03a dest struct member array rejected"

On Error Skip 1
f!() = ra(0).values()
reperr "S03b source struct member array rejected"

' ---------- S04: STRUCT COPY regression --------------------------------
Dim pc(3) As TPoint
Struct Copy pa() To pc()
report pc(3).x = 30, "S04  STRUCT COPY still works"

' ---------- summary ----------------------------------------------------
Print
Print "----------------------------------------"
Print tests%; " tests, "; fails%; " failed";
If fails% = 0 Then Print "  *** ALL PASS ***" Else Print "  *** FAILURES ***"
