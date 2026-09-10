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
  ' A new commander carries a pulse laser on the front view only.
  lasPower = 15 : lasTimer = 0 : lasFlash = 0
  kills = 0 : dead = 0 : energyUnit = 0
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
  ' A Cobra coming straight at us with its AI off - a trader, which flies
  ' on and does not evade, so a fixed forward shot can actually connect.
  MATH Q_EULER RAD(180), 0, 0, qA() : qA(4) = 1
  n = NewShip(T_COBRA3, 0, 0, 9000, qA())
  IF n >= 0 THEN sSpd(n) = 12 : sAI(n) = 0
  ' And a hostile Viper, which does evade and does shoot back.
  MATH Q_EULER RAD(-70), RAD(10), 0, qA() : qA(4) = 1
  n = NewShip(T_VIPER, -1200, -300, 5000, qA())
  IF n >= 0 THEN sSpd(n) = 20 : sAI(n) = 128 OR (24 * 2)
  ' An asteroid dead ahead and tumbling.  It has no AI at all, so it
  ' cannot evade, and it is the one thing a fixed forward shot is certain
  ' to connect with - which is what makes it the honest test of the whole
  ' hit, damage, explode and score path.
  MATH Q_EULER RAD(30), RAD(20), RAD(10), qA() : qA(4) = 1
  n = NewShip(T_ASTEROID, 0, 0, 4000, qA())
  IF n >= 0 THEN sPit(n) = 127
END SUB

' ------------------------------------------------- the scripted demo
' Stands in for the keyboard so the flight core can be exercised and
' photographed without anyone at the keys.  Each stretch of frames tests
' one thing: accelerating, rolling, pitching, and each of the views.
SUB DemoInput(f AS INTEGER)
  kRollL = 0 : kRollR = 0 : kUp = 0 : kDn = 0
  kFaster = 0 : kSlower = 0 : kFire = 0 : kQuit = 0
  kTarget = 0 : kMissile = 0 : kECM = 0
  SELECT CASE f
    CASE 0 TO 9     : kFaster = 1                  ' ease forward only
    CASE 20 TO 120  : kFire = 1                    ' hold the trigger down
    CASE 130        : kTarget = 1                  ' lock on
    CASE 132        : kMissile = 1                 ' and launch
    CASE 200        : kECM = 1                     ' burst the E.C.M.
    CASE 160 TO 179 : vw = 1                       ' look behind
    CASE 180 TO 199 : vw = 3                       ' and to the right
    CASE 200 TO 209 : vw = 0
  END SELECT
  ' Halfway through, jump somewhere: this rebuilds the whole bubble from
  ' the destination's seeds and charges the tank for the distance.
  IF f = 250 THEN
    Hyperspace selSys
  ENDIF
  SELECT CASE f
    CASE 260 TO 999 : kFaster = 1
  END SELECT
END SUB

SUB SaveShot(f AS INTEGER)
  SAVE IMAGE "A:/fly" + STR$(f) + ".bmp"
END SUB

' One-shot diagnostic: what is actually in the bubble, and would the
' laser's alignment test accept it?
SUB DumpSlots
  LOCAL INTEGER n
  PRINT "slots at frame 60, nUsed"; nUsed; " lasPower"; lasPower
  FOR n = 0 TO nUsed - 1
    PRINT "  "; n; " typ"; sTyp(n); " bp"; sBp(n); " obj"; sObj(n);
    PRINT " x"; STR$(sX(n), 0, 0); " y"; STR$(sY(n), 0, 0); " z"; STR$(sZ(n), 0, 0);
    IF sBp(n) >= 0 THEN
      PRINT " area"; bArea(sBp(n)); " ene"; sEne(n); " ai"; sAI(n);
      IF ABS(sX(n)) < 256 AND ABS(sY(n)) < 256 AND sZ(n) > 0 THEN
        IF sX(n)*sX(n) + sY(n)*sY(n) < bArea(sBp(n)) THEN PRINT " <- IN THE SIGHTS";
      ENDIF
    ENDIF
    PRINT
  NEXT n
END SUB

