' ============================================================================
' TURTLE GRAPHICS TEST SUITE FOR MMBASIC
' ============================================================================

Option EXPLICIT
Option DEFAULT INTEGER

Dim test_number As INTEGER = 0
Dim passed As INTEGER = 0
Dim failed As INTEGER = 0

' ============================================================================
' MAIN TEST MENU
' ============================================================================
Sub TestMenu()
  Local choice$

  Do
    CLS
    Print "TURTLE GRAPHICS TEST SUITE"
    Print "=========================="
    Print
    Print "1.  Basic Movement Tests"
    Print "2.  Pen Control Tests"
    Print "3.  Position Tests"
    Print "4.  Shape Tests (Circles)"
    Print "5.  Shape Tests (Rectangles)"
    Print "6.  Arc Tests"
    Print "7.  Fill Pattern Tests"
    Print "8.  Polygon Fill Tests"
    Print "9.  Stack (PUSH/POP) Tests"
    Print "10. Cursor Tests"
    Print "11. Complex Drawing Tests"
    Print "12. Stress Tests"
    Print "13. Run All Tests"
    Print "0.  Exit"
    Print
    Input "Select test: ", choice$

    Select Case Val(choice$)
      Case 1: Test_BasicMovement()
      Case 2: Test_PenControl()
      Case 3: Test_Position()
      Case 4: Test_Circles()
      Case 5: Test_Rectangles()
      Case 6: Test_Arcs()
      Case 7: Test_FillPatterns()
      Case 8: Test_PolygonFill()
      Case 9: Test_Stack()
      Case 10: Test_Cursor()
      Case 11: Test_ComplexDrawing()
      Case 12: Test_Stress()
      Case 13: RunAllTests()
      Case 0: Exit Sub
    End Select

    Print
    Print "Press any key to continue..."
    Do While Inkey$ = "": Loop
  Loop
End Sub

' ============================================================================
' TEST 1: BASIC MOVEMENT
' ============================================================================
Sub Test_BasicMovement()
  Local i As INTEGER

  StartTest("Basic Movement Tests")

  ' Test 1.1: Square using FORWARD and RIGHT
  TestCase("Square with FD and RT")
  Turtle RESET
  Turtle HIDETURTLE
  For i = 1 To 4
    Turtle FORWARD 100
    Turtle RIGHT 90
  Next i
  WaitForKey()

  ' Test 1.2: Square using short commands
  TestCase("Square with short commands (FD, RT)")
  Turtle RESET
  Turtle HT
  For i = 1 To 4
    Turtle FD 100
    Turtle RT 90
  Next i
  WaitForKey()

  ' Test 1.3: Triangle using LEFT
  TestCase("Triangle with FD and LT")
  Turtle RESET
  Turtle HT
  For i = 1 To 3
    Turtle FD 100
    Turtle LT 120
  Next i
  WaitForKey()

  ' Test 1.4: Star using large angles
  TestCase("Star pattern")
  Turtle RESET
  Turtle HT
  For i = 1 To 5
    Turtle FD 100
    Turtle RT 144
  Next i
  WaitForKey()

  ' Test 1.5: BACK command
  TestCase("Forward and Back")
  Turtle RESET
  Turtle HT
  Turtle FD 100
  Pause 500
  Turtle BACK 100
  WaitForKey()

  ' Test 1.6: Spiral
  TestCase("Spiral using increasing distances")
  Turtle RESET
  Turtle HT
  For i = 1 To 50
    Turtle FD i * 2
    Turtle RT 15
  Next i
  WaitForKey()

  EndTest()
End Sub

