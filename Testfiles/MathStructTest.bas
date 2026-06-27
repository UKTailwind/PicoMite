' MathStructTest.bas - Test MATH commands with structure arrays
' Tests stride-aware array access for struct().member syntax
' 
' This test validates:
' - MATH C_ADD, C_SUB, C_MUL, C_DIV (element-wise operations)
' - MATH SCALE, ADD, POWER (scalar transformations)
' - MATH(MAX), MATH(MIN), MATH(MEAN), MATH(SUM) (statistics)
' - MATH V_ROTATE (coordinate transformation)
' - MATH WINDOW (data normalization)

OPTION EXPLICIT
OPTION DEFAULT NONE

' Define a point structure for 2D coordinates
TYPE point
  x AS FLOAT
  y AS FLOAT
END TYPE

' Define a data structure for numeric operations
TYPE data_rec
  value AS FLOAT
  scaled AS FLOAT
  result AS FLOAT
END TYPE

' Define integer data structure
TYPE int_data
  a AS INTEGER
  b AS INTEGER
  c AS INTEGER
END TYPE

DIM pts(5) AS point
DIM drec(5) AS data_rec
DIM idata(5) AS int_data
DIM i AS INTEGER
DIM test_pass AS INTEGER = 0
DIM test_fail AS INTEGER = 0

' Helper arrays for some operations
DIM temp!(5), temp2!(5)
DIM idx%

PRINT "========================================"
PRINT "MATH Commands Structure Array Test Suite"
PRINT "========================================"
PRINT

' Initialize test data
FOR i = 0 TO 5
  pts(i).x = i * 10.0
  pts(i).y = i * 5.0
  drec(i).value = (i + 1) * 2.0  ' 2, 4, 6, 8, 10, 12
  drec(i).scaled = (i + 1)       ' 1, 2, 3, 4, 5, 6
  idata(i).a = i + 1
  idata(i).b = 2
NEXT i

' ============================================
' TEST 1: MATH C_ADD - Element-wise addition
' ============================================
PRINT "TEST 1: MATH C_ADD"
MATH C_ADD drec().value, drec().scaled, drec().result
' Expected: result = value + scaled = 3, 6, 9, 12, 15, 18
IF drec(0).result = 3 AND drec(2).result = 9 AND drec(5).result = 18 THEN
  PRINT "  PASS: C_ADD with float struct arrays"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: C_ADD expected 3,9,18 got "; drec(0).result; ","; drec(2).result; ","; drec(5).result
  test_fail = test_fail + 1
ENDIF