' A scene that exists only to prove the docking approach: the station
' ahead with its slot facing us, and the computer flying us in.  The
' station rolls all the time, so the alignment test is only satisfied for
' part of each turn - the approach has to arrive at the right moment.
SUB DockScene
  LOCAL INTEGER n
  pRoll = JCENTRE : pPitch = JCENTRE
  pEnergy = 255 : pFsh = 255 : pAsh = 255 : pFuel = 70
  pCabT = 30 : pLasT = 0 : pAltit = 200 : pMissl = 3
  cashTenths = 1000 : holdSize = 20
  lasPower = 15 : kills = 0 : dead = 0 : docked = 0
  vw = 0 : inWitch = 0 : msLock = -1
  InitStardust
  LoadMarket
  gGal = 1
  SetGalaxy 1
  FOR n = 0 TO 255
    SysData
    IF SysName$() = "LAVE" THEN EXIT FOR
    NextSystem
  NEXT n
  homeSys = n : selSys = n
  homeX = sysX : homeY = sysY * 2
  curX = homeX : curY = homeY

  ClearSlots
  MATH Q_EULER RAD(35), RAD(40), 0, qA() : qA(4) = 1
  n = NewShip(T_CRATER, 0, -20000, 3 * UNIT, qA())
  IF n >= 0 THEN sRol(n) = 127

  ' Slot towards us, and turning as it always does.
  MATH Q_EULER RAD(180), 0, 0, qA() : qA(4) = 1
  n = NewShip(T_STATION, 0, 0, 7000, qA())
  IF n >= 0 THEN sRol(n) = 255 : sAI(n) = 1
  dSpeed = 0
  inSafe = 1
  mcnt = 0
  dockComp = 1
END SUB

SUB DockInput(f AS INTEGER)
  kRollL = 0 : kRollR = 0 : kUp = 0 : kDn = 0
  kFaster = 0 : kSlower = 0 : kFire = 0 : kQuit = 0
  kTarget = 0 : kMissile = 0 : kECM = 0 : kDock = 0
END SUB

' Draw each docked screen once and photograph it.  Nothing here is
' interactive: it exists to prove the screens render from real state.
SUB DockedScreens
  LOCAL INTEGER n, i
  pFuel = 44 : cashTenths = 1000 : holdSize = 20
  pEnergy = 255 : pFsh = 255 : pAsh = 255 : pMissl = 3
  lasPower = 15 : kills = 20 : legal = 0 : docked = 1 : energyUnit = 0
  gGal = 1
  LoadMarket
  EquipTable
  SetGalaxy 1
  FOR n = 0 TO 255
    SysData
    IF SysName$() = "LAVE" THEN EXIT FOR
    NextSystem
  NEXT n
  homeSys = n : selSys = n
  homeX = sysX : homeY = sysY * 2
  curX = homeX : curY = homeY
  mkByte = 0
  MakeMarket sysEco, mkByte

  ' Trade a little so the screens have something to show.
  FOR i = 1 TO 5 : BuyOne 0 : NEXT i        ' five tonnes of food
  FOR i = 1 TO 3 : BuyOne 12 : NEXT i       ' and some minerals
  eqOwned(2) = 1                            ' fitted with E.C.M.

  MarketScreen 6
  FRAMEBUFFER COPY F, N
  SAVE IMAGE "A:/dock_market.bmp"
  StatusScreen
  FRAMEBUFFER COPY F, N
  SAVE IMAGE "A:/dock_status.bmp"
  EquipScreen 2
  FRAMEBUFFER COPY F, N
  SAVE IMAGE "A:/dock_equip.bmp"
  InventoryScreen
  FRAMEBUFFER COPY F, N
  SAVE IMAGE "A:/dock_inv.bmp"

  PRINT "cash after trading "; STR$(cashTenths / 10); " Cr, hold "; STR$(HoldUsed()); "/"; STR$(holdSize)
  SaveCommander "A:/cmdr.txt"
  cashTenths = 0 : kills = 0
  IF LoadCommander("A:/cmdr.txt") THEN
    PRINT "commander reloaded: cash "; STR$(cashTenths / 10); " Cr, kills "; STR$(kills); ", at "; SysName$()
  ELSE
    PRINT "** commander file did not load **"
  ENDIF
END SUB
