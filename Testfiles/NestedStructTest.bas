' =============================================================================
' NestedStructTest.bas - Nested Structure Test Suite
' =============================================================================
' Tests nested structures including:
' - Nested struct member access
' - Nested struct arrays
' - Passing nested struct members to subs
' - Nested struct assignment
' - Deep nesting levels
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

Type TColor
  r As Integer
  g As Integer
  b As Integer
End Type

Type TRect
  topLeft As TPoint
  bottomRight As TPoint
  name As String Length 20
End Type

Type TColoredRect
  rect As TRect
  fillColor As TColor
  borderColor As TColor
  visible As Integer
End Type

Type TWidget
  id As Integer
  bounds As TRect
  foreground As TColor
  background As TColor
  label As String Length 30
End Type

' -----------------------------------------------------------------------------
' Global Variables for Testing
' -----------------------------------------------------------------------------
Dim passed%, failed%, testNum%

' -----------------------------------------------------------------------------
' Main Test Runner
' -----------------------------------------------------------------------------
Sub RunTests
  passed% = 0
  failed% = 0
  testNum% = 0

  Print "============================================="
  Print "Nested Structure Test Suite"
  Print "============================================="
  Print

  TestBasicNestedAccess
  TestNestedAssignment
  TestNestedArrays
  TestNestedParams
  TestDeepNesting
  TestNestedModification
  TestNestedStringMembers
  TestNestedInExpressions

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
' TEST 1: Basic Nested Access
' =============================================================================
Sub TestBasicNestedAccess
  Local rect As TRect
  Local widget As TWidget

  Print
  Print "--- Test 1: Basic Nested Access ---"

  ' Initialize nested struct
  rect.topLeft.x = 10
  rect.topLeft.y = 20
  rect.bottomRight.x = 110
  rect.bottomRight.y = 120
  rect.name = "TestRect"

  ' Verify nested member access
  AssertEqual rect.topLeft.x, 10, "Nested member topLeft.x"
  AssertEqual rect.topLeft.y, 20, "Nested member topLeft.y"
  AssertEqual rect.bottomRight.x, 110, "Nested member bottomRight.x"
  AssertEqual rect.bottomRight.y, 120, "Nested member bottomRight.y"
  AssertEqualStr rect.name, "TestRect", "String member name"

  ' Test widget with multiple nested structs
  widget.id = 1
  widget.bounds.topLeft.x = 0
  widget.bounds.topLeft.y = 0
  widget.bounds.bottomRight.x = 200
  widget.bounds.bottomRight.y = 150
  widget.bounds.name = "WidgetBounds"
  widget.foreground.r = 255
  widget.foreground.g = 255
  widget.foreground.b = 255
  widget.background.r = 0
  widget.background.g = 0
  widget.background.b = 128
  widget.label = "MyWidget"

  AssertEqual widget.id, 1, "Widget id"
  AssertEqual widget.bounds.topLeft.x, 0, "Widget bounds.topLeft.x"
  AssertEqual widget.bounds.bottomRight.y, 150, "Widget bounds.bottomRight.y"
  AssertEqual widget.foreground.r, 255, "Widget foreground.r"
  AssertEqual widget.background.b, 128, "Widget background.b"
  AssertEqualStr widget.label, "MyWidget", "Widget label"
End Sub

' =============================================================================
' TEST 2: Nested Assignment
' =============================================================================
Sub TestNestedAssignment
  Local rect1 As TRect
  Local rect2 As TRect
  Local pt As TPoint

  Print
  Print "--- Test 2: Nested Assignment ---"

  ' Initialize first rect
  rect1.topLeft.x = 5
  rect1.topLeft.y = 10
  rect1.bottomRight.x = 105
  rect1.bottomRight.y = 110
  rect1.name = "Rect1"

  ' Copy nested member to another struct
  rect2.topLeft.x = rect1.topLeft.x
  rect2.topLeft.y = rect1.topLeft.y
  rect2.bottomRight.x = rect1.bottomRight.x + 50
  rect2.bottomRight.y = rect1.bottomRight.y + 50
  rect2.name = "Rect2"

  AssertEqual rect2.topLeft.x, 5, "Copied nested member x"
  AssertEqual rect2.topLeft.y, 10, "Copied nested member y"
  AssertEqual rect2.bottomRight.x, 155, "Modified nested member x"
  AssertEqual rect2.bottomRight.y, 160, "Modified nested member y"

  ' Assign to simple struct from nested
  pt.x = rect1.bottomRight.x
  pt.y = rect1.bottomRight.y
  AssertEqual pt.x, 105, "Simple struct from nested x"
  AssertEqual pt.y, 110, "Simple struct from nested y"

  ' Assign nested from simple struct
  pt.x = 200
  pt.y = 300
  rect1.topLeft.x = pt.x
  rect1.topLeft.y = pt.y
  AssertEqual rect1.topLeft.x, 200, "Nested from simple x"
  AssertEqual rect1.topLeft.y, 300, "Nested from simple y"
