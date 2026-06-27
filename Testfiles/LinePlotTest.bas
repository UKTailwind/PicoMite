' Drawing Commands Stride Test Program
' Tests both standard arrays and struct arrays for all drawing commands
' Display size: MM.HRES x MM.VRES

OPTION EXPLICIT
OPTION DEFAULT NONE
mode 2

' Define point structures for testing
TYPE Point
  x AS FLOAT
  y AS FLOAT
END TYPE

TYPE IPoint
  x AS INTEGER
  y AS INTEGER
END TYPE

TYPE RectDef
  x AS FLOAT
  y AS FLOAT
  w AS FLOAT
  h AS FLOAT
END TYPE

TYPE CircleDef
  x AS FLOAT
  y AS FLOAT
  r AS FLOAT
END TYPE

TYPE TriDef
  x1 AS FLOAT
  y1 AS FLOAT
  x2 AS FLOAT
  y2 AS FLOAT
  x3 AS FLOAT
  y3 AS FLOAT
END TYPE

DIM INTEGER i, numPoints, numShapes
DIM FLOAT yValues(100)
DIM Point points(100)

numPoints = 50
numShapes = 10

' Initialize test data - a sine wave pattern
FOR i = 0 TO numPoints - 1
  yValues(i) = MM.VRES/2 + SIN(i * 0.2) * (MM.VRES/4)
  points(i).x = i * (MM.HRES / numPoints)
  points(i).y = MM.VRES/2 + SIN(i * 0.2) * (MM.VRES/4)
NEXT i

CLS
PRINT "=== Drawing Commands Stride Test ==="
PRINT

'------------------------------------------------------------------------
' Test 1: LINE PLOT
'------------------------------------------------------------------------
PRINT "Test 1: LINE PLOT"
PRINT "Red = standard array, Green = struct array"
LINE PLOT yValues(), numPoints, 0, MM.HRES/numPoints, 0, 1, RGB(RED)
LINE PLOT points().y, numPoints, 0, MM.HRES/numPoints, 0, 1, RGB(GREEN)
PRINT "Press any key..."
DO : LOOP UNTIL INKEY$ <> ""

'------------------------------------------------------------------------
' Test 2: PIXEL
'------------------------------------------------------------------------
CLS
PRINT "Test 2: PIXEL command"
DIM FLOAT px(49), py(49)
DIM Point pixelPts(49)

FOR i = 0 TO 49
  px(i) = 50 + i * 4
  py(i) = 100 + SIN(i * 0.3) * 30
  pixelPts(i).x = 50 + i * 4
  pixelPts(i).y = 150 + SIN(i * 0.3) * 30
NEXT i

PRINT "Red dots = standard arrays, Green dots = struct arrays"
PIXEL px(), py(), RGB(RED)
PIXEL pixelPts().x, pixelPts().y, RGB(GREEN)
PRINT "Press any key..."
DO : LOOP UNTIL INKEY$ <> ""

'------------------------------------------------------------------------
' Test 3: LINE (array mode)
'------------------------------------------------------------------------
CLS
PRINT "Test 3: LINE command (array mode)"
DIM FLOAT lx1(9), ly1(9), lx2(9), ly2(9)
DIM Point lineStart(9), lineEnd(9)

FOR i = 0 TO 9
  lx1(i) = 50 + i * 30
  ly1(i) = 80
  lx2(i) = 50 + i * 30
  ly2(i) = 120
  lineStart(i).x = 50 + i * 30
  lineStart(i).y = 140
  lineEnd(i).x = 50 + i * 30
  lineEnd(i).y = 180
NEXT i

PRINT "Red lines = standard arrays, Green lines = struct arrays"
LINE lx1(), ly1(), lx2(), ly2(),, RGB(RED)
LINE lineStart().x, lineStart().y, lineEnd().x, lineEnd().y,, RGB(GREEN)
PRINT "Press any key..."
DO : LOOP UNTIL INKEY$ <> ""

'------------------------------------------------------------------------
' Test 4: CIRCLE
'------------------------------------------------------------------------
CLS
PRINT "Test 4: CIRCLE command"
DIM FLOAT cx(9), cy(9), cr(9)
DIM CircleDef circles(9)

