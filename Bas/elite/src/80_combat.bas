' =====================================================================
'  Combat: lasers, damage, explosions, and what the other ships do
'
'  The laser does not travel.  Pressing fire tests whatever is lined up
'  in the crosshairs and hits it immediately, which is why Elite is about
'  pointing rather than leading a target.  The test is on the lateral
'  offset alone and does not consider range at all: a ship is hittable
'  when it is within 256 units of the line of sight in both axes and its
'  squared offset is inside the blueprint's targetable area.
'
'  Ships fight back on a schedule rather than every frame - one slot in
'  eight per frame - which is what keeps a busy bubble affordable and
'  gives the AI its slightly considered feel.
' =====================================================================

' --- the player fires
SUB FireLaser
  LOCAL INTEGER n, best, bestz, dmg
  IF lasTimer > 0 THEN EXIT SUB
  IF pLasT >= 242 THEN EXIT SUB          ' too hot to fire
  IF lasPower = 0 THEN EXIT SUB          ' no laser on this view
  ' Every shot heats the gun by eight; it loses one a frame.
  pLasT = pLasT + 8
  IF pLasT > 255 THEN pLasT = 255
  lasFlash = 2
  ' A beam refires every frame, a pulse every ten of the original's ticks.
  IF lasPower >= 128 THEN lasTimer = 0 ELSE lasTimer = LASPULSE
  Sfx SFX_LASER

  ' Whatever is lined up and nearest gets hit.
  ' In the ship's own coordinates, not the world's: the crosshairs are
  ' drawn in the view being looked through, so the shot has to be tested
  ' there too.  Testing world z instead - which is what this did - meant
  ' that in the rear, left and right views the sight showed one ship and
  ' the laser hit whatever happened to be ahead.
  best = -1 : bestz = 999999
  FOR n = 2 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 AND sExp(n) = 0 THEN
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
  shots = shots + 1
  IF best < 0 THEN EXIT SUB
  hits = hits + 1

  dmg = lasPower AND 127
  sEne(best) = sEne(best) - dmg
  ' Anything hit turns on us, whatever it was doing before.
  IF sAI(best) < 128 THEN sAI(best) = sAI(best) OR 128
  IF sEne(best) <= 0 THEN
    IF sTyp(best) = T_STATION THEN
      sEne(best) = bEne(sBp(best))       ' a station cannot be shot down
      AngerStation
    ELSE
      Explode best
    ENDIF
  ENDIF
END SUB

' Start a ship exploding.  The cloud grows for a while and then goes out,
' and the ship is only removed when it does.
SUB Explode(n AS INTEGER)
  sExp(n) = 18
  ' Both halves of it, as the original plays them.
  Sfx SFX_BOOM
  Sfx SFX_BOOMT
  sSpd(n) = 0
  sAI(n) = 0
  ' Anything destroyed counts towards the combat rating, and pays out
  ' whatever bounty its blueprint carries - which is nothing for most
  ' things and half a credit for an asteroid.
  kills = kills + 1
  cashTenths = cashTenths + bBty(sBp(n))
  ' The original announces what the kill was worth.
  IF bBty(sBp(n)) > 0 THEN Message STR$(bBty(sBp(n)) / 10) + " CR"
  NoteKill n
  EjectCargo n
  DropObject n
END SUB

' Advance every cloud, and draw it.  The original scatters points around
' each of the ship's projected vertices; ours scatters them around the
' ship's centre with the same growth curve, which reads the same at these
' sizes and costs a fraction of the vertex work.
SUB Explosions
  LOCAL INTEGER n, i, px, py, sz, cnt
  n = 2
  DO WHILE n < nUsed
    IF sTyp(n) <> 0 AND sExp(n) > 0 THEN
      sExp(n) = sExp(n) + 4
      IF sExp(n) > 128 THEN
        KillShip n
      ELSE
        ViewXform n
        IF tz > NEARZ THEN
          px = VCX + SGN(tx) * ((VPLANE * ABS(tx)) \ tz)
          py = VCY - SGN(ty) * ((VPLANE * ABS(ty)) \ tz)
          sz = sExp(n) * VPLANE / tz
          IF sz > 60 THEN sz = 60
          IF sz > 0 THEN
            cnt = bExp(sBp(n))
            IF cnt > 20 THEN cnt = 20
            FOR i = 0 TO cnt
              spx(i) = px + (RND * 2 - 1) * sz
              spy(i) = py + (RND * 2 - 1) * sz
            NEXT i
            FOR i = cnt + 1 TO 4 * NSTAR - 1 : spx(i) = -1 : NEXT i
            PIXEL spx(), spy(), spc()
          ENDIF
        ENDIF
        n = n + 1
      ENDIF
    ELSE
      n = n + 1
    ENDIF
  LOOP
END SUB