End Sub

' =============================================================================
' TEST 3: Nested Arrays
' =============================================================================
Sub TestNestedArrays
  Local rects(5) As TRect
  Local widgets(3) As TWidget
  Local i%

  Print
  Print "--- Test 3: Nested Arrays ---"

  ' Initialize array of rects
  For i% = 0 To 5
    rects(i%).topLeft.x = i% * 10
    rects(i%).topLeft.y = i% * 20
    rects(i%).bottomRight.x = i% * 10 + 100
    rects(i%).bottomRight.y = i% * 20 + 100
    rects(i%).name = "Rect" + Str$(i%)
  Next

  AssertEqual rects(0).topLeft.x, 0, "Array[0] nested x"
  AssertEqual rects(3).topLeft.x, 30, "Array[3] nested x"
  AssertEqual rects(5).bottomRight.y, 200, "Array[5] nested y"
  AssertEqualStr rects(2).name, "Rect2", "Array[2] name"

  ' Initialize array of widgets (deeper nesting)
  For i% = 0 To 3
    widgets(i%).id = i% + 100
    widgets(i%).bounds.topLeft.x = i% * 50
    widgets(i%).bounds.topLeft.y = i% * 50
    widgets(i%).bounds.bottomRight.x = i% * 50 + 200
    widgets(i%).bounds.bottomRight.y = i% * 50 + 150
    widgets(i%).bounds.name = "Widget" + Str$(i%)
    widgets(i%).foreground.r = i% * 50
    widgets(i%).foreground.g = i% * 60
    widgets(i%).foreground.b = i% * 70
    widgets(i%).label = "Label" + Str$(i%)
  Next

  AssertEqual widgets(0).id, 100, "Widget array[0] id"
  AssertEqual widgets(2).bounds.topLeft.x, 100, "Widget array[2] bounds.topLeft.x"
  AssertEqual widgets(3).foreground.r, 150, "Widget array[3] foreground.r"
  AssertEqualStr widgets(1).label, "Label1", "Widget array[1] label"
End Sub

' =============================================================================
' TEST 4: Nested Params to Subs
' =============================================================================
Sub TestNestedParams
  Local rect As TRect
  Local widget As TWidget

  Print
  Print "--- Test 4: Nested Params to Subs ---"

  rect.topLeft.x = 25
  rect.topLeft.y = 35
  rect.bottomRight.x = 125
  rect.bottomRight.y = 135
  rect.name = "ParamRect"

  widget.id = 42
  widget.bounds.topLeft.x = 10
  widget.bounds.topLeft.y = 20
  widget.foreground.r = 128
  widget.foreground.g = 64
  widget.foreground.b = 32

  ' Pass nested members as simple params
  TestIntParam rect.topLeft.x
  TestIntParam rect.bottomRight.y
  TestIntParam widget.bounds.topLeft.x
  TestIntParam widget.foreground.r

  ' Pass whole struct with nested members
  TestRectParam rect
  AssertEqual rect.topLeft.x, 999, "Rect modified by sub (topLeft.x)"

  ' Pass string from nested struct
  TestStrParam rect.name
  TestStrParam widget.bounds.name
End Sub

Sub TestIntParam p%
  AssertEqual p%, p%, "Integer param value: " + Str$(p%)
End Sub

Sub TestStrParam p$
  AssertEqualStr p$, p$, "String param value: " + p$
End Sub

Sub TestRectParam r As TRect
  AssertEqual r.topLeft.x, 25, "Rect param topLeft.x received"
  AssertEqual r.bottomRight.y, 135, "Rect param bottomRight.y received"
  r.topLeft.x = 999
End Sub

' =============================================================================
' TEST 5: Deep Nesting
' =============================================================================
Sub TestDeepNesting
  Local cr As TColoredRect

  Print
  Print "--- Test 5: Deep Nesting ---"

  ' TColoredRect contains TRect which contains TPoint
  cr.rect.topLeft.x = 5
  cr.rect.topLeft.y = 10
  cr.rect.bottomRight.x = 205
  cr.rect.bottomRight.y = 210
  cr.rect.name = "ColoredRect"
  cr.fillColor.r = 255
  cr.fillColor.g = 128
  cr.fillColor.b = 64
  cr.borderColor.r = 0
  cr.borderColor.g = 0
  cr.borderColor.b = 0
  cr.visible = 1

  AssertEqual cr.rect.topLeft.x, 5, "Deep nested topLeft.x"
  AssertEqual cr.rect.topLeft.y, 10, "Deep nested topLeft.y"
  AssertEqual cr.rect.bottomRight.x, 205, "Deep nested bottomRight.x"
  AssertEqual cr.fillColor.r, 255, "Nested color r"
  AssertEqual cr.fillColor.g, 128, "Nested color g"
  AssertEqual cr.borderColor.b, 0, "Nested border color b"
  AssertEqual cr.visible, 1, "Simple member visible"
  AssertEqualStr cr.rect.name, "ColoredRect", "Deep nested string"
