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
  ' Every one of these has to be one of the sixteen the screen actually has,
  ' or it is rounded to the nearest and rarely to the one you meant: GRAY,
  ' which was here, is not in the palette at all.  In practice the ships are
  ' drawn entirely in col(0), because every blueprint edge is colour 0.
  col(0) = RGB(WHITE) : col(1) = RGB(MIDGREEN) : col(2) = RGB(BLUE)
  col(3) = RGB(GREEN) : col(4) = RGB(RED) : col(5) = RGB(MAGENTA)
  col(6) = RGB(CYAN)
  ' Pre-resolved so the drawing loops assign a variable rather than call
  ' RGB(), which the trace cache cannot compile.
  cGreen = RGB(GREEN) : cYellow = RGB(YELLOW) : cWhite = RGB(WHITE)
  cBlack = RGB(BLACK) : cCyan = RGB(CYAN)
  ' Secondary text and the outlines of the scanner and compass.  This was
  ' RGB(64, 64, 64), which a sixteen colour screen rounds to MYRTLE - a
  ' green so dark it can barely be read.
  cDim = RGB(MIDGREEN)
  ' The band behind the chosen row.  RGB(32, 32, 64) was rounded to black,
  ' so the selection could not be seen at all.
  cSel = RGB(BLUE)
  cRed = RGB(RED)
  ' One turn of the unit circle, for the planet's surface ellipses.
  LOCAL INTEGER k
  FOR k = 0 TO NSEG - 1
    ctab(k) = COS(2 * PI * k / NSEG)
    stab(k) = SIN(2 * PI * k / NSEG)
  NEXT k
  ' Bar rows, converted from the original's character rows.  Left column:
  ' forward shield, aft shield, fuel, cabin temperature, laser temperature,
  ' altitude.  Right column: speed, roll, dive/climb, four energy banks.
  DLY(0) = 178 : DLY(1) = 186 : DLY(2) = 194
  DLY(3) = 202 : DLY(4) = 210 : DLY(5) = 218
  DRY(0) = 178 : DRY(1) = 185 : DRY(2) = 193 : DRY(3) = 202
  DRY(4) = 210 : DRY(5) = 218 : DRY(6) = 226
  LLAB$(0) = "FS" : LLAB$(1) = "AS" : LLAB$(2) = "FU"
  LLAB$(3) = "CT" : LLAB$(4) = "LT" : LLAB$(5) = "AL"
  RLAB$(0) = "SP" : RLAB$(1) = "RL" : RLAB$(2) = "DC"
  RLAB$(3) = "1" : RLAB$(4) = "2" : RLAB$(5) = "3" : RLAB$(6) = "4"
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
