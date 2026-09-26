' proftest.bas - OPTION PROFILING still reports per-SUB times after the trace
' cache went, and a DO WHILE loop's speed (the trace cache's DO fast path
' compiled "DO WHILE var < const" into a direct compare).
'   DOLOOP  ms for 100,000 passes of DO WHILE i% < 100000 : INC i% : LOOP
Dim i%, n%
Timer = 0
Do While i% < 100000
  Inc i%
Loop
Print "DOLOOP"; Timer
Option Profiling On
For n% = 1 To 200
  Outer
Next
End

Sub Outer
  Local a%
  For a% = 1 To 10
    Inner a%
  Next
End Sub

Sub Inner x%
  Local b% = x% * 2
End Sub
