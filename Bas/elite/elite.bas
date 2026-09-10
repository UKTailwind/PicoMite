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
' The trace cache compiles each statement to bytecode on its first run and
' replays it thereafter, which removes the variable-name lookups and the
' re-parsing that dominate an interpreted frame loop.  It has to come before
' OPTION EXPLICIT and the DIMs, because those invalidate its entries.  The
' two named SUBs are the per-particle and per-contact loops, where the same
' handful of statements run hundreds of times a frame.
OPTION TRACECACHE ON 80             ' rounded up to 128 slots by the firmware
OPTION CACHE DEBUG ON
'OPTION PROFILING ON                ' [PERF] report of the hottest statements
OPTION CACHE SUB DrawStardust, DrawScanner

OPTION EXPLICIT
OPTION BASE 0
OPTION DEFAULT NONE

' ------------------------------------------------------------ geometry
CONST SCRW = 320, SCRH = 240
CONST VIEWH = 176                  ' space view occupies rows 0..VIEWH-1
CONST VCX = 160, VCY = 88          ' space view centre
CONST DASHY = 176                  ' first dashboard row
CONST VPLANE = 256                 ' focal length in pixels, as the BBC
CONST DEMOFRAMES = 260             ' >0 runs a scripted demo and exits; 0 plays
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
DIM INTEGER sEne(NSLOT-1), sAI(NSLOT-1), sFlg(NSLOT-1), sExp(NSLOT-1)
DIM INTEGER sTgt(NSLOT-1)   ' a missile's quarry: a slot, or -2 for us
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
DIM INTEGER cGreen, cYellow, cWhite, cBlack, cCyan, cGrey, cRed

' Draw3D object pool.  objOwn(n) is the slot that owns object n, or -1.
DIM INTEGER maxObj, objOwn(15)

' Quaternion scratch.  Draw3D and MATH both want a 5 element float array.
DIM FLOAT qA(4), qB(4), qC(4), qV(4), qP(4), vwQ(4, 3)

' Keyboard flags, refreshed once per frame.
DIM INTEGER kRollL, kRollR, kUp, kDn, kFaster, kSlower, kFire, kQuit
DIM INTEGER kView, kPause, kTarget, kMissile, kECM

' Frame timing.
DIM FLOAT frameMs, tFrame, tStage
DIM INTEGER frames

DIM FLOAT prof(5)                  ' cls, stardust, planet, ships, dash, move

' The galaxy.  Three 16-bit seeds and the fields the current system's
' seeds decode to.  The two-letter fragments names are built from are
' held as one string and indexed rather than as 32 separate entries.
DIM INTEGER gs0, gs1, gs2, gSys, gGal
DIM INTEGER sysX, sysY, sysGov, sysEco, sysTech, sysPop, sysProd, sysRad
' Where we are, where the chart cursor is, and which system it picked.
' All in raw galaxy coordinates: y is the unhalved value, and the charts
' halve it themselves.
DIM INTEGER homeX, homeY, homeSys, curX, curY, selSys, inWitch
CONST DIGRAPHS = "ALLEXEGEZACEBISOUSESARMAINDIREA?ERATENBERALAVETIEDORQUANTEISRION"

' The market: seventeen commodities, priced from the system's economy and
' the one random byte drawn on arrival.
' Combat.  The laser does not travel: firing tests what is lined up and
' hits it at once, so the only timing is how often it can be fired and
' how hot it has got.
CONST LASPULSE = 4                 ' frames between pulse laser shots
DIM INTEGER lasTimer, lasPower, lasFlash, kills, dead, energyUnit, shots, hits
CONST MSTURN = 0.22                ' how hard a missile swings onto a bearing
CONST ECMFRAMES = 24               ' how long one burst runs, and drains energy
DIM INTEGER msLock, ecmActive, legal

' Arrival distances are in units of the step the original's sign byte moves in.
CONST UNIT = 65536                 ' one step of the original's sign byte
CONST LAUNCHSPD = 12               ' speed immediately after launching

' Chart geometry, converted from the original x * 1.25.
CONST CHTOP = 24                   ' first chart row, under the title rule
CONST SRCX = 130                   ' short range chart centre, ours
CONST SRCY = 90
CONST SRDX = 5                     ' our pixels per galaxy unit across
CONST SRDY = 2                     ' and down

CONST NGOODS = 17
DIM mkName$(NGOODS-1) LENGTH 14, mkUnit$(NGOODS-1) LENGTH 2
DIM INTEGER mkBase(NGOODS-1), mkFact(NGOODS-1), mkQty(NGOODS-1), mkMask(NGOODS-1)
DIM INTEGER mkPrice(NGOODS-1), mkStock(NGOODS-1), mkByte
DIM INTEGER cargo(NGOODS-1), holdSize, cashTenths

' Rendering options and the view transform's output.
DIM INTEGER solidMode, showDot
DIM FLOAT tx, ty, tz

' ------------------------ constants belonging to the other modules
' MMBasic executes CONST and DIM, so every one of them has to run before
' the main flow reaches END - they cannot live beside the SUBs that use
' them further down the file.

' --- the original's control constants, all in units per frame
CONST JCENTRE = 128                ' the centre of the 1..255 range
CONST JROLLSTEP = 7                ' a held roll key moves JSTX this far
CONST JPITCHSTEP = 14              ' a held pitch key moves JSTY this far
CONST JDAMPROLL = 2                ' the spring pulls roll back this fast
CONST JDAMPPITCH = 1               ' and pitch this fast
CONST MAXSPEED = 40                ' DELTA's ceiling
CONST ANGSCALE = 256               ' ALP1 / 256 is the angle in radians

CONST SELFROT = 0.0625             ' a ship's own roll / pitch, radians per frame
CONST NPCSPEED = 1.5               ' a ship of speed s moves 1.5 * s per frame
CONST TIDYEVERY = 16               ' renormalise one ship's orientation this often

CONST NEARZ = 32                   ' nearer than this and nothing is drawn
CONST FARXY = 30000                ' Draw3D clamps its offsets at +-32766
' The original compares the blueprint's visibility byte directly against the
' high byte of z, so its unit is 256 world units.  Two hard cut-offs sit
' either side of that comparison: nothing at all is drawn beyond z_hi 192,
' and below z_hi 16 the mesh is always drawn whatever the blueprint says -
' which is why the escape pod, canister and missile, whose bytes are under
' 16, all turn into dots at exactly 4096.
CONST ZHI = 256                    ' world units per step of the visibility scale
CONST VISFLOOR = 16                ' below this the mesh is always drawn
CONST VISCUT = 192                 ' beyond this the ship is not drawn at all
CONST NSTAR = 18                   ' stardust particles, as the original
CONST NSEG = 16                    ' segments in a planet surface ellipse

DIM FLOAT ctab(NSEG-1), stab(NSEG-1), pgx(NSEG-1), pgy(NSEG-1)
DIM FLOAT stX(NSTAR-1), stY(NSTAR-1), stZ(NSTAR-1)
DIM INTEGER spx(4*NSTAR-1), spy(4*NSTAR-1), spc(4*NSTAR-1)
DIM INTEGER DLY(5), DRY(6)         ' dashboard bar rows, left and right
DIM LLAB$(5) LENGTH 3, RLAB$(6) LENGTH 3

' Dashboard columns, from the original's screen addresses.  Left column
' BBC x 16..47 -> ours 20..59; right column BBC x 208..239 -> ours
' 260..299.  A bar is 16 steps of 2.5 of our pixels.
CONST DL = 20                      ' left column, the status bars
CONST DR = 260                     ' right column, speed / roll / pitch / energy
CONST DW = 40                      ' a full length bar

' Scanner ellipse: BBC centre (124, 220), semi-axes 69 x 18.
CONST SCX = 155                    ' scanner centre
CONST SCY = 204
CONST SCA = 86                     ' scanner semi-axis across
CONST SCB = 18                     ' and down
CONST SCDOTX = 154                 ' BBC 123, the dot's x origin
CONST SCXDIV = 204.8               ' 256 world units per BBC pixel, x 1.25
CONST SCZDIV = 1024                ' a quarter of z's high byte, down the ellipse
CONST SCYDIV = 512                 ' half of y's high byte, for the stick
CONST SCTOP = 178                  ' BBC 194, the dot's clip
CONST SCBOT = 230                  ' BBC 246

' Compass: BBC centre (195, 203), a normalised component of +-96 becoming
' +-9 pixels.  Yellow and two rows deep when the target is ahead, green
' and one row deep when it is behind.
CONST CPX = 244
CONST CPY = 187
CONST CPR = 9
CONST PROFILE = 1                  ' accumulate per-stage frame times

' =====================================================================
'  Start up and the frame loop
' =====================================================================
SetupScreen
LoadStats
ProbeObjects
SetupViews
TestScene

frames = 0
tFrame = TIMER
DO
  IF DEMOFRAMES > 0 THEN
    DemoInput frames
  ELSE
    ReadKeys
  ENDIF
  IF kQuit OR dead THEN EXIT DO
  UpdatePlayer
  IF kFire THEN FireLaser
  IF kTarget THEN TargetMissile
  IF kMissile THEN LaunchMissile
  IF kECM THEN FireECM
  IF lasTimer > 0 THEN lasTimer = lasTimer - 1
  IF lasFlash > 0 THEN lasFlash = lasFlash - 1
  tStage = TIMER
  MoveShips
  Missiles
  Tactics
  ECMService
  Recharge
  StationCheck
  prof(5) = prof(5) + TIMER - tStage
  DrawFrame
  FRAMEBUFFER COPY F, N, B
  mcnt = (mcnt + 1) AND 255
  frames = frames + 1
  IF DEMOFRAMES > 0 THEN
    IF frames = 40 OR frames = 80 OR frames = 120 OR frames = 250 THEN SaveShot frames
    IF frames >= DEMOFRAMES THEN EXIT DO
  ENDIF
LOOP
frameMs = (TIMER - tFrame) / frames

IF dead THEN DeathScreen : PAUSE 1500
CloseAll
FRAMEBUFFER CLOSE
MODE 1
PRINT "frames"; frames; "  average"; STR$(frameMs, 4, 2); " ms per frame"
PRINT "shots"; shots; " hits"; hits; "  kills"; kills; "  cash"; cashTenths / 10; " Cr  rank "; RankName$()
PRINT "energy"; pEnergy; " fore shield"; pFsh; " laser temp"; pLasT; " fuel"; pFuel / 10; " LY"
PRINT "slots in use"; nUsed; "  dead"; dead; "  witchspace"; inWitch
PRINT "missiles left"; pMissl; "  legal status "; LegalName$()
IF PROFILE THEN
  PRINT "  CLS      "; STR$(prof(0) / frames, 5, 2); " ms"
  PRINT "  stardust "; STR$(prof(1) / frames, 5, 2); " ms"
  PRINT "  planet   "; STR$(prof(2) / frames, 5, 2); " ms"
  PRINT "  ships    "; STR$(prof(3) / frames, 5, 2); " ms"
  PRINT "  dash     "; STR$(prof(4) / frames, 5, 2); " ms"
  PRINT "  move     "; STR$(prof(5) / frames, 5, 2); " ms"
  PRINT "  the rest is the background framebuffer copy, which paces to 60 Hz"
ENDIF
END

