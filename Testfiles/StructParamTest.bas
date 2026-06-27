' =============================================================================
' SubParamTest.bas - Comprehensive Subroutine Parameter Passing Test Suite
' =============================================================================
' Tests all variants of passing variables to SUB/FUNCTION including:
' - Simple types (INTEGER, FLOAT, STRING)
' - Arrays of simple types
' - Structure members
' - Structure arrays
' - BYVAL and BYREF modifiers
' - Type conversions (INTEGER <-> FLOAT)
' - STRING LENGTH variations
' - LOCAL and STATIC variables
' - Nested structures
' =============================================================================

Option Base 0
Option Explicit

' -----------------------------------------------------------------------------
' Structure Definitions
' -----------------------------------------------------------------------------
Type TPoint
  x As Integer
  y As Integer
End Type

Type TRect
  topLeft As TPoint
  bottomRight As TPoint
  name As String Length 20
End Type

Type TData
  value As Float
  count As Integer
  label As String Length 30
End Type

Type TArrayStruct
  values(5) As Integer
  names(3) As String Length 15
End Type

' -----------------------------------------------------------------------------
' Global Variables for Testing
' -----------------------------------------------------------------------------
Dim passed%, failed%, testNum%
Dim globalInt%
Dim globalFloat!
Dim globalStr$

' -----------------------------------------------------------------------------
' Main Test Runner
' -----------------------------------------------------------------------------
Sub RunTests
  passed% = 0
  failed% = 0
  testNum% = 0

  Print "============================================="
  Print "Subroutine Parameter Passing Test Suite"
  Print "============================================="
  Print

  TestSimpleTypes
  TestArrayParameters
  TestStructMembers
  TestStructures
  TestByValByRef
  TestTypeConversions
  TestStringLengths
  TestLocalStatic
  TestNestedStructures
  TestEdgeCases
  TestFunctionReturns

  Print
  Print "============================================="
  Print "RESULTS: "; passed%; " passed, "; failed%; " failed"
  Print "============================================="
End Sub

' -----------------------------------------------------------------------------
' Helper: Assert equality
' -----------------------------------------------------------------------------
Sub AssertEqual(actual, expected, testName$)
  testNum% = testNum% + 1
  If actual = expected Then
    passed% = passed% + 1
    Print "  PASS #"; testNum%; ": "; testName$
  Else
    failed% = failed% + 1
    Print "  FAIL #"; testNum%; ": "; testName$
    Print "         Expected: "; expected; " Got: "; actual
  End If
End Sub

Sub AssertEqualStr(actual$, expected$, testName$)
  testNum% = testNum% + 1
  If actual$ = expected$ Then
    passed% = passed% + 1
    Print "  PASS #"; testNum%; ": "; testName$
  Else
    failed% = failed% + 1
    Print "  FAIL #"; testNum%; ": "; testName$
    Print "         Expected: '"; expected$; "' Got: '"; actual$; "'"
  End If
End Sub

' =============================================================================
' TEST 1: Simple Type Parameters
' =============================================================================
Sub TestSimpleTypes
  Local i%, f!, s$

  Print
  Print "--- Test 1: Simple Type Parameters ---"

  ' Test integer parameter
  i% = 42
  TestIntParam i%
  AssertEqual i%, 42, "Integer param unchanged after sub"

  ' Test float parameter
  f! = 3.14159
  TestFloatParam f!
  AssertEqual f!, 3.14159, "Float param unchanged after sub"

  ' Test string parameter
  s$ = "Hello"
  TestStrParam s$
  AssertEqualStr s$, "Hello", "String param unchanged after sub"

  ' Test literal parameters
  TestIntParam 100
  TestFloatParam 2.718
  TestStrParam "World"

  ' Test expression parameters
  i% = 10
  TestIntParam i% * 2 + 5
  TestFloatParam Sin(0.5) + Cos(0.5)
End Sub

Sub TestIntParam p%
  Local x%
  x% = p% + 1
  AssertEqual x%, p% + 1, "Integer param received correctly"
End Sub

Sub TestFloatParam p!
  Local x!
  x! = p! * 2
  AssertEqual x!, p! * 2, "Float param received correctly"
End Sub

Sub TestStrParam p$
  Local x$
  x$ = p$ + "!"
  AssertEqualStr x$, p$ + "!", "String param received correctly"
End Sub