' ============================================================================
' TEST 2: PEN CONTROL
' ============================================================================
Sub Test_PenControl()
  Local i As INTEGER
  Local w As INTEGER
  Local colors(6) As INTEGER

  StartTest("Pen Control Tests")

  ' Test 2.1: PENUP and PENDOWN
  TestCase("Dashed line with PENUP/PENDOWN")
  Turtle RESET
  Turtle HT
  For i = 1 To 5
    Turtle PD
    Turtle FD 20
    Turtle PU
    Turtle FD 20
  Next i
  WaitForKey()

  ' Test 2.2: PENCOLOR
  TestCase("Rainbow colors")
  Turtle RESET
  Turtle HT
  colors(0) = RGB(RED)
  colors(1) = RGB(255,127,0)
  colors(2) = RGB(YELLOW)
  colors(3) = RGB(GREEN)
  colors(4) = RGB(BLUE)
  colors(5) = RGB(75,0,130)
  colors(6) = RGB(148,0,211)
  For i = 0 To 6
    Turtle PENCOLOR colors(i)
    Turtle FD 40
    Turtle RT 51
  Next i
  WaitForKey()

  ' Test 2.3: PENWIDTH
  TestCase("Varying line widths")
  Turtle RESET
  Turtle HT
  For w = 1 To 5
    Turtle PENWIDTH w
    Turtle FD 50
    Turtle RT 90
  Next w
  WaitForKey()

  ' Test 2.4: Pen color and width combination
  TestCase("Colored thick spiral")
  Turtle RESET
  Turtle HT
  Turtle PENWIDTH 3
  For i = 1 To 36
    Turtle PENCOLOR RGB(255, (i*7) Mod 256, 255-((i*7) Mod 256))
    Turtle FD i * 2
    Turtle RT 10
  Next i
  WaitForKey()

  EndTest()
End Sub

' ============================================================================
' TEST 3: POSITION CONTROL
' ============================================================================
Sub Test_Position()
  Local x As INTEGER
  Local angle As INTEGER
  Local i As INTEGER

  StartTest("Position Control Tests")

  ' Test 3.1: SETXY
  TestCase("Connect four corners with SETXY")
  Turtle RESET
  Turtle HT
  Turtle PU
  Turtle SETXY 50, 50
  Turtle PD
  Turtle SETXY 270, 50
  Turtle SETXY 270, 190
  Turtle SETXY 50, 190
  Turtle SETXY 50, 50
  WaitForKey()

  ' Test 3.2: SETX and SETY
  TestCase("Grid pattern with SETX/SETY")
  Turtle RESET
  Turtle HT
  Turtle PU
  Turtle setxy 40,40
  Turtle pd
  For x = 40 To 280 Step 40
    Turtle SETX x
    Turtle SETY 40
    Turtle PD
    Turtle SETY 200
    Turtle PU
  Next x
  WaitForKey()

  ' Test 3.3: SETHEADING
  TestCase("Radial lines with SETHEADING")
  Turtle RESET
  Turtle HT
  For angle = 0 To 360 Step 30
    Turtle HOME
    Turtle SETHEADING angle
    Turtle FD 80
  Next angle
  WaitForKey()

  ' Test 3.4: HOME command
  TestCase("Random walk then HOME")
  Turtle RESET
  Turtle HT
  For i = 1 To 50
    Turtle FD 8
    Turtle RT(Rnd() * 90 - 45)
  Next i
  Pause 1000
  Turtle PU
  Turtle HOME
  Turtle PD
  Turtle PENCOLOR RGB(RED)
  Turtle CIRCLE 5
  WaitForKey()

  EndTest()
End Sub

' ============================================================================
' TEST 4: CIRCLE TESTS
' ============================================================================
Sub Test_Circles()
  Local r As INTEGER
  Local x As INTEGER
  Local y As INTEGER
  Local i As INTEGER

  StartTest("Circle Tests")

  ' Test 4.1: Basic circles
  TestCase("Concentric circles")
  Turtle RESET
  Turtle HT
  For r = 10 To 100 Step 10
    Turtle HOME
    Turtle CIRCLE r
  Next r
  WaitForKey()

  ' Test 4.2: Circle at different positions
  TestCase("Circle grid")
  Turtle RESET
  Turtle HT
  For x = 60 To 260 Step 50
    For y = 60 To 180 Step 40
      Turtle PU
      Turtle SETXY x, y
      Turtle PD
      Turtle CIRCLE 20
    Next y
  Next x
  WaitForKey()

  ' Test 4.3: DOT command
  TestCase("Dots of varying sizes")
  Turtle RESET
  Turtle HT
  For i = 1 To 10
    Turtle PU
    Turtle SETXY 30 + i * 25, 120
    Turtle PD
    Turtle DOT i * 3
  Next i
  WaitForKey()

  ' Test 4.4: FCIRCLE (filled circles)
  TestCase("Filled circles - solid")
  Turtle RESET
  Turtle HT
  Turtle FILLCOLOR RGB(255, 100, 100)
  Turtle PU
  Turtle SETXY 100, 120
  Turtle FCIRCLE 40
  Turtle SETXY 220, 120
  Turtle FILLCOLOR RGB(100, 100, 255)
  Turtle FCIRCLE 40
  WaitForKey()

  EndTest()