' Test integer C_ADD
MATH C_ADD idata().a, idata().b, idata().c
' Expected: c = a + b = 3, 4, 5, 6, 7, 8
IF idata(0).c = 3 AND idata(5).c = 8 THEN
  PRINT "  PASS: C_ADD with integer struct arrays"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: C_ADD integer expected 3,8 got "; idata(0).c; ","; idata(5).c
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 2: MATH C_SUB - Element-wise subtraction
' ============================================
PRINT "TEST 2: MATH C_SUB"
MATH C_SUB drec().value, drec().scaled, drec().result
' Expected: result = value - scaled = 1, 2, 3, 4, 5, 6
IF drec(0).result = 1 AND drec(3).result = 4 AND drec(5).result = 6 THEN
  PRINT "  PASS: C_SUB with float struct arrays"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: C_SUB expected 1,4,6 got "; drec(0).result; ","; drec(3).result; ","; drec(5).result
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 3: MATH C_MUL - Element-wise multiplication
' ============================================
PRINT "TEST 3: MATH C_MUL"
MATH C_MUL drec().value, drec().scaled, drec().result
' Expected: result = value * scaled = 2, 8, 18, 32, 50, 72
IF drec(0).result = 2 AND drec(2).result = 18 AND drec(5).result = 72 THEN
  PRINT "  PASS: C_MUL with float struct arrays"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: C_MUL expected 2,18,72 got "; drec(0).result; ","; drec(2).result; ","; drec(5).result
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 4: MATH C_DIV - Element-wise division
' ============================================
PRINT "TEST 4: MATH C_DIV"
MATH C_DIV drec().value, drec().scaled, drec().result
' Expected: result = value / scaled = 2, 2, 2, 2, 2, 2
IF drec(0).result = 2 AND drec(3).result = 2 AND drec(5).result = 2 THEN
  PRINT "  PASS: C_DIV with float struct arrays"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: C_DIV expected 2,2,2 got "; drec(0).result; ","; drec(3).result; ","; drec(5).result
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 5: MATH SCALE - Multiply by constant
' ============================================
PRINT "TEST 5: MATH SCALE"
' Reset values
FOR i = 0 TO 5 : drec(i).value = i + 1 : NEXT i
MATH SCALE drec().value, 3.0, drec().result
' Expected: result = value * 3 = 3, 6, 9, 12, 15, 18
IF drec(0).result = 3 AND drec(2).result = 9 AND drec(5).result = 18 THEN
  PRINT "  PASS: SCALE with float struct arrays"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: SCALE expected 3,9,18 got "; drec(0).result; ","; drec(2).result; ","; drec(5).result
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 6: MATH ADD - Add constant to array
' ============================================
PRINT "TEST 6: MATH ADD"
FOR i = 0 TO 5 : drec(i).value = i * 10 : NEXT i
MATH ADD drec().value, 100, drec().result
' Expected: result = value + 100 = 100, 110, 120, 130, 140, 150
IF drec(0).result = 100 AND drec(3).result = 130 AND drec(5).result = 150 THEN
  PRINT "  PASS: ADD with float struct arrays"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: ADD expected 100,130,150 got "; drec(0).result; ","; drec(3).result; ","; drec(5).result
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 7: MATH POWER - Raise to power
' ============================================
PRINT "TEST 7: MATH POWER"
FOR i = 0 TO 5 : drec(i).value = i + 1 : NEXT i  ' 1,2,3,4,5,6
MATH POWER drec().value, 2, drec().result
' Expected: result = value^2 = 1, 4, 9, 16, 25, 36
IF drec(0).result = 1 AND drec(2).result = 9 AND drec(5).result = 36 THEN
  PRINT "  PASS: POWER with float struct arrays"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: POWER expected 1,9,36 got "; drec(0).result; ","; drec(2).result; ","; drec(5).result
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 8: MATH(MAX) - Find maximum
' ============================================
PRINT "TEST 8: MATH(MAX)"
FOR i = 0 TO 5 : drec(i).value = (i - 2) * 10 : NEXT i  ' -20,-10,0,10,20,30
DIM max_val!
max_val! = MATH(MAX drec().value)
IF max_val! = 30 THEN
  PRINT "  PASS: MAX with float struct array = "; max_val!
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: MAX expected 30 got "; max_val!
  test_fail = test_fail + 1
ENDIF

' Test with index return
max_val! = MATH(MAX drec().value, idx%)
IF max_val! = 30 AND idx% = 5 THEN
  PRINT "  PASS: MAX with index = "; max_val!; " at index "; idx%
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: MAX with index expected 30 at 5, got "; max_val!; " at "; idx%
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 9: MATH(MIN) - Find minimum
' ============================================
PRINT "TEST 9: MATH(MIN)"
DIM min_val!
min_val! = MATH(MIN drec().value)
IF min_val! = -20 THEN
  PRINT "  PASS: MIN with float struct array = "; min_val!
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: MIN expected -20 got "; min_val!
  test_fail = test_fail + 1
ENDIF

min_val! = MATH(MIN drec().value, idx%)
IF min_val! = -20 AND idx% = 0 THEN
  PRINT "  PASS: MIN with index = "; min_val!; " at index "; idx%
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: MIN with index expected -20 at 0, got "; min_val!; " at "; idx%
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 10: MATH(MEAN) - Calculate mean
' ============================================
PRINT "TEST 10: MATH(MEAN)"
FOR i = 0 TO 5 : drec(i).value = (i + 1) * 10 : NEXT i  ' 10,20,30,40,50,60
DIM mean_val!
mean_val! = MATH(MEAN drec().value)
' Expected: (10+20+30+40+50+60)/6 = 210/6 = 35
IF mean_val! = 35 THEN
  PRINT "  PASS: MEAN with float struct array = "; mean_val!
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: MEAN expected 35 got "; mean_val!
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 11: MATH(SUM) - Sum all elements
' ============================================
PRINT "TEST 11: MATH(SUM)"
DIM sum_val!
sum_val! = MATH(SUM drec().value)
' Expected: 10+20+30+40+50+60 = 210
IF sum_val! = 210 THEN
  PRINT "  PASS: SUM with float struct array = "; sum_val!
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: SUM expected 210 got "; sum_val!
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 12: MATH V_ROTATE - Rotate coordinates
' ============================================
PRINT "TEST 12: MATH V_ROTATE"
OPTION ANGLE DEGREES
' Set up a simple point at (100, 0)
FOR i = 0 TO 5
  pts(i).x = 100
  pts(i).y = 0