' =============================================================================
' TEST 2: Array Parameters
' =============================================================================
Sub TestArrayParameters
  Local intArr%(10), floatArr!(5), strArr$(3)
  Local i%

  Print
  Print "--- Test 2: Array Parameters ---"

  ' Initialize arrays
  For i% = 0 To 10
    intArr%(i%) = i% * 10
  Next
  For i% = 0 To 5
    floatArr!(i%) = i% * 1.5
  Next
  For i% = 0 To 3
    strArr$(i%) = "Item" + Str$(i%)
  Next

  ' Test passing whole arrays
  TestIntArray intArr%()
  TestFloatArray floatArr!()
  TestStrArray strArr$()

  ' Verify arrays modified by subs
  AssertEqual intArr%(5), 999, "Integer array element modified by sub"
  AssertEqual floatArr!(2), 999.5, "Float array element modified by sub"
  AssertEqualStr strArr$(1), "Modified", "String array element modified by sub"

  ' Test passing array elements (not whole arrays)
  intArr%(0) = 100
  TestIntParam intArr%(0)
  AssertEqual intArr%(0), 100, "Array element unchanged when passed by value"
End Sub

Sub TestIntArray arr%()
  AssertEqual arr%(3), 30, "Integer array received correctly"
  arr%(5) = 999  ' Modify to verify it's by reference
End Sub

Sub TestFloatArray arr!()
  AssertEqual arr!(4), 6.0, "Float array received correctly"
  arr!(2) = 999.5
End Sub

Sub TestStrArray arr$()
  AssertEqualStr arr$(2), "Item2", "String array received correctly"
  arr$(1) = "Modified"
End Sub

' =============================================================================
' TEST 3: Structure Member Parameters
' =============================================================================
Sub TestStructMembers
  Local pt As TPoint
  Local rect As TRect
  Local data As TData

  Print
  Print "--- Test 3: Structure Member Parameters ---"

  ' Initialize structures
  pt.x = 100
  pt.y = 200

  rect.topLeft.x = 10
  rect.topLeft.y = 20
  rect.bottomRight.x = 110
  rect.bottomRight.y = 120
  rect.name = "TestRect"

  data.value = 3.14159
  data.count = 42
  data.label = "TestData"

  ' Test passing struct members as simple parameters
  TestIntParam pt.x
  TestIntParam pt.y
  AssertEqual pt.x, 100, "Struct member pt.x passed correctly"

  TestIntParam rect.topLeft.x
  TestIntParam rect.bottomRight.y
  AssertEqual rect.topLeft.x, 10, "Nested struct member passed correctly"

  TestFloatParam data.value
  AssertEqual data.value, 3.14159, "Float struct member passed correctly"

  TestIntParam data.count
  AssertEqual data.count, 42, "Integer struct member passed correctly"

  TestStrParam data.label
  AssertEqualStr data.label, "TestData", "String struct member passed correctly"

  TestStrParam rect.name
  AssertEqualStr rect.name, "TestRect", "Nested string member passed correctly"

  ' Test modifying struct members through sub
  ModifyStructMember pt.x
  ' Note: pt.x should be unchanged as it's passed by value (expression)
End Sub

Sub ModifyStructMember val%
  val% = 999
End Sub

' =============================================================================
' TEST 4: Whole Structure Parameters
' =============================================================================
Sub TestStructures
  Local pt As TPoint
  Local rect As TRect
  Local pts(5) As TPoint
  Local i%

  Print
  Print "--- Test 4: Whole Structure Parameters ---"

  pt.x = 50
  pt.y = 75

  ' Test passing whole structure
  TestPointStruct pt
  AssertEqual pt.x, 999, "Structure modified by sub (x)"
  AssertEqual pt.y, 888, "Structure modified by sub (y)"

  ' Reset and test structure array
  For i% = 0 To 5
    pts(i%).x = i% * 10
    pts(i%).y = i% * 20
  Next

  TestPointArray pts()
  AssertEqual pts(2).x, 777, "Structure array element modified (x)"
  AssertEqual pts(2).y, 666, "Structure array element modified (y)"

  ' Test rect with nested structures
  rect.topLeft.x = 0
  rect.topLeft.y = 0
  rect.bottomRight.x = 100
  rect.bottomRight.y = 100
  rect.name = "Rectangle"

  TestRectStruct rect
  AssertEqual rect.topLeft.x, 5, "Nested struct modified (topLeft.x)"
  AssertEqualStr rect.name, "Modified", "Struct string member modified"
End Sub

Sub TestPointStruct pt As TPoint
  AssertEqual pt.x, 50, "Struct param received (x)"
  AssertEqual pt.y, 75, "Struct param received (y)"
  pt.x = 999
  pt.y = 888