End Sub

' ============================================================================
' TEST 5: RECTANGLE TESTS
' ============================================================================
Sub Test_Rectangles()
  Local angle As INTEGER

  StartTest("Rectangle Tests")

  ' Test 5.1: FRECT (axis-aligned filled rectangles)
  TestCase("Axis-aligned filled rectangles")
  Turtle RESET
  Turtle HT
  Turtle FILLCOLOR RGB(RED)
  Turtle PU
  Turtle SETXY 80, 120
  Turtle FRECT 60, 40
  Turtle SETXY 160, 120
  Turtle FILLCOLOR RGB(GREEN)
  Turtle FRECT 60, 40
  Turtle SETXY 240, 120
  Turtle FILLCOLOR RGB(BLUE)
  Turtle FRECT 60, 40
  WaitForKey()

  ' Test 5.2: ARECT (heading-aligned rectangles)
  TestCase("Rotated rectangles with ARECT")
  Turtle RESET
  Turtle HT
  Turtle FILLCOLOR RGB(255, 200, 0)
  For angle = 0 To 360 Step 45
    Turtle HOME
    Turtle SETHEADING angle
    Turtle ARECT 80, 40
  Next angle
  WaitForKey()

  ' Test 5.3: ARECT vs FRECT comparison
  TestCase("ARECT vs FRECT at 45 degrees")
  Turtle RESET
  Turtle HT
  Turtle PU
  Turtle SETXY 100, 120
  Turtle SETHEADING 45
  Turtle FILLCOLOR RGB(255, 0, 0)
  Turtle ARECT 80, 50
  Turtle SETXY 220, 120
  Turtle FILLCOLOR RGB(0, 0, 255)
  Turtle FRECT 80, 50
  WaitForKey()

  EndTest()
End Sub

' ============================================================================
' TEST 6: ARC TESTS
' ============================================================================
Sub Test_Arcs()
  Local i As INTEGER
  Local x As INTEGER
  Local angles(3) As INTEGER
  Local colors(3) As INTEGER
  Local start_angle As INTEGER

  StartTest("Arc Tests")

  ' Test 6.1: Basic arc
  TestCase("Simple arc - semicircle")
  Turtle RESET
  Turtle HT
  Turtle ARC 50, 180
  WaitForKey()

  ' Test 6.2: Flower pattern with arcs
  TestCase("Flower with arc petals")
  Turtle RESET
  Turtle HT
  For i = 1 To 30
    Turtle ARC 40, 60
    Turtle RT 120
    Turtle ARC 40, 60
    Turtle RT 195
  Next i
  WaitForKey()

  ' Test 6.3: ARCL and ARCR
  TestCase("ARCL vs ARCR")
  Turtle RESET
  Turtle HT
  Turtle PU
  Turtle SETXY 100, 120
  Turtle PD
  Turtle PENCOLOR RGB(RED)
  Turtle ARCL 50, 180
  Turtle PU
  Turtle SETXY 220, 120
  Turtle PD
  Turtle PENCOLOR RGB(BLUE)
  Turtle ARCR 50, 180
  WaitForKey()

  ' Test 6.4: Spiral with arcs
  TestCase("Arc spiral")
  Turtle RESET
  Turtle HT
  For i = 1 To 20
    Turtle ARC(i * 3), 30
  Next i
  WaitForKey()

  ' Test 6.5: WEDGE (pie slice)
  TestCase("Pac-Man with WEDGE")
  Turtle RESET
  Turtle HT
  Turtle FILLCOLOR RGB(255, 255, 0)
  Turtle WEDGE 60, 30, 330
  ' Add eye
  Turtle PU
  Turtle SETXY 175, 100
  Turtle FILLCOLOR RGB(0, 0, 0)
  Turtle FCIRCLE 5
  WaitForKey()

  ' Test 6.6: Pie chart
  TestCase("Simple pie chart")
  Turtle RESET
  Turtle HT
  angles(0) = 90
  angles(1) = 120
  angles(2) = 150
  angles(3) = 0
  colors(0) = RGB(255,0,0)
  colors(1) = RGB(0,255,0)
  colors(2) = RGB(0,0,255)
  colors(3) = RGB(255,255,0)
  start_angle = 0
  For i = 0 To 3
    Turtle HOME
    Turtle FILLCOLOR colors(i)
    Turtle WEDGE 70, start_angle,(start_angle + angles(i))
    start_angle = start_angle + angles(i)
  Next i
  WaitForKey()

  EndTest()