' =====================================================================
'  Ship blueprints, the universe slot table, and the Draw3D object pool
' =====================================================================

' Read every blueprint once: the header statistics, and the mesh (which
' we throw away again) so we can record each ship's size.  Slow, but it
' happens once at start up and it means nothing else has to guess.
SUB LoadStats
  LOCAL INTEGER b
  FOR b = 0 TO NBP - 1
    LoadMesh b
  NEXT b
  tBp(1) = 0 : tBp(2) = 1 : tBp(3) = 2 : tBp(4) = 3 : tBp(5) = 4
  tBp(6) = 5 : tBp(7) = 4 : tBp(8) = 6 : tBp(9) = 7 : tBp(10) = 8
  tBp(11) = 9 : tBp(12) = 10 : tBp(13) = 11
END SUB

' Load blueprint b into the scratch mesh buffers and fill in its stats.
' The DATA layout is fixed by elite_tools/blueprints.py:
'   name, nv, nf, nfv, nf0, nv0, canisters, area, bounty, visdist,
'   energy, speed, laser, missiles, gun vertex, explosion count
'   then nv vertices, nf face vertex counts, nf host faces,
'   nf0 stored normals, nfv face vertex indices.
SUB LoadMesh(b AS INTEGER)
  LOCAL INTEGER j, k
  SELECT CASE b
    CASE 0  : RESTORE dat_sidewinder
    CASE 1  : RESTORE dat_viper
    CASE 2  : RESTORE dat_mamba
    CASE 3  : RESTORE dat_python
    CASE 4  : RESTORE dat_cobra_mk_3
    CASE 5  : RESTORE dat_thargoid
    CASE 6  : RESTORE dat_coriolis
    CASE 7  : RESTORE dat_missile
    CASE 8  : RESTORE dat_asteroid
    CASE 9  : RESTORE dat_canister
    CASE 10 : RESTORE dat_thargon
    CASE 11 : RESTORE dat_escape_pod
  END SELECT
  READ bName$(b), bNv(b), bNf(b), bNfv(b), bNf0(b), bNv0(b)
  READ bCan(b), bArea(b), bBty(b), bVis(b), bEne(b), bSpd(b)
  READ bLas(b), bMis(b), bGun(b), bExp(b)
  FOR j = 0 TO bNv(b) - 1 : READ mV(0, j), mV(1, j), mV(2, j) : NEXT j
  FOR j = 0 TO bNf(b) - 1 : READ mFc(j) : NEXT j
  FOR j = 0 TO bNf(b) - 1 : READ mHost(j) : NEXT j
  FOR j = 0 TO bNf0(b) - 1 : READ mNrm(0, j), mNrm(1, j), mNrm(2, j) : NEXT j
  FOR j = 0 TO bNfv(b) - 1 : READ mF(j) : NEXT j
  ' Ships are white lines, as on the BBC; the fill colours only matter
  ' when the solid renderer is switched on.
  FOR j = 0 TO bNf(b) - 1
    mEc(j) = 0
    mFl(j) = 1 + (mHost(j) MOD 6)
  NEXT j
  bSize(b) = 0
  FOR j = 0 TO bNv0(b) - 1
    FOR k = 0 TO 2
      IF ABS(mV(k, j)) > bSize(b) THEN bSize(b) = ABS(mV(k, j))
    NEXT k
  NEXT j
END SUB

' ------------------------------------------------------- the slot table
SUB ClearSlots
  LOCAL INTEGER n
  CloseAll
  FOR n = 0 TO NSLOT - 1
    sTyp(n) = 0 : sObj(n) = 0
  NEXT n
  nUsed = 0
END SUB

' Create a ship of type t at x, y, z with orientation q (5 elements) and
' return its slot, or -1 if the bubble is full.  Slot 0 is reserved for
' the planet and slot 1 for the sun or the station, exactly as FRIN.
FUNCTION NewShip(t AS INTEGER, x AS FLOAT, y AS FLOAT, z AS FLOAT, q() AS FLOAT) AS INTEGER
  LOCAL INTEGER n, i, first
  first = 2
  IF t = T_PLANET OR t = T_CRATER THEN first = 0
  IF t = T_SUN OR t = T_STATION THEN first = 1
  n = -1
  IF first < 2 THEN
    IF sTyp(first) = 0 THEN n = first
  ELSE
    FOR i = 2 TO NSLOT - 1
      IF sTyp(i) = 0 THEN n = i : EXIT FOR
    NEXT i
  ENDIF
  NewShip = n
  IF n < 0 THEN EXIT FUNCTION

  sTyp(n) = t
  sX(n) = x : sY(n) = y : sZ(n) = z
  MATH INSERT sQ(), , n, q()
  sQ(4, n) = 1                      ' Draw3D scales by this squared
  sObj(n) = 0
  sSpd(n) = 0 : sAcc(n) = 0 : sRol(n) = 0 : sPit(n) = 0
  sFlg(n) = 0 : sAI(n) = 0
  IF t < T_PLANET THEN
    sBp(n) = tBp(t)
    sEne(n) = bEne(sBp(n))
    GetObject n
  ELSE
    sBp(n) = -1                     ' planet and sun are drawn by hand
    sEne(n) = 0
  ENDIF
  IF n >= nUsed THEN nUsed = n + 1
END FUNCTION

' Remove a slot.  The original shuffles the table down to close the gap
' so the loop over ships never sees a hole; we do the same, because the
' AI and the scanner both walk the table in order.
SUB KillShip(n AS INTEGER)
  LOCAL INTEGER i
  DropObject n
  IF n < 2 THEN
    ' The planet and the sun / station keep their reserved slots.
    sTyp(n) = 0 : sObj(n) = 0
    EXIT SUB
  ENDIF
  FOR i = n TO nUsed - 2
    CopySlot i, i + 1
  NEXT i
  sTyp(nUsed - 1) = 0
  sObj(nUsed - 1) = 0
  nUsed = nUsed - 1
END SUB

SUB CopySlot(d AS INTEGER, s AS INTEGER)
  LOCAL INTEGER i
  sTyp(d) = sTyp(s) : sBp(d) = sBp(s) : sObj(d) = sObj(s)
  sX(d) = sX(s) : sY(d) = sY(s) : sZ(d) = sZ(s)
  MATH SLICE sQ(), , s, qA()
  MATH INSERT sQ(), , d, qA()
  sSpd(d) = sSpd(s) : sAcc(d) = sAcc(s)
  sRol(d) = sRol(s) : sPit(d) = sPit(s)
  sEne(d) = sEne(s) : sAI(d) = sAI(s) : sFlg(d) = sFlg(s)
  IF sObj(d) > 0 THEN objOwn(sObj(d)) = d
  sTyp(s) = 0 : sObj(s) = 0
END SUB

' ------------------------------------------------- Draw3D object pool
' A slot only needs an object while its ship is close enough to be drawn
' as a mesh; there are fewer objects than slots, so they are handed out
' on a first come basis and the rest of the bubble shows up as dots.
SUB GetObject(n AS INTEGER)
  LOCAL INTEGER o, b
  IF sObj(n) > 0 THEN EXIT SUB
  FOR o = 1 TO maxObj
    IF objOwn(o) < 0 THEN
      b = sBp(n)
      LoadMesh b
      IF solidMode THEN
        Draw3D CREATE o, bNv(b), bNf(b), 1, mV(), mFc(), mF(), col(), mEc(), mFl()
      ELSE
        Draw3D CREATE o, bNv(b), bNf(b), 1, mV(), mFc(), mF(), col(), mEc()
      ENDIF
      objOwn(o) = n
      sObj(n) = o
      EXIT SUB
    ENDIF
  NEXT o
END SUB

SUB DropObject(n AS INTEGER)
  IF sObj(n) <= 0 THEN EXIT SUB
  Draw3D CLOSE sObj(n)
  objOwn(sObj(n)) = -1
  sObj(n) = 0
END SUB

' ----------------------------------------------------- view transform
' Front leaves the universe alone; rear turns it through 180 degrees
' about the vertical axis; left and right through plus and minus 90.
' Written out rather than done with a quaternion because it runs for
' every slot every frame.
SUB ViewXform(n AS INTEGER)
  SELECT CASE vw
    CASE 0 : tx = sX(n)  : ty = sY(n) : tz = sZ(n)
    CASE 1 : tx = -sX(n) : ty = sY(n) : tz = -sZ(n)
    CASE 2 : tx = sZ(n)  : ty = sY(n) : tz = -sX(n)
    CASE 3 : tx = -sZ(n) : ty = sY(n) : tz = sX(n)
  END SELECT
END SUB

' The ship's orientation seen from the current view.  Leaves the result
' in qC() ready for Draw3D ROTATE.
' MATH SLICE lifts one ship's quaternion out of the 5 x NSLOT table in a
' single call, and a whole-array assignment copies one in a single memcpy;
' both replace five interpreted statements, which is what this costs when
' it runs for every ship every frame.
SUB ViewOrient(n AS INTEGER)
  MATH SLICE sQ(), , n, qA()
  IF vw = 0 THEN
    qC() = qA()
  ELSE
    MATH SLICE vwQ(), , vw, qB()
    MATH Q_MULT qB(), qA(), qC()
  ENDIF
  qC(4) = 1
END SUB

' =====================================================================
'  Screen, camera, view and object-pool set up
' =====================================================================
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
  ' Pre-resolved so the drawing loops assign a variable rather than call
  ' RGB(), which the trace cache cannot compile.
  cGreen = RGB(GREEN) : cYellow = RGB(YELLOW) : cWhite = RGB(WHITE)
  cBlack = RGB(BLACK) : cCyan = RGB(CYAN) : cGrey = RGB(64, 64, 64)
  cRed = RGB(RED)
  ' One turn of the unit circle, for the planet's surface ellipses.
  LOCAL INTEGER k
  FOR k = 0 TO NSEG - 1
    ctab(k) = COS(2 * PI * k / NSEG)
    stab(k) = SIN(2 * PI * k / NSEG)
  NEXT k
  ' Bar rows, converted from the original's character rows.  Left column:
  ' forward shield, aft shield, fuel, cabin temperature, laser temperature,
  ' altitude.  Right column: speed, roll, dive/climb, four energy banks.
  DLY(0) = 178 : DLY(1) = 186 : DLY(2) = 194
  DLY(3) = 202 : DLY(4) = 210 : DLY(5) = 218
  DRY(0) = 178 : DRY(1) = 185 : DRY(2) = 193 : DRY(3) = 202
  DRY(4) = 210 : DRY(5) = 218 : DRY(6) = 226
  LLAB$(0) = "FS" : LLAB$(1) = "AS" : LLAB$(2) = "FU"
  LLAB$(3) = "CT" : LLAB$(4) = "LT" : LLAB$(5) = "AL"
  RLAB$(0) = "SP" : RLAB$(1) = "RL" : RLAB$(2) = "DC"
  RLAB$(3) = "1" : RLAB$(4) = "2" : RLAB$(5) = "3" : RLAB$(6) = "4"
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

' =====================================================================
'  Reading the keyboard, and turning it into the player's rotation
'
'  The original keeps two "joystick" values, one for roll and one for
'  pitch, that a held key pushes away from centre and a spring pulls
'  back, and then derives the frame's rotation angles from how far from
'  centre they have got.  That spring is why Elite's controls feel the
'  way they do, so it is reproduced exactly rather than replaced with a
'  direct key-to-angle mapping.
' =====================================================================

