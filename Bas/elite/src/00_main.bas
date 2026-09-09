' =====================================================================
'  E L I T E        PicoMite MMBasic, PicoComputer 3
'  after the 1984 BBC Micro game by Ian Bell and David Braben
'
'  Phase 2: the flight core.  Universe slots, player controls, ship
'  movement, the four views, ship / planet / sun / stardust rendering,
'  the dashboard and the 3D scanner.  No trading, no galaxy, no combat
'  yet - those are phases 3 and 4.
'
'  Geometry.  The BBC space view is 256 x 192 pixels centred on
'  (128, 96), with the dashboard in the bottom 64 rows of a 256 row
'  screen.  Ours is 320 x 176 centred on (160, 88) with the dashboard in
'  rows 176..239, and the projection keeps the BBC's focal length of 256
'  pixels, so every ship is exactly the size it was in 1984 and the
'  extra width is extra peripheral vision.
'
'  Ship meshes and statistics come straight out of the original 6502
'  source: Bas/elite_tools/blueprints.py turns the SHIP_* blueprints
'  into the DATA blocks at the end of this file.
'
'  Built by elite_tools/build.py from src/*.bas - do not edit elite.bas.
' =====================================================================
OPTION EXPLICIT
OPTION BASE 0
OPTION DEFAULT NONE

' ------------------------------------------------------------ geometry
CONST SCRW = 320, SCRH = 240
CONST VIEWH = 176                  ' space view occupies rows 0..VIEWH-1
CONST VCX = 160, VCY = 88          ' space view centre
CONST DASHY = 176                  ' first dashboard row
CONST VPLANE = 256                 ' focal length in pixels, as the BBC
CONST PANY = VCY - (SCRH \ 2 - 1)  ' shifts Draw3D's centre up to VCY

' ------------------------------------------------------- universe size
CONST NSLOT = 12                   ' NOSH: planet + sun/station + 10 ships
CONST NBP = 12                     ' ship blueprints in the cassette game
CONST SLOT_PLANET = 0              ' FRIN slot 0 is always the planet
CONST SLOT_STAR = 1                ' slot 1 is the sun or the station

' -------------------------------------------------------- Elite scales
CONST PRADIUS = 24576              ' planet radius, (96 0) in the original
CONST FAROFF = 57344               ' beyond this on any axis a ship is gone
CONST SAFEZONE = 49152             ' station safe zone, (192 0)

' -------------------------------------------------------- object types
CONST T_PLANET = 128, T_SUN = 129, T_CRATER = 130
CONST T_SIDEWINDER = 1, T_VIPER = 2, T_MAMBA = 3, T_PYTHON = 4
CONST T_COBRA3 = 5, T_THARGOID = 6, T_TRADER = 7, T_STATION = 8
CONST T_MISSILE = 9, T_ASTEROID = 10, T_CANISTER = 11, T_THARGON = 12
CONST T_ESCAPE = 13

' ============================================================ globals
' Player.  pRoll and pPitch are the original's JSTX and JSTY: 1..255
' centred on 128.  alp1 / bet1 are the magnitudes the game derives from
' them, alp2 / bet2 the signs, and alpha / beta the same angles in
' radians for the rotation maths.
DIM INTEGER pRoll, pPitch, alp1, alp2, bet1, bet2, dSpeed
DIM FLOAT alpha, beta
DIM INTEGER pEnergy, pFsh, pAsh, pFuel, pCabT, pLasT, pAltit, pMissl
DIM INTEGER vw                     ' 0 front, 1 rear, 2 left, 3 right
DIM INTEGER mcnt                   ' the original's main loop counter
DIM INTEGER inSafe                 ' inside the station's safe zone

' Universe slots, as parallel arrays.  sObj is the Draw3D object number
' this slot owns, or 0 when it has none and is drawn as a dot.
DIM INTEGER sTyp(NSLOT-1), sBp(NSLOT-1), sObj(NSLOT-1)
DIM FLOAT sX(NSLOT-1), sY(NSLOT-1), sZ(NSLOT-1)
DIM FLOAT sQ(4, NSLOT-1)           ' orientation quaternion w,x,y,z,m
DIM INTEGER sSpd(NSLOT-1), sAcc(NSLOT-1), sRol(NSLOT-1), sPit(NSLOT-1)
DIM INTEGER sEne(NSLOT-1), sAI(NSLOT-1), sFlg(NSLOT-1)
DIM INTEGER nUsed                  ' slots in use, 0..NSLOT

