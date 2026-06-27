' StackRecursionTest.bas
' Test program to explore recursion limits in MMBasic
' 
' MMBasic has TWO separate limits affecting recursion:
' 1. MAXGOSUB (~50) - limits nested SUB/FUN call depth
' 2. MAXLOCALVARS (~240-256) - limits TOTAL local variables across ALL levels
'
' Test 1 (no locals) hits MAXGOSUB first
' Tests 2-4 (with locals) hit MAXLOCALVARS first
' Test 5 (Fibonacci) works because max depth ~20 stays within both limits

Option Explicit
Option Default None

Dim Integer max_depth, target_depth
Dim Integer start_time, end_time

Print "=== MMBasic Recursion Limits Test ==="
Print
Print "Two limits affect recursion:"
Print "  1. MAXGOSUB (~50) - nested call depth"
Print "  2. MAXLOCALVARS (~240) - total local vars across all levels"
Print

' Test 1: Simple recursion depth test
Print "Test 1: No local vars - hits MAXGOSUB limit"
max_depth = 0
On Error Skip 1
TestMinimalRecursion(1)
Print "  Depth reached: "; max_depth; " (limited by MAXGOSUB ~50)"
Print

' Test 2: Recursion with local variables  
Print "Test 2: 4 local integers - hits MAXLOCALVARS limit"
max_depth = 0
On Error Skip 1
TestWithLocals(1)
Print "  Depth reached: "; max_depth; " (limited by MAXLOCALVARS/4 ~60)"
Print

' Test 3: Recursion with float variables
Print "Test 3: 3 local floats - hits MAXLOCALVARS limit"
max_depth = 0
On Error Skip 1
TestWithFloats(1)
Print "  Depth reached: "; max_depth; " (limited by MAXLOCALVARS/3 ~80)"
Print

' Test 4: Recursion with string variables
Print "Test 4: 2 local strings - hits MAXLOCALVARS limit"
max_depth = 0
On Error Skip 1
TestWithStrings(1)
Print "  Depth reached: "; max_depth; " (limited by MAXLOCALVARS/2 ~120)"
Print

' Test 5: Fibonacci - classic exponential recursion
Print "Test 5: Fibonacci(20) - exponential recursion"
Print "  (Max depth ~20, but millions of calls)"
start_time = Timer
Dim Integer result
result = Fibonacci(20)
end_time = Timer
Print "  Fibonacci(20) = "; result
Print "  Time: "; end_time - start_time; " ms"
Print

' Test 6: Safe depth iteration test
Print "Test 6: Recursion timing at depth 40"
start_time = Timer
Dim Integer i
For i = 1 To 1000
  max_depth = 0
  RecurseToDepth(40)
Next i
end_time = Timer
Print "  1000 iterations of 40-deep recursion"
Print "  Time: "; end_time - start_time; " ms"
Print

Print "=== Test Complete ==="
Print
Print "Summary of limits:"
Print "  - No locals: ~50 depth (MAXGOSUB limit)"
Print "  - With N locals per call: ~min(50, 240/N) depth"
Print "  - Fibonacci(20) works: max depth 20, within both limits"
End

' Minimal recursion - just tracks depth
Sub TestMinimalRecursion(depth As Integer)
  If depth > max_depth Then max_depth = depth
  TestMinimalRecursion(depth + 1)
End Sub

' Recursion with local integer variables
Sub TestWithLocals(depth As Integer)
  Local Integer a, b, c, d
  a = depth : b = depth * 2 : c = depth * 3 : d = depth * 4
  If depth > max_depth Then max_depth = depth
  TestWithLocals(depth + 1)
End Sub

' Recursion with local float variables
Sub TestWithFloats(depth As Integer)
  Local Float x, y, z
  x = depth * 1.1 : y = depth * 2.2 : z = depth * 3.3
  If depth > max_depth Then max_depth = depth
  TestWithFloats(depth + 1)
End Sub

' Recursion with local string variables
Sub TestWithStrings(depth As Integer)
  Local String s1$, s2$
  s1$ = "Depth=" + Str$(depth)
  s2$ = "Test string at level " + Str$(depth)
  If depth > max_depth Then max_depth = depth
  TestWithStrings(depth + 1)
End Sub

' Recurse to a specific depth then return
Sub RecurseToDepth(target As Integer)
  max_depth = max_depth + 1
  If max_depth < target Then RecurseToDepth(target)
End Sub

' Classic Fibonacci - demonstrates exponential recursion
' Max depth equals n, but total calls is exponential
Function Fibonacci(n As Integer) As Integer
  If n <= 1 Then
    Fibonacci = n
  Else
    Fibonacci = Fibonacci(n - 1) + Fibonacci(n - 2)
  EndIf
End Function
