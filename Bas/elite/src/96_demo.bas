' =====================================================================
'  The attract demo: a game of Elite that plays itself
'
'  It drives the real game.  Nothing here draws a screen or moves a ship
'  of its own - it stands in for the keyboard at the two places the shell
'  reads one, so every screen, every shot and the docking at the end are
'  the game's own code doing the work.
'
'    docked   DockKey asks DemoKey, which holds the screen for a moment
'             and then returns the next key from a scripted list
'    flight   ReadKeys asks DemoFly, which sets the same flags a held key
'             would from a timeline of ticks, one tick per frame
'
'  The flying is not a recording of stick positions - a recorded dogfight
'  would miss by the second frame, because the ships evade.  DemoAim flies
'  the ship the way the docking computer does: it reads where the target
'  actually is and rolls and pitches towards it, so the demo hits what it
'  aims at however the fight goes.
'
'  Three things are compressed, because they take real minutes to happen:
'  the demo's commander starts with 2500 credits rather than 100 so the
'  shop is worth visiting, the safe zone is declared behind us rather than
'  flown out of, and the planet is moved to where the station's orbit
'  brings it into range instead of the ship crossing the distance.  Each
'  is marked where it happens.  Everything else is played.
'
'  Any key hands the game over to whoever pressed it; escape stops.
' =====================================================================

SUB RunDemo
  demoStop = 0
  demoTakeover = 0
  DemoScript
  DO
    demoMode = 1
    demoStep = 0
    demoLeg = 0
    demoTick = 0
    demoTgt = -1
    demoCap$ = ""
    NewCommander
    ' A demo commander with something to spend and room to fill.
    cashTenths = 25000
    ' And a docking computer, which Lave's technology level cannot sell:
    ' without it the demo could not show the approach at all.
    eqOwned(EQ_DOCK) = 1
    ClearSlots
    docked = 1
    dscreen = SCR_STATUS
    dbuy = 1
    DemoPickTarget
    RunGame
  LOOP UNTIL demoStop OR DEMOLOOP = 0
  demoMode = 0
END SUB

' --- standing in for the keyboard, docked
'
' Hold whatever is on the screen for a moment, then press the next key.
FUNCTION DemoKey() AS INTEGER
  LOCAL INTEGER k
  ' An information screen opened from the cockpit: there is no script for
  ' those, so hold it and close it again.
  IF docked = 0 THEN
    DemoKey = DemoHold(DEMOREAD, 13)
    EXIT FUNCTION
  ENDIF
  IF demoStep >= dkCount THEN
    DemoKey = DemoHold(DEMOREAD, 27)
    EXIT FUNCTION
  ENDIF
  k = DemoHold(dkWait(demoStep), dkKey(demoStep))
  IF demoMode = 0 THEN DemoKey = k : EXIT FUNCTION
  demoStep = demoStep + 1
  ' Launching starts a flight leg, and the flight timeline with it.
  IF k = 145 THEN demoLeg = demoLeg + 1 : demoTick = 0
  DemoKey = k
END FUNCTION

' Wait, unless somebody at the keyboard would rather play.
FUNCTION DemoHold(ms AS INTEGER, k AS INTEGER) AS INTEGER
  LOCAL FLOAT t
  LOCAL kb$ LENGTH 2
  t = TIMER + ms
  DO
    ' Without this the whole docked half of the demo is a blocking wait, and
    ' anything started during it plays until the next effect replaces it.
    SoundService
    kb$ = INKEY$
    IF kb$ <> "" THEN
      demoStop = 1
      demoMode = 0
      demoCap$ = ""
      quitGame = 1
      ' Escape goes back to the title; anything else means somebody wants
      ' a game of their own, and they get a new commander rather than the
      ' demo's, which has been given money it did not earn.
      IF kb$ <> CHR$(27) THEN demoTakeover = 1
      DemoHold = 27
      EXIT FUNCTION
    ENDIF
  LOOP UNTIL TIMER > t
  DemoHold = k
END FUNCTION

' --- standing in for the keyboard, in flight
SUB DemoFly
  LOCAL kb$ LENGTH 2
  kb$ = INKEY$
  IF kb$ <> "" THEN
    demoStop = 1
    demoMode = 0
    demoCap$ = ""
    kQuit = 1
    IF kb$ <> CHR$(27) THEN demoTakeover = 1
    EXIT SUB
  ENDIF
  kRollL = 0 : kRollR = 0 : kUp = 0 : kDn = 0
  kFaster = 0 : kSlower = 0 : kFire = 0 : kQuit = 0
  kTarget = 0 : kMissile = 0 : kECM = 0 : kDock = 0
  kJump = 0 : kChart = 0 : kPause = 0
  kBomb = 0 : kHop = 0 : kGal = 0
  demoTick = demoTick + 1
  IF demoLeg <= 1 THEN DemoLeg1 ELSE DemoLeg2
