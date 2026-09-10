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
' The starting state: a new commander at Lave, just launched from the
' station.  The bubble is built by the same code the game uses on
' arrival, so the planet's markings, the station's spin and the sun's
' position all come from Lave's seeds rather than being placed by hand.
SUB TestScene
  LOCAL INTEGER n, i
  ' Player state
  pRoll = JCENTRE : pPitch = JCENTRE
  pEnergy = 255 : pFsh = 255 : pAsh = 255 : pFuel = 70
  pCabT = 30 : pLasT = 0 : pAltit = 200 : pMissl = 3
  cashTenths = 1000 : holdSize = 20
  vw = 0 : inWitch = 0
  InitStardust
  LoadMarket

  ' Find Lave and make it home.
  gGal = 1
  SetGalaxy 1
  FOR i = 0 TO 255
    SysData
    IF SysName$() = "LAVE" THEN EXIT FOR
    NextSystem
  NEXT i
  homeSys = i : selSys = i
  homeX = sysX : homeY = sysY * 2
  curX = homeX : curY = homeY
  mkByte = 0
  MakeMarket sysEco, mkByte

  LaunchState

  ' Some traffic to look at.
  MATH Q_EULER RAD(20), 0, 0, qA() : qA(4) = 1
  n = NewShip(T_COBRA3, 900, 150, 3500, qA())
  IF n >= 0 THEN sSpd(n) = 12
  MATH Q_EULER RAD(-70), RAD(10), 0, qA() : qA(4) = 1
  n = NewShip(T_VIPER, -1200, -300, 5000, qA())
  IF n >= 0 THEN sSpd(n) = 20
  MATH Q_EULER RAD(30), RAD(20), RAD(10), qA() : qA(4) = 1
  n = NewShip(T_ASTEROID, 400, 600, 2200, qA())
  IF n >= 0 THEN sPit(n) = 127
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
  ' Halfway through, jump somewhere: this rebuilds the whole bubble from
  ' the destination's seeds and charges the tank for the distance.
  IF f = 205 THEN
    Hyperspace selSys
  ENDIF
  SELECT CASE f
    CASE 260 TO 999 : kFaster = 1
  END SELECT
END SUB

SUB SaveShot(f AS INTEGER)
  SAVE IMAGE "A:/fly" + STR$(f) + ".bmp"
END SUB
