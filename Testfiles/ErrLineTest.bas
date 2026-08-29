' ErrLineTest.bas - MM.ERRLINE (line number of the last error)
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

' E01: fresh RUN starts clear
report MM.ERRLINE = 0 And MM.ERRNO = 0, "E01  clear at start of RUN"

' E02: trapped error records its line
Dim x%
On Error Skip 1
x% = 1 \ 0                                     ' <-- this is line 23
report MM.ERRNO <> 0 And MM.ERRLINE = 23, "E02  ON ERROR SKIP records line 23 (got " + Str$(MM.ERRLINE) + ")"
On Error Clear

' E03: ON ERROR CLEAR resets it
report MM.ERRLINE = 0, "E03  ON ERROR CLEAR resets MM.ERRLINE"

' E04: second error updates the line
On Error Ignore
x% = 1 \ 0                                     ' <-- this is line 32
On Error Abort
report MM.ERRLINE = 32, "E04  second error updates line (got " + Str$(MM.ERRLINE) + ")"
On Error Clear

' E05: error inside a subroutine reports the line in the sub
Sub blowup
  Local y%
  On Error Skip 1
  y% = 1 \ 0                                   ' <-- this is line 41
End Sub
blowup
report MM.ERRLINE = 41, "E05  error in a SUB records the SUB's line (got " + Str$(MM.ERRLINE) + ")"
On Error Clear

' ---------- summary ----------
Print
Print "----------------------------------------"
Print tests%; " tests, "; fails%; " failed";
If fails% = 0 Then Print "  *** ALL PASS ***" Else Print "  *** FAILURES ***"
