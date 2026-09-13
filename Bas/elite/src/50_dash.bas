' =====================================================================
'  The dashboard, the 3D scanner and the compass
'
'  Geometry taken from the original's screen addresses, converted with
'  x_ours = ROUND(x_bbc * 1.25) and y_ours = y_bbc - 16.  The BBC's
'  dashboard is character rows 24 to 30, i.e. BBC y 192..247, landing on
'  our rows 176..231; row 31 exists in memory but the video chip never
'  displays it, so our last eight rows have no counterpart and are free.
'
'  Note which column is which: speed, roll, dive/climb and the four
'  energy banks are on the RIGHT, and the six status bars on the LEFT.
'
'  Colour follows the original's threshold rule, which runs in opposite
'  directions for different indicators.  A high reading is dangerous for
'  speed and the two temperatures, so those turn red at the top of their
'  range; a low reading is dangerous for energy, shields and fuel, so
'  those turn red at the bottom.  Altitude never changes colour.
'
'  The whole screen is cleared each frame, because the planet's disc and
'  the outermost stardust reach below the space view and the drawing
'  primitives clip to the screen rather than to a region.  The fixed
'  artwork is therefore laid down again every frame, but by restoring a
'  photograph of it rather than by redrawing it.
' =====================================================================

' Drawn once, then read straight out of the framebuffer into an array.
' Every later call puts it back with one word-aligned memory copy of the
' 64 rows, a fraction of the cost of redrawing the labels and frames.
' fadd caches the write buffer's address, so this holds only while the
' framebuffer stays the write target - which it does for the whole game.
SUB DashStatic
  STATIC INTEGER wordcount = (SCRH - DASHY) * SCRW \ 16
  STATIC INTEGER store(wordcount - 1)
  STATIC INTEGER addr = 0, fadd = 0
  IF fadd = 0 THEN
    LOCAL INTEGER i
    addr = PEEK(VARADDR store())
    fadd = MM.INFO(WRITEBUFF) + DASHY * SCRW / 2
    BOX 0, DASHY, SCRW, SCRH - DASHY, 0, cBlack, cBlack
    LINE 0, DASHY, SCRW - 1, DASHY, 1, cCyan
    ' The labels go in the character blocks the original never writes to:
    ' ours x 0..19 left of the left column, x 300..319 right of the right.
    FOR i = 0 TO 5
      TEXT 17, DLY(i) - 1, LLAB$(i), "RT", 7, 1, cWhite
      BOX DL, DLY(i) - 1, DW + 2, 5, 1, cDim, -1
    NEXT i
    FOR i = 0 TO 6
      TEXT 303, DRY(i) - 1, RLAB$(i), "LT", 7, 1, cWhite
      BOX DR, DRY(i) - 1, DW + 2, 5, 1, cDim, -1
    NEXT i
    CIRCLE CPX, CPY, CPR + 2, 1, 1.25, cDim, -1
    MEMORY COPY INTEGER fadd, addr, wordcount
  ELSE
    MEMORY COPY INTEGER addr, fadd, wordcount
  ENDIF
END SUB

SUB DrawDash
  LOCAL INTEGER i, e
  DashStatic
  ' --- right column: what the ship is doing
  Bar DR, DRY(0), dSpeed \ 2, 14, cRed, cYellow             ' speed
  Pointer DR, DRY(1), 8 + alp2 * (alp1 \ 4)                 ' roll
  Pointer DR, DRY(2), 8 + bet2 * bet1                       ' dive / climb
  FOR i = 0 TO 3
    e = (pEnergy \ 4) - (3 - i) * 16
    IF e < 0 THEN e = 0
    IF e > 16 THEN e = 16
    Bar DR, DRY(3 + i), e, 3, cYellow, cRed                 ' the four banks
  NEXT i
  ' --- left column: what the ship has left
  Bar DL, DLY(0), pFsh \ 16, 3, cYellow, cRed
  Bar DL, DLY(1), pAsh \ 16, 3, cYellow, cRed
  Bar DL, DLY(2), pFuel \ 4, 3, cYellow, cRed
  Bar DL, DLY(3), pCabT \ 16, 11, cRed, cYellow
  Bar DL, DLY(4), pLasT \ 16, 11, cRed, cYellow
  Bar DL, DLY(5), pAltit \ 16, 99, cRed, cYellow            ' 99 is unreachable
  MissileBlocks
  DrawScanner
  DrawCompass
END SUB