End Sub

' ============================================================================
' TEST 7: FILL PATTERN TESTS
' ============================================================================
Sub Test_FillPatterns()
  Local x As INTEGER
  Local y As INTEGER
  Local p As INTEGER
  Local i As INTEGER
  Local gradient_patterns(7) As INTEGER

  StartTest("Fill Pattern Tests")

  ' Test 7.1: All patterns showcase
  TestCase("Pattern showcase (0-15)")
  Turtle RESET
  Turtle HT
  For p = 0 To 15
    x = (p Mod 4) * 70 + 50
    y = (p \ 4) * 50 + 40
    Turtle PU
    Turtle SETXY x, y
    Turtle FILLPATTERN p
    Turtle FILLCOLOR RGB(200, 100, 50)
    Turtle FCIRCLE 25
  Next p
  WaitForKey()

  ' Test 7.2: More patterns
  TestCase("Pattern showcase (16-31)")
  Turtle RESET
  Turtle HT
  For p = 16 To 31
    x = ((p-16) Mod 4) * 70 + 50
    y = ((p-16) \ 4) * 50 + 40
    Turtle PU
    Turtle SETXY x, y
    Turtle FILLPATTERN p
    Turtle FILLCOLOR RGB(50, 100, 200)
    Turtle FCIRCLE 25
  Next p
  WaitForKey()

  ' Test 7.3: Patterns on rectangles
  TestCase("Patterns on rectangles")
  Turtle RESET
  Turtle HT
  For p = 0 To 7
    Turtle PU
    Turtle SETXY 40 + p * 35, 120
    Turtle FILLPATTERN p
    Turtle FILLCOLOR RGB(100, 200, 100)
    Turtle FRECT 30, 80
  Next p
  WaitForKey()

  ' Test 7.4: Gradient effect
  TestCase("Density gradient (sparse to dense)")
  Turtle RESET
  Turtle HT
  gradient_patterns(0) = 24
  gradient_patterns(1) = 14
  gradient_patterns(2) = 7
  gradient_patterns(3) = 1
  gradient_patterns(4) = 8
  gradient_patterns(5) = 11
  gradient_patterns(6) = 27
  gradient_patterns(7) = 0
  For i = 0 To 7
    Turtle PU
    Turtle SETXY 40 + i * 35, 120
    Turtle FILLPATTERN gradient_patterns(i)
    Turtle FILLCOLOR RGB(150, 150, 150)
    Turtle FCIRCLE 25
  Next i
  WaitForKey()

  ' Test 7.5: Pattern on rotated rectangle
  TestCase("Patterns on rotated rectangles")
  Local angle As INTEGER
  Turtle RESET
  Turtle HT
  Turtle FILLPATTERN 6
  Turtle FILLCOLOR RGB(255, 150, 0)
  For angle = 0 To 360 Step 45
    CLS
    Turtle HOME
    Turtle SETHEADING angle
    Turtle ARECT 70, 30
    Pause 1000
  Next angle
  WaitForKey()

  EndTest()
End Sub

