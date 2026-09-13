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
    t = TIMER : DrawShips : Explosions : SpaceFurniture : prof(3) = prof(3) + TIMER - t
    t = TIMER : DrawDash      : prof(4) = prof(4) + TIMER - t
    ViewName
    DrawMessage
  ELSE
    CLS
    DrawStardust
    DrawPlanetSun
    DrawShips
    Explosions
    SpaceFurniture
    DrawDash
    ViewName
    DrawMessage
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
  ' The laser is drawn as two lines converging on the crosshairs from the
  ' bottom corners of the view, for the couple of frames after a shot.
  IF lasFlash > 0 THEN
    LINE 40, VIEWH - 2, VCX - 4 + RND * 8, VCY, 1, cWhite
    LINE SCRW - 40, VIEWH - 2, VCX - 4 + RND * 8, VCY, 1, cWhite
  ENDIF
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
' they are drawn directly.  On-screen radius is 256 * 24576 / z, and the
' original clamps it: a radius that reaches 256 is forced to 248, so a
' planet you are nearly touching stops growing rather than filling the
' screen.  Nothing is drawn closer than z = 256 or further than the
' radius-2-pixels distance.
'
' The cassette game has two kinds of planet and picks between them on a
' bit of the system's tech level: one with an equator and meridians, one
' with a crater.  The crater is an ellipse of half the planet's radius,
' its centre pushed 0.87 of the radius along the up vector, drawn only
' while the up vector points towards us.
'
' Deviation: the original draws the outline as an 8, 16 or 32 sided
' polygon depending on radius, which is visibly faceted.  Ours is a true
' circle, because one CIRCLE call costs a fraction of thirty-two
' interpreted line segments.
SUB DrawPlanetSun
  LOCAL INTEGER n, cx, cy, r
  FOR n = 0 TO 1
    IF sTyp(n) = T_PLANET OR sTyp(n) = T_CRATER OR sTyp(n) = T_SUN THEN
      ViewXform n
      IF tz > 255 AND tz < 3145728 THEN
        cx = VCX + SGN(tx) * ((VPLANE * ABS(tx)) \ tz)
        cy = VCY - SGN(ty) * ((VPLANE * ABS(ty)) \ tz)
        r = 6291456 / tz
        IF r >= 256 THEN r = 248
        IF cx + r > 0 AND cx - r < SCRW AND cy + r > 0 AND cy - r < VIEWH THEN
          IF sTyp(n) = T_SUN THEN
            CIRCLE cx, cy, r, 1, 1, cWhite, cWhite
          ELSE
            CIRCLE cx, cy, r, 1, 1, cWhite, -1
            IF r >= 6 THEN Surface n, cx, cy, r
          ENDIF
        ENDIF
      ENDIF
    ENDIF
  NEXT n
END SUB

' The planet's surface detail, in the plane of the screen.  Both the
' crater and the meridians are built from the planet's own orientation
' vectors projected flat, so the pattern turns with the planet.
SUB Surface(n AS INTEGER, cx AS INTEGER, cy AS INTEGER, r AS INTEGER)
  LOCAL INTEGER k
  LOCAL FLOAT vnx, vny, vnz, vrx, vry, vrz, vsx, vsy, vsz, ox, oy, ax, ay, bx, by
  MATH SLICE sQ(), , n, qA()
  MATH Q_VECTOR 0, 0, 1, qB() : MATH Q_ROTATE qA(), qB(), qV()
  vnx = qV(1) : vny = qV(2) : vnz = qV(3)          ' nose
  MATH Q_VECTOR 0, 1, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
  vrx = qV(1) : vry = qV(2) : vrz = qV(3)          ' up
  MATH Q_VECTOR 1, 0, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
  vsx = qV(1) : vsy = qV(2) : vsz = qV(3)          ' side

  IF sTyp(n) = T_CRATER THEN
    ' The crater sits on the up pole, in the plane the nose and side
    ' vectors span, and is only drawn while that pole faces us.
    IF vrz > 0 THEN EXIT SUB
    ox = cx + 0.867 * r * vrx
    oy = cy - 0.867 * r * vry
    ax = 0.5 * r * vnx : ay = 0.5 * r * vny
    bx = 0.5 * r * vsx : by = 0.5 * r * vsy
    FOR k = 0 TO NSEG - 1
      pgx(k) = ox + ax * ctab(k) + bx * stab(k)
      pgy(k) = oy - (ay * ctab(k) + by * stab(k))
    NEXT k
    POLYGON NSEG, pgx(), pgy(), cWhite
  ELSE
    ' An equator and one meridian.  Only the half of each great circle
    ' that faces us is drawn - a closed ellipse would show the far side
    ' too and the planet would read as a ball of wire.
    HalfCircle cx, cy, r, vnx, vny, vnz, vrx, vry, vrz
    HalfCircle cx, cy, r, vsx, vsy, vsz, vrx, vry, vrz
  ENDIF
