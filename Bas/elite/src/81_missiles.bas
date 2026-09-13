' =====================================================================
'  Missiles, E.C.M., cargo and consequences
'
'  A missile is a ship like any other, with its own blueprint and its own
'  place in the bubble.  What makes it a missile is that it runs its
'  tactics every single frame instead of one frame in eight, so it turns
'  faster than anything it chases, and that reaching its target destroys
'  them both.
'
'  It is stopped by one thing only: an E.C.M. burst destroys every missile
'  in the bubble, whoever fired it.  That is why a ship carrying E.C.M.
'  is so much harder to kill with missiles, and why firing one at a
'  Thargoid is usually a waste.
' =====================================================================

' Lock on to whatever is lined up, the same alignment test the laser uses.
SUB TargetMissile
  LOCAL INTEGER n, best, bestz
  IF pMissl = 0 THEN Sfx SFX_BOOP : EXIT SUB
  best = -1 : bestz = 999999
  ' The same view coordinates the laser uses.
  FOR n = 2 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 AND sExp(n) = 0 AND sTyp(n) <> T_MISSILE THEN
      ViewXform n
      IF tz > 0 AND tz < bestz THEN
        IF ABS(tx) < 256 AND ABS(ty) < 256 THEN
          IF tx * tx + ty * ty < bArea(sBp(n)) THEN
            best = n : bestz = tz
          ENDIF
        ENDIF
      ENDIF
    ENDIF
  NEXT n
  IF best >= 0 THEN
    msLock = best
    Sfx SFX_BEEP
  ELSE
    Sfx SFX_BOOP
  ENDIF
END SUB

' Launch one at whatever is locked.  It appears just ahead of us already
' pointing the right way and moving faster than anything it chases.
SUB LaunchMissile
  LOCAL INTEGER n
  IF pMissl = 0 OR msLock < 0 THEN EXIT SUB
  IF sTyp(msLock) = 0 OR sExp(msLock) > 0 THEN
    msLock = -1
    Message "TARGET LOST"
    EXIT SUB
  ENDIF
  MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
  n = NewShip(T_MISSILE, 0, -28, 200, qA())
  IF n < 0 THEN EXIT SUB
  sSpd(n) = bSpd(sBp(n))
  sAI(n) = 128 OR 126               ' as aggressive as anything gets
  sTgt(n) = msLock
  pMissl = pMissl - 1
  msLock = -1
  Sfx SFX_LAUNCH
END SUB

' Everything a missile does, every frame.  It turns towards whatever it is
' chasing and goes off when it gets there.
SUB Missiles
  LOCAL INTEGER n, t
  LOCAL FLOAT dx, dy, dz, d
  FOR n = 2 TO nUsed - 1
    ' A missile aimed at us is left to the second loop: its target is not
    ' a slot at all, and the test below would read that as a dead one and
    ' set it off the moment it was launched.
    IF sTyp(n) = T_MISSILE AND sExp(n) = 0 AND sTgt(n) <> -2 THEN
      t = sTgt(n)
      ' The target may have died, or been shuffled down the table.
      IF t < 0 OR t >= nUsed THEN
        Explode n
      ELSEIF sTyp(t) = 0 OR sExp(t) > 0 THEN
        Explode n
      ELSE
        dx = sX(t) - sX(n) : dy = sY(t) - sY(n) : dz = sZ(t) - sZ(n)
        d = SQR(dx * dx + dy * dy + dz * dz)
        IF d < 256 THEN
          ' Close enough: both of them go.
          Explode n
          IF sTyp(t) <> T_STATION THEN
            sEne(t) = 0
            Explode t
          ENDIF
        ELSE
          HomeOn n, dx, dy, dz, d
        ENDIF
      ENDIF
    ENDIF
  NEXT n
  ' A missile aimed at us behaves the same way, but there is nothing in a
  ' slot to chase - we are the origin.
  FOR n = 2 TO nUsed - 1
    IF sTyp(n) = T_MISSILE AND sExp(n) = 0 AND sTgt(n) = -2 THEN
      d = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
      IF d < 256 THEN
        Explode n
        IF d < 128 THEN HitPlayer 250 ELSE HitPlayer 80
      ELSE
        HomeOn n, -sX(n), -sY(n), -sZ(n), d
      ENDIF
    ENDIF
  NEXT n
END SUB

