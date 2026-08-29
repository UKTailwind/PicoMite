' ArrayCopyTest.bas - whole-array assignment  a() = b()  (Phase 1)
' Runs on every build variant. Prints PASS/FAIL per test and a summary.
' To also verify OPTION BASE 1, change line 5 and run again.
Option Explicit
Option Base 0

Dim tests% = 0, fails% = 0

' ---------- helpers -------------------------------------------------
Sub report chk%, msg$
  tests% = tests% + 1
  If chk% Then
    Print "PASS  "; msg$
  Else
    fails% = fails% + 1
    Print "FAIL  "; msg$
  EndIf
End Sub

' expects MM.ERRNO <> 0 after an ON ERROR SKIP'd statement
Sub reperr msg$
  report MM.ERRNO <> 0, msg$ + "  [" + MM.ERRMSG$ + "]"
  On Error Clear
End Sub

' ---------- T01: integer, float, string 1-D exact copy --------------
Dim a%(5), b%(5), i%
For i% = 0 To 5 : a%(i%) = i% * i% : b%(i%) = -1 : Next
b%() = a%()
report b%(0) = 0 And b%(3) = 9 And b%(5) = 25, "T01a integer 1-D copy"
a%(3) = 999
report b%(3) = 9, "T01b copy is by value (source edit does not affect dest)"

Dim af!(4), bf!(4)
For i% = 0 To 4 : af!(i%) = i% + 0.5 : Next
bf!() = af!()
report bf!(0) = 0.5 And bf!(4) = 4.5, "T01c float 1-D copy"

Dim srcs$(3), dsts$(3)
srcs$(0) = "alpha" : srcs$(3) = "delta"
dsts$() = srcs$()
report dsts$(0) = "alpha" And dsts$(3) = "delta" And dsts$(1) = "", "T01d string 1-D copy"

' ---------- T02: 2-D exact and reshape ------------------------------
Dim m%(1, 2), n%(1, 2), flat%(5)
For i% = 0 To 5 : m%(i% Mod 2, i% \ 2) = 100 + i% : Next
n%() = m%()
report n%(1, 2) = 105 And n%(0, 0) = 100, "T02a 2-D exact copy"
flat%() = m%()
report flat%(0) = 100 And flat%(5) = 105, "T02b reshape 2x3 -> 6 (shape-agnostic)"

' ---------- T03: rejected mismatches --------------------------------
Dim short%(3)
On Error Skip 1
short%() = a%()
reperr "T03a count mismatch rejected"

On Error Skip 1
bf!() = a%()
reperr "T03b float() = integer() rejected"

Dim ls$(3) Length 10, ms$(3) Length 20
On Error Skip 1
ls$() = ms$()
reperr "T03c string LENGTH mismatch rejected"

Dim es$(3) Length 10
es$(2) = "ten chars!"
ls$() = es$()
report ls$(2) = "ten chars!", "T03d string copy with matching LENGTH works"

' ---------- T04: undeclared names, no stub left behind --------------
On Error Skip 1
nosuchdst%() = a%()
reperr "T04a undeclared destination rejected"
Dim nosuchdst%(5)
report 1, "T04b no half-created stub (DIM after failure works)"

On Error Skip 1
a%() = nosuchsrc%()
reperr "T04c undeclared source rejected"

' ---------- T05: LOCAL and byref parameter arrays --------------------
Sub subcopy dst%(), src%()
  dst%() = src%()
End Sub

Dim c%(5)
subcopy c%(), a%()
report c%(5) = 25 And c%(3) = 999, "T05a copy via byref parameters"

subcopy a%(), a%()
report a%(5) = 25, "T05b aliased self-assign via parameters is a no-op"

a%() = a%()
report a%(3) = 999, "T05c direct self-copy is a no-op"

Sub localtest src%()
  Local t%(5)
  t%() = src%()
  report t%(5) = 25, "T05d copy into LOCAL array"
End Sub
localtest a%()

' ---------- T06: rejected forms --------------------------------------
On Error Skip 1
a%() = 5
reperr "T06a a() = scalar rejected"

On Error Skip 1
a%() = b%() + 1
reperr "T06b array arithmetic rejected"

Dim x%
On Error Skip 1
x% = a%()
reperr "T06c scalar = whole array rejected"

' ---------- T07: scalar LET regression -------------------------------
Dim s$
x% = 42 : s$ = "hello" : a%(1) = 77
report x% = 42 And s$ = "hello" And a%(1) = 77, "T07  scalar and element LET unchanged"

' ---------- summary ---------------------------------------------------
Print
Print "----------------------------------------"
Print tests%; " tests, "; fails%; " failed";
If fails% = 0 Then Print "  *** ALL PASS ***" Else Print "  *** FAILURES ***"