SUB ReadKeys
  LOCAL INTEGER i, k
  LOCAL kb$ LENGTH 2
  kRollL = 0 : kRollR = 0 : kUp = 0 : kDn = 0
  kFaster = 0 : kSlower = 0 : kFire = 0 : kQuit = 0
  kTarget = 0 : kMissile = 0 : kECM = 0
  kView = -1 : kPause = 0
  ' INKEY$ first: every KEYDOWN call empties the console input buffer.
  kb$ = INKEY$
  IF kb$ = CHR$(27) THEN kQuit = 1
  IF kb$ = "p" OR kb$ = "P" THEN kPause = 1
  FOR i = 1 TO 6
    k = KEYDOWN(i)
    SELECT CASE k
      CASE 130, 44, 60   : kRollL = 1        ' left arrow, comma, less than
      CASE 131, 46, 62   : kRollR = 1        ' right arrow, full stop, greater than
      CASE 128, 88, 120  : kUp = 1           ' up arrow, X
      CASE 129, 83, 115  : kDn = 1           ' down arrow, S
      CASE 32            : kFaster = 1       ' space
      CASE 47, 63        : kSlower = 1       ' slash, question mark
      CASE 65, 97        : kFire = 1         ' A
      CASE 27            : kQuit = 1
      CASE 145 TO 148    : kView = k - 145   ' F1..F4 select the four views
    END SELECT
  NEXT i
  IF kView >= 0 THEN vw = kView
END SUB

' One frame of control input.  Order matters: the keys move the value,
' then the spring pulls it back, then the angle is derived from where it
' ended up.
SUB UpdatePlayer
  LOCAL INTEGER d
  ' --- roll and pitch.  A held key pushes the rate away from centre, but
  '     it cannot cross the centre in one press: if the step would take it
  '     to the far side, it stops dead at centre instead.  That is the
  '     original's auto-recentre, and it is what makes a reversal feel
  '     crisp rather than sluggish.
  IF kRollL THEN
    pRoll = Recentre(pRoll, -JROLLSTEP)
  ELSEIF kRollR THEN
    pRoll = Recentre(pRoll, JROLLSTEP)
  ELSE
    pRoll = Spring(pRoll, JDAMPROLL)
  ENDIF
  IF kUp THEN
    pPitch = Recentre(pPitch, -JPITCHSTEP)
  ELSEIF kDn THEN
    pPitch = Recentre(pPitch, JPITCHSTEP)
  ELSE
    pPitch = Spring(pPitch, JDAMPPITCH)
  ENDIF

  ' --- speed.  The ceiling is tested before the increase but not before
  '     the decrease, so holding both keys at full speed settles at one
  '     below it rather than doing nothing.
  IF kFaster THEN
    IF dSpeed < MAXSPEED THEN dSpeed = dSpeed + 1
  ENDIF
  IF kSlower THEN dSpeed = dSpeed - 1
  IF dSpeed < 1 THEN dSpeed = 1

  ' --- the rotation angles.  Both curves are deliberately non-linear:
  '     small deflections are halved again, which gives fine control near
  '     centre and a hard bank at the extremes.  Roll reaches 31/256 of a
  '     radian a frame, pitch only 8/256, so a ship rolls nearly four
  '     times as fast as it pitches - which is why Elite is flown by
  '     rolling onto a target and then pulling.
  d = ABS(pRoll - JCENTRE)
  alp1 = d \ 4
  IF alp1 < 8 THEN alp1 = alp1 \ 2
  alp2 = SGN(pRoll - JCENTRE)

  d = ABS(pPitch - JCENTRE) + 4
  bet1 = d \ 16
  IF bet1 < 3 THEN bet1 = bet1 \ 2
  bet2 = SGN(pPitch - JCENTRE)

  alpha = alp2 * alp1 / ANGSCALE
  beta = bet2 * bet1 / ANGSCALE
END SUB

' Move a rate by one key step, stopping at the centre rather than through
' it.  Returns the new value, clamped to the 1..255 range it lives in.
FUNCTION Recentre(n AS INTEGER, dv AS INTEGER) AS INTEGER
  LOCAL INTEGER v
  v = n + dv
  IF SGN(n - JCENTRE) <> 0 THEN
    IF SGN(v - JCENTRE) <> SGN(n - JCENTRE) THEN v = JCENTRE
  ENDIF
  IF v < 1 THEN v = 1
  IF v > 255 THEN v = 255
  Recentre = v
END FUNCTION

' The spring: move n one step per call towards the centre, without
' overshooting it.
FUNCTION Spring(n AS INTEGER, nstep AS INTEGER) AS INTEGER
  LOCAL INTEGER v, i
  v = n
  FOR i = 1 TO nstep
    IF v > JCENTRE THEN
      v = v - 1
    ELSEIF v < JCENTRE THEN
      v = v + 1
    ENDIF
  NEXT i
  Spring = v
END FUNCTION

' =====================================================================
'  Moving the universe
'
'  Elite never moves the player.  The player sits at the origin looking
'  down +Z, and every frame the whole universe is rotated by the
'  opposite of the player's roll and pitch and shifted back by the
'  player's speed.  Each ship then moves along its own nose and applies
'  its own roll and pitch counters.  This SUB is the whole of that.
' =====================================================================

SUB MoveShips
  LOCAL INTEGER n, i, mag, dir, gone
  LOCAL FLOAT k

  ' The player's rotation, as a quaternion, built once for the frame.
  ' To first order the original's position transform is a rotation of
  ' -alpha about the nose axis followed by +beta about the side axis.
  MATH Q_CREATE -alpha, 0, 0, 1, qA()
  MATH Q_CREATE beta, 1, 0, 0, qB()
  MATH Q_MULT qB(), qA(), qP()

  ' A FOR loop fixes its limit when it starts, and killing a ship shortens
  ' the table, so walk it by hand.
  n = 0
  DO WHILE n < nUsed
    IF sTyp(n) = 0 THEN
      n = n + 1
    ELSE
      ' --- 1. the ship moves along its own nose (not the planet or sun)
      IF sBp(n) >= 0 AND sSpd(n) <> 0 THEN
        NoseVec n
        sX(n) = sX(n) + qV(1) * qV(4) * sSpd(n) * NPCSPEED
        sY(n) = sY(n) + qV(2) * qV(4) * sSpd(n) * NPCSPEED
        sZ(n) = sZ(n) + qV(3) * qV(4) * sSpd(n) * NPCSPEED
      ENDIF

      ' --- 2. acceleration is applied once and then forgotten.  The
      '     original tests bit 7 of the raw eight bit sum, so anything
      '     that lands in 128..255 is zeroed - an overshoot at the top
      '     stops the ship dead rather than pinning it at maximum.
      IF sAcc(n) <> 0 THEN
        mag = sSpd(n) + sAcc(n)
        IF mag < 0 OR mag > 127 THEN mag = 0
        IF mag > bSpd(sBp(n)) THEN mag = bSpd(sBp(n))
        sSpd(n) = mag
        sAcc(n) = 0
      ENDIF

      ' --- 3. the player's roll and pitch, applied to the ship's
      '     position.  Written exactly as the original does it: each
      '     line uses the value the line before it just produced, which
      '     is what makes this a rotation rather than a shear.
      k = sY(n) - alpha * sX(n)
      sZ(n) = sZ(n) + beta * k
      sY(n) = k - beta * sZ(n)
      sX(n) = sX(n) + alpha * sY(n)

      ' --- 4. and the player's speed
      sZ(n) = sZ(n) - dSpeed

      ' --- 5. the same rotation applied to the ship's orientation
      IF sBp(n) >= 0 THEN
        MATH SLICE sQ(), , n, qA()
        MATH Q_MULT qP(), qA(), qC()

        ' --- 6. the ship's own roll and pitch counters.  Bits 0 to 6
        '     are the magnitude and bit 7 the direction; a magnitude of
        '     127 means "keep turning forever", which is how the space
        '     station spins.
        mag = sPit(n) AND 127
        IF mag <> 0 THEN
          dir = 1
          IF (sPit(n) AND 128) <> 0 THEN dir = -1
          MATH Q_CREATE dir * SELFROT, 1, 0, 0, qB()
          qA() = qC()
          MATH Q_MULT qA(), qB(), qC()
          IF mag <> 127 THEN sPit(n) = (mag - 1) OR (sPit(n) AND 128)
        ENDIF
        mag = sRol(n) AND 127
        IF mag <> 0 THEN
          dir = 1
          IF (sRol(n) AND 128) <> 0 THEN dir = -1
          MATH Q_CREATE dir * SELFROT, 0, 0, 1, qB()
          qA() = qC()
          MATH Q_MULT qA(), qB(), qC()
          IF mag <> 127 THEN sRol(n) = (mag - 1) OR (sRol(n) AND 128)
        ENDIF

        MATH INSERT sQ(), , n, qC()
        sQ(4, n) = 1
        ' The original tidies one ship's orientation vectors every 16
        ' frames to stop rounding error accumulating; a quaternion needs
        ' the same treatment for the same reason.
        IF ((mcnt XOR n) AND (TIDYEVERY - 1)) = 0 THEN NormQuat n
      ENDIF

      ' --- 7. anything that has drifted out of the bubble is gone.  The
      '     table closes up behind it, so the next ship is now at this
      '     index and n must not advance.
      gone = 0
      IF sBp(n) >= 0 THEN
        IF ABS(sX(n)) > FAROFF OR ABS(sY(n)) > FAROFF OR ABS(sZ(n)) > FAROFF THEN
          KillShip n
          gone = 1
        ENDIF
      ENDIF
      IF gone = 0 THEN n = n + 1
    ENDIF
  LOOP
END SUB

' The ship's nose direction in world coordinates, left in qV().
' A ship's own +Z axis is its nose.
SUB NoseVec(n AS INTEGER)
  LOCAL INTEGER i
  MATH SLICE sQ(), , n, qA()
  MATH Q_VECTOR 0, 0, 1, qB()
  MATH Q_ROTATE qA(), qB(), qV()
END SUB

SUB NormQuat(n AS INTEGER)
  LOCAL FLOAT m
  m = SQR(sQ(0,n)*sQ(0,n) + sQ(1,n)*sQ(1,n) + sQ(2,n)*sQ(2,n) + sQ(3,n)*sQ(3,n))
  IF m > 0.000001 THEN
    sQ(0,n) = sQ(0,n)/m : sQ(1,n) = sQ(1,n)/m
    sQ(2,n) = sQ(2,n)/m : sQ(3,n) = sQ(3,n)/m
  ELSE
    sQ(0,n) = 1 : sQ(1,n) = 0 : sQ(2,n) = 0 : sQ(3,n) = 0
  ENDIF
  sQ(4,n) = 1
END SUB

' =====================================================================
'  Drawing the space view
'
'  Everything is redrawn into the off-screen buffer every frame, so none
'  of the original's XOR-erase and line-heap machinery is needed - only
'  the shapes it produced.
' =====================================================================