End Sub

Sub TestPointArray pts() As TPoint
  AssertEqual pts(3).x, 30, "Struct array element received (x)"
  AssertEqual pts(3).y, 60, "Struct array element received (y)"
  pts(2).x = 777
  pts(2).y = 666
End Sub

Sub TestRectStruct r As TRect
  AssertEqual r.topLeft.x, 0, "Nested struct received (topLeft.x)"
  AssertEqual r.bottomRight.x, 100, "Nested struct received (bottomRight.x)"
  r.topLeft.x = 5
  r.name = "Modified"
End Sub

' =============================================================================
' TEST 5: BYVAL and BYREF Modifiers
' =============================================================================
Sub TestByValByRef
  Local i%, f!, s$

  Print
  Print "--- Test 5: BYVAL and BYREF Modifiers ---"

  ' Test BYVAL - should NOT modify original
  i% = 100
  TestByValInt i%
  AssertEqual i%, 100, "BYVAL integer not modified"

  f! = 3.14
  TestByValFloat f!
  AssertEqual f!, 3.14, "BYVAL float not modified"

  s$ = "Original"
  TestByValStr s$
  AssertEqualStr s$, "Original", "BYVAL string not modified"

  ' Test BYREF - SHOULD modify original
  i% = 100
  TestByRefInt i%
  AssertEqual i%, 999, "BYREF integer modified"

  f! = 3.14
  TestByRefFloat f!
  AssertEqual f!, 999.99, "BYREF float modified"

  s$ = "Original"
  TestByRefStr s$
  AssertEqualStr s$, "Changed", "BYREF string modified"
End Sub

Sub TestByValInt ByVal p%
  p% = 999
End Sub

Sub TestByValFloat ByVal p!
  p! = 999.99
End Sub

Sub TestByValStr ByVal p$
  p$ = "Changed"
End Sub

Sub TestByRefInt ByRef p%
  p% = 999
End Sub

Sub TestByRefFloat ByRef p!
  p! = 999.99
End Sub

Sub TestByRefStr ByRef p$
  p$ = "Changed"
End Sub

' =============================================================================
' TEST 6: Type Conversions
' =============================================================================
Sub TestTypeConversions
  Local i%, f!

  Print
  Print "--- Test 6: Type Conversions ---"

  ' Pass integer to float parameter
  i% = 42
  TestFloatFromInt i%

  ' Pass float to integer parameter
  f! = 3.7
  TestIntFromFloat f!

  ' Pass expressions with mixed types
  i% = 10
  f! = 2.5
  TestFloatParam i% * f!
  TestIntParam Int(f! * 10)
End Sub

Sub TestFloatFromInt p!
  AssertEqual p!, 42.0, "Integer converted to float param"
End Sub

Sub TestIntFromFloat p%
  AssertEqual p%, 4, "Float converted to integer param (rounded)"
End Sub

' =============================================================================
' TEST 7: String Length Variations
' =============================================================================
Sub TestStringLengths
  Local shortStr$ Length 10
  Local longStr$ Length 100
  Local data As TData  ' has label As String Length 30

  Print
  Print "--- Test 7: String Length Variations ---"

  shortStr$ = "Short"
  longStr$ = "This is a much longer string for testing"
  data.label = "StructString"

  TestStrParam shortStr$
  TestStrParam longStr$
  TestStrParam data.label

  ' Test string truncation in fixed-length fields
  TestFixedLengthStr shortStr$
  AssertEqualStr shortStr$, "Short", "Short string preserved"

  ' Test struct string member
  TestStructStrMember data
  AssertEqualStr data.label, "NewLabel", "Struct string member modified"
End Sub

Sub TestFixedLengthStr s$
  Local temp$
  temp$ = s$ + " test"
  AssertEqualStr temp$, s$ + " test", "Fixed length string concatenated"
End Sub

Sub TestStructStrMember d As TData
  AssertEqualStr d.label, "StructString", "Struct string member received"
  d.label = "NewLabel"
End Sub

' =============================================================================
' TEST 8: LOCAL and STATIC Variables in Subs
' =============================================================================
Sub TestLocalStatic
  Local i%

  Print
  Print "--- Test 8: LOCAL and STATIC Variables ---"

  ' Test that STATIC persists across calls
  For i% = 1 To 3
    TestStaticCounter
  Next

  ' Test LOCAL variables are isolated
  globalInt% = 100
  TestLocalScope
  AssertEqual globalInt%, 100, "Global unchanged by local of same name"

  ' Test STATIC with structures
  TestStaticStruct
  TestStaticStruct
  TestStaticStruct
