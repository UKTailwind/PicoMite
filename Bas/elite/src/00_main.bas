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
DIM INTEGER cGreen, cYellow, cWhite, cBlack, cCyan, cGrey, cRed

' Draw3D object pool.  objOwn(n) is the slot that owns object n, or -1.
DIM INTEGER maxObj, objOwn(15)

' Quaternion scratch.  Draw3D and MATH both want a 5 element float array.
DIM FLOAT qA(4), qB(4), qC(4), qV(4), qP(4), vwQ(4, 3)

' Keyboard flags, refreshed once per frame.
DIM INTEGER kRollL, kRollR, kUp, kDn, kFaster, kSlower, kFire, kQuit
DIM INTEGER kView, kPause

' Frame timing.
DIM FLOAT frameMs, tFrame, tStage
DIM INTEGER frames

DIM FLOAT prof(5)                  ' cls, stardust, planet, ships, dash, move

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