NEXT i

' Rotate 90 degrees around origin (0,0)
' Point (100,0) rotated 90 degrees should become (0, 100)
MATH V_ROTATE 0, 0, 90, pts().x, pts().y, pts().x, pts().y

' Check result (allowing for floating point tolerance)
IF ABS(pts(0).x) < 0.001 AND ABS(pts(0).y - 100) < 0.001 THEN
  PRINT "  PASS: V_ROTATE 90 degrees: (100,0) -> ("; pts(0).x; ","; pts(0).y; ")"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: V_ROTATE expected (0,100) got ("; pts(0).x; ","; pts(0).y; ")"
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 13: MATH WINDOW - Normalize to range
' ============================================
PRINT "TEST 13: MATH WINDOW"
' Set values 0 to 50
FOR i = 0 TO 5 : drec(i).value = i * 10 : NEXT i  ' 0,10,20,30,40,50
MATH WINDOW drec().value, 0, 100, drec().result
' Expected: 0->0, 10->20, 20->40, 30->60, 40->80, 50->100
IF drec(0).result = 0 AND drec(5).result = 100 AND ABS(drec(2).result - 40) < 0.001 THEN
  PRINT "  PASS: WINDOW normalized to 0-100"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: WINDOW expected 0,40,100 got "; drec(0).result; ","; drec(2).result; ","; drec(5).result
  test_fail = test_fail + 1
ENDIF

' Test with min/max output (returns actual min/max of INPUT data, not target range)
DIM wmin!, wmax!
MATH WINDOW drec().value, 0, 100, drec().result, wmin!, wmax!
' Input values are 0,10,20,30,40,50 so min=0, max=50
IF wmin! = 0 AND wmax! = 50 THEN
  PRINT "  PASS: WINDOW returned input min="; wmin!; " max="; wmax!
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: WINDOW min/max expected 0,50 got "; wmin!; ","; wmax!
  test_fail = test_fail + 1
ENDIF

' Normal array WINDOW test for comparison
DIM wn_in!(5), wn_out!(5), wn_min!, wn_max!
FOR i = 0 TO 5 : wn_in!(i) = i * 10 : NEXT i  ' 0,10,20,30,40,50
MATH WINDOW wn_in!(), 0, 100, wn_out!(), wn_min!, wn_max!
IF wn_out!(0) = 0 AND wn_out!(5) = 100 AND wn_min! = 0 AND wn_max! = 50 THEN
  PRINT "  PASS: Normal array WINDOW: out="; wn_out!(0); ","; wn_out!(5); " min/max="; wn_min!; ","; wn_max!
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: Normal WINDOW got out="; wn_out!(0); ","; wn_out!(5); " min/max="; wn_min!; ","; wn_max!
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 14: Mixed struct and regular arrays
' ============================================
PRINT "TEST 14: Mixed struct/regular arrays"
DIM regular!(5)
FOR i = 0 TO 5
  drec(i).value = i + 1
  regular!(i) = 10
NEXT i
MATH C_ADD drec().value, regular!(), drec().result
' Expected: 1+10=11, 2+10=12, ... 6+10=16
IF drec(0).result = 11 AND drec(5).result = 16 THEN
  PRINT "  PASS: Mixed struct input with regular array"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: Mixed expected 11,16 got "; drec(0).result; ","; drec(5).result
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 15: Normal float arrays (regression test)
' ============================================
PRINT "TEST 15: Normal float arrays (regression)"
DIM nf1!(5), nf2!(5), nf3!(5)
FOR i = 0 TO 5
  nf1!(i) = (i + 1) * 2   ' 2, 4, 6, 8, 10, 12
  nf2!(i) = i + 1         ' 1, 2, 3, 4, 5, 6