End Sub

Sub TestStaticCounter
  Static count%
  count% = count% + 1
  AssertEqual count%, count%, "Static counter increments: " + Str$(count%)
End Sub

Sub TestLocalScope
  Local globalInt%  ' Shadows global
  globalInt% = 999
  AssertEqual globalInt%, 999, "Local shadows global correctly"
End Sub

Sub TestStaticStruct
  Static pt As TPoint
  pt.x = pt.x + 10
  pt.y = pt.y + 20
  AssertEqual pt.x, pt.x, "Static struct.x = " + Str$(pt.x)
  AssertEqual pt.y, pt.y, "Static struct.y = " + Str$(pt.y)
End Sub

' =============================================================================
' TEST 9: Nested Structure Access
' =============================================================================
Sub TestNestedStructures
  Local rect As TRect
  Local rects(3) As TRect
  Local i%

  Print
  Print "--- Test 9: Nested Structure Access ---"

  rect.topLeft.x = 10
  rect.topLeft.y = 20
  rect.bottomRight.x = 110
  rect.bottomRight.y = 120
  rect.name = "MainRect"

  ' Pass nested struct members
  TestIntParam rect.topLeft.x
  TestIntParam rect.bottomRight.y
  TestStrParam rect.name

  ' Test with array of structs containing nested structs
  For i% = 0 To 3
    rects(i%).topLeft.x = i% * 10
    rects(i%).topLeft.y = i% * 20
    rects(i%).bottomRight.x = i% * 10 + 100
    rects(i%).bottomRight.y = i% * 20 + 100
    rects(i%).name = "Rect " + Str$(i%)
  Next

  TestRectArray rects()
  AssertEqual rects(1).topLeft.x, 555, "Rect array nested member modified"
End Sub

Sub TestRectArray r() As TRect
  AssertEqual r(2).topLeft.x, 20, "Rect array nested member received"
  AssertEqualStr r(2).name, "Rect 2", "Rect array string member received"
  r(1).topLeft.x = 555
End Sub

' =============================================================================
' TEST 10: Edge Cases
' =============================================================================
Sub TestEdgeCases
  Local i%
  Local arr%(1)
  Local pt As TPoint

  Print
  Print "--- Test 10: Edge Cases ---"

  ' Test zero values
  i% = 0
  TestIntParam i%
  AssertEqual i%, 0, "Zero integer handled"

  pt.x = 0
  pt.y = 0
  TestIntParam pt.x
  AssertEqual pt.x, 0, "Zero struct member handled"

  ' Test negative values
  i% = -100
  TestIntParam i%
  pt.x = -50
  TestIntParam pt.x

  ' Test empty string
  TestStrParam ""

  ' Test single element array
  arr%(0) = 42
  TestSingleElementArray arr%()
End Sub

Sub TestSingleElementArray arr%()
  AssertEqual arr%(0), 42, "Single element array handled"
End Sub

' =============================================================================
' TEST 11: Function Return Values
' =============================================================================
Sub TestFunctionReturns
  Local i%, f!, s$
  Local pt As TPoint

  Print
  Print "--- Test 11: Function Return Values ---"

  i% = GetIntValue(10)
  AssertEqual i%, 20, "Integer function return"

  f! = GetFloatValue(3.14)
  AssertEqual f!, 6.28, "Float function return"

  s$ = GetStrValue("Hello")
  AssertEqualStr s$, "Hello World", "String function return"

  ' Test function with struct member param
  i% = GetIntValue(pt.x)
  AssertEqual i%, 0, "Function with struct member param (zero)"

  pt.x = 25
  i% = GetIntValue(pt.x)
  AssertEqual i%, 50, "Function with struct member param"

  ' Test function returning struct
  pt = GetPoint(100, 200)
  AssertEqual pt.x, 100, "Struct function return (x)"
  AssertEqual pt.y, 200, "Struct function return (y)"
End Sub

Function GetIntValue(p%) As Integer
  GetIntValue = p% * 2
End Function

Function GetFloatValue(p!) As Float
  GetFloatValue = p! * 2
End Function

Function GetStrValue(p$) As String
  GetStrValue = p$ + " World"
End Function

Function GetPoint(x%, y%) As TPoint
  GetPoint.x = x%
  GetPoint.y = y%
End Function

' =============================================================================
' RUN ALL TESTS
' =============================================================================
RunTests
End