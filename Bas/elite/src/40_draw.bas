' =====================================================================
'  Drawing the space view
'
'  Everything is redrawn into the off-screen buffer every frame, so none
'  of the original's XOR-erase and line-heap machinery is needed - only
'  the shapes it produced.
' =====================================================================

SUB DrawFrame
  LOCAL FLOAT t
  IF PROFILE THEN
    t = TIMER : CLS           : prof(0) = prof(0) + TIMER - t
    t = TIMER : DrawStardust  : prof(1) = prof(1) + TIMER - t
    t = TIMER : DrawPlanetSun : prof(2) = prof(2) + TIMER - t
    t = TIMER : DrawShips : SpaceFurniture : prof(3) = prof(3) + TIMER - t
    t = TIMER : DrawDash      : prof(4) = prof(4) + TIMER - t
    ViewName
  ELSE
    CLS
    DrawStardust
    DrawPlanetSun
    DrawShips
    SpaceFurniture
    DrawDash
    ViewName
  ENDIF
END SUB


' Whether a ship is drawn at all, and as a mesh or a dot, is decided the
' way the original decides it - on the high byte of z alone, not on range.
' Outside a 45 degree cone nothing is drawn even though our wider screen
' could show it; beyond z_hi 192 nothing is drawn either.  Between those,
' the blueprint's visibility byte picks mesh or dot, except below z_hi 16
' where the mesh always wins.
SUB DrawShips
  LOCAL INTEGER n, px, py, zb
  FOR n = 0 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 THEN
      ViewXform n
      IF tz > NEARZ THEN
        zb = tz \ ZHI
        IF zb < VISCUT AND ABS(tx) < tz AND ABS(ty) < tz THEN
          IF zb >= VISFLOOR AND zb > bVis(sBp(n)) THEN
            ' Too far for a mesh.  The original's distant ship is not a
            ' single pixel but a short dash two rows deep, sitting one
            ' pixel right of the projected point.
            px = VCX + SGN(tx) * ((VPLANE * ABS(tx)) \ tz)
            py = VCY - SGN(ty) * ((VPLANE * ABS(ty)) \ tz)
            IF py > 0 AND py < VIEWH - 2 THEN BOX px + 1, py, 3, 2, 0, cWhite, cWhite
          ELSE
            IF sObj(n) > 0 AND ABS(tx) < FARXY AND ABS(ty) < FARXY THEN
              ViewOrient n
              Draw3D ROTATE qC(), sObj(n)
              Draw3D WRITE sObj(n), tx, ty, tz, 0, solidMode
            ENDIF
          ENDIF
        ENDIF
      ENDIF
    ENDIF
  NEXT n
END SUB

' The original frames the space view with a two pixel border, and puts
' crosshairs at the centre of any view that has a laser fitted.
SUB SpaceFurniture
  LINE 0, 0, SCRW - 2, 0, 1, cWhite
  BOX 0, 0, 2, VIEWH, 0, cWhite, cWhite
  BOX SCRW - 2, 0, 2, VIEWH, 0, cWhite, cWhite
  IF vw = 0 THEN
    LINE VCX - 25, VCY, VCX - 12, VCY, 1, cWhite
    LINE VCX + 12, VCY, VCX + 25, VCY, 1, cWhite
    LINE VCX, VCY - 20, VCX, VCY - 10, 1, cWhite
    LINE VCX, VCY + 10, VCX, VCY + 20, 1, cWhite
  ENDIF
END SUB

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
    stX(i) = (RND * 2 - 1) * 116
    stY(i) = (RND * 2 - 1) * 116
    stZ(i) = 1 + RND * 255
    spc(i) = RGB(WHITE)
  NEXT i
END SUB

SUB DrawStardust
  LOCAL INTEGER i
  LOCAL FLOAT q, x, y, z
  FOR i = 0 TO NSTAR - 1
    x = stX(i) : y = stY(i) : z = stZ(i)
    ' perspective: the nearer a particle is, the faster it flies outwards
    q = dSpeed / z
    z = z - dSpeed * 0.25
    x = x + x * q
    y = y + y * q + alpha * x * 4 - beta * 256
    x = x - alpha * y * 4
    IF ABS(x) >= 116 OR ABS(y) >= 116 OR z < 16 THEN
      x = (RND * 2 - 1) * 116
      y = (RND * 2 - 1) * 116
      z = 144 + RND * 111
    ENDIF
    stX(i) = x : stY(i) = y : stZ(i) = z
    spx(i) = VCX + x
    spy(i) = VCY - y
  NEXT i
  ' The array form draws the whole field in one call, and clips for us.
  PIXEL spx(), spy(), spc()
END SUB
