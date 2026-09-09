' =====================================================================
'  Drawing the space view
'
'  Everything is redrawn into the off-screen buffer every frame, so none
'  of the original's XOR-erase and line-heap machinery is needed - only
'  the shapes it produced.
' =====================================================================

CONST NEARZ = 32                   ' nearer than this and nothing is drawn
CONST FARXY = 30000                ' Draw3D clamps its offsets at +-32766
CONST BANDDIV = 2048               ' distance -> the 0..31 visibility band
CONST NSTAR = 18                   ' stardust particles, as the original

DIM FLOAT stX(NSTAR-1), stY(NSTAR-1), stZ(NSTAR-1)

SUB DrawFrame
  CLS
  DrawStardust
  DrawPlanetSun
  DrawShips
  DrawDash
END SUB

' A ship is drawn as its mesh when it is close enough and an object is
' free for it, and as a single dot otherwise - which is exactly the
' original's rule, and the reason a busy bubble stays affordable.
SUB DrawShips
  LOCAL INTEGER n, px, py, band
  FOR n = 0 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 THEN
      ViewXform n
      IF tz > NEARZ THEN
        px = VCX + VPLANE * tx / tz
        py = VCY - VPLANE * ty / tz
        band = MaxAbs3(tx, ty, tz) \ BANDDIV
        IF sObj(n) > 0 AND band <= bVis(sBp(n)) AND ABS(tx) < FARXY AND ABS(ty) < FARXY THEN
          ViewOrient n
          Draw3D ROTATE qC(), sObj(n)
          Draw3D WRITE sObj(n), tx, ty, tz, 0, solidMode
        ELSE
          IF px >= 0 AND px < SCRW AND py >= 0 AND py < VIEWH THEN
            PIXEL px, py, RGB(WHITE)
          ENDIF
        ENDIF
      ENDIF
    ENDIF
  NEXT n
END SUB

FUNCTION MaxAbs3(a AS FLOAT, b AS FLOAT, c AS FLOAT) AS FLOAT
  MaxAbs3 = ABS(a)
  IF ABS(b) > MaxAbs3 THEN MaxAbs3 = ABS(b)
  IF ABS(c) > MaxAbs3 THEN MaxAbs3 = ABS(c)
END FUNCTION

' The planet and the sun are far too big to go through the 3D engine, so
' they are drawn as discs, exactly as the original does: an outline for
' the planet with its surface detail, a filled disc for the sun.
SUB DrawPlanetSun
  LOCAL INTEGER n, px, py, r
  FOR n = 0 TO 1
    IF sTyp(n) = T_PLANET OR sTyp(n) = T_CRATER OR sTyp(n) = T_SUN THEN
      ViewXform n
      IF tz > NEARZ THEN
        px = VCX + VPLANE * tx / tz
        py = VCY - VPLANE * ty / tz
        r = VPLANE * PRADIUS / tz
        IF r > 0 AND r < 2000 THEN
          IF px + r > 0 AND px - r < SCRW AND py + r > 0 AND py - r < VIEWH THEN
            IF sTyp(n) = T_SUN THEN
              CIRCLE px, py, r, 1, 1, RGB(WHITE), RGB(WHITE)
            ELSE
              CIRCLE px, py, r, 1, 1, RGB(WHITE), -1
            ENDIF
          ENDIF
        ENDIF
      ENDIF
    ENDIF
  NEXT n
END SUB

' Stardust: particles that stream past to show the ship is moving.  They
' live in their own screen-referred space rather than the world, which
' is why they can be cheap.
SUB InitStardust
  LOCAL INTEGER i
  FOR i = 0 TO NSTAR - 1
    NewStar i
    stZ(i) = 1 + RND * 255
  NEXT i
END SUB

SUB NewStar(i AS INTEGER)
  stX(i) = (RND * 2 - 1) * 116
  stY(i) = (RND * 2 - 1) * 116
  stZ(i) = 144 + RND * 111
END SUB

SUB DrawStardust
  LOCAL INTEGER i, px, py
  LOCAL FLOAT q
  FOR i = 0 TO NSTAR - 1
    ' move towards the viewer at the player's speed, and swing with the
    ' player's roll and pitch
    q = dSpeed / stZ(i)
    stZ(i) = stZ(i) - dSpeed * 0.25
    stX(i) = stX(i) + stX(i) * q
    stY(i) = stY(i) + stY(i) * q
    stY(i) = stY(i) + alpha * stX(i) * 4
    stX(i) = stX(i) - alpha * stY(i) * 4
    stY(i) = stY(i) - beta * 256
    IF ABS(stX(i)) >= 116 OR ABS(stY(i)) >= 116 OR stZ(i) < 16 THEN NewStar i
    px = VCX + stX(i)
    py = VCY - stY(i)
    IF px >= 0 AND px < SCRW AND py >= 0 AND py < VIEWH THEN
      PIXEL px, py, RGB(WHITE)
    ENDIF
  NEXT i
END SUB
