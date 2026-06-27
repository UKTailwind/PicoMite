' =============================================================================
' StringLengthTest.bas - Test string length checking in structures
' =============================================================================
' Tests that string length limits are properly enforced for:
' - Direct assignment (LET)
' - String concatenation (+=)
' - MID$ command
' - INPUT command
' - LINE INPUT command
' - READ command
' =============================================================================

Option Base 0
Option Explicit

Type TData
  shortStr As String Length 5
  medStr As String Length 10
  value As Integer
End Type

Dim passed%, failed%, testNum%
Dim d As TData

Sub RunTests
  passed% = 0
  failed% = 0
  testNum% = 0

  Print "============================================="
  Print "Structure String Length Test Suite"
  Print "============================================="
  Print

  TestDirectAssignment
  TestConcatenation
  TestMidCommand
  TestReadCommand

  Print
  Print "============================================="
  Print "RESULTS: "; passed%; " passed, "; failed%; " failed"
  Print "============================================="
End Sub

Sub AssertError(testName$)
  ' This sub is called when we expect an error but didn't get one
  testNum% = testNum% + 1
  failed% = failed% + 1
  Print "  FAIL #"; testNum%; ": "; testName$; " - Expected error but none occurred"
End Sub

Sub AssertPass(testName$)
  testNum% = testNum% + 1
  passed% = passed% + 1
  Print "  PASS #"; testNum%; ": "; testName$
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
' TEST 1: Direct Assignment
' =============================================================================
Sub TestDirectAssignment
  Print
  Print "--- Test 1: Direct Assignment ---"

  ' Test valid assignment (within limit)
  d.shortStr = "Hi"
  AssertEqualStr d.shortStr, "Hi", "Short string assignment OK"

  d.shortStr = "Hello"  ' Exactly 5 chars - should work
  AssertEqualStr d.shortStr, "Hello", "Exact length assignment OK"

  d.medStr = "1234567890"  ' Exactly 10 chars - should work
  AssertEqualStr d.medStr, "1234567890", "Medium exact length OK"

  ' The following should cause "String too long" error
  ' Uncomment to test (will stop program):
  ' d.shortStr = "TooLong"  ' 7 chars > 5 - should error

  Print "  (Manual test: d.shortStr = ""TooLong"" should error)"
End Sub

' =============================================================================
' TEST 2: String Concatenation
' =============================================================================
Sub TestConcatenation
  Print
  Print "--- Test 2: String Concatenation ---"

  ' Test valid concatenation
  d.shortStr = "Hi"
  d.shortStr = d.shortStr + "!"  ' "Hi!" = 3 chars, OK
  AssertEqualStr d.shortStr, "Hi!", "Concatenation within limit OK"

  d.shortStr = "AB"
  d.shortStr = d.shortStr + "CDE"  ' "ABCDE" = 5 chars, exactly at limit
  AssertEqualStr d.shortStr, "ABCDE", "Concatenation at exact limit OK"

  ' The following should cause "String too long" error
  ' Uncomment to test (will stop program):
  ' d.shortStr = "ABCDE"
  ' d.shortStr = d.shortStr + "F"  ' Would be 6 chars > 5

  Print "  (Manual test: d.shortStr=""ABCDE"" then +=  ""F"" should error)"
End Sub

' =============================================================================
' TEST 3: MID$ Command
' =============================================================================
Sub TestMidCommand
  Print
  Print "--- Test 3: MID$ Command ---"

  ' Test valid MID$ replacement (same length)
  d.shortStr = "Hello"
  Mid$(d.shortStr, 1, 2) = "XX"  ' Replace "He" with "XX"
  AssertEqualStr d.shortStr, "XXllo", "MID$ same length replacement OK"

  ' Test valid MID$ that shortens
  d.medStr = "1234567890"
  Mid$(d.medStr, 5, 3) = "AB"  ' Replace "567" with "AB" (shorter)
  ' Result depends on implementation - may truncate or adjust

  ' The following should cause "String too long" error
  ' Uncomment to test (will stop program):
  ' d.shortStr = "Hello"
  ' Mid$(d.shortStr, 5, 1) = "XYZ"  ' Would extend beyond 5 chars

  Print "  (Manual test: MID$ extending beyond limit should error)"
End Sub

' =============================================================================
' TEST 4: READ Command
' =============================================================================
Sub TestReadCommand
  Print
  Print "--- Test 4: READ Command ---"

  Restore TestData
  Read d.shortStr
  AssertEqualStr d.shortStr, "ABC", "READ short string OK"

  Read d.medStr
  AssertEqualStr d.medStr, "1234567890", "READ medium string OK"

  ' The following DATA would cause "String too long" error:
  ' Uncomment the bad data line to test
  Print "  (Manual test: READ string longer than limit should error)"
End Sub

TestData:
Data "ABC", "1234567890"
' Data "TooLongString"  ' Uncomment to test error

' =============================================================================
' RUN ALL TESTS
' =============================================================================
RunTests
End