' Ship blueprint statistics, indexed by blueprint 0..NBP-1.
DIM bName$(NBP-1) LENGTH 16
DIM INTEGER bNv(NBP-1), bNf(NBP-1), bNfv(NBP-1), bNf0(NBP-1), bNv0(NBP-1)
DIM INTEGER bCan(NBP-1), bArea(NBP-1), bBty(NBP-1), bVis(NBP-1)
DIM INTEGER bEne(NBP-1), bSpd(NBP-1), bLas(NBP-1), bMis(NBP-1)
DIM INTEGER bGun(NBP-1), bExp(NBP-1), bSize(NBP-1)
DIM INTEGER tBp(13)                ' ship type 1..13 -> blueprint index

' Scratch mesh buffers, big enough for the largest blueprint (Missile:
' 33 vertices, 25 polygons, 80 face-vertex entries).
DIM FLOAT mV(2, 39), mNrm(2, 15)
DIM INTEGER mFc(31), mHost(31), mF(159), mEc(31), mFl(31)
DIM INTEGER col(6)

' Draw3D object pool.  objOwn(n) is the slot that owns object n, or -1.
DIM INTEGER maxObj, objOwn(15)

' Quaternion scratch.  Draw3D and MATH both want a 5 element float array.
DIM FLOAT qA(4), qB(4), qC(4), qV(4), qP(4), vwQ(4, 3)

' Keyboard flags, refreshed once per frame.
DIM INTEGER kRollL, kRollR, kUp, kDn, kFaster, kSlower, kFire, kQuit
DIM INTEGER kView, kPause

' Frame timing.
DIM FLOAT frameMs, tFrame
DIM INTEGER frames

' Rendering options and the view transform's output.
DIM INTEGER solidMode, showDot
DIM FLOAT tx, ty, tz

' ============================================================ start up
SetupScreen
LoadStats
ProbeObjects
SetupViews
TestScene

frames = 0
tFrame = TIMER
DO
  ReadKeys
  IF kQuit THEN EXIT DO
  UpdatePlayer
  MoveShips
  DrawFrame
  FRAMEBUFFER COPY F, N, B
  mcnt = (mcnt + 1) AND 255
  frames = frames + 1
LOOP
frameMs = (TIMER - tFrame) / frames

CloseAll
FRAMEBUFFER CLOSE
MODE 1
PRINT "frames"; frames; "  average"; STR$(frameMs, 4, 2); " ms"
END

' ============================================================ set up
SUB SetupScreen
  MODE 2
  FRAMEBUFFER CREATE
  FRAMEBUFFER WRITE F
  ' Draw3D's own centre is (W/2, H/2-1); pany lifts it to the space
  ' view's centre so ships sit above the dashboard, not behind it.
  Draw3D CAMERA 1, VPLANE, 0, 0, 0, PANY
  col(0) = RGB(WHITE) : col(1) = RGB(GRAY) : col(2) = RGB(BLUE)
  col(3) = RGB(GREEN) : col(4) = RGB(RED) : col(5) = RGB(MAGENTA)
  col(6) = RGB(CYAN)
END SUB

' The four views are rotations of the whole universe about the vertical
' axis: front none, rear 180, left +90, right -90.  Positions are
' transformed by hand in ViewXform (three assignments beat a quaternion
' multiply); orientations are pre-multiplied by these.
SUB SetupViews
  LOCAL INTEGER i
  MATH Q_EULER 0, 0, 0, qA()      : FOR i = 0 TO 4 : vwQ(i, 0) = qA(i) : NEXT i
  MATH Q_CREATE RAD(180), 0, 1, 0, qA() : FOR i = 0 TO 4 : vwQ(i, 1) = qA(i) : NEXT i
  MATH Q_CREATE RAD(90), 0, 1, 0, qA()  : FOR i = 0 TO 4 : vwQ(i, 2) = qA(i) : NEXT i
  MATH Q_CREATE RAD(-90), 0, 1, 0, qA() : FOR i = 0 TO 4 : vwQ(i, 3) = qA(i) : NEXT i
END SUB

' How many Draw3D objects does this firmware allow?  MAX3D was 8 and is
' 12 in the current build; creating one past the limit raises an error,
' so ask rather than assume.
SUB ProbeObjects
  LOCAL INTEGER n
  LoadMesh 0
  maxObj = 8
  FOR n = 9 TO 15
    ON ERROR SKIP 1
    Draw3D CREATE n, bNv(0), bNf(0), 1, mV(), mFc(), mF(), col(), mEc()
    IF MM.ERRNO <> 0 THEN EXIT FOR
    Draw3D CLOSE n
    maxObj = n
  NEXT n
  ON ERROR CLEAR
  FOR n = 0 TO 15 : objOwn(n) = -1 : NEXT n
END SUB

SUB CloseAll
  LOCAL INTEGER n
  FOR n = 1 TO maxObj
    IF objOwn(n) >= 0 THEN Draw3D CLOSE n
    objOwn(n) = -1
  NEXT n
END SUB