FOR i = 0 TO 9
  cx(i) = 50 + i * 35
  cy(i) = 100
  cr(i) = 12
  circles(i).x = 50 + i * 35
  circles(i).y = 160
  circles(i).r = 12
NEXT i

PRINT "Red circles = standard arrays, Green circles = struct arrays"
CIRCLE cx(), cy(), cr(),,,RGB(RED)
CIRCLE circles().x, circles().y, circles().r,,,RGB(GREEN)
PRINT "Press any key..."
DO : LOOP UNTIL INKEY$ <> ""

'------------------------------------------------------------------------
' Test 5: BOX
'------------------------------------------------------------------------
CLS
PRINT "Test 5: BOX command"
DIM FLOAT bx(9), by(9), bw(9), bh(9)
DIM RectDef boxes(9)

FOR i = 0 TO 9
  bx(i) = 20 + i * 38
  by(i) = 80
  bw(i) = 30
  bh(i) = 25
  boxes(i).x = 20 + i * 38
  boxes(i).y = 140
  boxes(i).w = 30
  boxes(i).h = 25
NEXT i

PRINT "Red boxes = standard arrays, Green boxes = struct arrays"
BOX bx(), by(), bw(), bh(),, RGB(RED)
BOX boxes().x, boxes().y, boxes().w, boxes().h,, RGB(GREEN)
PRINT "Press any key..."
DO : LOOP UNTIL INKEY$ <> ""

'------------------------------------------------------------------------
' Test 6: RBOX (rounded box)
'------------------------------------------------------------------------
CLS
PRINT "Test 6: RBOX command"
DIM FLOAT rbx(9), rby(9), rbw(9), rbh(9)
DIM RectDef rboxes(9)

FOR i = 0 TO 9
  rbx(i) = 20 + i * 38
  rby(i) = 80
  rbw(i) = 30
  rbh(i) = 25
  rboxes(i).x = 20 + i * 38
  rboxes(i).y = 140
  rboxes(i).w = 30
  rboxes(i).h = 25
NEXT i

PRINT "Red rboxes = standard arrays, Green rboxes = struct arrays"
RBOX rbx(), rby(), rbw(), rbh(), 5, RGB(RED)
RBOX rboxes().x, rboxes().y, rboxes().w, rboxes().h, 5, RGB(GREEN)
PRINT "Press any key..."
DO : LOOP UNTIL INKEY$ <> ""

'------------------------------------------------------------------------
' Test 7: TRIANGLE
'------------------------------------------------------------------------
CLS
PRINT "Test 7: TRIANGLE command"
DIM FLOAT tx1(4), ty1(4), tx2(4), ty2(4), tx3(4), ty3(4)
DIM TriDef triangles(4)

FOR i = 0 TO 4
  tx1(i) = 30 + i * 70
  ty1(i) = 120
  tx2(i) = 60 + i * 70
  ty2(i) = 80
  tx3(i) = 90 + i * 70
  ty3(i) = 120
  triangles(i).x1 = 30 + i * 70
  triangles(i).y1 = 200
  triangles(i).x2 = 60 + i * 70
  triangles(i).y2 = 160
  triangles(i).x3 = 90 + i * 70
  triangles(i).y3 = 200
NEXT i

PRINT "Red triangles = standard arrays, Green triangles = struct arrays"
TRIANGLE tx1(), ty1(), tx2(), ty2(), tx3(), ty3(), RGB(RED)
TRIANGLE triangles().x1, triangles().y1, triangles().x2, triangles().y2, triangles().x3, triangles().y3, RGB(GREEN)
PRINT "Press any key..."
DO : LOOP UNTIL INKEY$ <> ""

'------------------------------------------------------------------------
' Test 8: POLYGON
'------------------------------------------------------------------------
CLS
PRINT "Test 8: POLYGON command"
DIM FLOAT polyX(5), polyY(5)
DIM Point polyPts(5)

' Pentagon vertices for standard arrays (upper)
FOR i = 0 TO 4
  polyX(i) = 100 + COS(i * 2 * 3.14159 / 5 - 3.14159/2) * 40
  polyY(i) = 100 + SIN(i * 2 * 3.14159 / 5 - 3.14159/2) * 40
NEXT i

