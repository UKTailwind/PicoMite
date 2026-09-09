' =====================================================================
'  The phase 2 test scene: just launched from the station
'
'  Not yet the real arrival geometry - SOLAR places the planet and the
'  sun on arrival in a system, and NWSPS puts the station in orbit, and
'  both of those belong to phase 3.  This is a hand-placed bubble that
'  exercises every part of the flight core: a rotating station near
'  enough to fill part of the view, ships at three different ranges,
'  something tumbling, and the planet close enough to show its curve
'  along the bottom of the screen.
' =====================================================================
SUB TestScene
  LOCAL INTEGER n
  ClearSlots

  ' Player state
  pRoll = JCENTRE : pPitch = JCENTRE : dSpeed = 0
  pEnergy = 255 : pFsh = 255 : pAsh = 255 : pFuel = 70
  pCabT = 30 : pLasT = 0 : pAltit = 200 : pMissl = 3
  vw = 0 : mcnt = 0 : inSafe = 1
  InitStardust

  ' The planet, low and ahead, one station orbit away
  MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
  n = NewShip(T_CRATER, 0, -20000, 50000, qA())

  ' The station we have just left, turning as it always does
  MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
  n = NewShip(T_STATION, 0, 0, 800, qA())
  IF n >= 0 THEN sRol(n) = 127        ' 127 = turn for ever

  ' A Cobra heading away from us
  MATH Q_EULER RAD(20), 0, 0, qA() : qA(4) = 1
  n = NewShip(T_COBRA3, 900, 150, 3500, qA())
  IF n >= 0 THEN sSpd(n) = 12

  ' A Viper crossing
  MATH Q_EULER RAD(-70), RAD(10), 0, qA() : qA(4) = 1
  n = NewShip(T_VIPER, -1200, -300, 5000, qA())
  IF n >= 0 THEN sSpd(n) = 20

  ' An asteroid, tumbling
  MATH Q_EULER RAD(30), RAD(20), RAD(10), qA() : qA(4) = 1
  n = NewShip(T_ASTEROID, 400, 600, 2200, qA())
  IF n >= 0 THEN sPit(n) = 127

  ' And a canister drifting close by
  MATH Q_EULER RAD(10), RAD(200), 0, qA() : qA(4) = 1
  n = NewShip(T_CANISTER, -350, 200, 1200, qA())
  IF n >= 0 THEN sRol(n) = 130        ' slow tumble the other way
END SUB

' ------------------------------------------------- the scripted demo
' Stands in for the keyboard so the flight core can be exercised and
' photographed without anyone at the keys.  Each stretch of frames tests
' one thing: accelerating, rolling, pitching, and each of the views.
SUB DemoInput(f AS INTEGER)
  kRollL = 0 : kRollR = 0 : kUp = 0 : kDn = 0
  kFaster = 0 : kSlower = 0 : kFire = 0 : kQuit = 0
  SELECT CASE f
    CASE 0 TO 29    : kFaster = 1                  ' build up speed
    CASE 30 TO 79   : kRollR = 1                   ' roll right
    CASE 80 TO 109  : kUp = 1                      ' and pull up
    CASE 110 TO 139 : vw = 1                       ' look behind
    CASE 140 TO 169 : vw = 2                       ' look left
    CASE 170 TO 199 : vw = 3                       ' look right
    CASE 200 TO 209 : vw = 0
    CASE 210 TO 259 : kRollL = 1 : kDn = 1         ' roll and dive together
  END SELECT
END SUB

SUB SaveShot(f AS INTEGER)
  SAVE IMAGE "A:/fly" + STR$(f) + ".bmp"
END SUB
