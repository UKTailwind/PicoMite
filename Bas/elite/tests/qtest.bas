' qtest.bas - pin down MMBasic's quaternion conventions before the flight
' code depends on them.  Everything here has a right answer that depends on
' the firmware, not on Elite, so it only has to be run once.
'
'   1 which way MATH Q_CREATE turns a vector
'   2 what MATH Q_EULER's three arguments actually rotate about
'   3 which order MATH Q_MULT composes in
'   4 that Draw3D ROTATE agrees with MATH Q_ROTATE
OPTION EXPLICIT
OPTION BASE 0
DIM FLOAT q(4), q2(4), qm(4), v(4), r(4)
DIM INTEGER i

PRINT "MMBasic "; MM.VER

' --- 1. Q_CREATE: right hand rule or left?
PRINT
PRINT "1. Q_CREATE 90 deg about Y, applied to the three axes"
MATH Q_CREATE RAD(90), 0, 1, 0, q()
ShowRot "  x axis (1,0,0)", 1, 0, 0
ShowRot "  y axis (0,1,0)", 0, 1, 0
ShowRot "  z axis (0,0,1)", 0, 0, 1
PRINT "  right hand rule about +Y sends (1,0,0) to (0,0,-1)"
PRINT "  left  hand rule about +Y sends (1,0,0) to (0,0,+1)"

' --- 2. Q_EULER: which argument is which axis?
PRINT
PRINT "2. Q_EULER with one argument at 90 deg at a time, applied to (0,0,1)"
MATH Q_EULER RAD(90), 0, 0, q() : ShowRot "  arg1=90  (0,0,1)", 0, 0, 1
MATH Q_EULER 0, RAD(90), 0, q() : ShowRot "  arg2=90  (0,0,1)", 0, 0, 1
MATH Q_EULER 0, 0, RAD(90), q() : ShowRot "  arg3=90  (0,0,1)", 0, 0, 1
PRINT "  and applied to (1,0,0)"
MATH Q_EULER RAD(90), 0, 0, q() : ShowRot "  arg1=90  (1,0,0)", 1, 0, 0
MATH Q_EULER 0, RAD(90), 0, q() : ShowRot "  arg2=90  (1,0,0)", 1, 0, 0
MATH Q_EULER 0, 0, RAD(90), q() : ShowRot "  arg3=90  (1,0,0)", 1, 0, 0

' --- 3. Q_MULT: does MULT a,b apply b first or a first?
PRINT
PRINT "3. Q_MULT: A = 90 about Y, B = 90 about X, applied to (0,0,1)"
MATH Q_CREATE RAD(90), 0, 1, 0, q()
MATH Q_CREATE RAD(90), 1, 0, 0, q2()
MATH Q_VECTOR 0, 0, 1, v()
MATH Q_ROTATE q(), v(), r()  : PRINT "  A alone      "; Vec$(r())
MATH Q_ROTATE q2(), v(), r() : PRINT "  B alone      "; Vec$(r())
MATH Q_MULT q(), q2(), qm()
MATH Q_ROTATE qm(), v(), r() : PRINT "  MULT A,B     "; Vec$(r())
MATH Q_MULT q2(), q(), qm()
MATH Q_ROTATE qm(), v(), r() : PRINT "  MULT B,A     "; Vec$(r())
MATH Q_ROTATE q2(), v(), r()
MATH Q_ROTATE q(), r(), r()  : PRINT "  A applied to B(v) "; Vec$(r())

' --- 4. does Draw3D agree?  One flat triangle in the xy plane, spun 90
'     degrees about Y: if Draw3D matches MATH the corner lands where
'     Q_ROTATE says it does.  Reported as the object's screen extent.
PRINT
PRINT "4. Draw3D ROTATE vs MATH Q_ROTATE"
DIM FLOAT tv(2, 2)
DIM INTEGER tfc(0), tf(2), tcol(0), tec(0)
tv(0,0) = 100 : tv(1,0) = 0  : tv(2,0) = 0
tv(0,1) = 0   : tv(1,1) = 60 : tv(2,1) = 0
tv(0,2) = 0   : tv(1,2) = 0  : tv(2,2) = 100
tfc(0) = 3 : tf(0) = 0 : tf(1) = 1 : tf(2) = 2
tcol(0) = RGB(WHITE) : tec(0) = 0
MODE 2
FRAMEBUFFER CREATE
FRAMEBUFFER WRITE F
Draw3D CAMERA 1, 256, 0, 0, 0, -31
Draw3D CREATE 1, 3, 1, 1, tv(), tfc(), tf(), tcol(), tec()
MATH Q_EULER 0, 0, 0, q() : q(4) = 1
Draw3D SHOW 1, 0, 0, 1000, 1
PRINT "  unrotated   xmin"; DRAW3D(XMIN 1); " xmax"; DRAW3D(XMAX 1); " ymin"; DRAW3D(YMIN 1); " ymax"; DRAW3D(YMAX 1)
MATH Q_CREATE RAD(90), 0, 1, 0, q() : q(4) = 1
Draw3D ROTATE q(), 1
Draw3D SHOW 1, 0, 0, 1000, 1
PRINT "  Y+90        xmin"; DRAW3D(XMIN 1); " xmax"; DRAW3D(XMAX 1); " ymin"; DRAW3D(YMIN 1); " ymax"; DRAW3D(YMAX 1)
PRINT "  vertex 0 (100,0,0) rotates to "; : MATH Q_VECTOR 100, 0, 0, v() : MATH Q_ROTATE q(), v(), r() : PRINT Vec$(r())
PRINT "  centre of the view is x=160 y=88; screen x = 160 + 256*X/Z"
Draw3D CLOSE 1
FRAMEBUFFER CLOSE
MODE 1
PRINT
PRINT "qtest done"
END

SUB ShowRot(t$, x AS FLOAT, y AS FLOAT, z AS FLOAT)
  MATH Q_VECTOR x, y, z, v()
  MATH Q_ROTATE q(), v(), r()
  PRINT t$; " -> "; Vec$(r())
END SUB

FUNCTION Vec$(a() AS FLOAT)
  Vec$ = "(" + STR$(a(1) * a(4), 2, 3) + "," + STR$(a(2) * a(4), 2, 3) + "," + STR$(a(3) * a(4), 2, 3) + ")"
END FUNCTION
