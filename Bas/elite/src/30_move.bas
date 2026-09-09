' =====================================================================
'  Moving the universe
'
'  Elite never moves the player.  The player sits at the origin looking
'  down +Z, and every frame the whole universe is rotated by the
'  opposite of the player's roll and pitch and shifted back by the
'  player's speed.  Each ship then moves along its own nose and applies
'  its own roll and pitch counters.  This SUB is the whole of that.
' =====================================================================

CONST SELFROT = 0.0625             ' a ship's own roll / pitch, radians per frame
CONST NPCSPEED = 1.5               ' a ship of speed s moves 1.5 * s per frame
CONST TIDYEVERY = 16               ' renormalise one ship's orientation this often

SUB MoveShips
  LOCAL INTEGER n, i, mag, dir, gone
  LOCAL FLOAT k

  ' The player's rotation, as a quaternion, built once for the frame.
  ' To first order the original's position transform is a rotation of
  ' -alpha about the nose axis followed by +beta about the side axis.
  MATH Q_CREATE -alpha, 0, 0, 1, qA()
  MATH Q_CREATE beta, 1, 0, 0, qB()
  MATH Q_MULT qB(), qA(), qP()

  ' A FOR loop fixes its limit when it starts, and killing a ship shortens
  ' the table, so walk it by hand.
  n = 0
  DO WHILE n < nUsed
    IF sTyp(n) = 0 THEN
      n = n + 1
    ELSE
      ' --- 1. the ship moves along its own nose (not the planet or sun)
      IF sBp(n) >= 0 AND sSpd(n) <> 0 THEN
        NoseVec n
        sX(n) = sX(n) + qV(1) * qV(4) * sSpd(n) * NPCSPEED
        sY(n) = sY(n) + qV(2) * qV(4) * sSpd(n) * NPCSPEED
        sZ(n) = sZ(n) + qV(3) * qV(4) * sSpd(n) * NPCSPEED
      ENDIF

      ' --- 2. acceleration is applied once and then forgotten
      IF sAcc(n) <> 0 THEN
        sSpd(n) = sSpd(n) + sAcc(n)
        IF sSpd(n) > bSpd(sBp(n)) THEN sSpd(n) = bSpd(sBp(n))
        IF sSpd(n) < 1 THEN sSpd(n) = 1
        sAcc(n) = 0
      ENDIF

      ' --- 3. the player's roll and pitch, applied to the ship's
      '     position.  Written exactly as the original does it: each
      '     line uses the value the line before it just produced, which
      '     is what makes this a rotation rather than a shear.
      k = sY(n) - alpha * sX(n)
      sZ(n) = sZ(n) + beta * k
      sY(n) = k - beta * sZ(n)
      sX(n) = sX(n) + alpha * sY(n)

      ' --- 4. and the player's speed
      sZ(n) = sZ(n) - dSpeed

      ' --- 5. the same rotation applied to the ship's orientation
      IF sBp(n) >= 0 THEN
        FOR i = 0 TO 4 : qA(i) = sQ(i, n) : NEXT i
        MATH Q_MULT qP(), qA(), qC()

        ' --- 6. the ship's own roll and pitch counters.  Bits 0 to 6
        '     are the magnitude and bit 7 the direction; a magnitude of
        '     127 means "keep turning forever", which is how the space
        '     station spins.
        mag = sPit(n) AND 127
        IF mag <> 0 THEN
          dir = 1
          IF (sPit(n) AND 128) <> 0 THEN dir = -1
          MATH Q_CREATE dir * SELFROT, 1, 0, 0, qB()
          FOR i = 0 TO 4 : qA(i) = qC(i) : NEXT i
          MATH Q_MULT qA(), qB(), qC()
          IF mag <> 127 THEN sPit(n) = (mag - 1) OR (sPit(n) AND 128)
        ENDIF
        mag = sRol(n) AND 127
        IF mag <> 0 THEN
          dir = 1
          IF (sRol(n) AND 128) <> 0 THEN dir = -1
          MATH Q_CREATE dir * SELFROT, 0, 0, 1, qB()
          FOR i = 0 TO 4 : qA(i) = qC(i) : NEXT i
          MATH Q_MULT qA(), qB(), qC()
          IF mag <> 127 THEN sRol(n) = (mag - 1) OR (sRol(n) AND 128)
        ENDIF

        FOR i = 0 TO 4 : sQ(i, n) = qC(i) : NEXT i
        sQ(4, n) = 1
        ' The original tidies one ship's orientation vectors every 16
        ' frames to stop rounding error accumulating; a quaternion needs
        ' the same treatment for the same reason.
        IF ((mcnt XOR n) AND (TIDYEVERY - 1)) = 0 THEN NormQuat n
      ENDIF

      ' --- 7. anything that has drifted out of the bubble is gone.  The
      '     table closes up behind it, so the next ship is now at this
      '     index and n must not advance.
      gone = 0
      IF sBp(n) >= 0 THEN
        IF ABS(sX(n)) > FAROFF OR ABS(sY(n)) > FAROFF OR ABS(sZ(n)) > FAROFF THEN
          KillShip n
          gone = 1
        ENDIF
      ENDIF
      IF gone = 0 THEN n = n + 1
    ENDIF
  LOOP
END SUB

' The ship's nose direction in world coordinates, left in qV().
' A ship's own +Z axis is its nose.
SUB NoseVec(n AS INTEGER)
  LOCAL INTEGER i
  FOR i = 0 TO 4 : qA(i) = sQ(i, n) : NEXT i
  MATH Q_VECTOR 0, 0, 1, qB()
  MATH Q_ROTATE qA(), qB(), qV()
END SUB

SUB NormQuat(n AS INTEGER)
  LOCAL FLOAT m
  m = SQR(sQ(0,n)*sQ(0,n) + sQ(1,n)*sQ(1,n) + sQ(2,n)*sQ(2,n) + sQ(3,n)*sQ(3,n))
  IF m > 0.000001 THEN
    sQ(0,n) = sQ(0,n)/m : sQ(1,n) = sQ(1,n)/m
    sQ(2,n) = sQ(2,n)/m : sQ(3,n) = sQ(3,n)/m
  ELSE
    sQ(0,n) = 1 : sQ(1,n) = 0 : sQ(2,n) = 0 : sQ(3,n) = 0
  ENDIF
  sQ(4,n) = 1
END SUB
