' =====================================================================
'  Screen, camera, view and object-pool set up
' =====================================================================
SUB SetupScreen
  MODE 2
  FRAMEBUFFER CREATE
  FRAMEBUFFER WRITE F
  ' Draw3D's own centre is (W/2, H/2-1); pany lifts it to the space
  ' view's centre so ships sit above the dashboard, not behind it.
  Draw3D CAMERA 1, VPLANE, 0, 0, 0, PANY
  col(0) = RGB(WHITE) : col(1) = RGB(GRAY) : col(2) = RGB(BLUE)
  col(3) = RGB(GREEN) : col(4) = RGB(RED) : col(5) = RGB(MAGENTA)
  col(6) = RGB(CYAN)
  ' Pre-resolved so the drawing loops assign a variable rather than call
  ' RGB(), which the trace cache cannot compile.
  cGreen = RGB(GREEN) : cYellow = RGB(YELLOW) : cWhite = RGB(WHITE)
  cBlack = RGB(BLACK) : cCyan = RGB(CYAN) : cGrey = RGB(64, 64, 64)
END SUB

' The four views are rotations of the whole universe about the vertical
' axis: front none, rear 180, left +90, right -90.  Positions are
' transformed by hand in ViewXform (three assignments beat a quaternion
' multiply); orientations are pre-multiplied by these.
SUB SetupViews
  LOCAL INTEGER i
  MATH Q_EULER 0, 0, 0, qA()      : FOR i = 0 TO 4 : vwQ(i, 0) = qA(i) : NEXT i
  MATH Q_CREATE RAD(180), 0, 1, 0, qA() : FOR i = 0 TO 4 : vwQ(i, 1) = qA(i) : NEXT i
  MATH Q_CREATE RAD(90), 0, 1, 0, qA()  : FOR i = 0 TO 4 : vwQ(i, 2) = qA(i) : NEXT i
  MATH Q_CREATE RAD(-90), 0, 1, 0, qA() : FOR i = 0 TO 4 : vwQ(i, 3) = qA(i) : NEXT i
END SUB

' How many Draw3D objects does this firmware allow?  MAX3D was 8 and is
' 12 in the current build; creating one past the limit raises an error,
' so ask rather than assume.
SUB ProbeObjects
  LOCAL INTEGER n
  LoadMesh 0
  maxObj = 8
  FOR n = 9 TO 15
    ON ERROR SKIP 1
    Draw3D CREATE n, bNv(0), bNf(0), 1, mV(), mFc(), mF(), col(), mEc()
    IF MM.ERRNO <> 0 THEN EXIT FOR
    Draw3D CLOSE n
    maxObj = n
  NEXT n
  ON ERROR CLEAR
  FOR n = 0 TO 15 : objOwn(n) = -1 : NEXT n
END SUB

SUB CloseAll
  LOCAL INTEGER n
  FOR n = 1 TO maxObj
    IF objOwn(n) >= 0 THEN Draw3D CLOSE n
    objOwn(n) = -1
  NEXT n
END SUB