' ============================================================================
' TEST 8: POLYGON FILL TESTS
' ============================================================================
Sub Test_PolygonFill()
  Local i As INTEGER

  StartTest("Polygon Fill Tests")

  ' Test 8.1: Filled triangle
  TestCase("Filled triangle with BEGIN_FILL/END_FILL")
  Turtle RESET
  Turtle HT
  Turtle FILLCOLOR RGB(255, 100, 100)
  Turtle BEGIN_FILL
  For i = 1 To 3
    Turtle FD 100
    Turtle RT 120
  Next i
  Turtle END_FILL
  WaitForKey()

  ' Test 8.2: Filled star
  TestCase("Filled star")
  Turtle RESET
  Turtle HT
  Turtle FILLCOLOR RGB(255, 255, 0)
  Turtle BEGIN_FILL
  For i = 1 To 5
    Turtle FD 100
    Turtle RT 144
  Next i
  Turtle END_FILL
  WaitForKey()

  ' Test 8.3: Pattern-filled hexagon
  TestCase("Pattern-filled hexagon")
  Turtle RESET
  Turtle HT
  Turtle FILLCOLOR RGB(100, 200, 255)
  Turtle FILLPATTERN 6
  Turtle BEGIN_FILL
  For i = 1 To 6
    Turtle FD 60
    Turtle RT 60
  Next i
  Turtle END_FILL
  WaitForKey()

  ' Test 8.4: Complex polygon with curves
  TestCase("Polygon with arcs")
  Turtle RESET
  Turtle HT
  Turtle FILLCOLOR RGB(200, 100, 200)
  Turtle FILLPATTERN 1
  Turtle BEGIN_FILL
  For i = 1 To 4
    Turtle ARC 40, 90
  Next i
  Turtle END_FILL
  WaitForKey()

  EndTest()
End Sub

' ============================================================================
' TEST 9: STACK TESTS
' ============================================================================
Sub Test_Stack()
  Local i As INTEGER

  StartTest("Stack (PUSH/POP) Tests")

  ' Test 9.1: Simple PUSH/POP
  TestCase("Basic PUSH/POP - draw and return")
  Turtle RESET
  Turtle HT
  Turtle PUSH
  Turtle FD 100
  Turtle RT 90
  Turtle FD 50
  Turtle POP
  Turtle PENCOLOR RGB(RED)
  Turtle CIRCLE 5
  WaitForKey()

  ' Test 9.2: Recursive tree
  TestCase("Recursive tree with PUSH/POP")
  Turtle RESET
  Turtle HT
  Turtle PU
  Turtle SETXY 160, 200
  Turtle SETHEADING 90
  Turtle PD
  DrawBranch(60)
  WaitForKey()

  ' Test 9.3: Multiple PUSH/POP
  TestCase("Nested PUSH/POP")
  Turtle RESET
  Turtle HT
  Turtle PUSH
  Turtle FD 80
  Turtle PUSH
  Turtle RT 45
  Turtle FD 60
  Turtle POP
  Turtle RT 90
  Turtle FD 60
  Turtle POP
  Turtle PENCOLOR RGB(RED)
  Turtle CIRCLE 5
  WaitForKey()

  ' Test 9.4: Snowflake with PUSH/POP
  TestCase("Snowflake pattern")
  Turtle RESET
  Turtle HT
  For i = 1 To 6
    Turtle PUSH
    DrawSnowflakeBranch(70)
    Turtle POP
    Turtle RT 60
  Next i
  WaitForKey()

  EndTest()
End Sub

' ============================================================================
' TEST 10: CURSOR TESTS
' ============================================================================
Sub Test_Cursor()
  Local i As INTEGER
  Local size As INTEGER
  Local cursor_colors(4) As INTEGER

  StartTest("Cursor Tests")

  ' Test 10.1: SHOWTURTLE and HIDETURTLE
  TestCase("Toggle cursor visibility")
  Turtle RESET
  Turtle ST
  Print "Cursor should be visible"
  WaitForKey()
  Turtle HT
  Print "Cursor should be hidden"
  WaitForKey()

  ' Test 10.2: Cursor follows movement
  TestCase("Cursor following movement")
  Turtle RESET
  Turtle ST
  For i = 1 To 4
    Turtle FD 80
    Pause 500
    Turtle RT 90
  Next i
  WaitForKey()

  ' Test 10.3: CURSORSIZE
  TestCase("Different cursor sizes")
  Turtle RESET
  For size = 5 To 20 Step 5
    Turtle ST
    Turtle CS size
    Pause 800
  Next size
  WaitForKey()

  ' Test 10.4: CURSORCOLOR
  TestCase("Different cursor colors")
  Turtle RESET
  Turtle ST
  cursor_colors(0) = RGB(RED)
  cursor_colors(1) = RGB(GREEN)
  cursor_colors(2) = RGB(BLUE)
  cursor_colors(3) = RGB(YELLOW)
  cursor_colors(4) = RGB(MAGENTA)
  For i = 0 To 4
    Turtle CURSORCOLOR cursor_colors(i)
    Turtle FD 40
    Turtle RT 72
    Pause 500
  Next i
  WaitForKey()

  EndTest()
