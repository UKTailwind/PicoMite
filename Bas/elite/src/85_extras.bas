' =====================================================================
'  The equipment that does something drastic
'
'  An energy bomb kills everything in the bubble except the station, and
'  is gone once used.  An escape pod takes you home and leaves the ship,
'  its cargo and everything fitted to it behind - and clears your record,
'  because whoever they pull out of the capsule is somebody new.  The
'  in-system jump moves the whole bubble a unit closer, which is the
'  original's way of crossing the empty distance between a planet and its
'  sun without flying it; it refuses while anything but rocks and cargo is
'  about, inside the station's zone, in witchspace, or with the planet
'  close ahead.  And the galactic hyperdrive rotates the galaxy's seeds
'  one place, which is the whole of how the eight galaxies are made.
' =====================================================================

SUB EnergyBomb
  LOCAL INTEGER n
  IF eqOwned(EQ_BOMB) = 0 THEN Sfx SFX_BOOP : EXIT SUB
  eqOwned(EQ_BOMB) = 0                  ' one use and it is spent
  FOR n = 2 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 AND sExp(n) = 0 THEN
      ' A station is far too big to care.
      IF sTyp(n) <> T_STATION THEN Explode n
    ENDIF
  NEXT n
  Sfx SFX_BOOM
  Message "ENERGY BOMB"
END SUB

SUB EscapePod
  LOCAL INTEGER i
  IF eqOwned(EQ_POD) = 0 THEN Sfx SFX_BOOP : EXIT SUB
  FOR i = 0 TO NGOODS - 1 : cargo(i) = 0 : NEXT i
  FOR i = 0 TO NEQUIP - 1 : eqOwned(i) = 0 : NEXT i
  ' A new ship, as bare as the one you started with.
  lasView(0) = LAS_PULSE : lasView(1) = 0 : lasView(2) = 0 : lasView(3) = 0
  holdSize = 20 : pMissl = 0 : energyUnit = 0
  legal = 0
  Sfx SFX_LAUNCH
  DoDock
END SUB

' The pilot leaves.  The pod is given the original's own AI flag of 254 -
' full aggression, no E.C.M. - which sends it towards the planet rather than
' towards us, and it is worth scooping: an escape pod is slaves.
SUB BailOut(n AS INTEGER)
  LOCAL INTEGER m
  m = NewFacing(T_ESCAPE, sX(n), sY(n), sZ(n), 0)
  IF m < 0 THEN EXIT SUB
  sAI(m) = 254
  sSpd(m) = bSpd(sBp(m))
END SUB

SUB InSystemJump
  LOCAL INTEGER n
  IF inWitch OR inSafe THEN Sfx SFX_BOOP : EXIT SUB
  ' Anything out there that is not a rock or a canister stops it.
  FOR n = 2 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 THEN
      IF sTyp(n) <> T_ASTEROID AND sTyp(n) <> T_CANISTER AND sTyp(n) <> T_ESCAPE THEN
        Sfx SFX_BOOP
        EXIT SUB
      ENDIF
    ENDIF
  NEXT n
  ' And not while the planet is close ahead, or we would jump into it.
  IF sTyp(SLOT_PLANET) <> 0 AND sZ(SLOT_PLANET) > 0 THEN
    IF FarAxis(SLOT_PLANET) < 2 * UNIT THEN Sfx SFX_BOOP : EXIT SUB
  ENDIF
  IF sTyp(SLOT_PLANET) <> 0 THEN sZ(SLOT_PLANET) = sZ(SLOT_PLANET) - UNIT
  IF sTyp(SLOT_STAR) <> 0 THEN sZ(SLOT_STAR) = sZ(SLOT_STAR) - UNIT
  Sfx SFX_LAUNCH
END SUB

FUNCTION FarAxis(n AS INTEGER) AS FLOAT
  LOCAL FLOAT m
  m = ABS(sX(n))
  IF ABS(sY(n)) > m THEN m = ABS(sY(n))
  IF ABS(sZ(n)) > m THEN m = ABS(sZ(n))
  FarAxis = m
END FUNCTION

' Rotating the six seed bytes one place left is the whole of how the eight
' galaxies differ, and SetGalaxy already does it from the beginning, so a
' galactic jump is a galaxy number and a fresh arrival.  You land at (96,
' 96) - or rather at whatever system is nearest to it, which the original's
' cassette version famously got wrong and could strand you in empty space.
SUB GalacticJump
  IF eqOwned(EQ_GALHYP) = 0 THEN Sfx SFX_BOOP : EXIT SUB
  eqOwned(EQ_GALHYP) = 0
  legal = 0                             ' nobody here has heard of us
  gGal = gGal + 1
  IF gGal > 8 THEN gGal = 1
  SetGalaxy gGal
  curX = 96 : curY = 96
  FindSystem curX, curY
  homeSys = selSys
  GotoSystem gGal, homeSys
  SysData
  homeX = sysX : homeY = sysY * 2
  curX = homeX : curY = homeY
  mkByte = INT(RND * 256)
  MakeMarket sysEco, mkByte
  Sfx SFX_HYPER
  HyperTunnel
  ArriveInSystem
  Message "GALACTIC HYPERSPACE"
END SUB