SUB DrawFrame
  LOCAL FLOAT t
  IF PROFILE THEN
    t = TIMER : CLS           : prof(0) = prof(0) + TIMER - t
    t = TIMER : DrawStardust  : prof(1) = prof(1) + TIMER - t
    t = TIMER : DrawPlanetSun : prof(2) = prof(2) + TIMER - t
    t = TIMER : DrawShips : Explosions : SpaceFurniture : prof(3) = prof(3) + TIMER - t
    t = TIMER : DrawDash      : prof(4) = prof(4) + TIMER - t
    ViewName
  ELSE
    CLS
    DrawStardust
    DrawPlanetSun
    DrawShips
    Explosions
    SpaceFurniture
    DrawDash
    ViewName
  ENDIF
END SUB


' Whether a ship is drawn at all, and as a mesh or a dot, is decided the
' way the original decides it - on the high byte of z alone, not on range.
' Outside a 45 degree cone nothing is drawn even though our wider screen
' could show it; beyond z_hi 192 nothing is drawn either.  Between those,
' the blueprint's visibility byte picks mesh or dot, except below z_hi 16
' where the mesh always wins.
SUB DrawShips
  LOCAL INTEGER n, px, py, zb
  FOR n = 0 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 THEN
      ViewXform n
      IF tz > NEARZ THEN
        zb = tz \ ZHI
        IF zb < VISCUT AND ABS(tx) < tz AND ABS(ty) < tz THEN
          IF zb >= VISFLOOR AND zb > bVis(sBp(n)) THEN
            ' Too far for a mesh.  The original's distant ship is not a
            ' single pixel but a short dash two rows deep, sitting one
            ' pixel right of the projected point.
            px = VCX + SGN(tx) * ((VPLANE * ABS(tx)) \ tz)
            py = VCY - SGN(ty) * ((VPLANE * ABS(ty)) \ tz)
            IF py > 0 AND py < VIEWH - 2 THEN BOX px + 1, py, 3, 2, 0, cWhite, cWhite
          ELSE
            IF sObj(n) > 0 AND ABS(tx) < FARXY AND ABS(ty) < FARXY THEN
              ViewOrient n
              Draw3D ROTATE qC(), sObj(n)
              Draw3D WRITE sObj(n), tx, ty, tz, 0, solidMode
            ENDIF
          ENDIF
        ENDIF
      ENDIF
    ENDIF
  NEXT n
END SUB

' The original frames the space view with a two pixel border, and puts
' crosshairs at the centre of any view that has a laser fitted.
SUB SpaceFurniture
  ' The laser is drawn as two lines converging on the crosshairs from the
  ' bottom corners of the view, for the couple of frames after a shot.
  IF lasFlash > 0 THEN
    LINE 40, VIEWH - 2, VCX - 4 + RND * 8, VCY, 1, cWhite
    LINE SCRW - 40, VIEWH - 2, VCX - 4 + RND * 8, VCY, 1, cWhite
  ENDIF
  LINE 0, 0, SCRW - 2, 0, 1, cWhite
  BOX 0, 0, 2, VIEWH, 0, cWhite, cWhite
  BOX SCRW - 2, 0, 2, VIEWH, 0, cWhite, cWhite
  IF vw = 0 THEN
    LINE VCX - 25, VCY, VCX - 12, VCY, 1, cWhite
    LINE VCX + 12, VCY, VCX + 25, VCY, 1, cWhite
    LINE VCX, VCY - 20, VCX, VCY - 10, 1, cWhite
    LINE VCX, VCY + 10, VCX, VCY + 20, 1, cWhite
  ENDIF
END SUB

' The planet and the sun are far too big to go through the 3D engine, so
' they are drawn directly.  On-screen radius is 256 * 24576 / z, and the
' original clamps it: a radius that reaches 256 is forced to 248, so a
' planet you are nearly touching stops growing rather than filling the
' screen.  Nothing is drawn closer than z = 256 or further than the
' radius-2-pixels distance.
'
' The cassette game has two kinds of planet and picks between them on a
' bit of the system's tech level: one with an equator and meridians, one
' with a crater.  The crater is an ellipse of half the planet's radius,
' its centre pushed 0.87 of the radius along the up vector, drawn only
' while the up vector points towards us.
'
' Deviation: the original draws the outline as an 8, 16 or 32 sided
' polygon depending on radius, which is visibly faceted.  Ours is a true
' circle, because one CIRCLE call costs a fraction of thirty-two
' interpreted line segments.
SUB DrawPlanetSun
  LOCAL INTEGER n, cx, cy, r
  FOR n = 0 TO 1
    IF sTyp(n) = T_PLANET OR sTyp(n) = T_CRATER OR sTyp(n) = T_SUN THEN
      ViewXform n
      IF tz > 255 AND tz < 3145728 THEN
        cx = VCX + SGN(tx) * ((VPLANE * ABS(tx)) \ tz)
        cy = VCY - SGN(ty) * ((VPLANE * ABS(ty)) \ tz)
        r = 6291456 / tz
        IF r >= 256 THEN r = 248
        IF cx + r > 0 AND cx - r < SCRW AND cy + r > 0 AND cy - r < VIEWH THEN
          IF sTyp(n) = T_SUN THEN
            CIRCLE cx, cy, r, 1, 1, cWhite, cWhite
          ELSE
            CIRCLE cx, cy, r, 1, 1, cWhite, -1
            IF r >= 6 THEN Surface n, cx, cy, r
          ENDIF
        ENDIF
      ENDIF
    ENDIF
  NEXT n
END SUB

' The planet's surface detail, in the plane of the screen.  Both the
' crater and the meridians are built from the planet's own orientation
' vectors projected flat, so the pattern turns with the planet.
SUB Surface(n AS INTEGER, cx AS INTEGER, cy AS INTEGER, r AS INTEGER)
  LOCAL INTEGER k
  LOCAL FLOAT vnx, vny, vnz, vrx, vry, vrz, vsx, vsy, vsz, ox, oy, ax, ay, bx, by
  MATH SLICE sQ(), , n, qA()
  MATH Q_VECTOR 0, 0, 1, qB() : MATH Q_ROTATE qA(), qB(), qV()
  vnx = qV(1) : vny = qV(2) : vnz = qV(3)          ' nose
  MATH Q_VECTOR 0, 1, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
  vrx = qV(1) : vry = qV(2) : vrz = qV(3)          ' up
  MATH Q_VECTOR 1, 0, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
  vsx = qV(1) : vsy = qV(2) : vsz = qV(3)          ' side

  IF sTyp(n) = T_CRATER THEN
    ' The crater sits on the up pole, in the plane the nose and side
    ' vectors span, and is only drawn while that pole faces us.
    IF vrz > 0 THEN EXIT SUB
    ox = cx + 0.867 * r * vrx
    oy = cy - 0.867 * r * vry
    ax = 0.5 * r * vnx : ay = 0.5 * r * vny
    bx = 0.5 * r * vsx : by = 0.5 * r * vsy
    FOR k = 0 TO NSEG - 1
      pgx(k) = ox + ax * ctab(k) + bx * stab(k)
      pgy(k) = oy - (ay * ctab(k) + by * stab(k))
    NEXT k
    POLYGON NSEG, pgx(), pgy(), cWhite
  ELSE
    ' An equator and one meridian.  Only the half of each great circle
    ' that faces us is drawn - a closed ellipse would show the far side
    ' too and the planet would read as a ball of wire.
    HalfCircle cx, cy, r, vnx, vny, vnz, vrx, vry, vrz
    HalfCircle cx, cy, r, vsx, vsy, vsz, vrx, vry, vrz
  ENDIF
END SUB

' One great circle of the planet, drawn only where it faces the viewer.
' The pair of vectors are conjugate radii: the curve is a*cos + b*sin, and
' a point is on the near side when its own z is towards us.
SUB HalfCircle(cx AS INTEGER, cy AS INTEGER, r AS INTEGER, ax AS FLOAT, ay AS FLOAT, az AS FLOAT, bx AS FLOAT, by AS FLOAT, bz AS FLOAT)
  LOCAL INTEGER k, px, py, lx, ly, have
  LOCAL FLOAT c, sn, pz
  have = 0
  FOR k = 0 TO NSEG
    c = ctab(k AND (NSEG - 1))
    sn = stab(k AND (NSEG - 1))
    pz = az * c + bz * sn
    IF pz <= 0 THEN
      px = cx + r * (ax * c + bx * sn)
      py = cy - r * (ay * c + by * sn)
      IF have THEN LINE lx, ly, px, py, 1, cWhite
      lx = px : ly = py : have = 1
    ELSE
      have = 0
    ENDIF
  NEXT k
END SUB

' Stardust.  The particles do not live in the world: x and y are pixel
' offsets from the centre of the view and z is a depth in the same units
' the visibility scale uses, so the whole field costs no projection at
' all.  Each view moves them differently - outwards from the centre in
' front, inwards behind, and sideways in the two side views - and each
' has its own rule for where a particle that leaves the screen comes
' back.  A particle is one pixel far away, two abreast closer in, and a
' two by two block when it is nearly past, which is the depth cue that
' makes the field read as speed.
'
' alp1 and bet1 are used here as the small signed integers the original
' works in, not as the radian angles the ship rotation uses.
SUB InitStardust
  LOCAL INTEGER i
  FOR i = 0 TO NSTAR - 1
    stX(i) = SdSM(RND * 256)
    stY(i) = SdSM(RND * 256)
    stZ(i) = 1 + RND * 254
  NEXT i
  FOR i = 0 TO 4 * NSTAR - 1 : spc(i) = cWhite : NEXT i
END SUB

' A random byte read as a sign and a magnitude, giving -127..127.
FUNCTION SdSM(b AS FLOAT) AS FLOAT
  LOCAL INTEGER v
  v = b
  IF (v AND 128) <> 0 THEN SdSM = -(v AND 127) ELSE SdSM = (v AND 127)
END FUNCTION