NEXT i

' Test C_ADD with normal arrays
MATH C_ADD nf1!(), nf2!(), nf3!()
IF nf3!(0) = 3 AND nf3!(2) = 9 AND nf3!(5) = 18 THEN
  PRINT "  PASS: Normal float C_ADD"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: Normal float C_ADD expected 3,9,18 got "; nf3!(0); ","; nf3!(2); ","; nf3!(5)
  test_fail = test_fail + 1
ENDIF

' Test C_MUL with normal arrays
MATH C_MUL nf1!(), nf2!(), nf3!()
IF nf3!(0) = 2 AND nf3!(2) = 18 AND nf3!(5) = 72 THEN
  PRINT "  PASS: Normal float C_MUL"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: Normal float C_MUL expected 2,18,72 got "; nf3!(0); ","; nf3!(2); ","; nf3!(5)
  test_fail = test_fail + 1
ENDIF

' Test SCALE with normal arrays
FOR i = 0 TO 5 : nf1!(i) = i + 1 : NEXT i
MATH SCALE nf1!(), 3.0, nf3!()
IF nf3!(0) = 3 AND nf3!(2) = 9 AND nf3!(5) = 18 THEN
  PRINT "  PASS: Normal float SCALE"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: Normal float SCALE expected 3,9,18 got "; nf3!(0); ","; nf3!(2); ","; nf3!(5)
  test_fail = test_fail + 1
ENDIF

' Test MATH(MAX) with normal arrays
FOR i = 0 TO 5 : nf1!(i) = (i - 2) * 10 : NEXT i  ' -20,-10,0,10,20,30
DIM nmax!
nmax! = MATH(MAX nf1!())
IF nmax! = 30 THEN
  PRINT "  PASS: Normal float MAX = "; nmax!
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: Normal float MAX expected 30 got "; nmax!
  test_fail = test_fail + 1
ENDIF

' Test MATH(MIN) with normal arrays
DIM nmin!
nmin! = MATH(MIN nf1!())
IF nmin! = -20 THEN
  PRINT "  PASS: Normal float MIN = "; nmin!
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: Normal float MIN expected -20 got "; nmin!
  test_fail = test_fail + 1
ENDIF

' Test MATH(SUM) with normal arrays
FOR i = 0 TO 5 : nf1!(i) = (i + 1) * 10 : NEXT i  ' 10,20,30,40,50,60
DIM nsum!
nsum! = MATH(SUM nf1!())
IF nsum! = 210 THEN
  PRINT "  PASS: Normal float SUM = "; nsum!
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: Normal float SUM expected 210 got "; nsum!
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 16: Normal integer arrays (regression test)
' ============================================
PRINT "TEST 16: Normal integer arrays (regression)"
DIM ni1%(5), ni2%(5), ni3%(5)
FOR i = 0 TO 5
  ni1%(i) = (i + 1) * 2   ' 2, 4, 6, 8, 10, 12
  ni2%(i) = i + 1         ' 1, 2, 3, 4, 5, 6
NEXT i

' Test C_ADD with normal integer arrays
MATH C_ADD ni1%(), ni2%(), ni3%()
IF ni3%(0) = 3 AND ni3%(2) = 9 AND ni3%(5) = 18 THEN
  PRINT "  PASS: Normal integer C_ADD"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: Normal integer C_ADD expected 3,9,18 got "; ni3%(0); ","; ni3%(2); ","; ni3%(5)
  test_fail = test_fail + 1
ENDIF

' Test C_MUL with normal integer arrays
MATH C_MUL ni1%(), ni2%(), ni3%()
IF ni3%(0) = 2 AND ni3%(2) = 18 AND ni3%(5) = 72 THEN
  PRINT "  PASS: Normal integer C_MUL"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: Normal integer C_MUL expected 2,18,72 got "; ni3%(0); ","; ni3%(2); ","; ni3%(5)
  test_fail = test_fail + 1