End Sub

' =============================================================================
' TEST 6: Nested Modification
' =============================================================================
Sub TestNestedModification
  Local rect As TRect
  Local rects(3) As TRect

  Print
  Print "--- Test 6: Nested Modification ---"

  rect.topLeft.x = 50
  rect.topLeft.y = 60
  rect.bottomRight.x = 150
  rect.bottomRight.y = 160

  ' Modify nested members
  rect.topLeft.x = rect.topLeft.x + 10
  rect.bottomRight.y = rect.bottomRight.y * 2

  AssertEqual rect.topLeft.x, 60, "Modified nested x (+10)"
  AssertEqual rect.bottomRight.y, 320, "Modified nested y (*2)"

  ' Modify in array
  rects(0).topLeft.x = 100
  rects(0).topLeft.y = 200
  rects(1).topLeft.x = rects(0).topLeft.x + 50
  rects(1).topLeft.y = rects(0).topLeft.y + 50

  AssertEqual rects(1).topLeft.x, 150, "Array nested modified x"
  AssertEqual rects(1).topLeft.y, 250, "Array nested modified y"

  ' Swap values using temp
  Local temp%
  temp% = rects(0).topLeft.x
  rects(0).topLeft.x = rects(1).topLeft.x
  rects(1).topLeft.x = temp%

  AssertEqual rects(0).topLeft.x, 150, "Swapped array[0] x"
  AssertEqual rects(1).topLeft.x, 100, "Swapped array[1] x"
End Sub

' =============================================================================
' TEST 7: Nested String Members
' =============================================================================
Sub TestNestedStringMembers
  Local rect As TRect
  Local widget As TWidget
  Local rects(2) As TRect

  Print
  Print "--- Test 7: Nested String Members ---"

  rect.name = "Rectangle1"
  AssertEqualStr rect.name, "Rectangle1", "Simple string in nested struct"

  widget.bounds.name = "WidgetBounds"
  widget.label = "MainWidget"
  AssertEqualStr widget.bounds.name, "WidgetBounds", "Nested struct string"
  AssertEqualStr widget.label, "MainWidget", "Top level string"

  ' String in array of nested structs
  rects(0).name = "First"
  rects(1).name = "Second"
  rects(2).name = "Third"

  AssertEqualStr rects(0).name, "First", "Array[0] nested string"
  AssertEqualStr rects(1).name, "Second", "Array[1] nested string"
  AssertEqualStr rects(2).name, "Third", "Array[2] nested string"

  ' Concatenation with nested string
  Local result$
  result$ = rects(0).name + "-" + rects(1).name
  AssertEqualStr result$, "First-Second", "Concatenated nested strings"

  ' Modify nested string
  rects(1).name = "Modified"
  AssertEqualStr rects(1).name, "Modified", "Modified nested string"
End Sub

' =============================================================================
' TEST 8: Nested in Expressions
' =============================================================================
Sub TestNestedInExpressions
  Local rect As TRect
  Local widget As TWidget
  Local result%
  Local resultF!

  Print
  Print "--- Test 8: Nested in Expressions ---"

  rect.topLeft.x = 10
  rect.topLeft.y = 20
  rect.bottomRight.x = 110
  rect.bottomRight.y = 120

  widget.foreground.r = 100
  widget.foreground.g = 150
  widget.foreground.b = 200

  ' Arithmetic with nested members
  result% = rect.topLeft.x + rect.topLeft.y
  AssertEqual result%, 30, "Sum of nested members"

  result% = rect.bottomRight.x - rect.topLeft.x
  AssertEqual result%, 100, "Difference of nested members"

  result% = rect.topLeft.x * rect.topLeft.y
  AssertEqual result%, 200, "Product of nested members"

  ' Nested in function calls
  result% = Abs(rect.topLeft.x - rect.bottomRight.x)
  AssertEqual result%, 100, "Abs() with nested members"

  result% = Max(rect.topLeft.x, rect.topLeft.y)
  AssertEqual result%, 20, "Max() with nested members"

  result% = Min(rect.bottomRight.x, rect.bottomRight.y)
  AssertEqual result%, 110, "Min() with nested members"

  ' Nested in comparisons
  If rect.topLeft.x < rect.bottomRight.x Then
    AssertEqual 1, 1, "Nested comparison (less than)"
  Else
    AssertEqual 0, 1, "Nested comparison failed"
  End If

  ' RGB calculation
  result% = widget.foreground.r + widget.foreground.g + widget.foreground.b
  AssertEqual result%, 450, "Sum of RGB nested members"

  ' Average
  resultF! = (widget.foreground.r + widget.foreground.g + widget.foreground.b) / 3.0
  AssertEqual resultF!, 150.0, "Average of RGB nested members"
End Sub

' =============================================================================
' RUN ALL TESTS
' =============================================================================
RunTests
End