SUB DrawStardust
  LOCAL INTEGER i, zh, np, sx, sy, sy2, r
  LOCAL FLOAT q, x, y, z, a, b, h, qb, d, dsg, ratsg
  a = alp2 * alp1
  b = bet2 * bet1
  np = 0
  ARRAY SET -1, spx()
  IF vw > 1 THEN
    ' the right view runs the same maths with the angles negated
    IF vw = 3 THEN
      a = -a : b = -b : dsg = 1 : ratsg = -1
    ELSE
      dsg = -1 : ratsg = 1
    ENDIF
  ENDIF
  FOR i = 0 TO NSTAR - 1
    x = stX(i) : y = stY(i) : z = stZ(i)
    IF vw = 0 THEN
      ' --- front: everything streams out from the centre
      zh = z
      q = (INT(64 * dSpeed / zh)) OR 1
      z = z - dSpeed / 4
      y = y + FIX(y) * q / 256
      x = x + FIX(x) * q / 256
      y = y - a * FIX(x) / 256
      x = x + a * FIX(y) / 256
      qb = INT(ABS(b) * INT(ABS(y)) / 256)
      x = x + 2 * qb * qb / 256
      y = y - b
      IF ABS(x) >= 120 OR ABS(y) >= 120 OR z < 16 THEN
        y = SdSM((RND * 256) OR 4)
        x = SdSM((RND * 256) OR 8)
        z = (INT(RND * 256)) OR 144
      ENDIF
    ELSEIF vw = 1 THEN
      ' --- rear: everything streams in towards the centre
      zh = z
      q = (INT(64 * dSpeed / zh)) OR 1
      x = x - FIX(x) * q / 256
      y = y - FIX(y) * q / 256
      z = z + dSpeed / 4
      y = y + a * FIX(x) / 256
      x = x - a * FIX(y) / 256
      h = FIX(y)
      qb = -SGN(b) * SGN(h) * INT(ABS(b) * ABS(h) / 256)
      x = x + 2 * qb * (-FIX(x)) / 256
      y = y + b
      ' there is no test on x at all in the rear view
      IF ABS(y) >= 110 OR z >= 160 THEN
        r = (INT(RND * 256) AND 127) + 10 + INT(RND * 2)
        z = r
        IF (r AND 1) = 0 THEN
          IF (r AND 2) = 0 THEN x = 126 ELSE x = -126
          y = SdSM(RND * 256)
        ELSE
          r = INT(RND * 256)
          x = SdSM(r)
          IF (r AND 1) = 0 THEN y = 115 ELSE y = -115
        ENDIF
      ENDIF
    ELSE
      ' --- side views: depth never changes, the field just slides across
      zh = z
      d = INT(zh / 8)
      IF d < 1 THEN d = 1
      x = x + dsg * dSpeed / d
      x = x + b * FIX(y) / 256
      y = y - b * FIX(x) / 256
      h = FIX(y)
      qb = SGN(a) * SGN(h) * INT(ABS(a) * ABS(h) / 256)
      x = x - qb * FIX(x) / 256
      y = y + qb * h / 256 + a
      IF ABS(x) >= 116 THEN
        y = SdSM(RND * 256)
        x = 115 * ratsg
        z = (INT(RND * 256)) OR 8
      ELSEIF ABS(y) >= 116 THEN
        x = SdSM(RND * 256)
        IF a > 0 THEN y = -110 ELSE y = 110
        z = (INT(RND * 256)) OR 8
      ENDIF
    ENDIF
    stX(i) = x : stY(i) = y : stZ(i) = z
    ' plot: size grows as the particle gets close
    IF ABS(y) < VCY THEN
      zh = z
      sx = VCX + FIX(x)
      sy = VCY - FIX(y)
      spx(np) = sx : spy(np) = sy : np = np + 1
      IF zh < 144 THEN
        spx(np) = sx + 1 : spy(np) = sy : np = np + 1
        IF zh < 80 THEN
          IF (sy AND 7) = 0 THEN sy2 = sy + 1 ELSE sy2 = sy - 1
          spx(np) = sx : spy(np) = sy2 : np = np + 1
          spx(np) = sx + 1 : spy(np) = sy2 : np = np + 1
        ENDIF
      ENDIF
    ENDIF
  NEXT i
  ' One call draws the whole field, and clips it for us.
  PIXEL spx(), spy(), spc()
END SUB

' =====================================================================
'  The dashboard, the 3D scanner and the compass
'
'  Geometry taken from the original's screen addresses, converted with
'  x_ours = ROUND(x_bbc * 1.25) and y_ours = y_bbc - 16.  The BBC's
'  dashboard is character rows 24 to 30, i.e. BBC y 192..247, landing on
'  our rows 176..231; row 31 exists in memory but the video chip never
'  displays it, so our last eight rows have no counterpart and are free.
'
'  Note which column is which: speed, roll, dive/climb and the four
'  energy banks are on the RIGHT, and the six status bars on the LEFT.
'
'  Colour follows the original's threshold rule, which runs in opposite
'  directions for different indicators.  A high reading is dangerous for
'  speed and the two temperatures, so those turn red at the top of their
'  range; a low reading is dangerous for energy, shields and fuel, so
'  those turn red at the bottom.  Altitude never changes colour.
'
'  The whole screen is cleared each frame, because the planet's disc and
'  the outermost stardust reach below the space view and the drawing
'  primitives clip to the screen rather than to a region.  The fixed
'  artwork is therefore laid down again every frame, but by restoring a
'  photograph of it rather than by redrawing it.
' =====================================================================

' Drawn once, then read straight out of the framebuffer into an array.
' Every later call puts it back with one word-aligned memory copy of the
' 64 rows, a fraction of the cost of redrawing the labels and frames.
' fadd caches the write buffer's address, so this holds only while the
' framebuffer stays the write target - which it does for the whole game.
SUB DashStatic
  STATIC INTEGER wordcount = (SCRH - DASHY) * SCRW \ 16
  STATIC INTEGER store(wordcount - 1)
  STATIC INTEGER addr = 0, fadd = 0
  IF fadd = 0 THEN
    LOCAL INTEGER i
    addr = PEEK(VARADDR store())
    fadd = MM.INFO(WRITEBUFF) + DASHY * SCRW / 2
    BOX 0, DASHY, SCRW, SCRH - DASHY, 0, cBlack, cBlack
    LINE 0, DASHY, SCRW - 1, DASHY, 1, cCyan
    ' The labels go in the character blocks the original never writes to:
    ' ours x 0..19 left of the left column, x 300..319 right of the right.
    FOR i = 0 TO 5
      TEXT 17, DLY(i) - 1, LLAB$(i), "RT", 7, 1, cWhite
      BOX DL, DLY(i) - 1, DW + 2, 5, 1, cGrey, -1
    NEXT i
    FOR i = 0 TO 6
      TEXT 303, DRY(i) - 1, RLAB$(i), "LT", 7, 1, cWhite
      BOX DR, DRY(i) - 1, DW + 2, 5, 1, cGrey, -1
    NEXT i
    CIRCLE CPX, CPY, CPR + 2, 1, 1.25, cGrey, -1
    MEMORY COPY INTEGER fadd, addr, wordcount
  ELSE
    MEMORY COPY INTEGER addr, fadd, wordcount
  ENDIF
END SUB

SUB DrawDash
  LOCAL INTEGER i, e
  DashStatic
  ' --- right column: what the ship is doing
  Bar DR, DRY(0), dSpeed \ 2, 14, cRed, cYellow             ' speed
  Pointer DR, DRY(1), 8 + alp2 * (alp1 \ 4)                 ' roll
  Pointer DR, DRY(2), 8 + bet2 * bet1                       ' dive / climb
  FOR i = 0 TO 3
    e = (pEnergy \ 4) - (3 - i) * 16
    IF e < 0 THEN e = 0
    IF e > 16 THEN e = 16
    Bar DR, DRY(3 + i), e, 3, cYellow, cRed                 ' the four banks
  NEXT i
  ' --- left column: what the ship has left
  Bar DL, DLY(0), pFsh \ 16, 3, cYellow, cRed
  Bar DL, DLY(1), pAsh \ 16, 3, cYellow, cRed
  Bar DL, DLY(2), pFuel \ 4, 3, cYellow, cRed
  Bar DL, DLY(3), pCabT \ 16, 11, cRed, cYellow
  Bar DL, DLY(4), pLasT \ 16, 11, cRed, cYellow
  Bar DL, DLY(5), pAltit \ 16, 99, cRed, cYellow            ' 99 is unreachable
  MissileBlocks
  DrawScanner
  DrawCompass
END SUB

' A bar of length lv on the original's 0..16 scale.  It takes colour hi at
' or above the threshold and lo below it, which is how one routine serves
' both the "high is bad" and the "low is bad" indicators.
SUB Bar(x AS INTEGER, y AS INTEGER, lv AS INTEGER, t1 AS INTEGER, hi AS INTEGER, lo AS INTEGER)
  LOCAL INTEGER w, c, v
  v = lv
  IF v < 0 THEN v = 0
  IF v > 16 THEN v = 16
  c = lo
  IF v >= t1 THEN c = hi
  w = v * 2.5
  BOX x + 1, y, DW, 3, 0, cBlack, cBlack
  IF w > 0 THEN BOX x + 1, y, w, 3, 0, c, c
END SUB

' Roll and dive/climb are centre-zero: one marker sliding over sixteen
' positions with the centre at 8, and four rows deep rather than three.
SUB Pointer(x AS INTEGER, y AS INTEGER, p AS INTEGER)
  LOCAL INTEGER v
  v = p
  IF v < 0 THEN v = 0
  IF v > 15 THEN v = 15
  BOX x + 1, y, DW, 4, 0, cBlack, cBlack
  BOX x + 1 + v * 2.5, y, 3, 4, 0, cYellow, cYellow
END SUB

' Four missile blocks, filled from the left as missiles are carried.
SUB MissileBlocks
  LOCAL INTEGER i, c
  FOR i = 0 TO 3
    c = cBlack
    IF i < pMissl THEN c = cGreen
    BOX 20 + i * 10, 225, 7, 5, 0, c, c
  NEXT i
END SUB

' The scanner.  Each contact is a dash with a stick down to the plane of
' the ellipse, so the ellipse reads as the plane the player is flying in
' and the stick shows how far above or below it the contact sits.  It is
' always drawn in front-view coordinates whichever way the player is
' looking, exactly as the original does it.
SUB DrawScanner
  LOCAL INTEGER n, px, py, base, c
  BOX SCX - SCA, SCY - SCB - 9, 2 * SCA, 2 * SCB + 18, 0, cBlack, cBlack
  ' CIRCLE takes the vertical radius; its aspect is width over height.
  CIRCLE SCX, SCY, SCB, 1, SCA / SCB, cCyan, -1
  FOR n = 0 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 THEN
      ' A contact shows only while all three high bytes are 63 or less.
      IF ABS(sX(n)) < 16384 AND ABS(sY(n)) < 16384 AND ABS(sZ(n)) < 16384 THEN
        px = SCDOTX + sX(n) / SCXDIV
        base = SCY - sZ(n) / SCZDIV
        py = base - sY(n) / SCYDIV
        IF py < SCTOP THEN py = SCTOP
        IF py > SCBOT THEN py = SCBOT
        IF px > SCX - SCA AND px < SCX + SCA THEN
          c = cGreen
          IF sTyp(n) = T_MISSILE THEN c = cYellow
          LINE px, base, px, py, 1, c
          BOX px, py - 1, 5, 2, 0, c, c
        ENDIF
      ENDIF
    ENDIF
  NEXT n
END SUB

' The compass points at the station inside the safe zone and at the planet
' outside it.  Ahead it is a yellow dash two rows deep, behind it a green
' one a single row deep: the original tells the two apart by height and
' colour together, not by filling or hollowing one marker.
SUB DrawCompass
  LOCAL INTEGER n, px, py
  LOCAL FLOAT m
  n = SLOT_PLANET
  IF inSafe AND sTyp(SLOT_STAR) = T_STATION THEN n = SLOT_STAR
  IF sTyp(n) = 0 THEN EXIT SUB
  m = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
  IF m < 1 THEN EXIT SUB
  BOX CPX - CPR - 4, CPY - CPR - 3, 2 * CPR + 9, 2 * CPR + 7, 0, cBlack, cBlack
  CIRCLE CPX, CPY, CPR + 2, 1, 1.25, cGrey, -1
  px = CPX + CPR * 1.25 * sX(n) / m
  py = CPY - CPR * sY(n) / m
  IF sZ(n) >= 0 THEN
    BOX px, py, 3, 2, 0, cYellow, cYellow
  ELSE
    BOX px, py, 3, 1, 0, cGreen, cGreen
  ENDIF
END SUB