' --- what the other ships do
'
' One slot in eight each frame, as the original schedules it.  A ship
' points itself at us or away, decides whether to shoot, and nudges its
' speed to close or open the range.
SUB Tactics
  LOCAL INTEGER n, dmg
  LOCAL FLOAT d, cnt, nx, ny, nz
  FOR n = 2 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 AND sExp(n) = 0 THEN
      IF (sAI(n) AND 128) <> 0 THEN
        ' Even a pirate will not start something inside the station's
        ' no-fire zone, so its aggression is taken away while it is in
        ' there - bit 7 stays, so it still flies, it just will not fight.
        ' The police and anything bigger than a Mamba are not covered by
        ' that understanding, and neither are the Thargoids.
        IF inSafe THEN
          IF sTyp(n) < T_COBRA3 THEN
            IF sTyp(n) <> T_VIPER THEN sAI(n) = sAI(n) AND 129
          ENDIF
        ENDIF
        IF ((mcnt XOR n) AND 7) = 0 THEN
          d = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
          IF d > 1 THEN
            ' How squarely is it facing us?  Its nose against the
            ' direction from it to us, both unit vectors.
            NoseVec n
            nx = qV(1) * qV(4) : ny = qV(2) * qV(4) : nz = qV(3) * qV(4)
            cnt = (-sX(n) * nx - sY(n) * ny - sZ(n) * nz) / d

            ' Shooting: only from close in, and only when pointed almost
            ' straight at us.  A near miss still flashes and makes a noise.
            IF d < 8192 AND cnt > 0.917 AND (sAI(n) AND 126) <> 0 THEN
              dmg = bLas(sBp(n)) * 2
              IF cnt > 0.972 THEN
                HitPlayer dmg
              ENDIF
            ENDIF

            ' A missile, if it is hurt enough to want to spend one.  An
            ' E.C.M. burst - ours or anyone's - stops it trying.
            IF sEne(n) * 2 < bEne(sBp(n)) AND sMis(n) > 0 AND ecmActive = 0 THEN
              IF INT(RND * 32) < sMis(n) THEN
                sMis(n) = sMis(n) - 1
                EnemyMissile n
              ENDIF
            ENDIF

            ' Steering.  Very close in it breaks away; otherwise it turns
            ' towards us with a probability set by how aggressive it is.
            IF d < 1024 THEN
              sPit(n) = 3 : sRol(n) = 5
            ELSE
              IF (INT(RND * 128) OR 128) < sAI(n) THEN
                TurnTowards n, cnt
              ELSE
                sPit(n) = 3
              ENDIF
            ENDIF

            ' Speed: close if we are ahead of it, back off if too near.
            IF cnt > 0.85 THEN
              sAcc(n) = 3
            ELSEIF cnt < -0.7 THEN
              sAcc(n) = -1
            ENDIF
          ENDIF
        ENDIF
      ENDIF
    ENDIF
  NEXT n
END SUB

' Set the roll and pitch counters so the ship swings towards us.  The
' original works out the sign from the dot products of its roof and side
' vectors with the direction to the target; the counters themselves are
' small fixed values, so a ship turns at a fixed rate rather than
' proportionally.
SUB TurnTowards(n AS INTEGER, cnt AS FLOAT)
  LOCAL FLOAT rx, ry, rz, sx2, sy2, sz2, dr, ds, m
  LOCAL INTEGER i
  MATH SLICE sQ(), , n, qA()
  MATH Q_VECTOR 0, 1, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
  rx = qV(1) : ry = qV(2) : rz = qV(3)
  MATH Q_VECTOR 1, 0, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
  sx2 = qV(1) : sy2 = qV(2) : sz2 = qV(3)
  m = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
  IF m < 1 THEN EXIT SUB
  dr = (-sX(n) * rx - sY(n) * ry - sZ(n) * rz) / m
  ds = (-sX(n) * sx2 - sY(n) * sy2 - sZ(n) * sz2) / m
  ' Pitch towards, and roll so the turn happens in the shortest plane.
  IF dr > 0 THEN sPit(n) = 3 ELSE sPit(n) = 3 OR 128
  IF (sRol(n) AND 127) < 16 THEN
    IF ds > 0 THEN sRol(n) = 5 ELSE sRol(n) = 5 OR 128
  ENDIF
END SUB

' Damage to us.  The forward shield takes it while facing the shot, then
' energy; running out is the end.
SUB HitPlayer(dmg AS INTEGER)
  LOCAL INTEGER dleft
  dleft = dmg
  Sfx SFX_HIT
  IF pFsh >= dleft THEN
    pFsh = pFsh - dleft
    EXIT SUB
  ENDIF
  dleft = dleft - pFsh
  pFsh = 0
  pEnergy = pEnergy - dleft
  IF pEnergy <= 0 THEN
    pEnergy = 0
    dead = 1
  ENDIF
END SUB

' Once every eight frames the shields recharge from the energy banks, and
' the banks recharge themselves - the same schedule the original uses.
SUB Recharge
  IF (mcnt AND 7) <> 0 THEN EXIT SUB
  IF pEnergy >= 128 THEN
    IF pFsh < 255 THEN pFsh = pFsh + 1 : pEnergy = pEnergy - 1
    IF pAsh < 255 THEN pAsh = pAsh + 1 : pEnergy = pEnergy - 1
  ENDIF
  pEnergy = pEnergy + 1 + energyUnit
  IF pEnergy > 255 THEN pEnergy = 255
  IF pLasT > 0 THEN pLasT = pLasT - 1
END SUB

' Combat rating, from the number of kills.
FUNCTION RankName$()
  LOCAL INTEGER k
  k = kills
  IF k < 8 THEN
    RankName$ = "Harmless"
  ELSEIF k < 16 THEN
    RankName$ = "Mostly Harmless"
  ELSEIF k < 32 THEN
    RankName$ = "Poor"
  ELSEIF k < 64 THEN
    RankName$ = "Average"
  ELSEIF k < 128 THEN
    RankName$ = "Above Average"
  ELSEIF k < 512 THEN
    RankName$ = "Competent"
  ELSEIF k < 2560 THEN
    RankName$ = "Dangerous"
  ELSEIF k < 6400 THEN
    RankName$ = "Deadly"
  ELSE
    RankName$ = "ELITE"
  ENDIF
END FUNCTION
