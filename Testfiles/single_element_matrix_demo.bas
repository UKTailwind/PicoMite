' ============================================================================
' Single element array dimensions - matrix multiplication demo
'
' Every DIM in this program that has a 0 as one of its bounds was ILLEGAL
' before this change: MMBasic required every dimension to hold at least two
' elements, so "Dimensions" was raised for DIM a(2,0), DIM b(0,2), DIM c(0,0)
' and so on.  That made a whole class of ordinary linear algebra impossible to
' express, because a row vector, a column vector and a 1x1 scalar result all
' need a dimension of exactly one element.
'
' Arrays are indexed a(column, row).  MATH M_MULT requires
' columns(A) = rows(B), and produces C with rows(A) x columns(B).
' ============================================================================

Option base 0
Dim integer pass = 0, fail = 0

Print "Single element dimensions - matrix multiplication"
Print "================================================="
Print

' ----------------------------------------------------------------------------
' 1. Dot product:  (1 x 3) . (3 x 1)  ->  1 x 1
'    All three arrays need a single element dimension.  The result is a 1x1
'    matrix, which simply could not be declared before.
' ----------------------------------------------------------------------------
Print "1. Dot product   (1x3) . (3x1) -> 1x1"

Dim rowv(2,0)                    ' 3 columns, 1 row   <- was illegal
Dim colv(0,2)                    ' 1 column, 3 rows   <- was illegal
Dim dot(0,0)                     ' 1 column, 1 row    <- was illegal

rowv(0,0) = 1 : rowv(1,0) = 2 : rowv(2,0) = 3
colv(0,0) = 4 : colv(0,1) = 5 : colv(0,2) = 6

Math M_MULT rowv(), colv(), dot()

Print "   [1 2 3] . [4 5 6]' = "; dot(0,0); "   (expected 32)"
check(dot(0,0), 32, "dot product")
Print

' ----------------------------------------------------------------------------
' 2. Matrix times a column vector:  (3 x 3) . (3 x 1)  ->  3 x 1
'    The classic "apply a transform to a point".  The input vector and the
'    answer are both single column matrices.
' ----------------------------------------------------------------------------
Print "2. Matrix x vector   (3x3) . (3x1) -> 3x1"

Dim scale(2,2)                   ' ordinary 3x3
Dim point(0,2)                   ' 1 column, 3 rows   <- was illegal
Dim moved(0,2)                   ' 1 column, 3 rows   <- was illegal

' a diagonal scaling matrix: x2, x3, x4
scale(0,0)=2 : scale(1,0)=0 : scale(2,0)=0
scale(0,1)=0 : scale(1,1)=3 : scale(2,1)=0
scale(0,2)=0 : scale(1,2)=0 : scale(2,2)=4

point(0,0) = 1 : point(0,1) = 2 : point(0,2) = 3

Math M_MULT scale(), point(), moved()

Print "   diag(2,3,4) . [1 2 3]' = ["; moved(0,0); moved(0,1); moved(0,2); " ]"
check(moved(0,0), 2, "vector row 0")
check(moved(0,1), 6, "vector row 1")
check(moved(0,2), 12, "vector row 2")
Print

' ----------------------------------------------------------------------------
' 3. Outer product:  (3 x 1) . (1 x 3)  ->  3 x 3
'    Here the single element dimensions are on the INPUTS and the answer is a
'    full matrix - the mirror image of case 1.
' ----------------------------------------------------------------------------
Print "3. Outer product   (3x1) . (1x3) -> 3x3"

Dim cv(0,2)                      ' 1 column, 3 rows   <- was illegal
Dim rv(2,0)                      ' 3 columns, 1 row   <- was illegal
Dim outer(2,2)                   ' ordinary 3x3

cv(0,0) = 1  : cv(0,1) = 2  : cv(0,2) = 3
rv(0,0) = 10 : rv(1,0) = 20 : rv(2,0) = 30

Math M_MULT cv(), rv(), outer()

Print "   [1 2 3]' . [10 20 30]:"
Math M_PRINT outer()
check(outer(0,0), 10, "outer 0,0")
check(outer(2,0), 30, "outer 2,0")
check(outer(1,1), 40, "outer 1,1")
check(outer(2,2), 90, "outer 2,2")
Print

' ----------------------------------------------------------------------------
' 4. A chain that collapses to 1x1
'    (1x3).(3x3) -> (1x3), then .(3x1) -> 1x1.  An intermediate result with a
'    single element dimension used directly as the next input.
' ----------------------------------------------------------------------------
Print "4. Chained   (1x3).(3x3) -> (1x3), then .(3x1) -> 1x1"

Dim tmp(2,0)                     ' 3 columns, 1 row   <- was illegal
Dim answer(0,0)                  ' 1x1                <- was illegal

Math M_MULT rowv(), scale(), tmp()
Print "   [1 2 3] . diag(2,3,4) = ["; tmp(0,0); tmp(1,0); tmp(2,0); " ]"
Math M_MULT tmp(), colv(), answer()
Print "   ... then . [4 5 6]' = "; answer(0,0); "   (expected 2*4+6*5+12*6 = 110)"
check(answer(0,0), 110, "chained result")
Print

' ----------------------------------------------------------------------------
' 5. The motivating case: a dimension that is only known at run time
'    n is calculated, and nothing stops it coming out as a single element.
'    DIM v(n-1) used to fail for exactly one value of n - which is precisely
'    the value an edge case tends to produce.
' ----------------------------------------------------------------------------
Print "5. Computed dimension that lands on one element"

Dim integer n, k
Dim v(0)                         ' pre-declare so the first Erase has a target
For k = 3 To 1 Step -1
  n = k                          ' pretend this came out of a calculation
  Erase v
  Dim v(n-1)                     ' n=1 was illegal before
  v(0) = n * 100
  Print "   n = "; n; " -> DIM v(n-1) ok, bound = "; Bound(v()); ", v(0) = "; v(0)
Next k
check(Bound(v()), 0, "computed single element bound")
Print

Print "================================================="
Print "PASS: "; pass; "   FAIL: "; fail
If fail = 0 Then
  Print "All checks passed."
Else
  Print "SOME CHECKS FAILED."
EndIf
End

Sub check(got As float, want As float, what As string)
  If got = want Then
    pass = pass + 1
  Else
    fail = fail + 1
    Print "   FAIL: "; what; " got "; got; " want "; want
  EndIf
End Sub