SUB ViewName
  LOCAL v$
  SELECT CASE vw
    CASE 0 : v$ = "Front view"
    CASE 1 : v$ = "Rear view"
    CASE 2 : v$ = "Left view"
    CASE 3 : v$ = "Right view"
  END SELECT
  ' BBC text column 11, row 1: x = 11 * 8 * 1.25, y = 8.
  TEXT 110, 8, v$, "LT", 7, 1, cWhite
END SUB

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

' =====================================================================
'  The galaxy
'
'  Elite does not store its universe; it generates it.  Three 16-bit
'  seeds are stirred by one short routine, and every system's name,
'  position, government, economy, technology, population, productivity
'  and radius falls out of particular bits of those seeds.  Eight
'  galaxies of 256 systems each come out of six bytes.
'
'  All of this has to be bit for bit identical to the original or the
'  universe is a different one, so nothing here is tidied up: the twist
'  is a 16-bit add with the carry thrown away, and every field is the
'  slice of bits the original took.  elite_tools/galaxy_ref.py is the
'  oracle, and tests/galaxy_test.bas checks the board against it.
' =====================================================================

' One turn of the generator: the third seed becomes the sum of all three,
' and the other two shuffle down.  Anything above 16 bits is discarded.
SUB GalTwist
  LOCAL INTEGER t
  t = (gs0 + gs1) AND &HFFFF
  gs0 = gs1
  gs1 = gs2
  gs2 = (t + gs1) AND &HFFFF
END SUB

' Move to galaxy g, counting from 1.  A galaxy is reached by rotating
' each of the six seed bytes left by one bit, independently - which is
' why the eight galaxies are related but not similar.
SUB SetGalaxy(g AS INTEGER)
  LOCAL INTEGER i, k, b(5)
  gs0 = &H5A4A : gs1 = &H0248 : gs2 = &HB753
  FOR i = 2 TO g
    b(0) = gs0 AND 255 : b(1) = (gs0 >> 8) AND 255
    b(2) = gs1 AND 255 : b(3) = (gs1 >> 8) AND 255
    b(4) = gs2 AND 255 : b(5) = (gs2 >> 8) AND 255
    FOR k = 0 TO 5
      b(k) = ((b(k) << 1) OR (b(k) >> 7)) AND 255
    NEXT k
    gs0 = b(0) OR (b(1) << 8)
    gs1 = b(2) OR (b(3) << 8)
    gs2 = b(4) OR (b(5) << 8)
  NEXT i
  gSys = 0
END SUB

' Step to the next system: four turns of the generator.
SUB NextSystem
  GalTwist : GalTwist : GalTwist : GalTwist
  gSys = (gSys + 1) AND 255
END SUB

' Wind the seeds to system n of the current galaxy, from wherever they
' are now.  Going backwards means starting again, because the generator
' only runs one way.
SUB GotoSystem(g AS INTEGER, n AS INTEGER)
  LOCAL INTEGER i
  SetGalaxy g
  FOR i = 1 TO n
    NextSystem
  NEXT i
END SUB

' Everything the game knows about the system the seeds are sitting on.
' Technology is displayed one higher than it is stored, and population is
' in tenths of a billion.
SUB SysData
  LOCAL INTEGER h0, h1, h2, l1
  h0 = (gs0 >> 8) AND 255
  h1 = (gs1 >> 8) AND 255
  h2 = (gs2 >> 8) AND 255
  l1 = gs1 AND 255
  sysX = h1
  sysY = h0 >> 1
  sysGov = (l1 >> 3) AND 7
  sysEco = h0 AND 7
  ' An anarchy or a feudal state can never be rich.
  IF sysGov <= 1 THEN sysEco = sysEco OR 2
  sysTech = (sysEco XOR 7) + (h1 AND 3) + ((sysGov + 1) >> 1)
  sysPop = sysTech * 4 + sysEco + sysGov + 1
  sysProd = ((sysEco XOR 7) + 3) * (sysGov + 4) * sysPop * 8
  sysRad = ((h2 AND 15) + 11) * 256 + h1
END SUB

' The system's name, built from three or four two-letter fragments picked
' out of the seeds.  It runs on a copy, so the caller's seeds are left
' where they were.  Fragment 0 is never emitted, and one fragment
' contributes a single letter rather than two.
FUNCTION SysName$()
  LOCAL INTEGER i, n, k, t0, t1, t2
  LOCAL nm$ LENGTH 10
  t0 = gs0 : t1 = gs1 : t2 = gs2
  n = 3
  IF (gs0 AND 64) <> 0 THEN n = 4
  nm$ = ""
  FOR k = 1 TO n
    i = ((gs2 >> 8) AND 31)
    IF i <> 0 THEN nm$ = nm$ + MID$(DIGRAPHS, i * 2 + 1, 2)
    GalTwist
  NEXT k
  gs0 = t0 : gs1 = t1 : gs2 = t2
  ' One fragment contributes a single letter, and carries a marker for the
  ' second one it does not have.  A name can pick that fragment more than
  ' once - eleven of the 2048 do - so strip every marker, not just the
  ' first.
  DO
    i = INSTR(nm$, "?")
    IF i = 0 THEN EXIT DO
    nm$ = LEFT$(nm$, i - 1) + MID$(nm$, i + 1)
  LOOP
  SysName$ = nm$
END FUNCTION

FUNCTION GovName$(g AS INTEGER)
  GovName$ = FIELD$("Anarchy,Feudal,Multi-gov,Dictatorship,Communist,Confederacy,Democracy,Corporate State", g + 1)
END FUNCTION

FUNCTION EcoName$(e AS INTEGER)
  EcoName$ = FIELD$("Rich Ind,Average Ind,Poor Ind,Mainly Ind,Mainly Agri,Rich Agri,Average Agri,Poor Agri", e + 1)
END FUNCTION

' =====================================================================
'  The market
'
'  Prices are not stored either.  Each system draws one random byte when
'  the player arrives, and that byte together with the system's economy
'  fixes the price and the quantity of all seventeen commodities until
'  the player leaves and comes back.
'
'  The arithmetic is eight bit and it wraps, which is exactly why
'  narcotics and furs swing so wildly from system to system while food
'  and minerals barely move.  The wrap is reproduced, not tidied away.
'
'    price    = ((base + (rnd AND mask) + economy * factor) AND 255) * 4
'    quantity = base_qty + (rnd AND mask) - economy * factor
'               floored at 0, then AND 63
'
'  Price is in tenths of a credit.  A negative factor means the goods get
'  cheaper and more plentiful the more agricultural the system is.
' =====================================================================

SUB LoadMarket
  LOCAL INTEGER i
  RESTORE dat_market
  FOR i = 0 TO NGOODS - 1
    READ mkName$(i), mkBase(i), mkFact(i), mkUnit$(i), mkQty(i), mkMask(i)
  NEXT i
END SUB

' Work out the whole market for an economy and a market byte, leaving it
' in mkPrice() and mkStock().
SUB MakeMarket(eco AS INTEGER, mb AS INTEGER)
  LOCAL INTEGER i, q
  FOR i = 0 TO NGOODS - 1
    mkPrice(i) = ((mkBase(i) + (mb AND mkMask(i)) + eco * mkFact(i)) AND 255) * 4
    q = mkQty(i) + (mb AND mkMask(i)) - eco * mkFact(i)
    IF q < 0 THEN q = 0
    mkStock(i) = q AND 63
  NEXT i
END SUB

' Price as the game prints it: a credits and tenths string.
FUNCTION PriceStr$(p AS INTEGER)
  PriceStr$ = STR$(p \ 10) + "." + STR$(p MOD 10)
END FUNCTION

dat_market:
' name, base price, economic factor, unit, base quantity, mask
DATA "Food",19,-2,"t",6,1
DATA "Textiles",20,-1,"t",10,3
DATA "Radioactives",65,-3,"t",2,7
DATA "Slaves",40,-5,"t",226,31
DATA "Liquor/Wines",83,-5,"t",251,15
DATA "Luxuries",196,8,"t",54,3
DATA "Narcotics",235,29,"t",8,120
DATA "Computers",154,14,"t",56,3
DATA "Machinery",117,6,"t",40,7
DATA "Alloys",78,1,"t",17,31
DATA "Firearms",124,13,"t",29,7
DATA "Furs",176,-9,"t",220,63
DATA "Minerals",32,-1,"t",53,3
DATA "Gold",97,-1,"kg",66,7
DATA "Platinum",171,-2,"kg",55,31
DATA "Gem-Stones",45,-1,"g",250,15
DATA "Alien items",53,15,"t",192,7

' =====================================================================
'  The charts and the system data screen
'
'  Two views of the same generated galaxy.  The long range chart plots
'  all 256 systems of the current galaxy at their raw coordinates; the
'  short range chart shows only the neighbours, spread out four times
'  wider and twice taller so their names fit beside them.
'
'  Geometry from the original, converted x * 1.25 because our screen is
'  320 across where the BBC's was 256.  The galaxy is half as tall as it
'  is wide, so every chart halves the y coordinate.
'
'    long range   x = galaxy x,            y = galaxy y / 2 + 24
'    short range  x = 104 + dx * 4,        y = 90 + dy * 2
'                 shown only while |dx| < 20 and |dy| < 38
'
'  Distance is four times the root of dx squared plus half dy squared, in
'  tenths of a light year, and a full tank of 7.0 light years draws a
'  circle of radius 17 on the long range chart.
' =====================================================================

' Distance between two systems in tenths of a light year.  The y axis is
' halved because the galaxy is drawn half as tall as it is wide.
FUNCTION SysDist(x0 AS INTEGER, y0 AS INTEGER, x1 AS INTEGER, y1 AS INTEGER) AS INTEGER
  LOCAL INTEGER dx, dy
  dx = ABS(x1 - x0)
  dy = ABS(y1 - y0) \ 2
  ' The square root is taken as a whole number and only then multiplied by
  ' four.  Truncating after the multiply instead would put Lave to Zaonce
  ' at 5.7 light years rather than the 5.6 every player knows.
  SysDist = 4 * INT(SQR(dx * dx + dy * dy))
END FUNCTION

' Walk the current galaxy looking for the system nearest to (cx, cy) in
' raw galaxy coordinates, and leave the seeds sitting on it.
SUB FindSystem(cx AS INTEGER, cy AS INTEGER)
  LOCAL INTEGER i, best, bestd, d
  best = 0 : bestd = 32767
  SetGalaxy gGal
  FOR i = 0 TO 255
    SysData
    d = ABS(sysX - cx) + ABS(sysY * 2 - cy)
    IF d < bestd THEN bestd = d : best = i
    NextSystem
  NEXT i
  GotoSystem gGal, best
  SysData
  selSys = best
END SUB

' The whole galaxy: 256 dots, the reachable circle around where we are,
' and crosshairs on whatever the cursor has picked out.
SUB ChartLong
  LOCAL INTEGER i, px, py, r
  LOCAL nm$ LENGTH 10
  CLS
  TEXT VCX, 4, "GALACTIC CHART " + STR$(gGal), "CT", 7, 1, cWhite
  LINE 0, CHTOP - 5, SCRW - 1, CHTOP - 5, 1, cWhite
  LINE 0, CHTOP + 128, SCRW - 1, CHTOP + 128, 1, cWhite
  SetGalaxy gGal
  FOR i = 0 TO 255
    SysData
    px = sysX * 1.25
    py = sysY + CHTOP
    PIXEL px, py, cWhite
    NextSystem
  NEXT i
  ' The circle we can still reach on the fuel in the tank.
  r = pFuel \ 4
  CIRCLE curX * 1.25, curY \ 2 + CHTOP, r, 1, 1.25, cGreen, -1
  Crosshair curX * 1.25, curY \ 2 + CHTOP, 7
  GotoSystem gGal, selSys
  SysData
  nm$ = SysName$()
  TEXT 4, CHTOP + 134, nm$ + "   " + STR$(SysDist(homeX, homeY, sysX, sysY * 2) / 10) + " LY", "LT", 7, 1, cYellow