' Swing a missile onto a bearing.  It turns hard, which is what makes one
' so difficult to shake off without E.C.M.
SUB HomeOn(n AS INTEGER, dx AS FLOAT, dy AS FLOAT, dz AS FLOAT, d AS FLOAT)
  LOCAL FLOAT nx, ny, nz, ax, ay, az, m
  IF d < 1 THEN EXIT SUB
  NoseVec n
  nx = qV(1) * qV(4) : ny = qV(2) * qV(4) : nz = qV(3) * qV(4)
  ' Turn the nose towards the bearing by a fixed fraction each frame.
  nx = nx + (dx / d - nx) * MSTURN
  ny = ny + (dy / d - ny) * MSTURN
  nz = nz + (dz / d - nz) * MSTURN
  m = SQR(nx * nx + ny * ny + nz * nz)
  IF m < 0.0001 THEN EXIT SUB
  nx = nx / m : ny = ny / m : nz = nz / m
  ' Rebuild an orientation whose nose is that direction: the axis is the
  ' cross product of the ship's own forward axis with the new bearing.
  ax = -ny : ay = nx : az = 0
  m = SQR(ax * ax + ay * ay)
  IF m < 0.0001 THEN
    MATH Q_EULER 0, 0, 0, qA()
  ELSE
    MATH Q_CREATE ACOS(nz), ax / m, ay / m, 0, qA()
  ENDIF
  qA(4) = 1
  MATH INSERT sQ(), , n, qA()
END SUB

' One burst destroys every missile in the bubble, ours included, and
' costs energy to do it.
SUB FireECM
  LOCAL INTEGER n
  IF ecmActive > 0 THEN EXIT SUB
  ecmActive = ECMFRAMES
  Sfx SFX_ECM
  FOR n = 2 TO nUsed - 1
    IF sTyp(n) = T_MISSILE AND sExp(n) = 0 THEN
      IF sTgt(n) = -2 THEN Message "MISSILE JAMMED"
      Explode n
    ENDIF
  NEXT n
END SUB

' A ship with its energy down and a missile left would rather spend it than
' go on trading laser fire.  A Thargoid lets a Thargon off instead.
SUB EnemyMissile(n AS INTEGER)
  LOCAL INTEGER m
  IF sTyp(n) = T_THARGOID THEN
    m = NewFacing(T_THARGON, sX(n), sY(n) - 30, sZ(n), 180)
    IF m >= 0 THEN sAI(m) = 128 OR 126 : sSpd(m) = bSpd(sBp(m))
    EXIT SUB
  ENDIF
  m = NewFacing(T_MISSILE, sX(n), sY(n) - 30, sZ(n), 180)
  IF m < 0 THEN EXIT SUB
  sSpd(m) = bSpd(sBp(m))
  sAI(m) = 128 OR 126
  sTgt(m) = -2                      ' -2 is us
  Message "INCOMING MISSILE"
  Sfx SFX_LAUNCH
END SUB

SUB ECMService
  IF ecmActive > 0 THEN
    ecmActive = ecmActive - 1
    pEnergy = pEnergy - 1
    IF pEnergy < 0 THEN pEnergy = 0
    IF ecmActive = 0 THEN SfxStop SFX_ECM
  ENDIF
END SUB

' What a ship leaves behind.  Roughly half the time it sheds cargo, up to
' the number its blueprint says it can carry.
SUB EjectCargo(n AS INTEGER)
  LOCAL INTEGER i, cnt, m
  IF bCan(sBp(n)) = 0 THEN EXIT SUB
  IF RND < 0.5 THEN EXIT SUB
  cnt = INT(RND * (bCan(sBp(n)) + 1))
  FOR i = 1 TO cnt
    MATH Q_EULER RND * 6, RND * 6, 0, qA() : qA(4) = 1
    m = NewShip(T_CANISTER, sX(n) + (RND * 400 - 200), sY(n) + (RND * 400 - 200), sZ(n) + (RND * 400 - 200), qA())
    IF m < 0 THEN EXIT SUB
    sRol(m) = 130 : sPit(m) = 5
  NEXT i
END SUB

' Shooting a police ship makes an outlaw of you, and the station will send
' more of them.
SUB NoteKill(n AS INTEGER)
  ' Shooting the sheriff puts our record straight to fugitive.
  IF sTyp(n) = T_VIPER THEN legal = legal OR 64
END SUB

FUNCTION LegalName$()
  IF legal = 0 THEN
    LegalName$ = "Clean"
  ELSEIF legal < 50 THEN
    LegalName$ = "Offender"
  ELSE
    LegalName$ = "Fugitive"
  ENDIF
END FUNCTION

' The end.  The original scatters the wreck of your own ship across the
' view; ours says so plainly and stops.
SUB DeathScreen
  CLS
  TEXT VCX, 60, "GAME OVER", "CT", 1, 2, cRed
  TEXT VCX, 100, "Kills: " + STR$(kills) + "   " + RankName$(), "CT", 7, 1, cWhite
  TEXT VCX, 115, "Cash: " + STR$(cashTenths / 10) + " Cr", "CT", 7, 1, cWhite
  FRAMEBUFFER COPY F, N
END SUB
