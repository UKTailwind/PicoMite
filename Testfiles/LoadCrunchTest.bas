' LoadCrunchTest.bas - subject file for LOAD ,C
' Load this twice:   LOAD "LoadCrunchTest.bas"      then LIST  (comments present)
'                    LOAD "LoadCrunchTest.bas",C    then LIST  (comments gone)

' --- comments and blank lines must disappear -------------------------

Print "current file: " + MM.Info(current)   ' the '#hdr must survive the crunch

    ' an indented comment
        Print "indented code keeps working"

' --- quoted text must be preserved exactly ---------------------------
Print "two  spaces>  <and a quote ' inside"
Print "a", "b",  "c"

' --- REM is not stripped (same as AUTOSAVE C) ------------------------
Rem this REM line survives crunching

Dim i%
For i% = 1 To 3            ' trailing comment
  Print "loop", i%
Next i%

Print "done"