END SUB

' One great circle of the planet, drawn only where it faces the viewer.
' The pair of vectors are conjugate radii: the curve is a*cos + b*sin, and
' a point is on the near side when its own z is towards us.
SUB HalfCircle(cx AS INTEGER, cy AS INTEGER, r AS INTEGER, ax AS FLOAT, ay AS FLOAT, az AS FLOAT, bx AS FLOAT, by AS FLOAT, bz AS FLOAT)
  LOCAL INTEGER k, px, py, lx, ly, have
  LOCAL FLOAT c, sn, pz
  have = 0
  FOR k = 0 TO NSEG
    c = ctab(k AND (NSEG - 1))
    sn = stab(k AND (NSEG - 1))
    pz = az * c + bz * sn
    IF pz <= 0 THEN
      px = cx + r * (ax * c + bx * sn)
      py = cy - r * (ay * c + by * sn)
      IF have THEN LINE lx, ly, px, py, 1, cWhite
      lx = px : ly = py : have = 1
    ELSE
      have = 0
    ENDIF
  NEXT k
END SUB

' Stardust.  The particles do not live in the world: x and y are pixel
' offsets from the centre of the view and z is a depth in the same units
' the visibility scale uses, so the whole field costs no projection at
' all.  Each view moves them differently - outwards from the centre in
' front, inwards behind, and sideways in the two side views - and each
' has its own rule for where a particle that leaves the screen comes
' back.  A particle is one pixel far away, two abreast closer in, and a
' two by two block when it is nearly past, which is the depth cue that
' makes the field read as speed.
'
' alp1 and bet1 are used here as the small signed integers the original
' works in, not as the radian angles the ship rotation uses.
SUB InitStardust
  LOCAL INTEGER i
  FOR i = 0 TO NSTAR - 1
    stX(i) = SdSM(RND * 256)
    stY(i) = SdSM(RND * 256)
    stZ(i) = 1 + RND * 254
  NEXT i
  FOR i = 0 TO 4 * NSTAR - 1 : spc(i) = cWhite : NEXT i
END SUB

' A random byte read as a sign and a magnitude, giving -127..127.
FUNCTION SdSM(b AS FLOAT) AS FLOAT
  LOCAL INTEGER v
  v = b
  IF (v AND 128) <> 0 THEN SdSM = -(v AND 127) ELSE SdSM = (v AND 127)
END FUNCTION

