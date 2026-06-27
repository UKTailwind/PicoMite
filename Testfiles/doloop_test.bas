' doloop_test.bas - Comprehensive DO loop test
' Tests all variants: DO WHILE, DO UNTIL, LOOP WHILE, LOOP UNTIL,
' bare DO...LOOP, EXIT DO, nested loops, edge cases.

Dim integer i, j, k, cnt, passed, failed
passed = 0 : failed = 0

' ---- Test 1: DO...LOOP UNTIL (post-condition; body runs at least once) ----
i = 0 : cnt = 0
Do
  Inc i : Inc cnt
Loop Until i = 5
check 1, (cnt = 5 And i = 5), "DO...LOOP UNTIL"

' ---- Test 2: DO...LOOP WHILE (post-condition) ----
i = 0 : cnt = 0
Do
  Inc i : Inc cnt
Loop While i < 5
check 2, (cnt = 5 And i = 5), "DO...LOOP WHILE"

' ---- Test 3: DO UNTIL...LOOP (pre-condition, normal count) ----
i = 0 : cnt = 0
Do Until i = 5
  Inc i : Inc cnt
Loop
check 3, (cnt = 5 And i = 5), "DO UNTIL...LOOP"

' ---- Test 4: DO WHILE...LOOP (pre-condition, normal count) ----
i = 0 : cnt = 0
Do While i < 5
  Inc i : Inc cnt
Loop
check 4, (cnt = 5 And i = 5), "DO WHILE...LOOP"

' ---- Test 5: DO WHILE false on entry - body must not execute ----
i = 10 : cnt = 0
Do While i < 5
  Inc cnt
Loop
check 5, (cnt = 0 And i = 10), "DO WHILE (false on entry, body skipped)"

' ---- Test 6: DO UNTIL true on entry - body must not execute ----
i = 10 : cnt = 0
Do Until i = 10
  Inc cnt
Loop
check 6, (cnt = 0 And i = 10), "DO UNTIL (true on entry, body skipped)"

' ---- Test 7: DO...LOOP WHILE false - post-condition runs at least once ----
cnt = 0
Do
  Inc cnt
Loop While 1 = 0
check 7, (cnt = 1), "DO...LOOP WHILE (false: executes once)"

' ---- Test 8: DO...LOOP UNTIL true - post-condition runs at least once ----
cnt = 0
Do
  Inc cnt
Loop Until 1 = 1
check 8, (cnt = 1), "DO...LOOP UNTIL (true: executes once)"

' ---- Test 9: EXIT DO from bare DO...LOOP ----
i = 0 : cnt = 0
Do
  Inc i : Inc cnt
  If i = 3 Then Exit Do
Loop
check 9, (cnt = 3 And i = 3), "EXIT DO from DO...LOOP"

' ---- Test 10: EXIT DO from DO WHILE...LOOP ----
i = 0 : cnt = 0
Do While i < 100
  Inc i : Inc cnt
  If i = 3 Then Exit Do
Loop
check 10, (cnt = 3 And i = 3), "EXIT DO from DO WHILE...LOOP"

' ---- Test 11: EXIT DO from DO UNTIL...LOOP ----
i = 0 : cnt = 0
Do Until i = 100
  Inc i : Inc cnt
  If i = 3 Then Exit Do
Loop
check 11, (cnt = 3 And i = 3), "EXIT DO from DO UNTIL...LOOP"

' ---- Test 12: EXIT DO from DO...LOOP WHILE ----
i = 0 : cnt = 0
Do
  Inc i : Inc cnt
  If i = 3 Then Exit Do
Loop While i < 100
check 12, (cnt = 3 And i = 3), "EXIT DO from DO...LOOP WHILE"

' ---- Test 13: EXIT DO from DO...LOOP UNTIL ----
i = 0 : cnt = 0
Do
  Inc i : Inc cnt
  If i = 3 Then Exit Do