' A bar of length lv on the original's 0..16 scale.  It takes colour hi at
' or above the threshold and lo below it, which is how one routine serves
' both the "high is bad" and the "low is bad" indicators.
SUB Bar(x AS INTEGER, y AS INTEGER, lv AS INTEGER, t1 AS INTEGER, hi AS INTEGER, lo AS INTEGER)
  LOCAL INTEGER w, c, v
  v = lv
  IF v < 0 THEN v = 0
  IF v > 16 THEN v = 16
  c = lo
  IF v >= t1 THEN c = hi
  w = v * 2.5
  BOX x + 1, y, DW, 3, 0, cBlack, cBlack
  IF w > 0 THEN BOX x + 1, y, w, 3, 0, c, c
END SUB

' Roll and dive/climb are centre-zero: one marker sliding over sixteen
' positions with the centre at 8, and four rows deep rather than three.
SUB Pointer(x AS INTEGER, y AS INTEGER, p AS INTEGER)
  LOCAL INTEGER v
  v = p
  IF v < 0 THEN v = 0
  IF v > 15 THEN v = 15
  BOX x + 1, y, DW, 4, 0, cBlack, cBlack
  BOX x + 1 + v * 2.5, y, 3, 4, 0, cYellow, cYellow
END SUB

' Four missile blocks, filled from the left as missiles are carried.
SUB MissileBlocks
  LOCAL INTEGER i, c
  FOR i = 0 TO 3
    c = cBlack
    IF i < pMissl THEN c = cGreen
    BOX 20 + i * 10, 225, 7, 5, 0, c, c
  NEXT i
END SUB

' The scanner.  Each contact is a dash with a stick down to the plane of
' the ellipse, so the ellipse reads as the plane the player is flying in
' and the stick shows how far above or below it the contact sits.  It is
' always drawn in front-view coordinates whichever way the player is
' looking, exactly as the original does it.
SUB DrawScanner
  LOCAL INTEGER n, px, py, base, c
  BOX SCX - SCA, SCY - SCB - 9, 2 * SCA, 2 * SCB + 18, 0, cBlack, cBlack
  ' CIRCLE takes the vertical radius; its aspect is width over height.
  CIRCLE SCX, SCY, SCB, 1, SCA / SCB, cCyan, -1
  FOR n = 0 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 THEN
      ' A contact shows only while all three high bytes are 63 or less.
      IF ABS(sX(n)) < 16384 AND ABS(sY(n)) < 16384 AND ABS(sZ(n)) < 16384 THEN
        px = SCDOTX + sX(n) / SCXDIV
        base = SCY - sZ(n) / SCZDIV
        py = base - sY(n) / SCYDIV
        IF py < SCTOP THEN py = SCTOP
        IF py > SCBOT THEN py = SCBOT
        IF px > SCX - SCA AND px < SCX + SCA THEN
          c = cGreen
          IF sTyp(n) = T_MISSILE THEN c = cYellow
          LINE px, base, px, py, 1, c
          BOX px, py - 1, 5, 2, 0, c, c
        ENDIF
      ENDIF
    ENDIF
  NEXT n
END SUB

' The compass points at the station inside the safe zone and at the planet
' outside it.  Ahead it is a yellow dash two rows deep, behind it a green
' one a single row deep: the original tells the two apart by height and
' colour together, not by filling or hollowing one marker.
SUB DrawCompass
  LOCAL INTEGER n, px, py
  LOCAL FLOAT m
  n = SLOT_PLANET
  IF inSafe AND sTyp(SLOT_STAR) = T_STATION THEN n = SLOT_STAR
  IF sTyp(n) = 0 THEN EXIT SUB
  m = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
  IF m < 1 THEN EXIT SUB
  BOX CPX - CPR - 4, CPY - CPR - 3, 2 * CPR + 9, 2 * CPR + 7, 0, cBlack, cBlack
  CIRCLE CPX, CPY, CPR + 2, 1, 1.25, cDim, -1
  px = CPX + CPR * 1.25 * sX(n) / m
  py = CPY - CPR * sY(n) / m
  IF sZ(n) >= 0 THEN
    BOX px, py, 3, 2, 0, cYellow, cYellow
  ELSE
    BOX px, py, 3, 1, 0, cGreen, cGreen
  ENDIF
END SUB

SUB ViewName
  LOCAL v$
  SELECT CASE vw
    CASE 0 : v$ = "Front view"
    CASE 1 : v$ = "Rear view"
    CASE 2 : v$ = "Left view"
    CASE 3 : v$ = "Right view"
  END SELECT
  ' BBC text column 11, row 1: x = 11 * 8 * 1.25, y = 8.
  TEXT 110, 8, v$, "LT", 7, 1, cWhite
END SUB