End Sub

' ============================================================================
' TEST 11: COMPLEX DRAWING TESTS
' ============================================================================
Sub Test_ComplexDrawing()
  Local x As INTEGER
  Local layer As INTEGER
  Local i As INTEGER

  StartTest("Complex Drawing Tests")

  ' Test 11.1: House
  TestCase("Draw a house")
  Turtle RESET
  Turtle HT
  ' Base
  Turtle FILLCOLOR RGB(200, 150, 100)
  Turtle PU
  Turtle SETXY 160, 150
  Turtle FRECT 100, 80
  ' Roof
  Turtle PU
  Turtle SETXY 160, 110
  Turtle PD
  Turtle FILLCOLOR RGB(150, 50, 50)
  Turtle BEGIN_FILL
  Turtle setxy 110,110
  Turtle setxy 130,80
  Turtle setxy 190,80
  Turtle setxy 210,110
  Turtle setxy 110,110
  Turtle END_FILL
  ' Door
  Turtle PU
  Turtle SETXY 160, 170
  Turtle FILLCOLOR RGB(100, 50, 0)
  Turtle FRECT 25, 40
  ' Window
  Turtle SETXY 130, 135
  Turtle FILLCOLOR RGB(200, 220, 255)
  Turtle FRECT 20, 20
  WaitForKey()

' Test 11.2: Flower garden
  TestCase("Flower garden")
  Turtle RESET
  Turtle HT
  ' Ground - make it span the full width
  Turtle PU
  Turtle SETXY 160, 180
  Turtle FILLCOLOR RGB(brown)
  Turtle FILLPATTERN 1
  Turtle FRECT 320, 80

' Flowers - evenly spaced across the ground
  For x = 70 To 250 Step 60
    DrawFlower(x, 150 + Int(Rnd() * 10))
  Next x
  WaitForKey()

  ' Test 11.3: Spiral galaxy
  TestCase("Spiral galaxy")
  Turtle RESET
  Turtle HT
  Turtle PENWIDTH 2
  For i = 1 To 100
    Turtle PENCOLOR RGB((100 + i), (100 + i), 255)
    Turtle FD(i * 0.8)
    Turtle RT 20
  Next i
  WaitForKey()

  ' Test 11.4: Mandala
  TestCase("Mandala pattern")
  Turtle RESET
  Turtle HT
  For layer = 1 To 3
    For i = 1 To 12
      Turtle HOME
      Turtle SETHEADING(i * 30)
      Turtle FD(layer * 25)
      Turtle FILLCOLOR RGB((255 - layer * 60), (100 + layer * 40), 200)
      Turtle FILLPATTERN(layer * 5)
      Turtle FCIRCLE(20 - layer * 5)
    Next i
  Next layer
  WaitForKey()

  EndTest()
End Sub