SUB DrawStardust
  LOCAL INTEGER i, zh, np, sx, sy, sy2, r
  LOCAL FLOAT q, x, y, z, a, b, h, qb, d, dsg, ratsg
  a = alp2 * alp1
  b = bet2 * bet1
  np = 0
  ARRAY SET -1, spx()
  IF vw > 1 THEN
    ' the right view runs the same maths with the angles negated
    IF vw = 3 THEN
      a = -a : b = -b : dsg = 1 : ratsg = -1
    ELSE
      dsg = -1 : ratsg = 1
    ENDIF
  ENDIF
  FOR i = 0 TO NSTAR - 1
    x = stX(i) : y = stY(i) : z = stZ(i)
    IF vw = 0 THEN
      ' --- front: everything streams out from the centre
      zh = z
      q = (INT(64 * dSpeed / zh)) OR 1
      z = z - dSpeed / 4
      y = y + FIX(y) * q / 256
      x = x + FIX(x) * q / 256
      y = y - a * FIX(x) / 256
      x = x + a * FIX(y) / 256
      qb = INT(ABS(b) * INT(ABS(y)) / 256)
      x = x + 2 * qb * qb / 256
      y = y - b
      IF ABS(x) >= 120 OR ABS(y) >= 120 OR z < 16 THEN
        y = SdSM((RND * 256) OR 4)
        x = SdSM((RND * 256) OR 8)
        z = (INT(RND * 256)) OR 144
      ENDIF
    ELSEIF vw = 1 THEN
      ' --- rear: everything streams in towards the centre
      zh = z
      q = (INT(64 * dSpeed / zh)) OR 1
      x = x - FIX(x) * q / 256
      y = y - FIX(y) * q / 256
      z = z + dSpeed / 4
      y = y + a * FIX(x) / 256
      x = x - a * FIX(y) / 256
      h = FIX(y)
      qb = -SGN(b) * SGN(h) * INT(ABS(b) * ABS(h) / 256)
      x = x + 2 * qb * (-FIX(x)) / 256
      y = y + b
      ' there is no test on x at all in the rear view
      IF ABS(y) >= 110 OR z >= 160 THEN
        r = (INT(RND * 256) AND 127) + 10 + INT(RND * 2)
        z = r
        IF (r AND 1) = 0 THEN
          IF (r AND 2) = 0 THEN x = 126 ELSE x = -126
          y = SdSM(RND * 256)
        ELSE
          r = INT(RND * 256)
          x = SdSM(r)
          IF (r AND 1) = 0 THEN y = 115 ELSE y = -115
        ENDIF
      ENDIF
    ELSE
      ' --- side views: depth never changes, the field just slides across
      zh = z
      d = INT(zh / 8)
      IF d < 1 THEN d = 1
      x = x + dsg * dSpeed / d
      x = x + b * FIX(y) / 256
      y = y - b * FIX(x) / 256
      h = FIX(y)
      qb = SGN(a) * SGN(h) * INT(ABS(a) * ABS(h) / 256)
      x = x - qb * FIX(x) / 256
      y = y + qb * h / 256 + a
      IF ABS(x) >= 116 THEN
        y = SdSM(RND * 256)
        x = 115 * ratsg
        z = (INT(RND * 256)) OR 8
      ELSEIF ABS(y) >= 116 THEN
        x = SdSM(RND * 256)
        IF a > 0 THEN y = -110 ELSE y = 110
        z = (INT(RND * 256)) OR 8
      ENDIF
    ENDIF
    stX(i) = x : stY(i) = y : stZ(i) = z
    ' plot: size grows as the particle gets close
    IF ABS(y) < VCY THEN
      zh = z
      sx = VCX + FIX(x)
      sy = VCY - FIX(y)
      spx(np) = sx : spy(np) = sy : np = np + 1
      IF zh < 144 THEN
        spx(np) = sx + 1 : spy(np) = sy : np = np + 1
        IF zh < 80 THEN
          IF (sy AND 7) = 0 THEN sy2 = sy + 1 ELSE sy2 = sy - 1
          spx(np) = sx : spy(np) = sy2 : np = np + 1
          spx(np) = sx + 1 : spy(np) = sy2 : np = np + 1
        ENDIF
      ENDIF
    ENDIF
  NEXT i
  ' One call draws the whole field, and clips it for us.
  PIXEL spx(), spy(), spc()
END SUB