END SUB

' --- leg one: out of Lave, and what is on the space lane
SUB DemoLeg1
  SELECT CASE demoTick
    CASE 1 TO 55    : kFaster = 1
    CASE 90 TO 145  : kRollR = 1
    CASE 185 TO 240 : kRollL = 1
    CASE 280        : vw = 1 : demoCap$ = "REAR VIEW: LAVE STATION BEHIND US"
    CASE 360        : vw = 2 : demoCap$ = "LEFT VIEW"
    CASE 420        : vw = 3 : demoCap$ = "RIGHT VIEW"
    CASE 480        : vw = 0 : demoCap$ = ""
    CASE 510        : demoTgt = DemoSpawn(T_COBRA3, 0, 200, 5000, 14, 0, 0)
                      demoCap$ = "A COBRA MK III ON THE SPACE LANE"
    CASE 620        : demoCap$ = "IN THE SIGHTS"
    CASE 900        : demoCap$ = ""
    CASE 930        : demoTgt = DemoSpawn(T_VIPER, -1800, 500, 6000, 20, 128 OR 48, 180)
                      demoCap$ = "POLICE: THEY HAVE SEEN THE SLAVES"
    CASE 1540       : demoCap$ = "AN ASTEROID"
                      demoTgt = DemoSpawn(T_ASTEROID, 300, -200, 4000, 0, 0, 180)
                      IF demoTgt >= 0 THEN sPit(demoTgt) = 127
    CASE 1740       : demoCap$ = "MISSILE LOCKED"
    CASE 1810       : kMissile = 1 : demoCap$ = "MISSILE AWAY"
    CASE 1960       : demoCap$ = ""
    CASE 2000       : DemoIncoming
                      demoCap$ = "INCOMING MISSILE"
    CASE 2110       : kECM = 1 : demoCap$ = "E.C.M."
    CASE 2200       : demoCap$ = ""
    CASE 2230       : kChart = 4          ' market prices, from the cockpit
    CASE 2260       : kChart = 1          ' the galactic chart
    CASE 2290       : kChart = 3          ' and what is known about the target
    CASE 2330       : DemoPickTarget
                      ' Compressed: an hour of cruising out of the safe zone.
                      inSafe = 0
                      demoCap$ = "CLEAR OF THE SAFE ZONE"
    CASE 2380       : kJump = 1 : demoCap$ = "HYPERSPACE"
    CASE 2400       : demoLeg = 2 : demoTick = 0 : demoCap$ = ""
  END SELECT
  ' The chases.  Which slot is worth flying at changes as things die, so
  ' the target is looked up rather than remembered.
  IF demoTick > 530 AND demoTick < 900 THEN DemoAim demoTgt, 1
  IF demoTick > 950 AND demoTick < 1500 THEN DemoAim DemoNearestFoe(), 1
  ' Lining up for the missile: the laser is held off so it does not do the
  ' job first, and the lock is asked for over a stretch rather than on one
  ' tick, because it only takes when the target is inside the sights.
  IF demoTick > 1550 AND demoTick < 1809 THEN DemoAim demoTgt, 0
  IF demoTick > 1740 AND demoTick < 1809 THEN kTarget = 1
  IF demoTick > 1815 AND demoTick < 1950 THEN DemoAim demoTgt, 0
  IF demoTick > 3000 THEN kQuit = 1
END SUB

