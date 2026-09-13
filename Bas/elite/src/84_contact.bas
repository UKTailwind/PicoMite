' =====================================================================
'  Getting too close: altitude, heat, scooping and collisions
'
'  The original looks at the planet on frame 10 of every 32 and at the sun
'  on frame 20, which is why those two bars move in steps rather than
'  sliding.  Both work on the middle bytes of the coordinates and are
'  skipped entirely while anything is a whole unit away in any axis, so an
'  altitude bar reading full means no more than "further off than 65536".
'
'  Altitude.  q is the sum of the squares of the coordinates in high bytes,
'  divided by 256 again to keep it in a byte; the planet's own radius is 96
'  high bytes, which by the same arithmetic comes to 36.  One less than
'  that and you are inside the planet.  Above it the bar reads sixteen
'  times the square root, which is what the original's 16-bit square root
'  of the same figure comes to with its low byte left as it lay.
'
'  Cabin temperature is 285 less that same figure for the sun.  It starts
'  to climb around 65000 units out, reaches the scooping threshold of 224
'  at about 32000, and passes 255 - which is fatal - at about 22400.
'
'  Scooping needs fuel scoops fitted and the thing below us: you fly over
'  the top of a canister to take it in, which is the detail everybody
'  remembers.  Anything else that close is a collision.  A canister holds
'  anything from food to computers, an escape pod holds slaves, and a
'  Thargon counts as alien items.
' =====================================================================

SUB Altitude
  LOCAL FLOAT q, xh, yh, zh
  IF (mcnt AND 31) <> 10 THEN EXIT SUB
  pAltit = 255
  IF inWitch THEN EXIT SUB
  IF sTyp(SLOT_PLANET) = 0 THEN EXIT SUB
  IF ABS(sX(SLOT_PLANET)) >= UNIT THEN EXIT SUB
  IF ABS(sY(SLOT_PLANET)) >= UNIT THEN EXIT SUB
  IF ABS(sZ(SLOT_PLANET)) >= UNIT THEN EXIT SUB
  xh = sX(SLOT_PLANET) / 256
  yh = sY(SLOT_PLANET) / 256
  zh = sZ(SLOT_PLANET) / 256
  q = (xh * xh + yh * yh + zh * zh) / 256
  IF q > 255 THEN EXIT SUB
  q = q - 37
  IF q < 0 THEN Perish : EXIT SUB
  pAltit = 16 * SQR(q)
  IF pAltit > 255 THEN pAltit = 255
  IF pAltit = 0 THEN Perish
END SUB

SUB CabinTemp
  LOCAL FLOAT q, xh, yh, zh
  LOCAL INTEGER got
  IF (mcnt AND 31) <> 20 THEN EXIT SUB
  pCabT = 30                              ' deep space, one notch on the bar
  IF inWitch THEN EXIT SUB
  ' The station and the sun never share a bubble, so inside the safe zone
  ' there is nothing out there to be warmed by.
  IF inSafe THEN EXIT SUB
  IF sTyp(SLOT_STAR) <> T_SUN THEN EXIT SUB
  IF ABS(sX(SLOT_STAR)) >= UNIT THEN EXIT SUB
  IF ABS(sY(SLOT_STAR)) >= UNIT THEN EXIT SUB
  IF ABS(sZ(SLOT_STAR)) >= UNIT THEN EXIT SUB
  xh = sX(SLOT_STAR) / 256
  yh = sY(SLOT_STAR) / 256
  zh = sZ(SLOT_STAR) / 256
  q = (xh * xh + yh * yh + zh * zh) / 256
  IF q > 255 THEN EXIT SUB
  pCabT = 285 - q
  IF pCabT > 255 THEN Perish : EXIT SUB
  IF pCabT < 224 THEN EXIT SUB
  IF eqOwned(EQ_SCOOPS) = 0 THEN EXIT SUB
  ' The faster we are going the more we take, a tenth of a light year for
  ' every eight of speed, so between nothing and half a light year.
  got = dSpeed \ 8
  IF got = 0 THEN EXIT SUB
  pFuel = pFuel + got
  IF pFuel > 70 THEN pFuel = 70
  Message "FUEL SCOOPS ON"
END SUB

' Flying into things.  The original's test is that all three coordinates
' have a zero high byte, which is to say within 256 units.
SUB Contact
  LOCAL INTEGER n, t
  n = 2
  DO WHILE n < nUsed
    IF Touching(n) THEN
      t = sTyp(n)
      IF Scoopable(t) AND eqOwned(EQ_SCOOPS) <> 0 AND sY(n) < 0 THEN
        ScoopIt n, t
        ' The slot is gone and the table has closed up, so n stays put.
      ELSE
        ' Anything else at that range is a collision, and it goes as badly
        ' for whatever we hit as it does for us.
        HitPlayer 32
        Explode n
        n = n + 1
      ENDIF
    ELSE
      n = n + 1
    ENDIF
  LOOP
END SUB

FUNCTION Touching(n AS INTEGER) AS INTEGER
  Touching = 0
  IF sTyp(n) = 0 OR sBp(n) < 0 OR sExp(n) > 0 THEN EXIT FUNCTION
  IF ABS(sX(n)) >= 256 OR ABS(sY(n)) >= 256 OR ABS(sZ(n)) >= 256 THEN EXIT FUNCTION
  Touching = 1
END FUNCTION

FUNCTION Scoopable(t AS INTEGER) AS INTEGER
  Scoopable = 0
  IF t = T_CANISTER OR t = T_ESCAPE OR t = T_THARGON THEN Scoopable = 1
END FUNCTION

SUB ScoopIt(n AS INTEGER, t AS INTEGER)
  LOCAL INTEGER item
  SELECT CASE t
    CASE T_ESCAPE  : item = 3            ' an escape pod is slaves
    CASE T_THARGON : item = 16           ' and a Thargon is alien items
    CASE ELSE      : item = INT(RND * 8) ' a canister, anything up to computers
  END SELECT
  IF item < 13 AND HoldUsed() >= holdSize THEN
    ' No room, so it is lost rather than taken.
    Sfx SFX_BOOP
  ELSE
    cargo(item) = cargo(item) + 1
    Message mkName$(item)
    Sfx SFX_BEEP
  ENDIF
  KillShip n
END SUB

' The planet, or the sun, or something we flew into at speed.
SUB Perish
  pEnergy = 0
  dead = 1
  Sfx SFX_BOOM
  Sfx SFX_BOOMT
END SUB
