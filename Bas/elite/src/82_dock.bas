' =====================================================================
'  Docking, and what the station does about you
'
'  Docking is not a collision, it is five separate tests, and failing any
'  one of them at speed is fatal.
'
'    1  the station is not hostile
'    2  its own nose points back at us, within about 26 degrees
'    3  it is nearly dead ahead of us, within 22 degrees
'    4  and in front of us at all
'    5  the slot is within 33.6 degrees of horizontal
'
'  Test 5 is the one that makes docking a manoeuvre rather than an
'  approach, and it is worth being exact about what it means.  The slot is
'  a letterbox, 20 units across and 60 tall in the station's own frame, so
'  its long axis lies along the station's up vector.  The test measures the
'  x component of that up vector in OUR frame: when its magnitude is near
'  one, the slot's long axis is lying horizontally, which is the direction
'  our own ship is widest in.  So the ship has to be rolled to match the
'  station's rotation, with its wings along the long dimension of the port.
'  The station turns continuously, so that match has to be flown.
'
'  The original's own comment calls this 36.6 degrees; the arithmetic of
'  its threshold, 80 out of 96, is 33.6.
'
'  Below speed 5 a failed approach only bounces.  Above it, it does not.
' =====================================================================

' Run once a frame while the station is close enough to matter.
SUB DockCheck
  LOCAL INTEGER n
  LOCAL FLOAT d, tz2, rx, nz
  n = SLOT_STAR
  IF sTyp(n) <> T_STATION THEN EXIT SUB
  IF dead THEN EXIT SUB
  ' The station is only ever entered through the one face, so anything
  ' behind us is not an approach at all - which matters immediately after
  ' launching, when we are a couple of hundred units in front of the slot
  ' with the station at our back.
  IF sZ(n) <= 0 THEN EXIT SUB
  d = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
  IF d > DOCKRANGE THEN EXIT SUB

  ' The station's own orientation: where its nose points, and how its
  ' slot is rolled.
  MATH SLICE sQ(), , n, qA()
  MATH Q_VECTOR 0, 0, 1, qB() : MATH Q_ROTATE qA(), qB(), qV()
  nz = qV(3)
  MATH Q_VECTOR 0, 1, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
  rx = qV(1)
  tz2 = sZ(n) / d

  ' 1: a station we have attacked will not take us.
  IF (sAI(n) AND 128) <> 0 THEN Crash : EXIT SUB
  ' 2: we must be on the slot's side of it.
  IF nz > -DOCKFACE THEN Crash : EXIT SUB
  ' 3 and 4: it has to be nearly dead ahead.
  IF tz2 < DOCKCONE THEN Crash : EXIT SUB
  ' 5: and our wings have to lie along the long axis of the letterbox.
  IF ABS(rx) < DOCKROLL THEN Crash : EXIT SUB
  DoDock
END SUB

' A failed approach.  Slowly it is a bump; quickly it is the end.
SUB Crash
  IF dSpeed < 5 THEN
    dSpeed = 1
    sZ(SLOT_STAR) = sZ(SLOT_STAR) + 300
    HitPlayer 10
  ELSE
    pEnergy = 0
    dead = 1
  ENDIF
END SUB

' Inside.  The station repairs the ship and cools the laser, but it does
' not give anything away: fuel, missiles and equipment all have to be
' bought, and the hold and the legal record come in exactly as they were.
SUB DoDock
  docked = 1
  dSpeed = 0
  dockComp = 0
  msLock = -1
  pEnergy = 255 : pFsh = 255 : pAsh = 255
  pLasT = 0 : pCabT = 30 : pAltit = 200
  pRoll = JCENTRE : pPitch = JCENTRE
  ' Nothing outside matters any more, and the Draw3D objects the bubble
  ' was holding are better returned to the pool than kept.
  ClearSlots
END SUB

' The docking computer flies the approach for you: it lines the ship up
' on the slot and eases in.  The original's is a sequence of nudges to
' the same controls a player uses, and so is this.
SUB DockingComputer
  LOCAL INTEGER n
  LOCAL FLOAT d, ux, uy, uz, rx, ry
  n = SLOT_STAR
  IF sTyp(n) <> T_STATION THEN EXIT SUB
  d = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
  IF d < 1 THEN EXIT SUB
  ux = sX(n) / d : uy = sY(n) / d : uz = sZ(n) / d

  ' Steer towards it by pushing the same rate values a held key would.
  IF ux > 0.05 THEN
    pRoll = JCENTRE + 40
  ELSEIF ux < -0.05 THEN
    pRoll = JCENTRE - 40
  ELSE
    pRoll = JCENTRE
  ENDIF
  IF uy > 0.05 THEN
    pPitch = JCENTRE - 30
  ELSEIF uy < -0.05 THEN
    pPitch = JCENTRE + 30
  ELSE
    pPitch = JCENTRE
  ENDIF

  ' Roll to match the slot once we are pointing at it.  Our own roll moves
  ' the slot's up vector by rx' = rx + alpha * ry, so the direction that
  ' widens the component the docking test measures is the one whose sign
  ' matches rx * ry.  It has to be a full deflection: the station turns a
  ' sixteenth of a radian a frame and a gentle nudge cannot keep up with
  ' it, which is why a computer-flown approach used to bounce off the slot
  ' over and over instead of going in.
  IF uz > 0.9 THEN
    MATH SLICE sQ(), , n, qA()
    MATH Q_VECTOR 0, 1, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
    rx = qV(1) : ry = qV(2)
    IF ABS(rx) < DOCKROLL THEN
      IF rx * ry > 0 THEN pRoll = 255 ELSE pRoll = 1
    ENDIF
  ENDIF

  ' And close, gently.
  IF d > 4000 THEN
    dSpeed = 20
  ELSEIF d > 1200 THEN
    dSpeed = 8
  ELSE
    dSpeed = 3
  ENDIF
END SUB

' Leaving.  The original throws a tunnel of expanding squares at you for
' a moment; ours does the same and then hands back a flying ship.
SUB LaunchTunnel
  LOCAL INTEGER i, k, r
  FOR i = 0 TO 23
    CLS
    FOR k = 0 TO 5
      r = ((i + k * 4) MOD 24) * 7 + 8
      BOX VCX - r * 1.25, VCY - r, r * 2.5, r * 2, 1, cWhite, -1
    NEXT k
    DrawDash
    ViewName
    FRAMEBUFFER COPY F, N, B
  NEXT i
END SUB

' A station we have attacked sends police after us, on the same schedule
' that spawns everything else.
SUB StationPolice
  LOCAL INTEGER n
  IF sTyp(SLOT_STAR) <> T_STATION THEN EXIT SUB
  IF (sAI(SLOT_STAR) AND 128) = 0 THEN EXIT SUB
  IF (mcnt AND 31) <> 0 THEN EXIT SUB
  IF nUsed >= NSLOT THEN EXIT SUB
  MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
  n = NewShip(T_VIPER, sX(SLOT_STAR), sY(SLOT_STAR), sZ(SLOT_STAR) - 400, qA())
  IF n >= 0 THEN
    sSpd(n) = bSpd(sBp(n))
    sAI(n) = 128 OR 56              ' police are single minded about it
  ENDIF
END SUB

' Attacking the station turns it, and everything it can call on, against
' you.  Nothing can shoot the station down, so this is the only
' consequence it has.
SUB AngerStation
  IF sTyp(SLOT_STAR) = T_STATION THEN sAI(SLOT_STAR) = sAI(SLOT_STAR) OR 128
  legal = legal + 64
  IF legal > 255 THEN legal = 255
END SUB