END SUB

' The neighbourhood, spread out enough to label.  Only systems close
' enough in both axes appear at all, and a name is only printed when its
' text row is still free, which is how the original stops labels piling
' on top of one another.
SUB ChartShort
  LOCAL INTEGER i, dx, dy, px, py, row, r
  LOCAL nm$ LENGTH 10
  LOCAL INTEGER used(21)
  CLS
  TEXT VCX, 4, "SHORT RANGE CHART", "CT", 7, 1, cWhite
  LINE 0, 19, SCRW - 1, 19, 1, cWhite
  ARRAY SET 0, used()
  SetGalaxy gGal
  FOR i = 0 TO 255
    SysData
    dx = sysX - curX
    dy = sysY * 2 - curY
    IF ABS(dx) < 20 AND ABS(dy) < 38 THEN
      px = SRCX + dx * SRDX
      py = SRCY + dy * SRDY
      ' A system's dot is a small circle whose size stands in for the
      ' point size the original varies with the seeds.
      CIRCLE px, py, 2, 1, 1, cWhite, cWhite
      row = py \ 8
      IF row >= 0 AND row <= 21 THEN
        IF used(row) = 0 THEN
          used(row) = 1
          nm$ = SysName$()
          TEXT px + 5, py - 3, nm$, "LT", 7, 1, cWhite
        ENDIF
      ENDIF
    ENDIF
    NextSystem
  NEXT i
  r = (pFuel \ 4) * SRDX
  CIRCLE SRCX, SRCY, r, 1, 1, cGreen, -1
  Crosshair SRCX, SRCY, 8
END SUB

SUB Crosshair(px AS INTEGER, py AS INTEGER, sz AS INTEGER)
  LINE px - sz, py, px + sz, py, 1, cWhite
  LINE px, py - sz, px, py + sz, 1, cWhite
END SUB

' Everything the game knows about the selected system, which is all
' derived from its seeds rather than stored anywhere.
SUB SysDataScreen
  LOCAL INTEGER y
  LOCAL nm$ LENGTH 10
  CLS
  GotoSystem gGal, selSys
  SysData
  nm$ = SysName$()
  TEXT VCX, 4, "DATA ON " + nm$, "CT", 7, 1, cWhite
  LINE 0, 19, SCRW - 1, 19, 1, cWhite
  y = 30
  DataLine y, "Distance", STR$(SysDist(homeX, homeY, sysX, sysY * 2) / 10) + " Light Years" : y = y + 12
  DataLine y, "Economy", EcoName$(sysEco) : y = y + 12
  DataLine y, "Government", GovName$(sysGov) : y = y + 12
  DataLine y, "Tech Level", STR$(sysTech + 1) : y = y + 12
  DataLine y, "Population", STR$(sysPop / 10) + " Billion" : y = y + 12
  DataLine y, "Productivity", STR$(sysProd) + " M CR" : y = y + 12
  DataLine y, "Radius", STR$(sysRad) + " km" : y = y + 12
END SUB

SUB DataLine(y AS INTEGER, lb$, v$)
  TEXT 20, y, lb$ + ":", "LT", 7, 1, cWhite
  TEXT 150, y, v$, "LT", 7, 1, cYellow
END SUB

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
  IF d > pFuel THEN EXIT SUB
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
    MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
    n = NewShip(T_STATION, px, py, pz, qA())
    IF n >= 0 THEN sRol(n) = 255 : sAI(n) = 1
  ENDIF
END SUB

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

  ' Whatever is lined up and nearest gets hit.
  best = -1 : bestz = 999999
  FOR n = 2 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 AND sExp(n) = 0 THEN
      IF sZ(n) > 0 AND sZ(n) < bestz THEN
        IF ABS(sX(n)) < 256 AND ABS(sY(n)) < 256 THEN
          IF sX(n) * sX(n) + sY(n) * sY(n) < bArea(sBp(n)) THEN
            best = n : bestz = sZ(n)
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
    ELSE
      Explode best
    ENDIF
  ENDIF
END SUB

' Start a ship exploding.  The cloud grows for a while and then goes out,
' and the ship is only removed when it does.
SUB Explode(n AS INTEGER)
  sExp(n) = 18
  sSpd(n) = 0
  sAI(n) = 0
  ' Anything destroyed counts towards the combat rating, and pays out
  ' whatever bounty its blueprint carries - which is nothing for most
  ' things and half a credit for an asteroid.
  kills = kills + 1
  cashTenths = cashTenths + bBty(sBp(n))
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
            IF d < 8192 AND cnt > 0.917 THEN
              dmg = bLas(sBp(n)) * 2
              IF cnt > 0.972 THEN
                HitPlayer dmg
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
  IF pMissl = 0 THEN EXIT SUB
  best = -1 : bestz = 999999
  FOR n = 2 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 AND sExp(n) = 0 AND sTyp(n) <> T_MISSILE THEN
      IF sZ(n) > 0 AND sZ(n) < bestz THEN
        IF ABS(sX(n)) < 256 AND ABS(sY(n)) < 256 THEN
          IF sX(n) * sX(n) + sY(n) * sY(n) < bArea(sBp(n)) THEN
            best = n : bestz = sZ(n)
          ENDIF
        ENDIF
      ENDIF
    ENDIF
  NEXT n
  IF best >= 0 THEN msLock = best
END SUB

' Launch one at whatever is locked.  It appears just ahead of us already
' pointing the right way and moving faster than anything it chases.
SUB LaunchMissile
  LOCAL INTEGER n
  IF pMissl = 0 OR msLock < 0 THEN EXIT SUB
  IF sTyp(msLock) = 0 OR sExp(msLock) > 0 THEN msLock = -1 : EXIT SUB
  MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
  n = NewShip(T_MISSILE, 0, -28, 200, qA())
  IF n < 0 THEN EXIT SUB
  sSpd(n) = bSpd(sBp(n))
  sAI(n) = 128 OR 126               ' as aggressive as anything gets
  sTgt(n) = msLock
  pMissl = pMissl - 1
  msLock = -1
END SUB

' Everything a missile does, every frame.  It turns towards whatever it is
' chasing and goes off when it gets there.
SUB Missiles
  LOCAL INTEGER n, t
  LOCAL FLOAT dx, dy, dz, d
  FOR n = 2 TO nUsed - 1
    IF sTyp(n) = T_MISSILE AND sExp(n) = 0 THEN
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
  FOR n = 2 TO nUsed - 1
    IF sTyp(n) = T_MISSILE AND sExp(n) = 0 THEN Explode n
  NEXT n
END SUB

SUB ECMService
  IF ecmActive > 0 THEN
    ecmActive = ecmActive - 1
    pEnergy = pEnergy - 1
    IF pEnergy < 0 THEN pEnergy = 0
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
  IF sTyp(n) = T_VIPER THEN
    legal = legal + 64
    IF legal > 255 THEN legal = 255
  ENDIF
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