' ============================================================================
' TEST 12: STRESS TESTS
' ============================================================================
Sub Test_Stress()
  Local i As INTEGER
  Local start_time As INTEGER
  Local p As INTEGER

  StartTest("Stress Tests")

  ' Test 12.1: Many small lines
  TestCase("1000 random lines")
  Turtle RESET
  Turtle HT
  start_time = Timer
  For i = 1 To 1000
    Turtle PU
    Turtle SETXY Rnd() * 300 + 10, (Rnd() * 220 + 10)
    Turtle PD
    Turtle SETHEADING Rnd() * 360
    Turtle FD 10
  Next i
  Print "Time: " + Str$(Timer - start_time) + " ms"
  WaitForKey()

  ' Test 12.2: Many circles
  TestCase("100 random circles")
  Turtle RESET
  Turtle HT
  start_time = Timer
  For i = 1 To 100
    Turtle PU
    Turtle SETXY Rnd() * 280 + 20, (Rnd() * 200 + 20)
    Turtle FILLCOLOR RGB((Rnd()*255), (Rnd()*255), (Rnd()*255))
    Turtle FILLPATTERN Rnd() * 8
    Turtle FCIRCLE Rnd() * 15 + 5
  Next i
  Print "Time: " + Str$(Timer - start_time) + " ms"
  WaitForKey()

  ' Test 12.3: Deep stack
  TestCase("Deep PUSH/POP stack (10 levels)")
  Turtle RESET
  Turtle HT
  For i = 1 To 10
    Turtle PUSH
    Turtle FD(i * 10)
    Turtle RT 36
  Next i
  For i = 1 To 10
    Turtle POP
    Pause 200
  Next i
  WaitForKey()

  ' Test 12.4: Large polygon
  TestCase("100-sided polygon fill")
  Turtle RESET
  Turtle HT
  Turtle FILLCOLOR RGB(200, 100, 255)
  Turtle FILLPATTERN 1
  start_time = Timer
  Turtle BEGIN_FILL
  For i = 1 To 100
    Turtle FD 3
    Turtle RT 3.6
  Next i
  Turtle END_FILL
  Print "Time: " + Str$(Timer - start_time) + " ms"
  WaitForKey()

  EndTest()
End Sub

' ============================================================================
' HELPER FUNCTIONS FOR COMPLEX DRAWINGS
' ============================================================================

Sub DrawBranch(length As INTEGER)
  If length < 10 Then Exit Sub

  Turtle FD length

  Turtle PUSH
  Turtle LT 30
  DrawBranch(length * 0.7)
  Turtle POP

  Turtle PUSH
  Turtle RT 30
  DrawBranch(length * 0.7)
  Turtle POP
End Sub

Sub DrawSnowflakeBranch(length As INTEGER)
  Turtle FD length
  Turtle PUSH
  Turtle RT 45
  Turtle FD(length * 0.4)
  Turtle POP
  Turtle PUSH
  Turtle LT 45
  Turtle FD(length * 0.4)
  Turtle POP
End Sub

Sub DrawFlower(x As INTEGER, y As INTEGER)
  Local i As INTEGER

  Turtle PU
  Turtle SETXY x, y
  Turtle PD

  ' Stem
  Turtle PENCOLOR RGB(50, 150, 50)
  Turtle PENWIDTH 2
  Turtle SETHEADING 0
  Turtle FD 30

  ' Petals
  Turtle FILLCOLOR RGB(255, 100, 150)
  For i = 1 To 6
    Turtle PUSH
    Turtle FCIRCLE 8
    Turtle POP
    Turtle RT 60
  Next i

  ' Center
  Turtle FILLCOLOR RGB(255, 255, 0)
  Turtle FCIRCLE 5

  Turtle PENWIDTH 1
End Sub

' ============================================================================
' TEST FRAMEWORK HELPERS
' ============================================================================

Sub StartTest(test_name$)
  CLS
  Print "=========================================="
  Print test_name$
  Print "=========================================="
  Print
  test_number = 0
End Sub

Sub EndTest()
  Print
  Print "Test section complete."
End Sub

Sub TestCase(description$)
  test_number = test_number + 1
  Print "Test " + Str$(test_number) + ": " + description$
End Sub

Sub WaitForKey()
  Print "  (Press any key for next test)"
  Do While Inkey$ = "": Loop
End Sub

Sub RunAllTests()
  Print "Running all tests..."
  Test_BasicMovement()
  Test_PenControl()
  Test_Position()
  Test_Circles()
  Test_Rectangles()
  Test_Arcs()
  Test_FillPatterns()
  Test_PolygonFill()
  Test_Stack()
  Test_Cursor()
  Test_ComplexDrawing()
  Test_Stress()
  Print
  Print "ALL TESTS COMPLETE!"
End Sub

' ============================================================================
' RUN THE TEST MENU
' ============================================================================
TestMenu()
                                                                    