ENDIF

' Test C_SUB with normal integer arrays
MATH C_SUB ni1%(), ni2%(), ni3%()
IF ni3%(0) = 1 AND ni3%(3) = 4 AND ni3%(5) = 6 THEN
  PRINT "  PASS: Normal integer C_SUB"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: Normal integer C_SUB expected 1,4,6 got "; ni3%(0); ","; ni3%(3); ","; ni3%(5)
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 17: ARRAY SET with struct member arrays
' ============================================
PRINT "TEST 17: ARRAY SET with struct members"
' Reset drec values
FOR i = 0 TO 5 : drec(i).value = 0 : drec(i).scaled = 0 : drec(i).result = 0 : NEXT i

' Test ARRAY SET on float struct member
ARRAY SET 42.5, drec().value
IF drec(0).value = 42.5 AND drec(3).value = 42.5 AND drec(5).value = 42.5 THEN
  PRINT "  PASS: ARRAY SET struct float member"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: ARRAY SET expected 42.5 got "; drec(0).value; ","; drec(3).value; ","; drec(5).value
  test_fail = test_fail + 1
ENDIF

' Test ARRAY SET on integer struct member
FOR i = 0 TO 5 : idata(i).a = 0 : idata(i).b = 0 : idata(i).c = 0 : NEXT i
ARRAY SET 99, idata().a
IF idata(0).a = 99 AND idata(3).a = 99 AND idata(5).a = 99 THEN
  PRINT "  PASS: ARRAY SET struct integer member"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: ARRAY SET expected 99 got "; idata(0).a; ","; idata(3).a; ","; idata(5).a
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' TEST 18: ARRAY ADD with struct member arrays
' ============================================
PRINT "TEST 18: ARRAY ADD with struct members"
' Set up source values
FOR i = 0 TO 5
  drec(i).value = (i + 1) * 10   ' 10, 20, 30, 40, 50, 60
  drec(i).result = 0
NEXT i

' Test ARRAY ADD on float struct members (add constant to array)
ARRAY ADD drec().value, 5.5, drec().result
IF drec(0).result = 15.5 AND drec(3).result = 45.5 AND drec(5).result = 65.5 THEN
  PRINT "  PASS: ARRAY ADD struct float members"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: ARRAY ADD expected 15.5,45.5,65.5 got "; drec(0).result; ","; drec(3).result; ","; drec(5).result
  test_fail = test_fail + 1
ENDIF

' Test ARRAY ADD on integer struct members
FOR i = 0 TO 5
  idata(i).a = (i + 1) * 5   ' 5, 10, 15, 20, 25, 30
  idata(i).c = 0
NEXT i
ARRAY ADD idata().a, 100, idata().c
IF idata(0).c = 105 AND idata(3).c = 120 AND idata(5).c = 130 THEN
  PRINT "  PASS: ARRAY ADD struct integer members"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: ARRAY ADD expected 105,120,130 got "; idata(0).c; ","; idata(3).c; ","; idata(5).c
  test_fail = test_fail + 1
ENDIF

' Test ARRAY ADD copying array (add 0)
FOR i = 0 TO 5 : drec(i).scaled = 0 : NEXT i
ARRAY ADD drec().value, 0, drec().scaled
IF drec(0).scaled = 10 AND drec(3).scaled = 40 AND drec(5).scaled = 60 THEN
  PRINT "  PASS: ARRAY ADD copy (add 0) struct members"
  test_pass = test_pass + 1
ELSE
  PRINT "  FAIL: ARRAY ADD copy expected 10,40,60 got "; drec(0).scaled; ","; drec(3).scaled; ","; drec(5).scaled
  test_fail = test_fail + 1
ENDIF
PRINT

' ============================================
' Summary
' ============================================
PRINT "========================================"
PRINT "TEST SUMMARY"
PRINT "========================================"
PRINT "Passed: "; test_pass
PRINT "Failed: "; test_fail
PRINT "Total:  "; test_pass + test_fail
PRINT
IF test_fail = 0 THEN
  PRINT "*** ALL TESTS PASSED ***"
ELSE
  PRINT "*** SOME TESTS FAILED ***"
ENDIF

END