' ======================================================================
' ship blueprint data (generated by elite_tools/blueprints.py)
' ======================================================================
' ships.bas - Elite cassette ship meshes for Draw3D, generated by elite_tools/blueprints.py
' Do not edit: regenerate from the source.  One label per blueprint; RESTORE it, then READ:
'   name$, nv, nf, nfv, nf0, nv0, canisters, area, bounty10, visdist, energy, speed, laser, missiles, gun, explosion
'   nv * (x,y,z)   nf * facecount   nf * hostface   nf0 * stored normal (x,y,z)   nfv * vertex index
'   nf0 / nv0 = the blueprint's own face / vertex counts (polygons beyond nf0 are detail or sliver lines,
'   vertices beyond nv0 are the sliver vertices that turn bare lines into drawable triangles)
' Coordinates are blueprint units (same units as ship positions).  Faces beyond the
' blueprint's face count are surface-detail polygons that share their host face's normal.

dat_sidewinder:
DATA "Sidewinder", 10, 8, 26, 7, 10, 0, 4225, 50, 20, 70, 37, 2, 0, 0, 30
DATA -32,0,36, 32,0,36, 64,0,-28, -64,0,-28, 0,16,-28, 0,-16,-28
DATA -12,6,-28, 12,6,-28, 12,-6,-28, -12,-6,-28
DATA 3,3,3,4,4,3,3,3
DATA 0,1,2,3,3,4,5,6
DATA 0,32,8, -12,47,6, 12,47,6, 0,0,-112, -12,-47,6, 0,-32,8
DATA 12,-47,6
DATA 0,1,4
DATA 0,4,3
DATA 2,4,1
DATA 2,5,3,4
DATA 6,7,8,9
DATA 0,3,5
DATA 0,5,1
DATA 1,5,2

dat_viper:
DATA "Viper", 15, 9, 34, 7, 15, 0, 5625, 0, 23, 120, 32, 2, 1, 0, 42
DATA 0,0,72, 0,16,24, 0,-16,24, 48,0,-24, -48,0,-24, 24,-16,-24
DATA -24,-16,-24, 24,16,-24, -24,16,-24, -32,0,-24, 32,0,-24, 8,8,-24
DATA -8,8,-24, -8,-8,-24, 8,-8,-24
DATA 3,4,4,4,4,3,6,3,3
DATA 0,1,2,3,4,5,6,6,6
DATA 0,32,0, -22,33,11, 22,33,11, -22,-33,11, 22,-33,11, 0,-32,0
DATA 0,0,-48
DATA 1,7,8
DATA 0,1,8,4
DATA 0,3,7,1
DATA 0,4,6,2
DATA 0,2,5,3
DATA 2,6,5
DATA 3,5,6,4,8,7
DATA 10,14,11
DATA 9,12,13

dat_mamba:
DATA "Mamba", 25, 11, 36, 5, 25, 1, 4900, 150, 25, 90, 30, 2, 2, 0, 34
DATA 0,0,64, -64,-8,-32, -32,8,-32, 32,8,-32, 64,-8,-32, -4,4,16
DATA 4,4,16, 8,3,28, -8,3,28, -20,-4,16, 20,-4,16, -24,-7,-20
DATA -16,-7,-20, 16,-7,-20, 24,-7,-20, -8,4,-32, 8,4,-32, 8,-4,-32
DATA -8,-4,-32, -32,4,-32, 32,4,-32, 36,-4,-32, -36,-4,-32, -38,0,-32
DATA 38,0,-32
DATA 3,3,3,3,4,3,3,4,3,3,4
DATA 0,0,0,1,1,2,3,4,4,4,4
DATA 0,-24,2, 0,24,2, -32,64,16, 32,64,16, 0,0,-127
DATA 0,1,4
DATA 13,14,10
DATA 9,11,12
DATA 2,0,3
DATA 6,5,8,7
DATA 0,2,1
DATA 0,4,3
DATA 2,3,4,1
DATA 20,24,21
DATA 19,22,23
DATA 15,16,17,18

dat_python:
DATA "Python", 15, 17, 56, 13, 11, 3, 14400, 200, 40, 250, 20, 3, 3, 0, 46
DATA 0,0,224, 0,48,48, 96,0,-16, -96,0,-16, 0,48,-32, 0,24,-112
DATA -48,0,-112, 48,0,-112, 0,-48,48, 0,-48,-32, 0,-24,-112, 0.26,24.273,-111.672
DATA 0.26,23.727,-112.328, -0.26,-24.273,-111.672, -0.26,-23.727,-112.328
DATA 3,3,3,3,3,3,3,3,4,3,4,3,4,3,4,3,4
DATA 0,1,2,3,4,5,6,7,8,8,9,9,10,10,11,11,12
DATA -27,40,11, 27,40,11, -27,-40,11, 27,-40,11, -19,38,0, 19,38,0
DATA -19,-38,0, 19,-38,0, -25,37,-11, 25,37,-11, 25,-37,-11, -25,-37,-11
DATA 0,0,-112
DATA 0,1,3
DATA 0,2,1
DATA 0,3,8
DATA 0,8,2
DATA 1,4,3
DATA 2,4,1
DATA 3,9,8
DATA 2,8,9
DATA 4,5,6,3
DATA 3,11,5
DATA 4,2,7,5
DATA 2,12,5
DATA 9,10,7,2
DATA 2,13,10
DATA 9,3,6,10
DATA 3,14,10
DATA 7,10,6,5

dat_cobra_mk_3:
DATA "Cobra Mk III", 30, 19, 66, 13, 28, 3, 9025, 0, 50, 150, 28, 2, 3, 21, 42
DATA 32,0,76, -32,0,76, 0,26,24, -120,-3,-8, 120,-3,-8, -88,16,-40
DATA 88,16,-40, 128,-8,-40, -128,-8,-40, 0,26,-40, -32,-24,-40, 32,-24,-40
DATA -36,8,-40, -8,12,-40, 8,12,-40, 36,8,-40, 36,-12,-40, 8,-16,-40
DATA -8,-16,-40, -36,-12,-40, 0,0,76, 0,0,90, -80,-6,-40, -80,6,-40
DATA -88,0,-40, 80,6,-40, 88,0,-40, 80,-6,-40, -0.5,0,90, 0.5,0,90
DATA 3,3,3,3,3,3,3,3,3,3,7,3,3,4,4,4,4,3,4
DATA 0,0,1,2,3,4,5,6,7,8,9,9,9,9,9,10,11,11,12
DATA 0,62,31, -18,55,16, 18,55,16, -16,52,14, 16,52,14, -14,47,0
DATA 14,47,0, -61,102,0, 61,102,0, 0,0,-80, -7,-42,9, 0,-30,6
DATA 7,-42,9
DATA 0,2,1
DATA 20,28,21
DATA 1,2,5
DATA 0,6,2
DATA 1,5,3
DATA 4,6,0
DATA 5,2,9
DATA 2,6,9
DATA 3,5,8
DATA 6,4,7
DATA 10,8,5,9,6,7,11
DATA 25,26,27
DATA 23,22,24
DATA 14,15,16,17
DATA 12,13,18,19
DATA 8,10,1,3
DATA 0,1,10,11
DATA 20,29,21
DATA 7,4,0,11

dat_thargoid:
DATA "Thargoid", 22, 12, 54, 10, 20, 0, 9801, 500, 55, 240, 39, 2, 6, 15, 38
DATA 32,-48,48, 32,-68,0, 32,-48,-48, 32,0,-68, 32,48,-48, 32,68,0
DATA 32,48,48, 32,0,68, -24,-116,116, -24,-164,0, -24,-116,-116, -24,0,-164
DATA -24,116,-116, -24,164,0, -24,116,116, -24,0,164, -24,64,80, -24,64,-80
DATA -24,-64,-80, -24,-64,80, -24,64.5,-80, -24,-64.5,80
DATA 4,4,4,4,8,4,4,4,4,8,3,3
DATA 0,1,2,3,4,5,6,7,8,9,9,9
DATA 103,-60,25, 103,-60,-25, 103,-25,-60, 103,25,-60, 64,0,0, 103,60,-25
DATA 103,60,25, 103,25,60, 103,-25,60, -48,0,0
DATA 0,8,9,1
DATA 9,10,2,1
DATA 2,10,11,3
DATA 11,12,4,3
DATA 0,1,2,3,4,5,6,7
DATA 12,13,5,4
DATA 13,14,6,5
DATA 7,6,14,15
DATA 7,15,8,0
DATA 9,8,15,14,13,12,11,10
DATA 16,20,17
DATA 18,21,19

dat_coriolis:
DATA "Coriolis", 16, 15, 52, 14, 16, 0, 25600, 0, 120, 240, 0, 0, 6, 0, 54
DATA 160,0,160, 0,160,160, -160,0,160, 0,-160,160, 160,-160,0, 160,160,0
DATA -160,160,0, -160,-160,0, 160,0,-160, 0,160,-160, -160,0,-160, 0,-160,-160
DATA 10,-30,160, 10,30,160, -10,30,160, -10,-30,160
DATA 4,4,3,3,3,3,4,4,4,4,3,3,3,3,4
DATA 0,0,1,2,3,4,5,6,7,8,9,10,11,12,13
DATA 0,0,160, 107,-107,107, 107,107,107, -107,107,107, -107,-107,107, 0,-160,0
DATA 160,0,0, -160,0,0, 0,160,0, -107,-107,-107, 107,-107,-107, 107,107,-107
DATA -107,107,-107, 0,0,-160
DATA 0,1,2,3
DATA 13,14,15,12
DATA 0,3,4
DATA 0,5,1
DATA 1,6,2
DATA 2,7,3
DATA 3,7,11,4
DATA 4,8,5,0
DATA 7,2,6,10
DATA 1,5,9,6
DATA 7,10,11
DATA 4,11,8
DATA 5,8,9
DATA 9,10,6
DATA 9,8,11,10

dat_missile:
DATA "Missile", 33, 25, 80, 9, 17, 0, 1600, 0, 14, 2, 44, 0, 0, 0, 10
DATA 0,0,68, 8,-8,36, 8,8,36, -8,8,36, -8,-8,36, 8,8,-44
DATA 8,-8,-44, -8,-8,-44, -8,8,-44, 12,12,-44, 12,-12,-44, -12,-12,-44
DATA -12,12,-44, -8,8,-12, -8,-8,-12, 8,8,-12, 8,-8,-12, 8,8.496,-11.938
DATA 8,-7.504,-12.062, 8.496,-8,-11.938, -7.504,-8,-12.062, 12,-12,-44.5, -12,-12,-43.5, -8,7.504,-12.062
DATA -8,-8.496,-11.938, 7.504,8,-12.062, -8.496,8,-11.938, 12,12,-43.5, -12,12,-44.5, 12.354,-11.646,-44
DATA 11.646,12.354,-44, -12.354,11.646,-44, -11.646,-12.354,-44
DATA 3,3,3,3,4,3,3,4,3,3,3,3,4,3,3,4,3,3,3,3,4,3,3,3,3
DATA 0,1,2,3,4,4,4,5,5,5,5,5,6,6,6,7,7,7,7,7,8,8,8,8,8
DATA -64,0,16, 0,-64,16, 64,0,16, 0,64,16, 32,0,0, 0,-32,0
DATA -32,0,0, 0,32,0, 0,0,-176
DATA 0,3,4
DATA 0,4,1
DATA 0,1,2
DATA 2,3,0
DATA 1,6,5,2
DATA 9,17,15
DATA 10,18,16
DATA 1,4,7,6
DATA 10,19,16
DATA 11,20,14
DATA 6,21,10
DATA 7,22,11
DATA 4,3,8,7
DATA 12,23,13
DATA 11,24,14
DATA 2,5,8,3
DATA 9,25,15
DATA 12,26,13
DATA 5,27,9
DATA 8,28,12
DATA 6,7,8,5
DATA 6,29,10
DATA 5,30,9
DATA 8,31,12
DATA 7,32,11

dat_asteroid:
DATA "Asteroid", 9, 14, 42, 14, 9, 0, 6400, 5, 50, 60, 30, 0, 0, 0, 34
DATA 0,80,0, -80,-10,0, 0,-80,0, 70,-40,0, 60,50,0, 50,0,60
DATA -40,0,70, 0,30,-75, 0,-50,-60
DATA 3,3,3,3,3,3,3,3,3,3,3,3,3,3
DATA 0,1,2,3,4,5,6,7,8,9,10,11,12,13
DATA 9,66,81, 9,-66,81, -72,64,31, -64,-73,47, 45,-79,65, 135,15,35
DATA 38,76,70, -66,59,-39, -67,-15,-80, 66,-14,-75, -70,-80,-40, 58,-102,-51
DATA 81,9,-67, 47,94,-63
DATA 5,0,6
DATA 2,5,6
DATA 0,1,6
DATA 1,2,6
DATA 2,3,5
DATA 4,5,3
DATA 4,0,5
DATA 0,7,1
DATA 1,7,8
DATA 3,8,7
DATA 1,8,2
DATA 2,8,3
DATA 4,3,7
DATA 0,4,7

dat_canister:
DATA "Canister", 10, 7, 30, 7, 10, 0, 400, 0, 12, 17, 15, 0, 0, 0, 18
DATA 24,16,0, 24,5,15, 24,-13,9, 24,-13,-9, 24,5,-15, -24,16,0
DATA -24,5,15, -24,-13,9, -24,-13,-9, -24,5,-15
DATA 5,4,4,4,4,4,5
DATA 0,1,2,3,4,5,6
DATA 96,0,0, 0,41,30, 0,-18,48, 0,-51,0, 0,-18,-48, 0,41,-30
DATA -96,0,0
DATA 0,1,2,3,4
DATA 0,5,6,1
DATA 1,6,7,2
DATA 2,7,8,3
DATA 4,3,8,9
DATA 4,9,5,0
DATA 6,5,9,8,7

dat_thargon:
DATA "Thargon", 10, 7, 30, 7, 10, 0, 1600, 50, 20, 20, 30, 2, 0, 0, 18
DATA -9,0,40, -9,-38,12, -9,-24,-32, -9,24,-32, -9,38,12, 9,0,-8
DATA 9,-10,-15, 9,-6,-26, 9,6,-26, 9,10,-15
DATA 5,4,4,4,4,4,5
DATA 0,1,2,3,4,5,6
DATA -36,0,0, 20,-5,7, 46,-42,-14, 36,0,-104, 46,42,-14, 20,5,7
DATA 36,0,0
DATA 0,4,3,2,1
DATA 0,1,6,5
DATA 1,2,7,6
DATA 2,3,8,7
DATA 4,9,8,3
DATA 4,0,5,9
DATA 6,7,8,9,5

dat_escape_pod:
DATA "Escape pod", 4, 4, 12, 4, 4, 0, 256, 0, 8, 17, 8, 0, 0, 0, 22
DATA -7,0,36, -7,-14,-12, -7,14,-12, 21,0,0
DATA 3,3,3,3
DATA 0,1,2,3
DATA 26,0,-61, 19,51,15, 19,-51,15, -56,0,0
DATA 2,3,1
DATA 2,0,3
DATA 0,1,3
DATA 0,2,1