' --- leg two: the new system, a fight, and the way in
SUB DemoLeg2
  SELECT CASE demoTick
    CASE 1          : demoCap$ = "ARRIVED AT " + SysName$()
    CASE 2 TO 55    : kFaster = 1
    CASE 90         : vw = 1 : demoCap$ = "THE SUN, BEHIND US"
    CASE 190        : vw = 0 : demoCap$ = ""
    CASE 240        : demoTgt = DemoSpawn(T_MAMBA, 1400, -300, 6000, 24, 128 OR 56, 180)
                      demoCap$ = "PIRATES"
    CASE 250        : demoTgt = DemoSpawn(T_SIDEWINDER, -1600, 400, 7000, 22, 128 OR 56, 180)
    CASE 900        : demoCap$ = ""
    CASE 940        : DemoCloseOnStation
                      demoCap$ = "THE STATION IS IN RANGE"
    ' The computer has to have the controls before the station is reached.
    ' It only ever steers towards what is in front of it, so a ship still
    ' doing forty when the station appears goes straight past and the
    ' computer, with nothing ahead of it any more, flies on for ever.
    CASE 950        : kDock = 1 : demoCap$ = "DOCKING COMPUTER ENGAGED"
    CASE 1150       : demoCap$ = ""
  END SELECT
  IF demoTick > 260 AND demoTick < 900 THEN DemoAim DemoNearestFoe(), 1
  ' Nothing in a demo may stick: if the approach has not finished by now,
  ' something went wrong, so end this run and start the next one.
  IF demoTick > 3600 THEN kQuit = 1
END SUB

' --- flying at something
'
' The same idea as the docking computer: read where the target is, and
' push the controls a held key would push.  fire is off while lining up
' for a missile, so the laser does not do the job first.
SUB DemoAim(n AS INTEGER, fire AS INTEGER)
  LOCAL FLOAT d, ux, uy, uz
  IF n < 0 OR n >= nUsed THEN EXIT SUB
  IF sTyp(n) = 0 OR sExp(n) > 0 THEN EXIT SUB
  d = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
  IF d < 1 THEN EXIT SUB
  ux = sX(n) / d : uy = sY(n) / d : uz = sZ(n) / d
  ' Roll brings it onto the vertical, pitch brings it down to the sights.
  IF ux > 0.03 THEN
    kRollR = 1
  ELSEIF ux < -0.03 THEN
    kRollL = 1
  ENDIF
  IF uy < -0.03 THEN
    kUp = 1
  ELSEIF uy > 0.03 THEN
    kDn = 1
  ENDIF
  ' Dead astern there is nothing for either rule to work on, so pull.
  IF uz < 0 AND ABS(uy) < 0.05 THEN kUp = 1
  ' Hold a fighting range rather than flying through it.  This has to name
  ' the speed it wants and steer towards it: a rule that only brakes when
  ' too close and only accelerates when too far leaves the ship crawling at
  ' one in the gap between the two, unable to close again - which is how
  ' the first version of this managed a whole demo without a single kill.
  IF d > 1500 THEN
    IF dSpeed < 32 THEN kFaster = 1
  ELSEIF d < 450 THEN
    IF dSpeed > 5 THEN kSlower = 1
  ELSE
    IF dSpeed < 14 THEN
      kFaster = 1
    ELSEIF dSpeed > 18 THEN
      kSlower = 1
    ENDIF
  ENDIF
  ' Fire on the test the laser itself applies rather than on a guess at it.
  ' A window that only looks about right wastes most of its shots: the
  ' blueprint's targetable area is a few tens of units across, so at any
  ' range but point blank it is a much narrower cone than it appears.
  IF fire THEN
    IF uz > 0 AND sX(n)*sX(n) + sY(n)*sY(n) < bArea(sBp(n)) THEN kFire = 1
  ENDIF
END SUB

FUNCTION DemoNearestFoe() AS INTEGER
  LOCAL INTEGER n, best
  LOCAL FLOAT d, bd
  best = -1 : bd = 1e12
  FOR n = 2 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 AND sExp(n) = 0 THEN
     IF sTyp(n) <> T_MISSILE AND sTyp(n) <> T_CANISTER AND sTyp(n) <> T_ESCAPE THEN
      d = sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n)
      IF d < bd THEN bd = d : best = n
     ENDIF
    ENDIF
  NEXT n
  DemoNearestFoe = best
END FUNCTION

FUNCTION DemoSpawn(t AS INTEGER, x AS FLOAT, y AS FLOAT, z AS FLOAT, spd AS INTEGER, ai AS INTEGER, hdg AS INTEGER) AS INTEGER
  LOCAL INTEGER n
  ' hdg 180 faces us, which is how something on the lane meets a ship
  ' coming the other way; 0 gives us its back to chase.
  MATH Q_EULER RAD(hdg), 0, 0, qA() : qA(4) = 1
  n = NewShip(t, x, y, z, qA())
  IF n >= 0 THEN sSpd(n) = spd : sAI(n) = ai
  DemoSpawn = n
END FUNCTION

