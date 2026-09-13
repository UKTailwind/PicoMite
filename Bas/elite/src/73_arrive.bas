' =====================================================================
'  Arriving somewhere: hyperspace, the planet and sun, the station
'
'  When the player jumps, the destination system's seeds become the
'  current ones and a fresh bubble is built around them.  Nothing about
'  the destination was stored while we were away - the planet's size, its
'  markings, where the sun sits - it is all regenerated from the seeds.
'
'  Distances here are in units of 65536, which is the step the original's
'  sign byte moves in.  The planet lands three to seven of those ahead,
'  taken from the system's own seeds, so every system looks different on
'  arrival but the same system always looks the same.
' =====================================================================

' Build the bubble for the system the seeds are sitting on.  The planet
' goes ahead of us and the sun behind, both placed from the seeds, and
' the planet turns for ever.
SUB ArriveInSystem
  LOCAL INTEGER n, pz, sz, sx, ptype
  ClearSlots
  SysData
  ' Arriving somewhere new halves what is on our record: nobody this far
  ' away has heard the details, and the original is as forgiving as that.
  legal = legal \ 2
  spawnEV = 0
  ' Planet: three to seven units straight ahead, and it carries a crater
  ' or an equator depending on a bit of the system's technology level.
  pz = (((gs0 >> 8) AND 7) + 6) \ 2
  IF pz < 3 THEN pz = 3
  ptype = T_PLANET
  IF (sysTech AND 2) <> 0 THEN ptype = T_CRATER
  MATH Q_EULER RAD(35), RAD(40), 0, qA() : qA(4) = 1
  n = NewShip(ptype, 0, 0, pz * UNIT, qA())
  IF n >= 0 THEN sRol(n) = 127 : sPit(n) = 127

  ' Sun: behind us, an odd number of units away, offset sideways.
  sz = ((((gs2 >> 8) AND 7) OR 1))
  sx = ((gs2 >> 8) AND 3) * UNIT
  MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
  n = NewShip(T_SUN, sx, 0, -sz * UNIT, qA())

  dSpeed = 0
  inSafe = 0
  mcnt = 0
END SUB

' The state immediately after launching out of the station's slot: the
' planet dead ahead one unit away, the station just behind us, and the
' ship already moving.
SUB LaunchState
  LOCAL INTEGER n, ptype
  ClearSlots
  SysData
  ptype = T_PLANET
  IF (sysTech AND 2) <> 0 THEN ptype = T_CRATER
  MATH Q_EULER RAD(35), RAD(40), 0, qA() : qA(4) = 1
  n = NewShip(ptype, 0, 0, UNIT, qA())
  IF n >= 0 THEN sRol(n) = 127 : sPit(n) = 127
  MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
  n = NewShip(T_STATION, 0, 0, -256, qA())
  IF n >= 0 THEN
    ' Sign bit set for anticlockwise, magnitude 127 for no damping: the
    ' station turns at the same rate for ever.
    sRol(n) = 255
    sPit(n) = 0
    sAI(n) = 1
  ENDIF
  dSpeed = LAUNCHSPD
  inSafe = 1
  mcnt = 0
  ' Leaving the station with a hold full of contraband can only make
  ' matters worse, and the police outside will already know.
  legal = legal OR Contraband()
  IF legal > 255 THEN legal = 255
END SUB

' Can we get there on what is in the tank?
FUNCTION CanReach(target AS INTEGER) AS INTEGER
  LOCAL INTEGER d, hx, hy
  hx = homeX : hy = homeY
  GotoSystem gGal, target
  SysData
  d = SysDist(hx, hy, sysX, sysY * 2)
  GotoSystem gGal, homeSys
  SysData
  CanReach = (d <= pFuel)
END FUNCTION

' Jump.  Fuel pays for the distance, the destination becomes home, and a
' new bubble is generated.  Very occasionally the jump goes wrong and
' drops the ship into witchspace, which has no planet, no sun and no
' station - only Thargoids.
SUB Hyperspace(target AS INTEGER)
  LOCAL INTEGER d, hx, hy
  hx = homeX : hy = homeY
  GotoSystem gGal, target
  SysData
  d = SysDist(hx, hy, sysX, sysY * 2)
  IF d > pFuel THEN Sfx SFX_BOOP : EXIT SUB
  Sfx SFX_HYPER
  pFuel = pFuel - d
  homeSys = target
  homeX = sysX
  homeY = sysY * 2
  curX = homeX : curY = homeY
  selSys = target
  ' The market is redrawn on arrival and not touched again until we leave.
  mkByte = INT(RND * 256)
  MakeMarket sysEco, mkByte
  IF RND < 0.004 THEN
    Witchspace
  ELSE
    ArriveInSystem
  ENDIF
END SUB

' A mis-jump: nowhere at all, with company.
SUB Witchspace
  LOCAL INTEGER n, i
  ClearSlots
  FOR i = 0 TO 1
    MATH Q_EULER RAD(180), 0, 0, qA() : qA(4) = 1
    n = NewShip(T_THARGOID, (i * 2 - 1) * 1500, 200, 4000 + i * 1200, qA())
    IF n >= 0 THEN sSpd(n) = 20 : sAI(n) = 255
  NEXT i
  dSpeed = 0
  inSafe = 0
  mcnt = 0
  inWitch = 1
END SUB

' The station appears once we are close enough to where it orbits, on the
' same schedule the original uses - one check every 32 frames.
SUB StationCheck
  LOCAL INTEGER n, px, py, pz
  IF inWitch THEN EXIT SUB
  IF sTyp(SLOT_PLANET) = 0 THEN EXIT SUB
  IF (mcnt AND 31) <> 0 THEN EXIT SUB
  IF sTyp(SLOT_STAR) = T_STATION THEN EXIT SUB
  ' The station orbits two planet radii from the centre, so it sits one
  ' radius above the surface on the side we approach from.
  px = sX(SLOT_PLANET)
  py = sY(SLOT_PLANET)
  pz = sZ(SLOT_PLANET) - 2 * PRADIUS
  inSafe = 0
  IF ABS(px) < SAFEZONE AND ABS(py) < SAFEZONE AND ABS(pz) < SAFEZONE THEN
    inSafe = 1
    IF sTyp(SLOT_STAR) <> 0 THEN KillShip SLOT_STAR
    ' Turned to face away from the planet, which is the side a ship
    ' arrives from and so the side the slot has to be on.  Left pointing
    ' the other way - as this was - the second docking test can never pass
    ' and every approach ends as a crash, however well it is flown.
    MATH Q_EULER RAD(180), 0, 0, qA() : qA(4) = 1
    n = NewShip(T_STATION, px, py, pz, qA())
    IF n >= 0 THEN sRol(n) = 255 : sAI(n) = 1
  ENDIF
END SUB