' Pentagon vertices for struct arrays (lower)
FOR i = 0 TO 4
  polyPts(i).x = 100 + COS(i * 2 * 3.14159 / 5 - 3.14159/2) * 40
  polyPts(i).y = 180 + SIN(i * 2 * 3.14159 / 5 - 3.14159/2) * 40
NEXT i

' Second polygon (hexagon) for standard arrays
DIM FLOAT polyX2(6), polyY2(6)
DIM Point polyPts2(6)

FOR i = 0 TO 5
  polyX2(i) = 250 + COS(i * 2 * 3.14159 / 6) * 40
  polyY2(i) = 100 + SIN(i * 2 * 3.14159 / 6) * 40
  polyPts2(i).x = 250 + COS(i * 2 * 3.14159 / 6) * 40
  polyPts2(i).y = 180 + SIN(i * 2 * 3.14159 / 6) * 40
NEXT i

PRINT "Red = standard arrays, Green = struct arrays"
PRINT "Left: Pentagon, Right: Hexagon"
POLYGON 5, polyX(), polyY(), RGB(RED)
POLYGON 5, polyPts().x, polyPts().y, RGB(GREEN)
POLYGON 6, polyX2(), polyY2(), RGB(RED)
POLYGON 6, polyPts2().x, polyPts2().y, RGB(GREEN)
PRINT "Press any key..."
DO : LOOP UNTIL INKEY$ <> ""

'------------------------------------------------------------------------
' Test 9: POLYGON with fill
'------------------------------------------------------------------------
CLS
PRINT "Test 9: POLYGON with fill"
DIM FLOAT fillPolyX(4), fillPolyY(4)
DIM Point fillPolyPts(4)

' Square vertices for standard arrays
fillPolyX(0) = 80: fillPolyY(0) = 80
fillPolyX(1) = 140: fillPolyY(1) = 80
fillPolyX(2) = 140: fillPolyY(2) = 140
fillPolyX(3) = 80: fillPolyY(3) = 140

' Square vertices for struct arrays
fillPolyPts(0).x = 80: fillPolyPts(0).y = 160
fillPolyPts(1).x = 140: fillPolyPts(1).y = 160
fillPolyPts(2).x = 140: fillPolyPts(2).y = 220
fillPolyPts(3).x = 80: fillPolyPts(3).y = 220

' Triangle for standard arrays
DIM FLOAT fillTriX(3), fillTriY(3)
DIM Point fillTriPts(3)

fillTriX(0) = 200: fillTriY(0) = 140
fillTriX(1) = 260: fillTriY(1) = 80
fillTriX(2) = 320: fillTriY(2) = 140

fillTriPts(0).x = 200: fillTriPts(0).y = 220
fillTriPts(1).x = 260: fillTriPts(1).y = 160
fillTriPts(2).x = 320: fillTriPts(2).y = 220

PRINT "Top row = standard arrays, Bottom row = struct arrays"
PRINT "Red outline/Yellow fill"
POLYGON 4, fillPolyX(), fillPolyY(), RGB(RED), RGB(YELLOW)
POLYGON 4, fillPolyPts().x, fillPolyPts().y, RGB(RED), RGB(YELLOW)
POLYGON 3, fillTriX(), fillTriY(), RGB(RED), RGB(YELLOW)
POLYGON 3, fillTriPts().x, fillTriPts().y, RGB(RED), RGB(YELLOW)
PRINT "Press any key..."
DO : LOOP UNTIL INKEY$ <> ""

'------------------------------------------------------------------------
' Test 10: Mixed - all shapes with struct arrays
'------------------------------------------------------------------------
CLS
PRINT "Test 10: All shapes using struct member arrays"

' Draw various shapes
PIXEL pixelPts().x, pixelPts().y, RGB(WHITE)
LINE lineStart().x, lineStart().y, lineEnd().x, lineEnd().y,, RGB(CYAN)
CIRCLE circles().x, circles().y, circles().r,,,RGB(YELLOW)
BOX boxes().x, boxes().y, boxes().w, boxes().h,, RGB(MAGENTA)
TRIANGLE triangles().x1, triangles().y1, triangles().x2, triangles().y2, triangles().x3, triangles().y3, RGB(GREEN)
POLYGON 5, polyPts().x, polyPts().y, RGB(RED)

PRINT
PRINT "All tests complete!"
PRINT "If shapes appear correctly, stride support is working."

END