' Nothing in the game fires a missile at the player yet, so the demo puts
' one in the air itself to have something for the E.C.M. to answer.
SUB DemoIncoming
  LOCAL INTEGER n
  MATH Q_EULER RAD(180), 0, 0, qA() : qA(4) = 1
  n = NewShip(T_MISSILE, 1800, 0, 6500, qA())
  IF n >= 0 THEN
    sSpd(n) = bSpd(sBp(n))
    sAI(n) = 128 OR 126
    sTgt(n) = -2                    ' -2 is us
  ENDIF
END SUB

' Compressed: the planet is put where an hour of flying would have left
' it, which is close enough for the station's orbit to be found.  The
' station itself is still created by the game's own StationCheck.
SUB DemoCloseOnStation
  IF sTyp(SLOT_PLANET) = 0 THEN EXIT SUB
  ' Stop turning and slow down before moving anything.  The planet ends up
  ' nearly sixty thousand units away, and at that range one frame of full
  ' pitch swings it more than a thousand units off the axis - the station
  ' is then created wherever the planet has got to, off to one side and
  ' already going past.
  pRoll = JCENTRE : pPitch = JCENTRE
  dSpeed = 8
  sX(SLOT_PLANET) = 0
  sY(SLOT_PLANET) = 0
  sZ(SLOT_PLANET) = 2 * PRADIUS + 7000
  ' StationCheck only looks every thirty-two frames; this makes it look on
  ' this one, while the planet is still exactly where it was put.
  mcnt = 0
END SUB

' Somewhere worth jumping to: the furthest system the tank will reach,
' unless the chart cursor has already picked out one that it will.
SUB DemoPickTarget
  LOCAL INTEGER i, best, bd, d, hx, hy
  IF selSys <> homeSys THEN
    IF CanReach(selSys) THEN EXIT SUB
  ENDIF
  hx = homeX : hy = homeY
  best = -1 : bd = 0
  SetGalaxy gGal
  FOR i = 0 TO 255
    SysData
    d = SysDist(hx, hy, sysX, sysY * 2)
    IF i <> homeSys AND d <= pFuel AND d > bd THEN bd = d : best = i
    NextSystem
  NEXT i
  IF best >= 0 THEN
    GotoSystem gGal, best
    SysData
    selSys = best
    curX = sysX : curY = sysY * 2
  ENDIF
  GotoSystem gGal, homeSys
  SysData
END SUB

' What the demo is doing, in the empty rows under the space view.
SUB DemoCaption
  IF demoCap$ = "" THEN EXIT SUB
  TEXT VCX, VIEWH - 26, demoCap$, "CT", 7, 1, cDim
END SUB

' --- the docked script
'
' A key and how long the screen before it is held, in milliseconds.
SUB DemoScript
  LOCAL INTEGER i, k, w
  RESTORE dat_demo
  i = 0
  DO
    READ k, w
    IF k < 0 THEN EXIT DO
    dkKey(i) = k : dkWait(i) = w
    i = i + 1
  LOOP UNTIL i > 127
  dkCount = i
END SUB

dat_demo:
' --- at Lave: look around, trade, outfit the ship
DATA 152,3500          ' hold the status screen, then market prices
DATA 146,3000          ' F2, buying
DATA 32,400            ' food, eight tonnes of it
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,600
DATA 129,400           ' down to the slaves, which is asking for trouble
DATA 129,300
DATA 129,400
DATA 32,400
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,900
DATA 148,2500          ' F4, the equipment shop
DATA 32,1300           ' a missile
DATA 129,700           ' a larger hold
DATA 32,1300
DATA 129,700           ' an E.C.M. system
DATA 32,1300
DATA 129,700           ' past the pulse lasers
DATA 129,700           ' to the beam lasers
DATA 32,700
DATA 145,1500          ' F1: on the fore mount
DATA 149,3000          ' F5, the galactic chart
DATA 131,400           ' walk the cursor across it
DATA 131,400
DATA 131,400
DATA 131,400
DATA 129,700
DATA 151,3000          ' F7, what is known about the system it picked
DATA 150,3000          ' F6, the short range chart
DATA 154,3000          ' F10, what is in the hold
DATA 153,3000          ' F9, the commander
DATA 145,3500          ' F1, launch
' --- and after the jump, docked again: sell the cargo at the far end
DATA 147,4500          ' hold the status screen, then F3, selling
DATA 32,400
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,600
DATA 129,400           ' down to the slaves
DATA 129,300
DATA 129,400
DATA 32,400
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,900
DATA 154,3500          ' F10, an empty hold
DATA 153,5000          ' F9, the commander again: richer, and rated
DATA -1,0