Loop Until i = 100
check 13, (cnt = 3 And i = 3), "EXIT DO from DO...LOOP UNTIL"

' ---- Test 14: Nested DO loops; EXIT DO exits innermost only ----
i = 0 : j = 0 : cnt = 0
Do While i < 3
  Inc i : j = 0
  Do
    Inc j : Inc cnt
    If j = 2 Then Exit Do
  Loop
Loop
check 14, (i = 3 And j = 2 And cnt = 6), "Nested DO, EXIT DO exits innermost"

' ---- Test 15: DO UNTIL nested inside DO WHILE ----
i = 0 : cnt = 0
Do While i < 3
  Inc i : j = 0
  Do Until j = 2
    Inc j : Inc cnt
  Loop
Loop
check 15, (i = 3 And j = 2 And cnt = 6), "DO WHILE nesting DO UNTIL"

' ---- Test 16: DO...LOOP WHILE nested inside DO...LOOP UNTIL ----
i = 0 : cnt = 0
Do
  Inc i : j = 0
  Do
    Inc j : Inc cnt
  Loop While j < 2
Loop Until i = 3
check 16, (i = 3 And j = 2 And cnt = 6), "DO...LOOP UNTIL nesting DO...LOOP WHILE"

' ---- Test 17: Three levels of nesting with EXIT DO on middle level ----
cnt = 0
i = 0
Do While i < 2           ' outer: 2 passes
  Inc i : j = 0
  Do Until j = 5         ' middle: exit early at j=3
    Inc j
    k = 0
    Do                   ' inner: 2 iterations each time
      Inc k : Inc cnt
      If k = 2 Then Exit Do
    Loop
    If j = 3 Then Exit Do
  Loop
Loop
' outer 2x, middle exits at j=3 (iterations 1,2,3), inner runs 2 each = 2*3*2=12
check 17, (i = 2 And j = 3 And cnt = 12), "Three-level nesting with EXIT DO on middle"

' ---- Test 18: Loop variable correct after EXIT DO then new loop ----
i = 0
Do
  Inc i
  If i = 5 Then Exit Do
Loop
j = 0
Do
  Inc j
Loop Until j = 3
check 18, (i = 5 And j = 3), "Loop vars correct after EXIT DO + new loop"

' ---- Test 19: Float variable in DO WHILE...LOOP ----
Dim float x
x = 0.0 : cnt = 0
Do While x < 4.0
  x = x + 1.0 : Inc cnt
Loop
check 19, (cnt = 4 And x = 4.0), "DO WHILE float condition"

' ---- Test 20: Float variable in DO...LOOP UNTIL ----
x = 0.0 : cnt = 0
Do
  x = x + 1.0 : Inc cnt
Loop Until x >= 4.0
check 20, (cnt = 4 And x = 4.0), "DO...LOOP UNTIL float condition"

' ---- Test 21: Large count DO...LOOP UNTIL ----
i = 0
Do
  Inc i
Loop Until i = 100000
check 21, (i = 100000), "DO...LOOP UNTIL 100000 iterations"

' ---- Test 22: Large count DO WHILE...LOOP ----
i = 0
Do While i < 100000
  Inc i
Loop
check 22, (i = 100000), "DO WHILE...LOOP 100000 iterations"

' ---- Test 23: Large count DO UNTIL...LOOP ----
i = 0
Do Until i = 100000
  Inc i
Loop
check 23, (i = 100000), "DO UNTIL...LOOP 100000 iterations"

' ---- Summary ----
Print
Print "Results:"; passed; "passed,"; failed; "failed"
If failed = 0 Then
  Print "ALL TESTS PASSED"
Else
  Print "*** FAILURES DETECTED ***"
End If
End

Sub check(tnum As Integer, ok As Integer, desc As String)
  If ok Then
    Print "Test"; tnum; " PASS  "; desc
    Inc passed
  Else
    Print "Test"; tnum; " FAIL  "; desc
    Inc failed
  End If
End Sub
