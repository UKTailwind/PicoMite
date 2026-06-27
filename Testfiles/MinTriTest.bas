' Minimal TRIANGLE array test
OPTION EXPLICIT
OPTION DEFAULT NONE

DIM FLOAT tx1(4), ty1(4), tx2(4), ty2(4), tx3(4), ty3(4)
DIM INTEGER i

PRINT "Initializing arrays..."
FOR i = 0 TO 4
  tx1(i) = 30 + i * 70
  ty1(i) = 120
  tx2(i) = 60 + i * 70
  ty2(i) = 80
  tx3(i) = 90 + i * 70
  ty3(i) = 120
  PRINT "i=";i;" tx1=";tx1(i);" ty1=";ty1(i)
NEXT i

PRINT "Drawing triangles..."
TRIANGLE tx1(), ty1(), tx2(), ty2(), tx3(), ty3(), RGB(RED)
PRINT "Done!"
