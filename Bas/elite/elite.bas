' Elite for the PicoMite, built by elite_tools/build.py from src/*.bas.
' Comments and indentation are stripped to fit program memory - read src/.
LIBRARY LOAD MM.INFO(PATH) + "elite_lib.bas", RAM
IF MM.INFO(PSRAM SIZE) > 0 THEN OPTION TRACECACHE ON 256 ELSE OPTION TRACECACHE ON 128
OPTION CACHE SUB DrawStardust, DrawScanner
OPTION EXPLICIT
OPTION DEFAULT NONE
SetupScreen
LoadStats
ProbeObjects
SetupViews
ShipColours
EquipTable
LoadTokens
LoadSounds
IF DEMOFRAMES = 0 THEN
DO
titleKey = TitleScreen()
IF titleKey = 27 THEN EXIT DO
IF titleKey = 0 AND DEMOPLAY THEN
RunDemo
IF demoTakeover = 0 THEN titleKey = -1
ENDIF
IF titleKey <> -1 THEN
NewGame
RunGame
ENDIF
LOOP
ELSE
IF DEMOSCENE = 9 THEN
LeftScene
FRAMEBUFFER CLOSE
RestoreScreen
PRINT "left-list done"
END
ELSEIF DEMOSCENE = 8 THEN
FRAMEBUFFER CLOSE
RestoreScreen
DockNPCScene
PRINT "npc dock done"
END
ELSEIF DEMOSCENE = 7 THEN
FRAMEBUFFER CLOSE
RestoreScreen
NewbScene
PRINT "newb done"
END
ELSEIF DEMOSCENE = 6 THEN
MissionScene
FRAMEBUFFER CLOSE
RestoreScreen
PRINT "missions done,"; shotNo; " pages"
END
ELSEIF DEMOSCENE = 5 THEN
NewCommander
MissionBrief 10, 0
MissionBrief 11, 0
MissionBrief 222, 0
MissionBrief 223, 0
FRAMEBUFFER CLOSE
RestoreScreen
PRINT "briefings done,"; shotNo; " pages"
GotoSystem gGal, homeSys
SysData
PRINT SysName$(); ": "; SysDesc$()
END
ELSEIF DEMOSCENE = 4 THEN
FOR frames = 1 TO 8
HangarScreen
SaveShot frames
NEXT frames
FRAMEBUFFER CLOSE
RestoreScreen
PRINT "hangar done"
END
ELSEIF DEMOSCENE = 3 THEN
DockedScreens
FRAMEBUFFER CLOSE
RestoreScreen
PRINT "docked screens done"
END
ELSEIF DEMOSCENE = 2 THEN
DockScene
ELSE
TestScene
ENDIF
frames = 0
tFrame = TIMER
ResetTick
DO
NextTick
IF DEMOSCENE = 2 THEN DockInput frames ELSE DemoInput frames
IF kQuit OR dead OR docked THEN EXIT DO
UpdatePlayer
IF kFire THEN FireLaser
IF kTarget THEN TargetMissile
IF kMissile THEN LaunchMissile
IF kECM THEN FireECM
IF kDock THEN
IF eqOwned(EQ_DOCK) THEN dockComp = 1 - dockComp ELSE Sfx SFX_BOOP
ENDIF
IF dockComp THEN DockingComputer
IF lasTimer > 0 THEN lasTimer = lasTimer - 1
IF lasFlash > 0 THEN lasFlash = lasFlash - 1
tStage = TIMER
MoveShips
Contact
Missiles
Tactics
ECMService
Recharge
Altitude
CabinTemp
StationCheck
StationPolice
SpawnTraffic
DockCheck
prof(5) = prof(5) + TIMER - tStage
DrawFrame
FRAMEBUFFER COPY F, N
mcnt = (mcnt + 1) AND 255
frames = frames + 1
IF frames = 40 OR frames = 80 OR frames = 120 OR frames = 250 THEN SaveShot frames
IF frames >= DEMOFRAMES THEN EXIT DO
LOOP
tFlight = TIMER - tFrame
ENDIF
IF dead THEN DeathScreen : PAUSE 1500
SoundOff
CloseAll
FRAMEBUFFER CLOSE
RestoreScreen
frameMs = 0
IF frames > 0 THEN frameMs = tFlight / frames
PRINT "frames"; frames; "  average"; STR$(frameMs, 4, 2); " ms per frame"
PRINT "shots"; shots; " hits"; hits; "  kills"; kills; "  cash"; cashTenths / 10; " Cr  rank "; RankName$()
PRINT "energy"; pEnergy; " fore shield"; pFsh; " laser temp"; pLasT; " fuel"; pFuel / 10; " LY"
PRINT "slots in use"; nUsed; "  dead"; dead; "  witchspace"; inWitch
PRINT "missiles left"; pMissl; "  legal status "; LegalName$()
PRINT "docked"; docked; "  docking computer"; dockComp
IF PROFILE THEN
IF frames > 0 THEN
PRINT "  CLS      "; STR$(prof(0) / frames, 5, 2); " ms"
PRINT "  stardust "; STR$(prof(1) / frames, 5, 2); " ms"
PRINT "  planet   "; STR$(prof(2) / frames, 5, 2); " ms"
PRINT "  ships    "; STR$(prof(3) / frames, 5, 2); " ms"
PRINT "  dash     "; STR$(prof(4) / frames, 5, 2); " ms"
PRINT "  move     "; STR$(prof(5) / frames, 5, 2); " ms"
PRINT "  the rest is the background framebuffer copy, which paces to 60 Hz"
ENDIF
ENDIF
END
SUB LoadStats
LOCAL INTEGER b
FOR b = 0 TO NBP - 1
LoadMesh b
NEXT b
tBp(1) = 0 : tBp(2) = 1 : tBp(3) = 2 : tBp(4) = 3 : tBp(5) = 4
tBp(6) = 5 : tBp(7) = 4 : tBp(8) = 6 : tBp(9) = 7 : tBp(10) = 8
tBp(11) = 9 : tBp(12) = 10 : tBp(13) = 11 : tBp(T_COUGAR) = 12
tBp(T_KRAIT) = 13 : tBp(T_ADDER) = 14 : tBp(T_GECKO) = 15
tBp(T_COBRA1) = 16 : tBp(T_WORM) = 17 : tBp(T_ASP) = 18
tBp(T_FERDELANCE) = 19 : tBp(T_BOA) = 20 : tBp(T_ANACONDA) = 21
tBp(T_BOULDER) = 22 : tBp(T_SPLINTER) = 23 : tBp(T_HERMIT) = 24
tBp(T_SHUTTLE) = 25 : tBp(T_TRANSPORT) = 26
tBp(T_CONSTRICT) = 28 : tBp(T_PYTHONP) = 3
NewbTable
END SUB
SUB NewbTable
LOCAL INTEGER i
FOR i = 0 TO NTYPE : tNewb(i) = 0 : NEXT i
tNewb(T_ESCAPE) = NB_TRADER
tNewb(T_SHUTTLE) = NB_TRADER OR NB_INNOCENT
tNewb(T_TRANSPORT) = NB_TRADER OR NB_INNOCENT OR NB_COP
tNewb(T_COBRA3) = NB_HOSTILE OR NB_PIRATE OR NB_POD
tNewb(T_TRADER) = NB_INNOCENT OR NB_POD
tNewb(T_PYTHON) = NB_INNOCENT OR NB_POD
tNewb(T_PYTHONP) = NB_HOSTILE OR NB_PIRATE OR NB_POD
tNewb(T_BOA) = NB_INNOCENT OR NB_POD
tNewb(T_ANACONDA) = NB_TRADER OR NB_INNOCENT OR NB_POD
tNewb(T_HERMIT) = NB_TRADER OR NB_INNOCENT OR NB_POD
tNewb(T_VIPER) = NB_HUNTER OR NB_COP OR NB_POD
tNewb(T_SIDEWINDER) = NB_HOSTILE OR NB_PIRATE
tNewb(T_MAMBA) = NB_HOSTILE OR NB_PIRATE OR NB_POD
tNewb(T_KRAIT) = NB_HOSTILE OR NB_PIRATE OR NB_POD
tNewb(T_ADDER) = NB_HOSTILE OR NB_PIRATE OR NB_POD
tNewb(T_GECKO) = NB_HOSTILE OR NB_PIRATE
tNewb(T_COBRA1) = NB_HOSTILE OR NB_PIRATE OR NB_POD
tNewb(T_WORM) = NB_HOSTILE OR NB_TRADER
tNewb(T_ASP) = NB_HOSTILE OR NB_PIRATE OR NB_POD
tNewb(T_FERDELANCE) = NB_HUNTER OR NB_POD
tNewb(T_THARGOID) = NB_HOSTILE OR NB_PIRATE
tNewb(T_THARGON) = NB_HOSTILE
tNewb(T_CONSTRICT) = NB_HOSTILE
tNewb(T_COUGAR) = NB_INNOCENT
END SUB
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
CASE 12 : RESTORE dat_cougar
CASE 13 : RESTORE dat_krait
CASE 14 : RESTORE dat_adder
CASE 15 : RESTORE dat_gecko
CASE 16 : RESTORE dat_cobra_mk_1
CASE 17 : RESTORE dat_worm
CASE 18 : RESTORE dat_asp_mk_2
CASE 19 : RESTORE dat_fer_de_lance
CASE 20 : RESTORE dat_boa
CASE 21 : RESTORE dat_anaconda
CASE 22 : RESTORE dat_boulder
CASE 23 : RESTORE dat_splinter
CASE 24 : RESTORE dat_rock_hermit
CASE 25 : RESTORE dat_shuttle
CASE 26 : RESTORE dat_transporter
CASE 27 : RESTORE dat_dodo
CASE 28 : RESTORE dat_constrictor
END SELECT
READ bName$(b), bNv(b), bNf(b), bNfv(b), bNf0(b), bNv0(b)
READ bCan(b), bArea(b), bBty(b), bVis(b), bEne(b), bSpd(b)
READ bLas(b), bMis(b), bGun(b), bExp(b)
FOR j = 0 TO bNv(b) - 1 : READ mV(0, j), mV(1, j), mV(2, j) : NEXT j
FOR j = 0 TO bNf(b) - 1 : READ mFc(j) : NEXT j
FOR j = 0 TO bNf(b) - 1 : READ mHost(j) : NEXT j
FOR j = 0 TO bNf0(b) - 1 : READ mNrm(0, j), mNrm(1, j), mNrm(2, j) : NEXT j
FOR j = 0 TO bNfv(b) - 1 : READ mF(j) : NEXT j
FOR j = 0 TO bNf(b) - 1
mEc(j) = 0
IF b = BP_CORIOLIS OR b = BP_DODO OR fillBlack THEN
mFl(j) = C_FILL
ELSE
mFl(j) = 1 + (mHost(j) MOD 6)
ENDIF
NEXT j
bSize(b) = 0
FOR j = 0 TO bNv0(b) - 1
FOR k = 0 TO 2
IF ABS(mV(k, j)) > bSize(b) THEN bSize(b) = ABS(mV(k, j))
NEXT k
NEXT j
END SUB
SUB ClearSlots
LOCAL INTEGER n
CloseAll
FOR n = 0 TO NSLOT - 1
sTyp(n) = 0 : sObj(n) = 0
NEXT n
nUsed = 0
END SUB
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
sQ(4, n) = 1
sObj(n) = 0
sSpd(n) = 0 : sAcc(n) = 0 : sRol(n) = 0 : sPit(n) = 0
sFlg(n) = 0 : sAI(n) = 0
sNewb(n) = 0
IF t <= NTYPE THEN sNewb(n) = (tNewb(t) AND 111) OR newbFlags
newbFlags = 0
sExp(n) = 0 : sTgt(n) = -1
sMis(n) = 0
IF t < T_PLANET THEN
sBp(n) = tBp(t)
sEne(n) = bEne(sBp(n))
sMis(n) = bMis(sBp(n))
ELSE
sBp(n) = -1
sEne(n) = 0
ENDIF
IF n >= nUsed THEN nUsed = n + 1
END FUNCTION
FUNCTION NewFacing(t AS INTEGER, x AS FLOAT, y AS FLOAT, z AS FLOAT, hdg AS INTEGER) AS INTEGER
MATH Q_EULER RAD(hdg), 0, 0, qA() : qA(4) = 1
NewFacing = NewShip(t, x, y, z, qA())
END FUNCTION
SUB KillShip(n AS INTEGER)
LOCAL INTEGER i
DropObject n
IF n < 2 THEN
sTyp(n) = 0 : sObj(n) = 0
EXIT SUB
ENDIF
FOR i = n TO nUsed - 2
CopySlot i, i + 1
NEXT i
sTyp(nUsed - 1) = 0
sObj(nUsed - 1) = 0
sExp(nUsed - 1) = 0
sTgt(nUsed - 1) = -1
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
sExp(d) = sExp(s) : sTgt(d) = sTgt(s) : sMis(d) = sMis(s)
IF sObj(d) > 0 THEN objOwn(sObj(d)) = d
sTyp(s) = 0 : sObj(s) = 0
sExp(s) = 0 : sTgt(s) = -1
END SUB
SUB GetObject(n AS INTEGER)
LOCAL INTEGER o, b, faces, j, e
IF sObj(n) > 0 THEN EXIT SUB
FOR o = 1 TO maxObj
IF objOwn(o) < 0 THEN
b = sBp(n)
LoadMesh b
e = shpCol(sTyp(n))
FOR j = 0 TO bNf(b) - 1 : mEc(j) = e : NEXT j
faces = solidMode
IF fillBlack THEN faces = 1
IF STNSOLID THEN
IF b = BP_CORIOLIS OR b = BP_DODO THEN faces = 1
ENDIF
IF faces THEN
ON ERROR SKIP 1
Draw3D CREATE o, bNv(b), bNf(b), 1, mV(), mFc(), mF(), col(), mEc(), mFl()
ELSE
ON ERROR SKIP 1
Draw3D CREATE o, bNv(b), bNf(b), 1, mV(), mFc(), mF(), col(), mEc()
ENDIF
IF MM.ERRNO <> 0 THEN
ON ERROR CLEAR
maxObj = o - 1
EXIT SUB
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
SUB ViewXform(n AS INTEGER)
SELECT CASE vw
CASE 0 : tx = sX(n)  : ty = sY(n) : tz = sZ(n)
CASE 1 : tx = -sX(n) : ty = sY(n) : tz = -sZ(n)
CASE 2 : tx = sZ(n)  : ty = sY(n) : tz = -sX(n)
CASE 3 : tx = -sZ(n) : ty = sY(n) : tz = sX(n)
END SELECT
END SUB
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
SUB RestoreScreen
IF scrVres > MM.VRES THEN POKE DISPLAY VRES scrVres
ON ERROR SKIP 1
MODE 1
ON ERROR CLEAR
END SUB
SUB SetupScreen
ON ERROR SKIP 1
MODE 2
ON ERROR CLEAR
scrVres = MM.VRES
IF scrVres > SCRH THEN POKE DISPLAY VRES SCRH
FRAMEBUFFER CREATE
FRAMEBUFFER WRITE F
homeDir$ = MM.INFO(PATH)
IF homeDir$ = "NONE" THEN homeDir$ = "A:/"
cmdrFile$ = homeDir$ + "cmdr.txt"
titlePic$ = homeDir$ + "title.jpg"
Draw3D CAMERA 1, VPLANE, 0, 0, 0, PANY
col(C_WHITE) = RGB(WHITE) : col(C_CYAN) = RGB(CYAN)
col(C_YELLOW) = RGB(YELLOW) : col(C_RED) = RGB(RED)
col(4) = RGB(GREEN) : col(5) = RGB(MAGENTA) : col(6) = RGB(BLUE)
col(C_FILL) = RGB(BLACK)
cGreen = RGB(GREEN) : cYellow = RGB(YELLOW) : cWhite = RGB(WHITE)
cBlack = RGB(BLACK) : cCyan = RGB(CYAN)
cDim = RGB(MIDGREEN)
cSel = RGB(BLUE)
cRed = RGB(RED) : cMagenta = RGB(MAGENTA) : cBlue = RGB(BLUE)
LOCAL INTEGER k
FOR k = 0 TO NSEG - 1
ctab(k) = COS(2 * PI * k / NSEG)
stab(k) = SIN(2 * PI * k / NSEG)
NEXT k
DLY(0) = 178 : DLY(1) = 186 : DLY(2) = 194
DLY(3) = 202 : DLY(4) = 210 : DLY(5) = 218
DRY(0) = 178 : DRY(1) = 185 : DRY(2) = 193 : DRY(3) = 202
DRY(4) = 210 : DRY(5) = 218 : DRY(6) = 226
LLAB$(0) = "FS" : LLAB$(1) = "AS" : LLAB$(2) = "FU"
LLAB$(3) = "CT" : LLAB$(4) = "LT" : LLAB$(5) = "AL"
RLAB$(0) = "SP" : RLAB$(1) = "RL" : RLAB$(2) = "DC"
RLAB$(3) = "1" : RLAB$(4) = "2" : RLAB$(5) = "3" : RLAB$(6) = "4"
END SUB
SUB ShipColours
LOCAL INTEGER t
FOR t = 0 TO NTYPE
shpCol(t) = C_CYAN
scaCol(t) = cCyan
NEXT t
shpCol(T_MISSILE) = C_YELLOW  : scaCol(T_MISSILE) = cYellow
shpCol(T_ASTEROID) = C_RED    : scaCol(T_ASTEROID) = cRed
shpCol(T_THARGOID) = C_WHITE  : scaCol(T_THARGOID) = cWhite
shpCol(T_THARGON) = C_WHITE
scaCol(T_STATION) = cGreen
scaCol(T_PYTHON) = cMagenta
scaCol(T_PYTHONP) = cMagenta
scaCol(T_TRADER) = cCyan
scaCol(T_CANISTER) = cBlue
scaCol(T_ESCAPE) = cBlue
shpCol(T_BOULDER) = C_RED     : scaCol(T_BOULDER) = cRed
shpCol(T_SPLINTER) = C_RED    : scaCol(T_SPLINTER) = cRed
shpCol(T_HERMIT) = C_RED      : scaCol(T_HERMIT) = cRed
scaCol(T_BOA) = cMagenta
scaCol(T_ANACONDA) = cMagenta
scaCol(T_WORM) = cBlue
END SUB
SUB SetupViews
LOCAL INTEGER i
MATH Q_EULER 0, 0, 0, qA()      : FOR i = 0 TO 4 : vwQ(i, 0) = qA(i) : NEXT i
MATH Q_CREATE RAD(180), 0, 1, 0, qA() : FOR i = 0 TO 4 : vwQ(i, 1) = qA(i) : NEXT i
MATH Q_CREATE RAD(90), 0, 1, 0, qA()  : FOR i = 0 TO 4 : vwQ(i, 2) = qA(i) : NEXT i
MATH Q_CREATE RAD(-90), 0, 1, 0, qA() : FOR i = 0 TO 4 : vwQ(i, 3) = qA(i) : NEXT i
END SUB
SUB ProbeObjects
LOCAL INTEGER n, h0, cost, room
LoadMesh 0
maxObj = 8
FOR n = 9 TO 35
ON ERROR SKIP 1
Draw3D CREATE n, bNv(0), bNf(0), 1, mV(), mFc(), mF(), col(), mEc()
IF MM.ERRNO <> 0 THEN EXIT FOR
Draw3D CLOSE n
maxObj = n
NEXT n
ON ERROR CLEAR
h0 = MM.INFO(HEAP)
Draw3D CREATE 1, bNv(0), bNf(0), 1, mV(), mFc(), mF(), col(), mEc()
cost = h0 - MM.INFO(HEAP)
Draw3D CLOSE 1
IF cost > 0 THEN
room = (h0 - HEAPKEEP) \ cost
IF room < maxObj THEN maxObj = room
ENDIF
IF maxObj < 1 THEN maxObj = 1
objCost = cost
FOR n = 0 TO 35 : objOwn(n) = -1 : NEXT n
END SUB
SUB CloseAll
LOCAL INTEGER n
FOR n = 1 TO maxObj
IF objOwn(n) >= 0 THEN Draw3D CLOSE n
objOwn(n) = -1
NEXT n
END SUB
SUB ReadKeys
IF demoMode THEN DemoFly : EXIT SUB
LOCAL INTEGER i, k, hnow, hnew
LOCAL kb$ LENGTH 2
kRollL = 0 : kRollR = 0 : kUp = 0 : kDn = 0
kFaster = 0 : kSlower = 0 : kFire = 0 : kQuit = 0
kView = -1 : kPause = 0
hnow = 0
kb$ = INKEY$
IF kb$ = CHR$(27) THEN kQuit = 1
IF kb$ = "p" OR kb$ = "P" THEN kPause = 1
FOR i = 1 TO 6
k = KEYDOWN(i)
SELECT CASE k
CASE 130, 44, 60   : kRollL = 1
CASE 131, 46, 62   : kRollR = 1
CASE 128, 88, 120  : kUp = 1
CASE 129, 83, 115  : kDn = 1
CASE 32            : kFaster = 1
CASE 47, 63        : kSlower = 1
CASE 65, 97        : kFire = 1
CASE 27            : kQuit = 1
CASE 145 TO 148    : kView = k - 145
CASE 84, 116       : hnow = hnow OR KB_TARGET
CASE 77, 109       : hnow = hnow OR KB_MISSILE
CASE 69, 101       : hnow = hnow OR KB_ECM
CASE 67, 99        : hnow = hnow OR KB_DOCK
CASE 72, 104       : hnow = hnow OR KB_JUMP
CASE 9             : hnow = hnow OR KB_BOMB
CASE 74, 106       : hnow = hnow OR KB_HOP
CASE 71, 103       : hnow = hnow OR KB_GAL
CASE 149 TO 154    : hnow = hnow OR (KB_SCREEN << (k - 149))
END SELECT
NEXT i
IF kView >= 0 THEN vw = kView
hnew = hnow AND (hnow XOR kHeld)
kHeld = hnow
kTarget = (hnew AND KB_TARGET) <> 0
kMissile = (hnew AND KB_MISSILE) <> 0
kECM = (hnew AND KB_ECM) <> 0
kDock = (hnew AND KB_DOCK) <> 0
kJump = (hnew AND KB_JUMP) <> 0
kBomb = (hnew AND KB_BOMB) <> 0
kHop = (hnew AND KB_HOP) <> 0
kGal = (hnew AND KB_GAL) <> 0
kChart = 0
IF (hnew AND KB_SCREENS) <> 0 THEN
FOR i = 0 TO 5
IF (hnew AND (KB_SCREEN << i)) <> 0 THEN kChart = i + 1 : EXIT FOR
NEXT i
ENDIF
END SUB
SUB UpdatePlayer
LOCAL INTEGER d
IF tickWhole THEN
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
IF kFaster THEN
IF dSpeed < MAXSPEED THEN dSpeed = dSpeed + 1
ENDIF
IF kSlower THEN dSpeed = dSpeed - 1
IF dSpeed < 1 THEN dSpeed = 1
ENDIF
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
SUB MoveShips
LOCAL INTEGER n, i, mag, dir, gone
LOCAL FLOAT k, aT, bT
aT = alpha * tick
bT = beta * tick
MATH Q_CREATE -aT, 0, 0, 1, qA()
MATH Q_CREATE bT, 1, 0, 0, qB()
MATH Q_MULT qB(), qA(), qP()
n = 0
DO WHILE n < nUsed
IF sTyp(n) = 0 THEN
n = n + 1
ELSE
IF sBp(n) >= 0 AND sSpd(n) <> 0 THEN
NoseVec n
sX(n) = sX(n) + qV(1) * qV(4) * sSpd(n) * NPCSPEED * tick
sY(n) = sY(n) + qV(2) * qV(4) * sSpd(n) * NPCSPEED * tick
sZ(n) = sZ(n) + qV(3) * qV(4) * sSpd(n) * NPCSPEED * tick
ENDIF
IF sAcc(n) <> 0 AND tickWhole THEN
mag = sSpd(n) + sAcc(n)
IF mag < 0 OR mag > 127 THEN mag = 0
IF mag > bSpd(sBp(n)) THEN mag = bSpd(sBp(n))
sSpd(n) = mag
sAcc(n) = 0
ENDIF
k = sY(n) - aT * sX(n)
sZ(n) = sZ(n) + bT * k
sY(n) = k - bT * sZ(n)
sX(n) = sX(n) + aT * sY(n)
sZ(n) = sZ(n) - dSpeed * tick
IF sBp(n) >= 0 THEN
MATH SLICE sQ(), , n, qA()
MATH Q_MULT qP(), qA(), qC()
mag = sPit(n) AND 127
IF mag <> 0 THEN
dir = 1
IF (sPit(n) AND 128) <> 0 THEN dir = -1
MATH Q_CREATE dir * SELFROT * tick, 1, 0, 0, qB()
qA() = qC()
MATH Q_MULT qA(), qB(), qC()
IF mag <> 127 AND tickWhole THEN sPit(n) = (mag - 1) OR (sPit(n) AND 128)
ENDIF
mag = sRol(n) AND 127
IF mag <> 0 THEN
dir = 1
IF (sRol(n) AND 128) <> 0 THEN dir = -1
MATH Q_CREATE dir * SELFROT * tick, 0, 0, 1, qB()
qA() = qC()
MATH Q_MULT qA(), qB(), qC()
IF mag <> 127 AND tickWhole THEN sRol(n) = (mag - 1) OR (sRol(n) AND 128)
ENDIF
MATH INSERT sQ(), , n, qC()
sQ(4, n) = 1
IF ((mcnt XOR n) AND (TIDYEVERY - 1)) = 0 THEN NormQuat n
ENDIF
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
SUB DrawFrame
LOCAL FLOAT t
IF PROFILE THEN
t = TIMER : CLS           : prof(0) = prof(0) + TIMER - t
t = TIMER : DrawStardust  : prof(1) = prof(1) + TIMER - t
t = TIMER : DrawPlanetSun : prof(2) = prof(2) + TIMER - t
t = TIMER : DrawShips : Explosions : SpaceFurniture : prof(3) = prof(3) + TIMER - t
t = TIMER : DrawDash      : prof(4) = prof(4) + TIMER - t
ViewName
DrawMessage
ELSE
CLS
DrawStardust
DrawPlanetSun
DrawShips
Explosions
SpaceFurniture
DrawDash
ViewName
DrawMessage
ENDIF
END SUB
SUB DrawShips
LOCAL INTEGER n, px, py, zb, c, near
FOR n = 0 TO nUsed - 1
IF sTyp(n) <> 0 AND sBp(n) >= 0 THEN
near = 0
ViewXform n
IF tz > NEARZ THEN
zb = tz \ ZHI
IF zb < VISCUT AND ABS(tx) < tz AND ABS(ty) < tz THEN
px = VCX + SGN(tx) * ((VPLANE * ABS(tx)) \ tz)
py = VCY - SGN(ty) * ((VPLANE * ABS(ty)) \ tz)
IF zb >= VISFLOOR AND zb > bVis(sBp(n)) THEN
c = col(shpCol(sTyp(n)))
IF py > 0 AND py < VIEWH - 2 THEN BOX px + 1, py, 3, 2, 0, c, c
ELSE
near = 1
IF sObj(n) = 0 THEN GetObject n
IF sObj(n) > 0 AND ABS(tx) < FARXY AND ABS(ty) < FARXY THEN
ViewOrient n
Draw3D ROTATE qC(), sObj(n)
Draw3D WRITE sObj(n), tx, ty, tz, 0, solidMode
ENDIF
ENDIF
IF (sFlg(n) AND 2) <> 0 THEN EnemyBeam n, px, py
ENDIF
ENDIF
IF near = 0 AND sObj(n) > 0 THEN
IF tz <= NEARZ THEN
DropObject n
ELSEIF (tz \ ZHI) > bVis(sBp(n)) + 4 THEN
DropObject n
ENDIF
ENDIF
sFlg(n) = sFlg(n) AND 253
ENDIF
NEXT n
END SUB
SUB EnemyBeam(n AS INTEGER, px AS INTEGER, py AS INTEGER)
LOCAL INTEGER ex, ey
IF tx > 0 THEN ex = 0 ELSE ex = SCRW - 1
ey = (sZ(n) AND 255) * VIEWH / 256
LINE px, py, ex, ey, 1, cRed
END SUB
SUB SpaceFurniture
IF lasFlash > 0 THEN
LINE 40, VIEWH - 2, VCX - 4 + RND * 8, VCY, 1, cRed
LINE SCRW - 40, VIEWH - 2, VCX - 4 + RND * 8, VCY, 1, cRed
ENDIF
LINE 0, 0, SCRW - 2, 0, 1, cWhite
BOX 0, 0, 2, VIEWH, 0, cWhite, cWhite
BOX SCRW - 2, 0, 2, VIEWH, 0, cWhite, cWhite
IF hypCount > 0 THEN TEXT 6, 4, STR$(hypCount), "LT", 7, 1, cWhite
IF vw = 0 THEN
LINE VCX - 25, VCY, VCX - 12, VCY, 1, cWhite
LINE VCX + 12, VCY, VCX + 25, VCY, 1, cWhite
LINE VCX, VCY - 20, VCX, VCY - 10, 1, cWhite
LINE VCX, VCY + 10, VCX, VCY + 20, 1, cWhite
ENDIF
END SUB
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
CIRCLE cx, cy, r, 1, 1, cGreen, cGreen
ELSE
CIRCLE cx, cy, r, 1, 1, cGreen, -1
IF r >= 6 THEN Surface n, cx, cy, r
ENDIF
ENDIF
ENDIF
ENDIF
NEXT n
END SUB
SUB Surface(n AS INTEGER, cx AS INTEGER, cy AS INTEGER, r AS INTEGER)
LOCAL INTEGER k
LOCAL FLOAT vnx, vny, vnz, vrx, vry, vrz, vsx, vsy, vsz, ox, oy, ax, ay, bx, by
MATH SLICE sQ(), , n, qA()
MATH Q_VECTOR 0, 0, 1, qB() : MATH Q_ROTATE qA(), qB(), qV()
vnx = qV(1) : vny = qV(2) : vnz = qV(3)
MATH Q_VECTOR 0, 1, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
vrx = qV(1) : vry = qV(2) : vrz = qV(3)
MATH Q_VECTOR 1, 0, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
vsx = qV(1) : vsy = qV(2) : vsz = qV(3)
IF sTyp(n) = T_CRATER THEN
IF vrz > 0 THEN EXIT SUB
ox = cx + 0.867 * r * vrx
oy = cy - 0.867 * r * vry
ax = 0.5 * r * vnx : ay = 0.5 * r * vny
bx = 0.5 * r * vsx : by = 0.5 * r * vsy
FOR k = 0 TO NSEG - 1
pgx(k) = ox + ax * ctab(k) + bx * stab(k)
pgy(k) = oy - (ay * ctab(k) + by * stab(k))
NEXT k
POLYGON NSEG, pgx(), pgy(), cGreen
ELSE
HalfCircle cx, cy, r, vnx, vny, vnz, vrx, vry, vrz
HalfCircle cx, cy, r, vsx, vsy, vsz, vrx, vry, vrz
ENDIF
END SUB
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
IF have THEN LINE lx, ly, px, py, 1, cGreen
lx = px : ly = py : have = 1
ELSE
have = 0
ENDIF
NEXT k
END SUB
SUB InitStardust
LOCAL INTEGER i
FOR i = 0 TO NSTAR - 1
stX(i) = SdSM(RND * 256)
stY(i) = SdSM(RND * 256)
stZ(i) = 1 + RND * 254
NEXT i
FOR i = 0 TO 4 * NSTAR - 1 : spc(i) = cWhite : NEXT i
END SUB
FUNCTION SdSM(b AS FLOAT) AS FLOAT
LOCAL INTEGER v
v = b
IF (v AND 128) <> 0 THEN SdSM = -(v AND 127) ELSE SdSM = (v AND 127)
END FUNCTION
SUB DrawStardust
LOCAL INTEGER i, zh, np, sx, sy, sy2, r
LOCAL FLOAT q, x, y, z, a, b, h, qb, d, dsg, ratsg, sp
a = alp2 * alp1 * tick
b = bet2 * bet1 * tick
sp = dSpeed * tick
np = 0
ARRAY SET -1, spx()
IF vw > 1 THEN
IF vw = 3 THEN
a = -a : b = -b : dsg = 1 : ratsg = -1
ELSE
dsg = -1 : ratsg = 1
ENDIF
ENDIF
FOR i = 0 TO NSTAR - 1
x = stX(i) : y = stY(i) : z = stZ(i)
IF vw = 0 THEN
zh = z
q = (INT(64 * sp / zh)) OR 1
z = z - sp / 4
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
zh = z
q = (INT(64 * sp / zh)) OR 1
x = x - FIX(x) * q / 256
y = y - FIX(y) * q / 256
z = z + sp / 4
y = y + a * FIX(x) / 256
x = x - a * FIX(y) / 256
h = FIX(y)
qb = -SGN(b) * SGN(h) * INT(ABS(b) * ABS(h) / 256)
x = x + 2 * qb * (-FIX(x)) / 256
y = y + b
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
zh = z
d = INT(zh / 8)
IF d < 1 THEN d = 1
x = x + dsg * sp / d
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
PIXEL spx(), spy(), spc()
END SUB
SUB NextTick
LOCAL FLOAT now
now = TIMER
tick = (now - tickPrev) * TICKRATE / 1000
IF tick > TICKMAX THEN tick = TICKMAX
IF tick < 0 THEN tick = 0
tickPrev = now
tickAcc = tickAcc + tick
tickWhole = 0
IF tickAcc >= 1 THEN
tickAcc = tickAcc - 1
tickWhole = 1
ENDIF
END SUB
SUB ResetTick
tickPrev = TIMER
tickAcc = 0
tick = 0
tickWhole = 0
END SUB
SUB LoadSounds
LOCAL INTEGER i
IF SOUNDON = 0 THEN EXIT SUB
PLAY BBC ENVELOPE 1, 1,  0, 111, -8,  4,  1,   8,  8, -2, 0,   -1, 112,  44
PLAY BBC ENVELOPE 2, 1, 14, -18, -1, 44, 32,  50,  6,  1, 0,   -2, 120, 126
PLAY BBC ENVELOPE 3, 1,  1,  -1, -3, 17, 32, 128,  1,  0, 0,   -1,   1,   1
PLAY BBC ENVELOPE 4, 1,  4,  -8, 44,  4,  6,   8, 22,  0, 0, -127, 126,   0
RESTORE dat_sfx
FOR i = 0 TO NSFX - 1
READ sfxCh(i), sfxAmp(i), sfxPit(i), sfxDur(i)
NEXT i
END SUB
SUB Sfx(n AS INTEGER)
IF SOUNDON = 0 THEN EXIT SUB
PLAY BBC SOUND sfxCh(n), sfxAmp(n), sfxPit(n), sfxDur(n)
END SUB
SUB SoundOff
PLAY STOP
END SUB
dat_sfx:
DATA      18,    1,     0,       16
DATA      18,    2,    44,        8
DATA      16,  -15,     7,       26
DATA      17,    3,   240,       24
DATA       3,  -15,   188,        1
DATA      19,  -12,    12,        8
DATA      16,  -15,     6,       12
DATA      16,    2,    96,       16
DATA      19,    4,   194,      255
DATA      19,    0,     0,        0
SUB Message(t$)
msgText$ = t$
msgUntil = TIMER + MSGTIME
END SUB
SUB DrawMessage
IF msgText$ = "" THEN EXIT SUB
IF TIMER > msgUntil THEN
msgText$ = ""
EXIT SUB
ENDIF
TEXT MSGX, MSGY, msgText$, "LT", 7, 1, cWhite
END SUB
SUB EnergyWarning
IF (mcnt AND 31) <> 10 THEN EXIT SUB
IF pEnergy >= 50 THEN EXIT SUB
Message "ENERGY LOW"
Sfx SFX_BEEP
END SUB
SUB DashStatic
STATIC INTEGER addr = 0, fadd = 0
IF fadd = 0 THEN
LOCAL INTEGER i
addr = PEEK(VARADDR dashStore())
fadd = MM.INFO(WRITEBUFF) + DASHY * SCRW / 2
BOX 0, DASHY, SCRW, SCRH - DASHY, 0, cBlack, cBlack
LINE 0, DASHY, SCRW - 1, DASHY, 1, cCyan
FOR i = 0 TO 5
TEXT 17, DLY(i) - 1, LLAB$(i), "RT", 7, 1, cWhite
BOX DL, DLY(i) - 1, DW + 2, 5, 1, cDim, -1
NEXT i
FOR i = 0 TO 6
TEXT 303, DRY(i) - 1, RLAB$(i), "LT", 7, 1, cWhite
BOX DR, DRY(i) - 1, DW + 2, 5, 1, cDim, -1
NEXT i
CIRCLE CPX, CPY, CPR + 2, 1, 1.25, cDim, -1
MEMORY COPY INTEGER fadd, addr, DASHWORDS
ELSE
MEMORY COPY INTEGER addr, fadd, DASHWORDS
ENDIF
END SUB
SUB DrawDash
LOCAL INTEGER i, e
DashStatic
Bar DR, DRY(0), dSpeed \ 2, 14, cRed, cYellow
Pointer DR, DRY(1), 8 + alp2 * (alp1 \ 4)
Pointer DR, DRY(2), 8 + bet2 * bet1
FOR i = 0 TO 3
e = (pEnergy \ 4) - (3 - i) * 16
IF e < 0 THEN e = 0
IF e > 16 THEN e = 16
Bar DR, DRY(3 + i), e, 3, cYellow, cRed
NEXT i
Bar DL, DLY(0), pFsh \ 16, 3, cYellow, cRed
Bar DL, DLY(1), pAsh \ 16, 3, cYellow, cRed
Bar DL, DLY(2), pFuel \ 4, 3, cYellow, cRed
Bar DL, DLY(3), pCabT \ 16, 11, cRed, cYellow
Bar DL, DLY(4), pLasT \ 16, 11, cRed, cYellow
Bar DL, DLY(5), pAltit \ 16, 99, cRed, cYellow
MissileBlocks
DrawScanner
DrawCompass
Bulbs
END SUB
SUB Bulbs
BOX BULBX, BULBY, 9, 7, 0, cBlack, cBlack
BOX BULBX + 13, BULBY, 9, 7, 0, cBlack, cBlack
IF ecmActive THEN TEXT BULBX + 2, BULBY, "E", "LT", 7, 1, cYellow
IF inSafe THEN TEXT BULBX + 15, BULBY, "S", "LT", 7, 1, cGreen
END SUB
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
SUB Pointer(x AS INTEGER, y AS INTEGER, p AS INTEGER)
LOCAL INTEGER v
v = p
IF v < 0 THEN v = 0
IF v > 15 THEN v = 15
BOX x + 1, y, DW, 3, 0, cBlack, cBlack
BOX x + 1 + v * 2.5, y, 3, 3, 0, cYellow, cYellow
END SUB
SUB MissileBlocks
LOCAL INTEGER i, c
FOR i = 0 TO 3
c = cBlack
IF i < pMissl THEN c = cGreen
BOX 20 + i * 10, 225, 7, 5, 0, c, c
NEXT i
END SUB
SUB DrawScanner
LOCAL INTEGER n, px, py, base, c
BOX SCX - SCA, SCY - SCB - 9, 2 * SCA, 2 * SCB + 18, 0, cBlack, cBlack
CIRCLE SCX, SCY, SCB, 1, SCA / SCB, cCyan, -1
FOR n = 0 TO nUsed - 1
IF sTyp(n) <> 0 AND sBp(n) >= 0 THEN
IF ABS(sX(n)) < 16384 AND ABS(sY(n)) < 16384 AND ABS(sZ(n)) < 16384 THEN
px = SCDOTX + sX(n) / SCXDIV
base = SCY - sZ(n) / SCZDIV
py = base - sY(n) / SCYDIV
IF py < SCTOP THEN py = SCTOP
IF py > SCBOT THEN py = SCBOT
IF px > SCX - SCA AND px < SCX + SCA THEN
c = scaCol(sTyp(n))
LINE px, base, px, py, 1, c
BOX px, py - 1, 5, 2, 0, c, c
ENDIF
ENDIF
ENDIF
NEXT n
END SUB
SUB DrawCompass
LOCAL INTEGER n, px, py
LOCAL FLOAT m
n = SLOT_PLANET
IF inSafe AND sTyp(SLOT_STAR) = T_STATION THEN n = SLOT_STAR
IF sTyp(n) = 0 THEN EXIT SUB
m = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
IF m < 1 THEN EXIT SUB
BOX CPX - CPR - 4, CPY - CPR - 3, 2 * CPR + 9, 2 * CPR + 7, 0, cBlack, cBlack
CIRCLE CPX, CPY, CPR + 2, 1, 1.25, cDim, -1
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
TEXT 110, 8, v$, "LT", 7, 1, cWhite
END SUB
SUB NewCommander
LOCAL INTEGER i
pRoll = JCENTRE : pPitch = JCENTRE
pEnergy = 255 : pFsh = 255 : pAsh = 255 : pFuel = 70
pCabT = 30 : pLasT = 0 : pAltit = 200 : pMissl = 3
cashTenths = 1000 : holdSize = 20
lasView(0) = LAS_PULSE : lasView(1) = 0 : lasView(2) = 0 : lasView(3) = 0
lasTimer = 0 : lasFlash = 0
kills = 0 : dead = 0 : energyUnit = 0 : legal = 0 : mission = 0
shots = 0 : hits = 0
docked = 0 : dockComp = 0 : msLock = -1 : hypCount = 0
vw = 0 : inWitch = 0
FOR i = 0 TO NEQUIP - 1 : eqOwned(i) = 0 : NEXT i
FOR i = 0 TO NGOODS - 1 : cargo(i) = 0 : NEXT i
InitStardust
LoadMarket
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
END SUB
SUB TestScene
LOCAL INTEGER n
NewCommander
LaunchState
MATH Q_EULER RAD(180), 0, 0, qA() : qA(4) = 1
n = NewShip(T_COBRA3, 0, 0, 9000, qA())
IF n >= 0 THEN sSpd(n) = 12 : sAI(n) = 0
MATH Q_EULER RAD(-70), RAD(10), 0, qA() : qA(4) = 1
n = NewShip(T_VIPER, -1200, -300, 5000, qA())
IF n >= 0 THEN sSpd(n) = 20 : sAI(n) = 128 OR (24 * 2)
MATH Q_EULER RAD(30), RAD(20), RAD(10), qA() : qA(4) = 1
n = NewShip(T_ASTEROID, 0, 0, 4000, qA())
IF n >= 0 THEN sPit(n) = 127
END SUB
SUB DemoInput(f AS INTEGER)
kRollL = 0 : kRollR = 0 : kUp = 0 : kDn = 0
kFaster = 0 : kSlower = 0 : kFire = 0 : kQuit = 0
kTarget = 0 : kMissile = 0 : kECM = 0
kJump = 0 : kChart = 0
SELECT CASE f
CASE 0 TO 9     : kFaster = 1
CASE 20 TO 120  : kFire = 1
CASE 130        : kTarget = 1
CASE 132        : kMissile = 1
CASE 200        : kECM = 1
CASE 160 TO 179 : vw = 1
CASE 180 TO 199 : vw = 3
CASE 200 TO 209 : vw = 0
END SELECT
IF f = 250 THEN
Hyperspace selSys
ENDIF
SELECT CASE f
CASE 260 TO 999 : kFaster = 1
END SELECT
END SUB
SUB MissionScene
NewCommander
MissionFind 2, 144, 33
MissionFind 3, 215, 84
MissionFind 3, 63, 72
PRINT
PRINT "mission byte at the start:"; mission
kills = 100 : gGal = 1
MissionCheck
PRINT "after docking with 100 kills:"; mission
kills = 300
MissionCheck
PRINT "after docking with 300 kills:"; mission; " (expect 1)"
gGal = 2 : conHere = 1
PRINT "in its system, want one?"; WantConstrictor(); " (expect 1)"
conHere = 0
PRINT "somewhere else, want one?"; WantConstrictor(); " (expect 0)"
mission = mission OR MI_1DONE
gGal = 2
MissionCheck
PRINT "after killing it and docking:"; mission; " (expect 2)"
PRINT "  kills"; kills; " (expect 556)  cash"; cashTenths / 10; " Cr"
gGal = 3
MissionCheck
PRINT "third galaxy, 556 kills:"; mission; " (expect 2)"
kills = 1300
MissionCheck
PRINT "third galaxy, 1300 kills:"; mission; " (expect 6)"
MissionGoto 3, 215, 84
MissionCheck
PRINT "docked at Ceerdi:"; mission; " (expect 10)"
PRINT "  carrying the plans?"; CarryingPlans(); " (expect 1)"
MissionGoto 3, 63, 72
MissionCheck
PRINT "docked at Birera:"; mission; " (expect 14)"
PRINT "  carrying the plans?"; CarryingPlans(); " (expect 0)"
PRINT "  energy unit"; energyUnit; " (expect 2, the navy's own)"
END SUB
SUB MissionGoto(g AS INTEGER, x AS INTEGER, y AS INTEGER)
LOCAL INTEGER i
gGal = g
SetGalaxy g
FOR i = 0 TO 255
SysData
IF sysX = x AND sysYr = y THEN EXIT FOR
NextSystem
NEXT i
homeSys = i : selSys = i
homeX = sysX : homeY = sysY * 2
END SUB
SUB MissionFind(g AS INTEGER, x AS INTEGER, y AS INTEGER)
LOCAL INTEGER i, found
found = -1
SetGalaxy g
FOR i = 0 TO 255
SysData
IF sysX = x AND sysYr = y THEN found = i : EXIT FOR
NextSystem
NEXT i
IF found < 0 THEN
PRINT "galaxy"; g; " ("; STR$(x); ","; STR$(y); ") - nothing there"
ELSE
PRINT "galaxy"; g; " ("; STR$(x); ","; STR$(y); ") is "; SysName$()
ENDIF
SetGalaxy gGal
END SUB
SUB NewbScene
LOCAL INTEGER n
NewCommander
LaunchState
PRINT "ship                flags   leaves us alone, clean / fugitive"
NewbOne T_TRADER, "Cobra III trader"
NewbOne T_COBRA3, "Cobra III pirate"
NewbOne T_ANACONDA, "Anaconda"
NewbOne T_VIPER, "Viper"
NewbOne T_SIDEWINDER, "Sidewinder"
NewbOne T_WORM, "Worm"
NewbOne T_HERMIT, "Rock hermit"
NewbOne T_THARGOID, "Thargoid"
NewbOne T_CONSTRICT, "Constrictor"
PRINT
ClearSlots
LaunchState
PRINT "station AI before"; sAI(SLOT_STAR); " legal"; legal
MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
n = NewShip(T_TRADER, 0, 0, 4000, qA())
IF n >= 0 THEN sAI(n) = 128 OR 56
Angry n
PRINT "after shooting a trader: station AI"; sAI(SLOT_STAR); " legal"; legal;
PRINT " the trader is now hostile?"; (sNewb(n) AND NB_HOSTILE) <> 0
ClearSlots
LaunchState
legal = 0
MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
n = NewShip(T_SIDEWINDER, 0, 0, 4000, qA())
IF n >= 0 THEN sAI(n) = 128 OR 56
Angry n
PRINT "after shooting a pirate:  station AI"; sAI(SLOT_STAR); " legal"; legal
PRINT
PRINT "who carries an escape pod:"
PRINT "  Krait"; (tNewb(T_KRAIT) AND NB_POD) <> 0;
PRINT "  Gecko"; (tNewb(T_GECKO) AND NB_POD) <> 0;
PRINT "  Thargoid"; (tNewb(T_THARGOID) AND NB_POD) <> 0
END SUB
SUB NewbOne(t AS INTEGER, nm$)
LOCAL INTEGER n, i, c0, c1, keep
MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
n = NewShip(t, 0, 0, 4000, qA())
IF n < 0 THEN PRINT nm$; " - no slot" : EXIT SUB
sAI(n) = 128 OR 56
keep = sNewb(n)
legal = 0
c0 = 0
FOR i = 1 TO 100
sNewb(n) = keep
IF Peaceful(n) THEN c0 = c0 + 1
NEXT i
legal = 60
c1 = 0
FOR i = 1 TO 100
sNewb(n) = keep
IF Peaceful(n) THEN c1 = c1 + 1
NEXT i
PRINT nm$ + SPACE$(20 - LEN(nm$)); keep; SPACE$(6); c0; "%"; SPACE$(4); c1; "%"
legal = 0
KillShip n
END SUB
SUB DockNPCScene
NewCommander
DockTrial 180
DockTrial 140
DockTrial 100
DockTrial 60
END SUB
SUB DockTrial(hdg AS INTEGER)
LOCAL INTEGER n, f, gone, k0, aligned, slow
ClearSlots
LaunchState
MATH Q_EULER RAD(180), 0, 0, qA() : qA(4) = 1
KillShip SLOT_STAR
n = NewShip(T_STATION, 0, 0, 8000, qA())
IF n >= 0 THEN sRol(n) = 255 : sAI(n) = 1
dSpeed = 0
newbFlags = NB_DOCKING
n = NewFacing(T_TRADER, 600, -400, 2000, hdg)
IF n < 0 THEN PRINT "no slot for the trader" : EXIT SUB
sSpd(n) = 20
sAI(n) = 128 OR 64
k0 = kills
gone = 0
aligned = 0
slow = 0
tick = 1
tickWhole = 1
FOR f = 1 TO 4000
MoveShips
Tactics
mcnt = (mcnt + 1) AND 255
IF sTyp(n) <> T_TRADER THEN gone = f : EXIT FOR
IF ShipRange(n) < DOCKAPPR THEN
aligned = ABS(SlotAlign(n)) >= DOCKALIGN
IF aligned = 0 THEN slow = slow + 1
ENDIF
NEXT f
IF gone THEN
PRINT "heading"; hdg; ": docked at frame"; gone; ", waited"; slow;
PRINT " frames for the slot, lined up"; aligned; ", kills"; kills - k0
ELSE
PRINT "heading"; hdg; ": still out there, range"; ShipRange(n); " lined up"; aligned
ENDIF
END SUB
FUNCTION ShipRange(n AS INTEGER) AS INTEGER
LOCAL FLOAT dx, dy, dz
dx = sX(n) - sX(SLOT_STAR)
dy = sY(n) - sY(SLOT_STAR)
dz = sZ(n) - sZ(SLOT_STAR)
ShipRange = SQR(dx*dx + dy*dy + dz*dz)
END FUNCTION
SUB LeftScene
LOCAL INTEGER i, n, f, lost, tgt
NewCommander
LaunchState
inSafe = 0
tgt = -1
SetGalaxy gGal
FOR i = 0 TO 255
SysData
IF i <> homeSys THEN
IF CanReach(i) THEN tgt = i : EXIT FOR
ENDIF
NextSystem
NEXT i
GotoSystem gGal, homeSys
SysData
selSys = tgt
PRINT "jumping to system"; tgt
JumpAway
PRINT "the key starts it at"; hypCount; " (expect 15)"
JumpAway
PRINT "a second press leaves it at"; hypCount; " (expect 15)"
f = 0
DO
HyperCount
f = f + 1
LOOP UNTIL hypCount = 0 OR f > 300
PRINT "jumped after"; f; " iterations (expect 85)"
PRINT "  which at"; TICKRATE; "a second is"; STR$(f / TICKRATE, 3, 1); " seconds"
PRINT "  home system is now"; homeSys; " (expect"; tgt; ")"
PRINT
NewCommander
LaunchState
MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
n = NewShip(T_VIPER, 0, 0, 4000, qA())
pFsh = 255 : pAsh = 255
HitPlayer 40, n
PRINT "hit from in front: fore"; pFsh; " aft"; pAsh; " (expect 215, 255)"
sZ(n) = -4000
pFsh = 255 : pAsh = 255
HitPlayer 40, n
PRINT "hit from behind:   fore"; pFsh; " aft"; pAsh; " (expect 255, 215)"
pFsh = 255 : pAsh = 255
HitPlayer 40, -1
PRINT "hit from nowhere:  fore"; pFsh; " aft"; pAsh; " (expect 215, 255)"
KillShip n
PRINT
NewCommander
FOR i = 0 TO NEQUIP - 1 : eqOwned(i) = 1 : NEXT i
FOR i = 0 TO NGOODS - 1 : cargo(i) = 5 : NEXT i
energyUnit = 1
lost = 0
FOR n = 1 TO 5000
msgText$ = ""
Ouch
IF msgText$ <> "" THEN lost = lost + 1
FOR i = 0 TO NEQUIP - 1 : eqOwned(i) = 1 : NEXT i
FOR i = 0 TO NGOODS - 1 : cargo(i) = 5 : NEXT i
NEXT n
PRINT "5000 hits destroyed"; lost; " things (expect about 215, one in 23)"
FOR i = 0 TO NEQUIP - 1 : eqOwned(i) = 1 : NEXT i
FOR i = 0 TO NGOODS - 1 : cargo(i) = 5 : NEXT i
energyUnit = 1
FOR n = 1 TO 20000
msgText$ = ""
Ouch
NEXT n
PRINT "after 20000 hits, still fitted:"
FOR i = 0 TO NEQUIP - 1
IF eqOwned(i) THEN PRINT "  "; eqName$(i)
NEXT i
PRINT "cargo left"; HoldUsed(); " (expect 0)   energy unit"; energyUnit; " (expect 0)"
END SUB
SUB SaveShot(f AS INTEGER)
SAVE IMAGE "A:/fly" + STR$(f) + ".bmp"
END SUB
SUB DumpSlots
LOCAL INTEGER n
PRINT "slots at frame 60, nUsed"; nUsed; " fore laser"; lasView(0)
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
SUB DockScene
LOCAL INTEGER n
pRoll = JCENTRE : pPitch = JCENTRE
pEnergy = 255 : pFsh = 255 : pAsh = 255 : pFuel = 70
pCabT = 30 : pLasT = 0 : pAltit = 200 : pMissl = 3
cashTenths = 1000 : holdSize = 20
lasView(0) = LAS_PULSE : kills = 0 : dead = 0 : docked = 0
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
StationBlueprint
MATH Q_EULER RAD(35), RAD(40), 0, qA() : qA(4) = 1
n = NewShip(T_CRATER, 0, -20000, 3 * UNIT, qA())
IF n >= 0 THEN sRol(n) = 127
MATH Q_EULER RAD(180), 0, 0, qA() : qA(4) = 1
n = NewShip(T_STATION, 0, 0, 7000, qA())
IF n >= 0 THEN sRol(n) = 255 : sAI(n) = 1
dSpeed = 0
inSafe = 1
mcnt = 0
eqOwned(EQ_DOCK) = 1 : dockComp = 1
END SUB
SUB DockInput(f AS INTEGER)
kRollL = 0 : kRollR = 0 : kUp = 0 : kDn = 0
kFaster = 0 : kSlower = 0 : kFire = 0 : kQuit = 0
kTarget = 0 : kMissile = 0 : kECM = 0 : kDock = 0
kJump = 0 : kChart = 0
END SUB
SUB DockedScreens
LOCAL INTEGER n, i
pFuel = 44 : cashTenths = 1000 : holdSize = 20
pEnergy = 255 : pFsh = 255 : pAsh = 255 : pMissl = 3
lasView(0) = LAS_PULSE : kills = 20 : legal = 0 : docked = 1 : energyUnit = 0
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
FOR i = 1 TO 5 : BuyOne 0 : NEXT i
FOR i = 1 TO 3 : BuyOne 12 : NEXT i
eqOwned(EQ_ECM) = 1
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
SUB GalTwist
LOCAL INTEGER t
t = (gs0 + gs1) AND &HFFFF
gs0 = gs1
gs1 = gs2
gs2 = (t + gs1) AND &HFFFF
END SUB
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
SUB NextSystem
GalTwist : GalTwist : GalTwist : GalTwist
gSys = (gSys + 1) AND 255
END SUB
SUB GotoSystem(g AS INTEGER, n AS INTEGER)
LOCAL INTEGER i
SetGalaxy g
FOR i = 1 TO n
NextSystem
NEXT i
END SUB
SUB SysData
LOCAL INTEGER h0, h1, h2, l1
h0 = (gs0 >> 8) AND 255
h1 = (gs1 >> 8) AND 255
h2 = (gs2 >> 8) AND 255
l1 = gs1 AND 255
sysX = h1
sysY = h0 >> 1
sysYr = h0
sysGov = (l1 >> 3) AND 7
sysEco = h0 AND 7
IF sysGov <= 1 THEN sysEco = sysEco OR 2
sysTech = (sysEco XOR 7) + (h1 AND 3) + ((sysGov + 1) >> 1)
sysPop = sysTech * 4 + sysEco + sysGov + 1
sysProd = ((sysEco XOR 7) + 3) * (sysGov + 4) * sysPop * 8
sysRad = ((h2 AND 15) + 11) * 256 + h1
END SUB
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
SUB LoadMarket
LOCAL INTEGER i
RESTORE dat_market
FOR i = 0 TO NGOODS - 1
READ mkName$(i), mkBase(i), mkFact(i), mkUnit$(i), mkQty(i), mkMask(i)
NEXT i
END SUB
SUB MakeMarket(eco AS INTEGER, mb AS INTEGER)
LOCAL INTEGER i, q
FOR i = 0 TO NGOODS - 1
mkPrice(i) = ((mkBase(i) + (mb AND mkMask(i)) + eco * mkFact(i)) AND 255) * 4
q = mkQty(i) + (mb AND mkMask(i)) - eco * mkFact(i)
IF q < 0 THEN q = 0
mkStock(i) = q AND 63
NEXT i
END SUB
FUNCTION PriceStr$(p AS INTEGER)
PriceStr$ = STR$(p \ 10) + "." + STR$(p MOD 10)
END FUNCTION
dat_market:
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
FUNCTION SysDist(x0 AS INTEGER, y0 AS INTEGER, x1 AS INTEGER, y1 AS INTEGER) AS INTEGER
LOCAL INTEGER dx, dy
dx = ABS(x1 - x0)
dy = ABS(y1 - y0) \ 2
SysDist = 4 * INT(SQR(dx * dx + dy * dy))
END FUNCTION
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
SUB FindByName
LOCAL INTEGER i, found
LOCAL nm$ LENGTH 20
nm$ = UCASE$(AskText$("Find system: ", 10))
IF nm$ = "" THEN EXIT SUB
found = -1
SetGalaxy gGal
FOR i = 0 TO 255
SysData
IF SysName$() = nm$ THEN found = i : EXIT FOR
NextSystem
NEXT i
IF found < 0 THEN
GotoSystem gGal, selSys
SysData
Sfx SFX_BOOP
EXIT SUB
ENDIF
GotoSystem gGal, found
SysData
selSys = found
curX = sysX
curY = sysY * 2
Sfx SFX_BEEP
END SUB
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
r = pFuel \ 4
CIRCLE curX * 1.25, curY \ 2 + CHTOP, r, 1, 1.25, cGreen, -1
Crosshair curX * 1.25, curY \ 2 + CHTOP, 7
GotoSystem gGal, selSys
SysData
nm$ = SysName$()
TEXT 4, CHTOP + 134, nm$ + "   " + STR$(SysDist(homeX, homeY, sysX, sysY * 2) / 10) + " LY", "LT", 7, 1, cYellow
END SUB
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
y = DrawDesc(y + 6, 36)
END SUB
SUB DataLine(y AS INTEGER, lb$, v$)
TEXT 20, y, lb$ + ":", "LT", 7, 1, cWhite
TEXT 150, y, v$, "LT", 7, 1, cYellow
END SUB
SUB ArriveInSystem
LOCAL INTEGER n, pz, sz, sx, ptype
ClearSlots
SysData
StationBlueprint
MissionHere
legal = legal \ 2
spawnEV = 0
pz = (((gs0 >> 8) AND 7) + 6) \ 2
IF pz < 3 THEN pz = 3
ptype = T_PLANET
IF (sysTech AND 2) <> 0 THEN ptype = T_CRATER
MATH Q_EULER RAD(35), RAD(40), 0, qA() : qA(4) = 1
n = NewShip(ptype, 0, 0, pz * UNIT, qA())
IF n >= 0 THEN sRol(n) = 127 : sPit(n) = 127
sz = ((((gs2 >> 8) AND 7) OR 1))
sx = ((gs2 >> 8) AND 3) * UNIT
MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
n = NewShip(T_SUN, sx, 0, -sz * UNIT, qA())
dSpeed = 0
inSafe = 0
mcnt = 0
END SUB
SUB LaunchState
LOCAL INTEGER n, ptype
ClearSlots
SysData
StationBlueprint
MissionHere
ptype = T_PLANET
IF (sysTech AND 2) <> 0 THEN ptype = T_CRATER
MATH Q_EULER RAD(35), RAD(40), 0, qA() : qA(4) = 1
n = NewShip(ptype, 0, 0, UNIT, qA())
IF n >= 0 THEN sRol(n) = 127 : sPit(n) = 127
MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
n = NewShip(T_STATION, 0, 0, -256, qA())
IF n >= 0 THEN
sRol(n) = 255
sPit(n) = 0
sAI(n) = 1
ENDIF
dSpeed = LAUNCHSPD
inSafe = 1
mcnt = 0
legal = legal OR Contraband()
IF legal > 255 THEN legal = 255
END SUB
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
SUB Hyperspace(target AS INTEGER)
LOCAL INTEGER d, hx, hy
hx = homeX : hy = homeY
GotoSystem gGal, target
SysData
d = SysDist(hx, hy, sysX, sysY * 2)
IF d > pFuel THEN Sfx SFX_BOOP : EXIT SUB
Sfx SFX_HYPER
HyperTunnel
ResetTick
pFuel = pFuel - d
homeSys = target
homeX = sysX
homeY = sysY * 2
curX = homeX : curY = homeY
selSys = target
mkByte = INT(RND * 256)
MakeMarket sysEco, mkByte
IF RND < 0.004 THEN
Witchspace
ELSE
ArriveInSystem
ENDIF
END SUB
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
SUB MissionHere
conHere = 0
IF gGal = 2 AND sysX = 144 AND sysYr = 33 THEN conHere = 1
END SUB
SUB StationBlueprint
IF sysTech >= 10 THEN
tBp(T_STATION) = BP_DODO
ELSE
tBp(T_STATION) = BP_CORIOLIS
ENDIF
END SUB
SUB StationCheck
LOCAL INTEGER n, px, py, pz
IF inWitch THEN EXIT SUB
IF sTyp(SLOT_PLANET) = 0 THEN EXIT SUB
IF (mcnt AND 31) <> 0 THEN EXIT SUB
IF sTyp(SLOT_STAR) = T_STATION THEN EXIT SUB
px = sX(SLOT_PLANET)
py = sY(SLOT_PLANET)
pz = sZ(SLOT_PLANET) - 2 * PRADIUS
inSafe = 0
IF ABS(px) < SAFEZONE AND ABS(py) < SAFEZONE AND ABS(pz) < SAFEZONE THEN
inSafe = 1
IF sTyp(SLOT_STAR) <> 0 THEN KillShip SLOT_STAR
MATH Q_EULER RAD(180), 0, 0, qA() : qA(4) = 1
n = NewShip(T_STATION, px, py, pz, qA())
IF n >= 0 THEN sRol(n) = 255 : sAI(n) = 1
ENDIF
END SUB
SUB LoadTokens
LOCAL INTEGER i, j, k
RESTORE dat_tokens
FOR i = 0 TO 255 : READ tk$(i) : NEXT i
RESTORE dat_digrams
FOR i = 0 TO 31 : READ dg$(i) : NEXT i
RESTORE dat_rndgroups
FOR i = 0 TO 37 : READ rgBase(i) : NEXT i
RESTORE dat_longtok
k = 0
FOR i = 0 TO NLONG - 1
READ ltNo(i), ltCnt(i)
ltFirst(i) = k
FOR j = 1 TO ltCnt(i)
READ ltPart$(k)
k = k + 1
NEXT j
NEXT i
RESTORE dat_stdtok
FOR i = 0 TO NSTD - 1 : READ stdNo(i), stdTx$(i) : NEXT i
END SUB
FUNCTION TokPart$(tok AS INTEGER, part AS INTEGER)
LOCAL INTEGER i
FOR i = 0 TO NLONG - 1
IF ltNo(i) = tok THEN
IF part < ltCnt(i) THEN TokPart$ = ltPart$(ltFirst(i) + part) ELSE TokPart$ = ""
EXIT FUNCTION
ENDIF
NEXT i
IF part = 0 THEN TokPart$ = tk$(tok) ELSE TokPart$ = ""
END FUNCTION
FUNCTION Dornd() AS INTEGER
LOCAL INTEGER a, x, c
a = rndS(0) * 2
c = (a >> 8) AND 1
a = a AND 255
x = a
a = a + rndS(2) + c
c = (a >> 8) AND 1
rndS(0) = a AND 255
rndS(2) = x
a = rndS(1)
x = a
a = a + rndS(3) + c
rndS(1) = a AND 255
rndS(3) = x
Dornd = rndS(1)
END FUNCTION
FUNCTION RndPick() AS INTEGER
LOCAL INTEGER r, n
r = Dornd()
n = 0
IF r >= 51 THEN n = n + 1
IF r >= 102 THEN n = n + 1
IF r >= 153 THEN n = n + 1
IF r >= 204 THEN n = n + 1
RndPick = n
END FUNCTION
SUB PutCh(c$)
LOCAL ch$ LENGTH 1
ch$ = c$
IF ch$ = CHR$(96) THEN ch$ = CHR$(39)
IF ch$ >= "A" AND ch$ <= "Z" THEN
IF dtCase = DT_LOWER THEN
ch$ = LCASE$(ch$)
IF dtCapNext THEN ch$ = UCASE$(ch$)
ELSEIF dtInWord THEN
IF dtCase = DT_SENT THEN ch$ = LCASE$(ch$)
ELSE
IF dtCapNext THEN ch$ = UCASE$(ch$)
ENDIF
dtCapNext = 0
dtInWord = 1
ELSEIF ch$ = " " THEN
dtInWord = 0
ENDIF
IF dtSink THEN BriefPut ch$ : EXIT SUB
IF ch$ = " " THEN
IF LEN(descBuf$) = 0 THEN EXIT SUB
IF RIGHT$(descBuf$, 1) = " " THEN EXIT SUB
ENDIF
IF LEN(descBuf$) < 250 THEN descBuf$ = descBuf$ + ch$
END SUB
SUB PutName(s$)
LOCAL INTEGER i
IF dtSink THEN
FOR i = 1 TO LEN(s$) : BriefPut MID$(s$, i, 1) : NEXT i
dtCapNext = 0
EXIT SUB
ENDIF
IF LEN(descBuf$) + LEN(s$) < 250 THEN descBuf$ = descBuf$ + s$
dtCapNext = 0
END SUB
SUB PutStr(s$)
LOCAL INTEGER i
FOR i = 1 TO LEN(s$) : PutCh MID$(s$, i, 1) : NEXT i
END SUB
FUNCTION NameCap$()
LOCAL nm$ LENGTH 10
nm$ = SysName$()
IF LEN(nm$) = 0 THEN NameCap$ = "" : EXIT FUNCTION
NameCap$ = LEFT$(nm$, 1) + LCASE$(MID$(nm$, 2))
END FUNCTION
FUNCTION NameAdj$()
LOCAL nm$ LENGTH 12
LOCAL last$ LENGTH 1
nm$ = NameCap$()
IF LEN(nm$) = 0 THEN NameAdj$ = "" : EXIT FUNCTION
last$ = UCASE$(RIGHT$(nm$, 1))
IF INSTR("AEIOU", last$) > 0 THEN nm$ = LEFT$(nm$, LEN(nm$) - 1)
NameAdj$ = nm$ + "ian"
END FUNCTION
SUB PutAlien
LOCAL INTEGER n, i, k
dtCapNext = 1
n = Dornd() AND 3
FOR i = 0 TO n
k = (Dornd() AND 62) \ 2
PutStr dg$(k)
NEXT i
END SUB
FUNCTION DoControl(n AS INTEGER) AS INTEGER
DoControl = -1
SELECT CASE n
CASE 1  : dtCase = DT_CAPS
CASE 2  : dtCase = DT_SENT
CASE 3  : PutName NameCap$()
CASE 4  : PutName CMDRNAME$
CASE 5  : dtStd = 0
CASE 6  : dtStd = 1 : dtCase = DT_SENT
CASE 8  : IF dtSink THEN BriefTab 6
CASE 9  : IF dtSink THEN BriefPage
CASE 12 : IF dtSink THEN BriefBreak ELSE PutCh " "
CASE 13 : dtCase = DT_LOWER
CASE 17 : PutName NameAdj$()
CASE 18 : PutAlien
CASE 19 : dtCapNext = 1
CASE 22 : IF dtSink THEN BriefShip
CASE 23 : IF dtSink THEN BriefRow 10
dtCase = DT_LOWER
CASE 24 : IF dtSink THEN BriefWait
CASE 25 : IF dtSink THEN BriefIncoming
CASE 27 : DoControl = 217 + gGal - 1
CASE 28 : DoControl = 220 + gGal - 1
CASE 29 : IF dtSink THEN BriefTab 6
dtCase = DT_LOWER
CASE ELSE
END SELECT
END FUNCTION
SUB ExpandTok(start AS INTEGER)
LOCAL INTEGER sp, v, j, isRnd, p, k
LOCAL t$ LENGTH 160
LOCAL c$ LENGTH 1
LOCAL m$ LENGTH 8
sp = 0
exTok(0) = start
exPos(0) = 1
exPart(0) = 0
DO
t$ = TokPart$(exTok(sp), exPart(sp))
p = exPos(sp)
IF p > LEN(t$) THEN
IF TokPart$(exTok(sp), exPart(sp) + 1) <> "" THEN
exPart(sp) = exPart(sp) + 1
exPos(sp) = 1
ELSE
sp = sp - 1
IF sp < 0 THEN EXIT DO
ENDIF
ELSE
c$ = MID$(t$, p, 1)
IF c$ = "[" THEN
j = INSTR(p, t$, "]")
m$ = MID$(t$, p + 1, j - p - 1)
isRnd = 0
IF RIGHT$(m$, 1) = "?" THEN
isRnd = 1
m$ = LEFT$(m$, LEN(m$) - 1)
ENDIF
v = VAL(m$)
exPos(sp) = j + 1
IF isRnd THEN v = rgBase(v) + RndPick()
IF dtStd THEN
FOR k = 0 TO NSTD - 1
IF stdNo(k) = v THEN PutStr stdTx$(k)
NEXT k
ELSEIF sp < EXDEPTH AND v >= 0 AND v <= 255 THEN
sp = sp + 1
exTok(sp) = v
exPos(sp) = 1
exPart(sp) = 0
ENDIF
ELSEIF c$ = "{" THEN
j = INSTR(p, t$, "}")
v = DoControl(VAL(MID$(t$, p + 1, j - p - 1)))
exPos(sp) = j + 1
IF v >= 0 AND sp < EXDEPTH THEN
sp = sp + 1
exTok(sp) = v
exPos(sp) = 1
exPart(sp) = 0
ENDIF
ELSE
PutCh c$
exPos(sp) = p + 1
ENDIF
ENDIF
LOOP
END SUB
FUNCTION SysDesc$()
rndS(0) = gs1 AND 255
rndS(1) = (gs1 >> 8) AND 255
rndS(2) = gs2 AND 255
rndS(3) = (gs2 >> 8) AND 255
descBuf$ = ""
dtCase = DT_CAPS
dtCapNext = 0
dtInWord = 0
dtSink = 0
dtStd = 0
ExpandTok 5
SysDesc$ = descBuf$
END FUNCTION
FUNCTION DrawDesc(y AS INTEGER, wide AS INTEGER) AS INTEGER
LOCAL INTEGER i, yy
LOCAL t$ LENGTH 255
LOCAL ln$ LENGTH 50
LOCAL w$ LENGTH 30
t$ = SysDesc$() + " "
ln$ = "" : w$ = "" : yy = y
FOR i = 1 TO LEN(t$)
IF MID$(t$, i, 1) = " " THEN
IF LEN(ln$) + LEN(w$) + 1 > wide THEN
TEXT 20, yy, ln$, "LT", 7, 1, cWhite
yy = yy + 9
ln$ = w$
ELSEIF ln$ = "" THEN
ln$ = w$
ELSE
ln$ = ln$ + " " + w$
ENDIF
w$ = ""
ELSE
IF LEN(w$) < 29 THEN w$ = w$ + MID$(t$, i, 1)
ENDIF
NEXT i
IF ln$ <> "" THEN TEXT 20, yy, ln$, "LT", 7, 1, cWhite : yy = yy + 9
DrawDesc = yy
END FUNCTION
SUB FireLaser
LOCAL INTEGER n, best, bestz, dmg
IF lasTimer > 0 THEN EXIT SUB
IF pLasT >= 242 THEN EXIT SUB
IF lasView(vw) = 0 THEN EXIT SUB
pLasT = pLasT + 8
IF pLasT > 255 THEN pLasT = 255
lasFlash = 2
IF lasView(vw) >= 128 THEN lasTimer = 0 ELSE lasTimer = LASPULSE
Sfx SFX_LASER
best = -1 : bestz = 999999
FOR n = 2 TO nUsed - 1
IF sTyp(n) <> 0 AND sBp(n) >= 0 AND sExp(n) = 0 THEN
ViewXform n
IF tz > 0 AND tz < bestz THEN
IF ABS(tx) < 256 AND ABS(ty) < 256 THEN
IF tx * tx + ty * ty < bArea(sBp(n)) THEN
best = n : bestz = tz
ENDIF
ENDIF
ENDIF
ENDIF
NEXT n
shots = shots + 1
IF best < 0 THEN EXIT SUB
hits = hits + 1
Angry best
dmg = lasView(vw) AND 127
IF sTyp(best) = T_CONSTRICT THEN
IF lasView(vw) <> LAS_MILITARY THEN
Sfx SFX_HIT
EXIT SUB
ENDIF
dmg = dmg \ 4
ENDIF
sEne(best) = sEne(best) - dmg
IF sEne(best) <= 0 THEN
IF sTyp(best) = T_STATION THEN
sEne(best) = bEne(sBp(best))
ELSE
IF lasView(vw) = LAS_MINING THEN Mine best
Explode best
ENDIF
ENDIF
END SUB
SUB Mine(n AS INTEGER)
LOCAL INTEGER i, cnt, m, t
SELECT CASE sTyp(n)
CASE T_ASTEROID, T_HERMIT : t = T_BOULDER
CASE T_BOULDER            : t = T_SPLINTER
CASE ELSE                 : EXIT SUB
END SELECT
cnt = INT(RND * 4)
FOR i = 1 TO cnt
MATH Q_EULER RND * 6, RND * 6, 0, qA() : qA(4) = 1
m = NewShip(t, sX(n) + (RND * 400 - 200), sY(n) + (RND * 400 - 200), sZ(n) + (RND * 400 - 200), qA())
IF m < 0 THEN EXIT SUB
sRol(m) = 130 : sPit(m) = 5
NEXT i
END SUB
SUB Explode(n AS INTEGER)
sExp(n) = 18
Sfx SFX_BOOM
Sfx SFX_BOOMT
sSpd(n) = 0
sAI(n) = 0
IF sTyp(n) = T_CONSTRICT THEN mission = mission OR MI_1DONE
kills = kills + 1
cashTenths = cashTenths + bBty(sBp(n))
IF bBty(sBp(n)) > 0 THEN Message STR$(bBty(sBp(n)) / 10) + " CR"
IF kills > 0 AND (kills AND 255) = 0 THEN Message "RIGHT ON COMMANDER!"
NoteKill n
EjectCargo n
DropObject n
END SUB
SUB Explosions
LOCAL INTEGER n, i, px, py, sz, cnt, tinted
n = 2
DO WHILE n < nUsed
IF sTyp(n) <> 0 AND sExp(n) > 0 THEN
IF tickWhole THEN sExp(n) = sExp(n) + 4
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
ARRAY SET col(shpCol(sTyp(n))), spc()
tinted = 1
PIXEL spx(), spy(), spc()
ENDIF
ENDIF
n = n + 1
ENDIF
ELSE
n = n + 1
ENDIF
LOOP
IF tinted THEN ARRAY SET cWhite, spc()
END SUB
SUB Tactics
LOCAL INTEGER n, dmg
LOCAL FLOAT d, cnt, nx, ny, nz
FOR n = 2 TO nUsed - 1
IF sTyp(n) <> 0 AND sBp(n) >= 0 AND sExp(n) = 0 AND sTyp(n) <> T_MISSILE THEN
IF (sAI(n) AND 128) <> 0 THEN
IF inSafe THEN
IF (sNewb(n) AND NB_PIRATE) <> 0 THEN sAI(n) = sAI(n) AND 129
ENDIF
IF ((mcnt XOR n) AND 7) = 0 THEN
IF Peaceful(n) THEN
Bystander n
ELSEIF sTyp(n) = T_HERMIT THEN
IF INT(RND * 256) >= 200 THEN HermitLaunch n
ELSE
d = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
IF d > 1 THEN
NoseVec n
nx = qV(1) * qV(4) : ny = qV(2) * qV(4) : nz = qV(3) * qV(4)
cnt = (-sX(n) * nx - sY(n) * ny - sZ(n) * nz) / d
IF d < 8192 AND cnt > 0.917 AND (sAI(n) AND 126) <> 0 THEN
dmg = bLas(sBp(n)) * 2
sFlg(n) = sFlg(n) OR 2
IF cnt > 0.972 THEN
HitPlayer dmg, n
ENDIF
ENDIF
IF sEne(n) * 8 < bEne(sBp(n)) AND (tNewb(sTyp(n)) AND NB_POD) <> 0 THEN
IF (sFlg(n) AND 1) = 0 AND INT(RND * 256) >= 230 THEN
sFlg(n) = sFlg(n) OR 1
BailOut n
ENDIF
ENDIF
IF sEne(n) * 2 < bEne(sBp(n)) AND sMis(n) > 0 AND ecmActive = 0 THEN
IF INT(RND * 32) < sMis(n) THEN
sMis(n) = sMis(n) - 1
EnemyMissile n
ENDIF
ENDIF
IF d < 1024 THEN
sPit(n) = 3 : sRol(n) = 5
ELSE
IF (INT(RND * 128) OR 128) < sAI(n) THEN
TurnTowards n, cnt
ELSE
sPit(n) = 3
ENDIF
ENDIF
IF cnt > 0.85 THEN
sAcc(n) = 3
ELSEIF cnt < -0.7 THEN
sAcc(n) = -1
ENDIF
ENDIF
ENDIF
ENDIF
ENDIF
ENDIF
NEXT n
END SUB
FUNCTION Peaceful(n AS INTEGER) AS INTEGER
LOCAL INTEGER nb
nb = sNewb(n)
Peaceful = 0
IF (nb AND NB_TRADER) <> 0 THEN
IF INT(RND * 256) >= 50 THEN Peaceful = 1 : EXIT FUNCTION
ENDIF
IF (nb AND NB_HUNTER) <> 0 THEN
IF legal >= 40 THEN
sNewb(n) = nb OR NB_HOSTILE
nb = sNewb(n)
ENDIF
ENDIF
IF (nb AND NB_HOSTILE) = 0 THEN Peaceful = 1
END FUNCTION
SUB Bystander(n AS INTEGER)
LOCAL INTEGER tgt
tgt = SLOT_PLANET
IF (sNewb(n) AND NB_DOCKING) <> 0 THEN
IF sTyp(SLOT_STAR) = T_STATION THEN tgt = SLOT_STAR
ENDIF
IF sTyp(tgt) = 0 THEN EXIT SUB
TurnToward n, tgt
IF tgt = SLOT_STAR THEN
DockNPC n
ELSEIF sAcc(n) = 0 THEN
sAcc(n) = 1
ENDIF
END SUB
SUB DockNPC(n AS INTEGER)
LOCAL FLOAT d
IF sTyp(SLOT_STAR) <> T_STATION THEN EXIT SUB
d = ShipToStation(n)
IF d > DOCKAPPR THEN
IF sAcc(n) = 0 THEN sAcc(n) = 1
EXIT SUB
ENDIF
IF d <= DOCKRANGE THEN
IF DockSide(n) THEN
sNewb(n) = sNewb(n) OR NB_GONE
KillShip n
EXIT SUB
ENDIF
ENDIF
IF ABS(SlotAlign(n)) < DOCKALIGN THEN
sAcc(n) = -2
IF sSpd(n) < 2 THEN sAcc(n) = 0
EXIT SUB
ENDIF
sRol(n) = sRol(SLOT_STAR)
IF sSpd(n) < 8 THEN sAcc(n) = 1
IF sSpd(n) > 10 THEN sAcc(n) = -2
END SUB
FUNCTION ShipToStation(n AS INTEGER) AS FLOAT
LOCAL FLOAT dx, dy, dz
dx = sX(n) - sX(SLOT_STAR)
dy = sY(n) - sY(SLOT_STAR)
dz = sZ(n) - sZ(SLOT_STAR)
ShipToStation = SQR(dx*dx + dy*dy + dz*dz)
END FUNCTION
FUNCTION DockSide(n AS INTEGER) AS INTEGER
LOCAL FLOAT dx, dy, dz, d
dx = sX(n) - sX(SLOT_STAR)
dy = sY(n) - sY(SLOT_STAR)
dz = sZ(n) - sZ(SLOT_STAR)
d = SQR(dx*dx + dy*dy + dz*dz)
DockSide = 1
IF d < 1 THEN EXIT FUNCTION
MATH SLICE sQ(), , SLOT_STAR, qA()
MATH Q_VECTOR 0, 0, 1, qB() : MATH Q_ROTATE qA(), qB(), qV()
IF (qV(1) * dx + qV(2) * dy + qV(3) * dz) / d < DOCKFACE THEN DockSide = 0
END FUNCTION
FUNCTION SlotAlign(n AS INTEGER) AS FLOAT
LOCAL FLOAT rx, ry, rz
MATH SLICE sQ(), , SLOT_STAR, qA()
MATH Q_VECTOR 0, 1, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
rx = qV(1) : ry = qV(2) : rz = qV(3)
MATH SLICE sQ(), , n, qA()
MATH Q_VECTOR 1, 0, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
SlotAlign = rx * qV(1) + ry * qV(2) + rz * qV(3)
END FUNCTION
SUB TurnToward(n AS INTEGER, tgt AS INTEGER)
LOCAL FLOAT rx, ry, rz, sx2, sy2, sz2, dr, ds, m, dx, dy, dz
dx = sX(tgt) - sX(n) : dy = sY(tgt) - sY(n) : dz = sZ(tgt) - sZ(n)
m = SQR(dx*dx + dy*dy + dz*dz)
IF m < 1 THEN EXIT SUB
MATH SLICE sQ(), , n, qA()
MATH Q_VECTOR 0, 1, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
rx = qV(1) : ry = qV(2) : rz = qV(3)
MATH Q_VECTOR 1, 0, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
sx2 = qV(1) : sy2 = qV(2) : sz2 = qV(3)
dr = (dx * rx + dy * ry + dz * rz) / m
ds = (dx * sx2 + dy * sy2 + dz * sz2) / m
IF dr > 0 THEN sPit(n) = 3 OR 128 ELSE sPit(n) = 3
IF (sRol(n) AND 127) < 16 THEN
IF ds > 0 THEN sRol(n) = 5 OR 128 ELSE sRol(n) = 5
ENDIF
END SUB
SUB Angry(n AS INTEGER)
IF sTyp(n) = T_STATION THEN AngerStation : EXIT SUB
IF (sNewb(n) AND NB_INNOCENT) <> 0 THEN AngerStation
IF sAI(n) = 0 THEN EXIT SUB
sAI(n) = sAI(n) OR 128
sAcc(n) = 2
sPit(n) = 4
sNewb(n) = sNewb(n) OR NB_HOSTILE
END SUB
SUB HermitLaunch(n AS INTEGER)
LOCAL INTEGER m, t
SELECT CASE INT(RND * 4)
CASE 0    : t = T_MAMBA
CASE 1    : t = T_KRAIT
CASE 2    : t = T_ADDER
CASE ELSE : t = T_GECKO
END SELECT
m = NewFacing(t, sX(n), sY(n), sZ(n), 0)
IF m >= 0 THEN
sAI(m) = 241
sSpd(m) = bSpd(sBp(m))
ENDIF
sAI(n) = 0
END SUB
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
IF dr > 0 THEN sPit(n) = 3 ELSE sPit(n) = 3 OR 128
IF (sRol(n) AND 127) < 16 THEN
IF ds > 0 THEN sRol(n) = 5 ELSE sRol(n) = 5 OR 128
ENDIF
END SUB
SUB HitPlayer(dmg AS INTEGER, from AS INTEGER)
LOCAL INTEGER dleft, aft
dleft = dmg
Sfx SFX_HIT
aft = 0
IF from >= 0 THEN
IF sZ(from) < 0 THEN aft = 1
ENDIF
IF aft THEN
IF pAsh >= dleft THEN
pAsh = pAsh - dleft
EXIT SUB
ENDIF
dleft = dleft - pAsh
pAsh = 0
ELSE
IF pFsh >= dleft THEN
pFsh = pFsh - dleft
EXIT SUB
ENDIF
dleft = dleft - pFsh
pFsh = 0
ENDIF
pEnergy = pEnergy - dleft
IF pEnergy <= 0 THEN
pEnergy = 0
dead = 1
EXIT SUB
ENDIF
Ouch
END SUB
SUB Ouch
LOCAL INTEGER i, e
IF INT(RND * 256) >= 128 THEN EXIT SUB
i = INT(RND * 256)
IF i >= 22 THEN EXIT SUB
IF msgText$ <> "" THEN EXIT SUB
IF i < NGOODS THEN
IF cargo(i) = 0 THEN EXIT SUB
cargo(i) = 0
Message UCASE$(mkName$(i)) + " DESTROYED"
Sfx SFX_BOOM
EXIT SUB
ENDIF
SELECT CASE i
CASE 17    : e = EQ_ECM
CASE 18    : e = EQ_SCOOPS
CASE 19    : e = EQ_BOMB
CASE 20    : e = EQ_ENERGY
CASE ELSE  : e = EQ_DOCK
END SELECT
IF eqOwned(e) = 0 THEN EXIT SUB
eqOwned(e) = 0
IF e = EQ_ENERGY AND energyUnit = 1 THEN energyUnit = 0
IF e = EQ_DOCK THEN dockComp = 0
Message UCASE$(eqName$(e)) + " DESTROYED"
Sfx SFX_BOOM
END SUB
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
SUB TargetMissile
LOCAL INTEGER n, best, bestz
IF pMissl = 0 THEN Sfx SFX_BOOP : EXIT SUB
best = -1 : bestz = 999999
FOR n = 2 TO nUsed - 1
IF sTyp(n) <> 0 AND sBp(n) >= 0 AND sExp(n) = 0 AND sTyp(n) <> T_MISSILE THEN
ViewXform n
IF tz > 0 AND tz < bestz THEN
IF ABS(tx) < 256 AND ABS(ty) < 256 THEN
IF tx * tx + ty * ty < bArea(sBp(n)) THEN
best = n : bestz = tz
ENDIF
ENDIF
ENDIF
ENDIF
NEXT n
IF best >= 0 THEN
msLock = best
Sfx SFX_BEEP
ELSE
Sfx SFX_BOOP
ENDIF
END SUB
SUB LaunchMissile
LOCAL INTEGER n
IF pMissl = 0 OR msLock < 0 THEN EXIT SUB
IF sTyp(msLock) = 0 OR sExp(msLock) > 0 THEN
msLock = -1
Message "TARGET LOST"
EXIT SUB
ENDIF
MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
n = NewShip(T_MISSILE, 0, -28, 200, qA())
IF n < 0 THEN EXIT SUB
sSpd(n) = bSpd(sBp(n))
sAI(n) = 128 OR 126
sTgt(n) = msLock
pMissl = pMissl - 1
msLock = -1
Sfx SFX_LAUNCH
END SUB
SUB Missiles
LOCAL INTEGER n, t
LOCAL FLOAT dx, dy, dz, d
FOR n = 2 TO nUsed - 1
IF sTyp(n) = T_MISSILE AND sExp(n) = 0 AND sTgt(n) <> -2 THEN
t = sTgt(n)
IF t < 0 OR t >= nUsed THEN
Explode n
ELSEIF sTyp(t) = 0 OR sExp(t) > 0 THEN
Explode n
ELSE
dx = sX(t) - sX(n) : dy = sY(t) - sY(n) : dz = sZ(t) - sZ(n)
d = SQR(dx * dx + dy * dy + dz * dz)
IF d < 256 THEN
Explode n
IF sTyp(t) <> T_STATION THEN
sEne(t) = 0
Explode t
ENDIF
ELSE
IF INT(RND * 256) < 16 AND (sAI(t) AND 1) <> 0 THEN
EnemyECM
ELSE
HomeOn n, dx, dy, dz, d
ENDIF
ENDIF
ENDIF
ENDIF
NEXT n
FOR n = 2 TO nUsed - 1
IF sTyp(n) = T_MISSILE AND sExp(n) = 0 AND sTgt(n) = -2 THEN
d = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
IF d < 256 THEN
Explode n
IF d < 128 THEN HitPlayer 250, n ELSE HitPlayer 80, n
ELSE
HomeOn n, -sX(n), -sY(n), -sZ(n), d
ENDIF
ENDIF
NEXT n
END SUB
SUB HomeOn(n AS INTEGER, dx AS FLOAT, dy AS FLOAT, dz AS FLOAT, d AS FLOAT)
LOCAL FLOAT nx, ny, nz, ax, ay, az, m
IF d < 1 THEN EXIT SUB
NoseVec n
nx = qV(1) * qV(4) : ny = qV(2) * qV(4) : nz = qV(3) * qV(4)
nx = nx + (dx / d - nx) * MSTURN * tick
ny = ny + (dy / d - ny) * MSTURN * tick
nz = nz + (dz / d - nz) * MSTURN * tick
m = SQR(nx * nx + ny * ny + nz * nz)
IF m < 0.0001 THEN EXIT SUB
nx = nx / m : ny = ny / m : nz = nz / m
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
SUB FireECM
IF eqOwned(EQ_ECM) = 0 THEN Sfx SFX_BOOP : EXIT SUB
IF ecmActive > 0 THEN EXIT SUB
ecmActive = ECMFRAMES
ecmMine = 1
Sfx SFX_ECM
KillMissiles
END SUB
SUB EnemyECM
IF ecmActive > 0 THEN EXIT SUB
ecmActive = ECMFRAMES
ecmMine = 0
Sfx SFX_ECM
KillMissiles
END SUB
SUB KillMissiles
LOCAL INTEGER n
FOR n = 2 TO nUsed - 1
IF sTyp(n) = T_MISSILE AND sExp(n) = 0 THEN
Message "MISSILE JAMMED"
Explode n
ENDIF
NEXT n
END SUB
SUB EnemyMissile(n AS INTEGER)
LOCAL INTEGER m
IF sTyp(n) = T_THARGOID THEN
m = NewFacing(T_THARGON, sX(n), sY(n) - 30, sZ(n), 180)
IF m >= 0 THEN sAI(m) = 128 OR 126 : sSpd(m) = bSpd(sBp(m))
EXIT SUB
ENDIF
m = NewFacing(T_MISSILE, sX(n), sY(n) - 30, sZ(n), 180)
IF m < 0 THEN EXIT SUB
sSpd(m) = bSpd(sBp(m))
sAI(m) = 128 OR 126
sTgt(m) = -2
Message "INCOMING MISSILE"
Sfx SFX_LAUNCH
END SUB
SUB ECMService
IF ecmActive > 0 THEN
ecmActive = ecmActive - 1
IF ecmMine THEN
pEnergy = pEnergy - 1
IF pEnergy < 0 THEN pEnergy = 0
ENDIF
IF ecmActive = 0 THEN Sfx SFX_ECMOFF
ENDIF
END SUB
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
SUB NoteKill(n AS INTEGER)
IF (sNewb(n) AND NB_COP) <> 0 THEN
legal = legal OR 64
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
SUB DeathScreen
CLS
TEXT VCX, 60, "GAME OVER", "CT", 1, 2, cRed
TEXT VCX, 100, "Kills: " + STR$(kills) + "   " + RankName$(), "CT", 7, 1, cWhite
TEXT VCX, 115, "Cash: " + STR$(cashTenths / 10) + " Cr", "CT", 7, 1, cWhite
FRAMEBUFFER COPY F, N
END SUB
SUB DockCheck
LOCAL INTEGER n
LOCAL FLOAT d, tz2, rx, nz
n = SLOT_STAR
IF sTyp(n) <> T_STATION THEN EXIT SUB
IF dead THEN EXIT SUB
IF sZ(n) <= 0 THEN EXIT SUB
d = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
IF d > DOCKRANGE THEN EXIT SUB
MATH SLICE sQ(), , n, qA()
MATH Q_VECTOR 0, 0, 1, qB() : MATH Q_ROTATE qA(), qB(), qV()
nz = qV(3)
MATH Q_VECTOR 0, 1, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
rx = qV(1)
tz2 = sZ(n) / d
IF (sAI(n) AND 128) <> 0 THEN Crash : EXIT SUB
IF nz > -DOCKFACE THEN Crash : EXIT SUB
IF tz2 < DOCKCONE THEN Crash : EXIT SUB
IF ABS(rx) < DOCKROLL THEN Crash : EXIT SUB
DoDock
END SUB
SUB Crash
IF dSpeed < 5 THEN
dSpeed = 1
sZ(SLOT_STAR) = sZ(SLOT_STAR) + 300
HitPlayer 10, SLOT_STAR
ELSE
pEnergy = 0
dead = 1
ENDIF
END SUB
SUB DoDock
docked = 1
dSpeed = 0
hypCount = 0
dockComp = 0
msLock = -1
pEnergy = 255 : pFsh = 255 : pAsh = 255
pLasT = 0 : pCabT = 30 : pAltit = 200
pRoll = JCENTRE : pPitch = JCENTRE
ClearSlots
END SUB
SUB DockingComputer
LOCAL INTEGER n
LOCAL FLOAT d, ux, uy, uz, rx, ry, e
n = SLOT_STAR
IF sTyp(n) <> T_STATION THEN EXIT SUB
d = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
IF d < 1 THEN EXIT SUB
ux = sX(n) / d : uy = sY(n) / d : uz = sZ(n) / d
IF ux > 0.05 THEN
pRoll = JCENTRE + 40
ELSEIF ux < -0.05 THEN
pRoll = JCENTRE - 40
ELSE
pRoll = JCENTRE
ENDIF
IF uy > 0.05 THEN
pPitch = JCENTRE - 30
ELSEIF uy < -0.05 THEN
pPitch = JCENTRE + 30
ELSE
pPitch = JCENTRE
ENDIF
IF uz > 0.9 THEN
MATH SLICE sQ(), , n, qA()
MATH Q_VECTOR 0, 1, 0, qB() : MATH Q_ROTATE qA(), qB(), qV()
rx = qV(1) : ry = qV(2)
e = ABS(ry) * 400
IF e > 127 THEN e = 127
IF rx * ry > 0 THEN pRoll = JCENTRE + e ELSE pRoll = JCENTRE - e
ENDIF
IF d > 4000 THEN
dSpeed = 32
ELSEIF d > 1500 THEN
dSpeed = 18
ELSEIF d > 500 THEN
dSpeed = 10
ELSE
dSpeed = 4
ENDIF
END SUB
SUB LaunchTunnel
LOCAL INTEGER i, k, r
Sfx SFX_LAUNCH
FOR i = 0 TO 23
CLS
FOR k = 0 TO 5
r = ((i + k * 4) MOD 24) * 7 + 8
BOX VCX - r * 1.25, VCY - r, r * 2.5, r * 2, 1, cWhite, -1
NEXT k
DrawDash
ViewName
FRAMEBUFFER COPY F, N
NEXT i
END SUB
SUB HyperTunnel
LOCAL INTEGER i, k, r, c
FOR i = 0 TO 31
CLS
FOR k = 0 TO 6
r = ((i + k * 5) MOD 35) * 5 + 4
c = cWhite
IF (k AND 1) <> 0 THEN c = cCyan
CIRCLE VCX, VCY, r, 1, 1.25, c, -1
NEXT k
DrawDash
ViewName
FRAMEBUFFER COPY F, N
NEXT i
END SUB
SUB StationPolice
LOCAL INTEGER n
IF sTyp(SLOT_STAR) <> T_STATION THEN EXIT SUB
IF (sAI(SLOT_STAR) AND 128) = 0 THEN EXIT SUB
IF (mcnt AND 31) <> 0 THEN EXIT SUB
IF nUsed >= NSLOT THEN EXIT SUB
MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
n = NewShip(T_VIPER, sX(SLOT_STAR), sY(SLOT_STAR), sZ(SLOT_STAR) - 400, qA())
IF n >= 0 THEN
sSpd(n) = bSpd(sBp(n))
sAI(n) = 128 OR 56
ENDIF
END SUB
SUB StationTraffic
LOCAL INTEGER n, t
IF sTyp(SLOT_STAR) <> T_STATION THEN EXIT SUB
IF (sAI(SLOT_STAR) AND 128) <> 0 THEN EXIT SUB
IF (mcnt AND 31) <> 0 THEN EXIT SUB
IF CountType(T_TRANSPORT) > 0 OR CountType(T_SHUTTLE) > 0 THEN EXIT SUB
IF INT(RND * 256) < 253 THEN EXIT SUB
IF INT(RND * 2) = 0 THEN t = T_SHUTTLE ELSE t = T_TRANSPORT
MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
n = NewShip(t, sX(SLOT_STAR), sY(SLOT_STAR), sZ(SLOT_STAR) - 400, qA())
IF n >= 0 THEN
sSpd(n) = bSpd(sBp(n))
sAI(n) = 241
ENDIF
END SUB
SUB AngerStation
IF sTyp(SLOT_STAR) = T_STATION THEN sAI(SLOT_STAR) = sAI(SLOT_STAR) OR 128
legal = legal + 64
IF legal > 255 THEN legal = 255
END SUB
SUB SpawnTraffic
IF mcnt <> 0 THEN EXIT SUB
IF docked OR dead OR inWitch THEN EXIT SUB
IF INT(RND * 256) < 35 THEN
IF CountType(T_ASTEROID) < 3 THEN
IF SpawnBenign() THEN EXIT SUB
ENDIF
ENDIF
IF inSafe THEN EXIT SUB
IF SpawnPolice() THEN EXIT SUB
SpawnHostiles
END SUB
FUNCTION SpawnBenign() AS INTEGER
LOCAL INTEGER n, t
LOCAL FLOAT x, y, z
SpawnBenign = 0
z = 38 * 256
x = INT(RND * 256)
IF RND < 0.5 THEN x = x + 512
IF RND < 0.5 THEN x = -x
y = INT(RND * 256)
IF RND < 0.5 THEN y = -y
IF RND < 0.5 THEN
LOCAL INTEGER docking
docking = 0
IF RND < 0.5 THEN docking = 1
IF docking THEN newbFlags = NB_DOCKING
n = NewFacing(TraderShip(), x, y, z, 180)
IF n >= 0 THEN
IF docking THEN sAI(n) = 128 OR 64 ELSE sAI(n) = 0
sSpd(n) = 16 + INT(RND * 16)
sRol(n) = INT(RND * 128)
ENDIF
SpawnBenign = 1
EXIT FUNCTION
ENDIF
IF inSafe THEN SpawnBenign = 1 : EXIT FUNCTION
IF INT(RND * 256) >= 252 THEN
t = T_HERMIT
ELSEIF INT(RND * 256) < 5 THEN
t = T_CANISTER
ELSEIF RND < 0.5 THEN
t = T_BOULDER
ELSE
t = T_ASTEROID
ENDIF
n = NewFacing(t, x, y, z, 180)
IF n >= 0 THEN
IF t = T_HERMIT THEN sAI(n) = T_HERMIT
IF RND < 0.5 THEN
sSpd(n) = 16 + INT(RND * 16)
sRol(n) = INT(RND * 256) OR 111
ELSE
sPit(n) = INT(RND * 256) OR 127
ENDIF
ENDIF
END FUNCTION
FUNCTION SpawnPolice() AS INTEGER
LOCAL INTEGER bad, n
bad = Contraband() * 2
IF CountType(T_VIPER) > 0 THEN bad = bad OR legal
IF INT(RND * 256) < bad THEN n = Aggressor(T_VIPER, 0)
SpawnPolice = 0
IF CountType(T_VIPER) > 0 THEN SpawnPolice = 1
END FUNCTION
SUB SpawnHostiles
LOCAL INTEGER r, i, t, ai, cnt, n
spawnEV = spawnEV - 1
IF spawnEV >= 0 THEN EXIT SUB
spawnEV = 0
IF CarryingPlans() THEN
IF INT(RND * 256) >= 200 THEN
IF Aggressor(T_THARGOID, 0) >= 0 THEN n = Aggressor(T_THARGON, 129)
EXIT SUB
ENDIF
ENDIF
r = INT(RND * 256)
IF sysGov <> 0 THEN
IF r >= 90 THEN EXIT SUB
IF (r AND 7) < sysGov THEN EXIT SUB
ENDIF
r = INT(RND * 256)
IF r >= 200 THEN
cnt = INT(RND * 4)
spawnEV = cnt
FOR i = 0 TO cnt
n = Aggressor(PackShip(), 0)
NEXT i
EXIT SUB
ENDIF
spawnEV = spawnEV + 1
t = (r AND 3) + 3
ai = 192
IF INT(RND * 256) >= 200 THEN ai = ai OR 1
IF t = T_THARGOID THEN
IF INT(RND * 256) >= 200 THEN
IF (INT(ABS(sZ(SLOT_PLANET))) AND 62) = 0 THEN
n = Aggressor(T_COUGAR, 121)
IF n >= 0 THEN sSpd(n) = 18
ELSE
IF Aggressor(T_THARGOID, ai) >= 0 THEN n = Aggressor(T_THARGON, 129)
ENDIF
ENDIF
EXIT SUB
ENDIF
IF ConstrictorHere() THEN
ai = 249
IF WantConstrictor() THEN
n = Aggressor(T_CONSTRICT, ai)
EXIT SUB
ENDIF
ENDIF
n = Aggressor(HunterShip(), ai)
END SUB
FUNCTION PackShip() AS INTEGER
LOCAL INTEGER i
i = (INT(RND * 256) AND INT(RND * 256)) AND 7
SELECT CASE i
CASE 0 : PackShip = T_SIDEWINDER
CASE 1 : PackShip = T_MAMBA
CASE 2 : PackShip = T_KRAIT
CASE 3 : PackShip = T_ADDER
CASE 4 : PackShip = T_GECKO
CASE 5 : PackShip = T_COBRA1
CASE 6 : PackShip = T_WORM
CASE ELSE : PackShip = T_COBRA3
END SELECT
END FUNCTION
FUNCTION HunterShip() AS INTEGER
SELECT CASE INT(RND * 4)
CASE 0 : HunterShip = T_COBRA3
CASE 1 : HunterShip = T_ASP
CASE 2 : HunterShip = T_PYTHONP
CASE ELSE : HunterShip = T_FERDELANCE
END SELECT
END FUNCTION
FUNCTION TraderShip() AS INTEGER
SELECT CASE INT(RND * 4)
CASE 0 : TraderShip = T_PYTHON
CASE 1 : TraderShip = T_BOA
CASE 2 : TraderShip = T_ANACONDA
CASE ELSE : TraderShip = T_TRADER
END SELECT
END FUNCTION
FUNCTION Aggressor(t AS INTEGER, ai AS INTEGER) AS INTEGER
LOCAL INTEGER n, a
LOCAL FLOAT x, y
x = 8192 : IF RND < 0.5 THEN x = -x
y = 8192 : IF RND < 0.5 THEN y = -y
a = ai
IF a = 0 THEN
a = 192
IF INT(RND * 256) >= 245 THEN a = a OR 1
ENDIF
n = NewFacing(t, x, y, 8192, 180)
IF n >= 0 THEN sAI(n) = a
Aggressor = n
END FUNCTION
FUNCTION Contraband() AS INTEGER
Contraband = (cargo(3) + cargo(6)) * 2 + cargo(10)
END FUNCTION
FUNCTION CountType(t AS INTEGER) AS INTEGER
LOCAL INTEGER n, c
c = 0
FOR n = 2 TO nUsed - 1
IF sTyp(n) = t THEN
IF sExp(n) = 0 THEN c = c + 1
ENDIF
NEXT n
CountType = c
END FUNCTION
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
pCabT = 30
IF inWitch THEN EXIT SUB
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
got = dSpeed \ 8
IF got = 0 THEN EXIT SUB
pFuel = pFuel + got
IF pFuel > 70 THEN pFuel = 70
Message "FUEL SCOOPS ON"
END SUB
SUB Contact
LOCAL INTEGER n, t
n = 2
DO WHILE n < nUsed
IF Touching(n) THEN
t = sTyp(n)
IF Scoopable(t) AND eqOwned(EQ_SCOOPS) <> 0 AND sY(n) < 0 THEN
ScoopIt n, t
ELSE
HitPlayer 32, n
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
IF t = T_SPLINTER THEN Scoopable = 1
END FUNCTION
SUB ScoopIt(n AS INTEGER, t AS INTEGER)
LOCAL INTEGER item
SELECT CASE t
CASE T_ESCAPE  : item = 3
CASE T_THARGON : item = 16
CASE T_SPLINTER
IF INT(RND * 8) = 0 THEN item = 15 ELSE item = 12
CASE ELSE      : item = INT(RND * 8)
END SELECT
IF item < 13 AND HoldUsed() >= holdSize THEN
Sfx SFX_BOOP
ELSE
cargo(item) = cargo(item) + 1
Message mkName$(item)
Sfx SFX_BEEP
ENDIF
KillShip n
END SUB
SUB Perish
pEnergy = 0
dead = 1
Sfx SFX_BOOM
Sfx SFX_BOOMT
END SUB
SUB EnergyBomb
LOCAL INTEGER n
IF eqOwned(EQ_BOMB) = 0 THEN Sfx SFX_BOOP : EXIT SUB
eqOwned(EQ_BOMB) = 0
FOR n = 2 TO nUsed - 1
IF sTyp(n) <> 0 AND sBp(n) >= 0 AND sExp(n) = 0 THEN
IF sTyp(n) <> T_STATION AND sTyp(n) <> T_CONSTRICT THEN Explode n
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
lasView(0) = LAS_PULSE : lasView(1) = 0 : lasView(2) = 0 : lasView(3) = 0
holdSize = 20 : pMissl = 0 : energyUnit = 0
legal = 0
Sfx SFX_LAUNCH
DoDock
END SUB
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
FOR n = 2 TO nUsed - 1
IF sTyp(n) <> 0 AND sBp(n) >= 0 THEN
IF sTyp(n) <> T_ASTEROID AND sTyp(n) <> T_CANISTER AND sTyp(n) <> T_ESCAPE THEN
Sfx SFX_BOOP
EXIT SUB
ENDIF
ENDIF
NEXT n
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
SUB GalacticJump
IF eqOwned(EQ_GALHYP) = 0 THEN Sfx SFX_BOOP : EXIT SUB
eqOwned(EQ_GALHYP) = 0
legal = 0
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
SUB HangarScreen
LOCAL INTEGER t, hx, hz
fillBlack = 1
ClearSlots
IF INT(RND * 2) = 0 THEN
SELECT CASE INT(RND * 4)
CASE 0
HangarShip T_SHUTTLE, -84, 315
HangarShip T_TRANSPORT, 130, 432
CASE 1
HangarShip T_CANISTER, -80, 273
HangarShip T_CANISTER, 209, 552
HangarShip T_CANISTER, 64, 262
CASE 2
HangarShip T_VIPER, 96, 400
HangarShip T_KRAIT, -16, 465
CASE ELSE
HangarShip T_VIPER, 81, 760
HangarShip T_KRAIT, -96, 373
END SELECT
ELSE
SELECT CASE INT(RND * 4)
CASE 0    : t = T_SIDEWINDER
CASE 1    : t = T_MAMBA
CASE 2    : t = T_KRAIT
CASE ELSE : t = T_ADDER
END SELECT
hx = INT(RND * 64)
IF RND < 0.5 THEN hx = -hx
hz = 256 + INT(RND * 2) * 256 + INT(RND * 256)
HangarShip t, hx, hz
ENDIF
CLS
HangarFloor
DrawShips
LINE 0, 0, SCRW - 2, 0, 1, cWhite
BOX 0, 0, 2, VIEWH, 0, cWhite, cWhite
BOX SCRW - 2, 0, 2, VIEWH, 0, cWhite, cWhite
FRAMEBUFFER COPY F, N
HangarHold
ClearSlots
fillBlack = 0
END SUB
SUB HangarHold
LOCAL INTEGER k
LOCAL FLOAT t
LOCAL kb$ LENGTH 2
IF demoMode THEN
k = DemoHold(HANGDEMO, 0)
EXIT SUB
ENDIF
t = TIMER + HANGWAIT
DO
kb$ = INKEY$
IF kb$ <> "" THEN EXIT SUB
LOOP UNTIL TIMER > t
END SUB
SUB HangarShip(t AS INTEGER, x AS INTEGER, z AS INTEGER)
LOCAL INTEGER n, h
h = (100 - INT(SQR(bArea(tBp(t))))) \ 2
IF h < 0 THEN h = 0
n = NewFacing(t, x, -h, z, INT(RND * 360))
IF n >= 0 THEN sSpd(n) = 0
END SUB
SUB HangarFloor
LOCAL INTEGER q, y, x, horiz
horiz = VCY + 130 \ 12
FOR q = 2 TO 12
y = VCY + 130 \ q
LINE 2, y, SCRW - 3, y, 1, cRed
NEXT q
FOR x = 16 TO SCRW - 16 STEP 16
LINE x, 1, x, horiz, 1, cRed
NEXT x
END SUB
SUB BriefPage
CLS
brLine$ = ""
brWord$ = ""
brY = BRTOP
brCol = 0
IF brShip >= 0 THEN DrawShips
FRAMEBUFFER COPY F, N
END SUB
SUB BriefRow(r AS INTEGER)
BriefBreak
brY = r * 8
END SUB
SUB BriefTab(c AS INTEGER)
BriefBreak
brCol = c
END SUB
SUB BriefPut(ch$)
IF ch$ = " " THEN
BriefWord
ELSE
IF LEN(brWord$) < 30 THEN brWord$ = brWord$ + ch$
ENDIF
END SUB
SUB BriefWord
IF brWord$ = "" THEN EXIT SUB
IF brLine$ = "" THEN
brLine$ = brWord$
ELSEIF LEN(brLine$) + 1 + LEN(brWord$) > BRWIDE - brCol THEN
BriefFlush
brLine$ = brWord$
ELSE
brLine$ = brLine$ + " " + brWord$
ENDIF
brWord$ = ""
END SUB
SUB BriefBreak
BriefWord
BriefFlush
END SUB
SUB BriefFlush
IF brLine$ = "" THEN EXIT SUB
TEXT BRLEFT + brCol * 6, brY, brLine$, "LT", 7, 1, cWhite
brY = brY + BRROW
brLine$ = ""
FRAMEBUFFER COPY F, N
END SUB
SUB BriefIncoming
CLS
TEXT VCX, 80, "INCOMING MESSAGE", "CT", 7, 1, cWhite
FRAMEBUFFER COPY F, N
PAUSE 2000
BriefPage
END SUB
SUB BriefShip
LOCAL INTEGER k
BriefBreak
IF brShip < 0 THEN BriefWait : EXIT SUB
IF DEMOFRAMES > 0 THEN
FOR k = 1 TO 40 : BriefSpin : NEXT k
BriefShot
EXIT SUB
ENDIF
DO
BriefSpin
k = INKEY$ <> ""
LOOP UNTIL k
END SUB
SUB BriefSpin
IF brShip < 0 THEN EXIT SUB
sRol(brShip) = 127
sPit(brShip) = 127
MoveShips
DrawShips
FRAMEBUFFER COPY F, N
END SUB
SUB BriefWait
IF DEMOFRAMES > 0 THEN BriefShot : EXIT SUB
DO WHILE INKEY$ <> "" : LOOP
DO WHILE INKEY$ = "" : LOOP
END SUB
SUB BriefShot
shotNo = shotNo + 1
SAVE IMAGE "A:/brief" + STR$(shotNo) + ".bmp"
PAUSE 400
END SUB
SUB MissionBrief(tok AS INTEGER, t AS INTEGER)
LOCAL INTEGER i
ClearSlots
brShip = -1
dtSink = 1
dtStd = 0
dtCase = DT_CAPS
dtCapNext = 0
dtInWord = 0
BriefIncoming
IF t > 0 THEN
MATH Q_EULER 0, 0, 0, qA() : qA(4) = 1
brShip = NewShip(t, 0, 80, 384, qA())
IF brShip >= 0 THEN
sSpd(brShip) = 0
sAI(brShip) = 0
FOR i = 1 TO 64
BriefSpin
NEXT i
ENDIF
ENDIF
brY = BRTOP
brCol = 0
brLine$ = ""
brWord$ = ""
ExpandTok tok
BriefBreak
BriefWait
dtSink = 0
brShip = -1
ClearSlots
END SUB
FUNCTION ConstrictorHere() AS INTEGER
ConstrictorHere = conHere
END FUNCTION
FUNCTION AtSystem(x AS INTEGER, y AS INTEGER) AS INTEGER
AtSystem = 0
IF sysX = x AND sysYr = y THEN AtSystem = 1
END FUNCTION
SUB MissionCheck
LOCAL INTEGER m
GotoSystem gGal, homeSys
SysData
m = mission AND 3
IF m = 0 THEN
IF kills >= 256 AND gGal <= 2 THEN
mission = mission OR MI_1RUN
MissionBrief 10, T_CONSTRICT
ENDIF
EXIT SUB
ENDIF
IF m = 3 THEN
mission = mission AND (255 - MI_1RUN)
kills = kills + 256
cashTenths = cashTenths + 50000
MissionBrief 15, 0
EXIT SUB
ENDIF
IF m <> 2 THEN EXIT SUB
IF gGal <> 3 THEN EXIT SUB
m = mission AND 15
IF m = MI_1DONE THEN
IF kills >= 1280 THEN
mission = mission OR MI_2RUN
MissionBrief 11, 0
ENDIF
ELSEIF m = (MI_1DONE OR MI_2RUN) THEN
IF AtSystem(215, 84) THEN
mission = (mission AND 240) OR MI_1DONE OR MI_2PLANS
MissionBrief 222, 0
ENDIF
ELSEIF m = (MI_1DONE OR MI_2PLANS) THEN
IF AtSystem(63, 72) THEN
mission = mission OR MI_2RUN
energyUnit = 2
MissionBrief 223, 0
ENDIF
ENDIF
END SUB
FUNCTION CarryingPlans() AS INTEGER
CarryingPlans = 0
IF (mission AND (MI_2RUN OR MI_2PLANS)) = MI_2PLANS THEN CarryingPlans = 1
END FUNCTION
FUNCTION WantConstrictor() AS INTEGER
WantConstrictor = 0
IF ConstrictorHere() = 0 THEN EXIT FUNCTION
IF (mission AND MI_1RUN) = 0 THEN EXIT FUNCTION
IF (mission AND MI_1DONE) <> 0 THEN EXIT FUNCTION
IF CountType(T_CONSTRICT) > 0 THEN EXIT FUNCTION
WantConstrictor = 1
END FUNCTION
SUB EquipTable
LOCAL INTEGER i
RESTORE dat_equip
FOR i = 0 TO NEQUIP - 1
READ eqName$(i), eqPrice(i), eqTech(i)
NEXT i
END SUB
SUB StatusScreen
LOCAL INTEGER y
CLS
GotoSystem gGal, homeSys
SysData
TEXT VCX, 4, "COMMANDER JAMESON", "CT", 7, 1, cWhite
LINE 0, 19, SCRW - 1, 19, 1, cWhite
y = 28
DataLine y, "Present System", SysName$() : y = y + 11
DataLine y, "Hyperspace Arm", SysName2$(selSys) : y = y + 11
DataLine y, "Condition", CondName$() : y = y + 11
DataLine y, "Fuel", STR$(pFuel / 10) + " Light Years" : y = y + 11
DataLine y, "Cash", STR$(cashTenths / 10) + " Cr" : y = y + 11
DataLine y, "Legal Status", LegalName$() : y = y + 11
DataLine y, "Rating", RankName$() : y = y + 14
TEXT 20, y, "Equipment:", "LT", 7, 1, cWhite : y = y + 11
IF eqOwned(1) THEN TEXT 30, y, "Large Cargo Bay", "LT", 7, 1, cYellow : y = y + 10
IF eqOwned(2) THEN TEXT 30, y, "E.C.M. System", "LT", 7, 1, cYellow : y = y + 10
IF eqOwned(EQ_SCOOPS) THEN TEXT 30, y, "Fuel Scoops", "LT", 7, 1, cYellow : y = y + 10
IF eqOwned(EQ_POD) THEN TEXT 30, y, "Escape Pod", "LT", 7, 1, cYellow : y = y + 10
IF eqOwned(EQ_BOMB) THEN TEXT 30, y, "Energy Bomb", "LT", 7, 1, cYellow : y = y + 10
IF eqOwned(EQ_ENERGY) THEN TEXT 30, y, "Energy Unit", "LT", 7, 1, cYellow : y = y + 10
IF eqOwned(EQ_DOCK) THEN TEXT 30, y, "Docking Computer", "LT", 7, 1, cYellow : y = y + 10
IF eqOwned(EQ_GALHYP) THEN TEXT 30, y, "Galactic Hyperdrive", "LT", 7, 1, cYellow : y = y + 10
LOCAL INTEGER v
FOR v = 0 TO 3
IF lasView(v) <> 0 THEN
TEXT 30, y, ViewWord$(v) + LaserWord$(lasView(v)), "LT", 7, 1, cYellow
y = y + 10
ENDIF
NEXT v
END SUB
FUNCTION ViewWord$(v AS INTEGER)
SELECT CASE v
CASE 0 : ViewWord$ = "Fore "
CASE 1 : ViewWord$ = "Aft "
CASE 2 : ViewWord$ = "Left "
CASE ELSE : ViewWord$ = "Right "
END SELECT
END FUNCTION
FUNCTION LaserWord$(p AS INTEGER)
SELECT CASE p
CASE LAS_PULSE    : LaserWord$ = "Pulse Laser"
CASE LAS_BEAM     : LaserWord$ = "Beam Laser"
CASE LAS_MINING   : LaserWord$ = "Mining Laser"
CASE LAS_MILITARY : LaserWord$ = "Military Laser"
CASE ELSE         : LaserWord$ = "Pulse Laser"
END SELECT
END FUNCTION
FUNCTION CondName$()
IF docked THEN
CondName$ = "Docked"
ELSEIF pEnergy < 128 THEN
CondName$ = "Red"
ELSE
CondName$ = "Green"
ENDIF
END FUNCTION
FUNCTION SysName2$(n AS INTEGER)
LOCAL INTEGER keep
keep = homeSys
GotoSystem gGal, n
SysName2$ = SysName$()
GotoSystem gGal, keep
SysData
END FUNCTION
SUB InventoryScreen
LOCAL INTEGER i, y
CLS
TEXT VCX, 4, "INVENTORY", "CT", 7, 1, cWhite
LINE 0, 19, SCRW - 1, 19, 1, cWhite
y = 28
DataLine y, "Fuel", STR$(pFuel / 10) + " Light Years" : y = y + 11
DataLine y, "Cash", STR$(cashTenths / 10) + " Cr" : y = y + 14
FOR i = 0 TO NGOODS - 1
IF cargo(i) > 0 THEN
TEXT 20, y, mkName$(i), "LT", 7, 1, cWhite
TEXT 180, y, STR$(cargo(i)) + " " + mkUnit$(i), "LT", 7, 1, cYellow
y = y + 10
ENDIF
NEXT i
IF y = 53 THEN TEXT 20, y, "Nothing in the hold.", "LT", 7, 1, cDim
END SUB
SUB MarketScreen(sel AS INTEGER)
LOCAL INTEGER i, y, c
CLS
GotoSystem gGal, homeSys
SysData
TEXT VCX, 3, SysName$() + " MARKET PRICES", "CT", 7, 1, cWhite
LINE 0, 15, SCRW - 1, 15, 1, cWhite
TEXT 20, 18, "PRODUCT", "LT", 7, 1, cDim
TEXT 150, 18, "UNIT", "LT", 7, 1, cDim
TEXT 190, 18, "PRICE", "LT", 7, 1, cDim
TEXT 238, 18, "FOR SALE", "LT", 7, 1, cDim
TEXT 292, 18, "HELD", "LT", 7, 1, cDim
y = 29
FOR i = 0 TO NGOODS - 1
c = cWhite
IF i = sel THEN
c = cYellow
BOX 16, y - 1, 292, 9, 0, cSel, cSel
ENDIF
TEXT 20, y, mkName$(i), "LT", 7, 1, c
TEXT 150, y, mkUnit$(i), "LT", 7, 1, c
TEXT 190, y, PriceStr$(mkPrice(i)), "LT", 7, 1, c
TEXT 245, y, STR$(mkStock(i)), "LT", 7, 1, c
TEXT 300, y, STR$(cargo(i)), "LT", 7, 1, c
y = y + 9
NEXT i
TEXT 20, y + 4, "Cash: " + STR$(cashTenths / 10) + " Cr", "LT", 7, 1, cWhite
TEXT 180, y + 4, "Hold: " + STR$(HoldUsed()) + "/" + STR$(holdSize), "LT", 7, 1, cWhite
END SUB
FUNCTION HoldUsed() AS INTEGER
LOCAL INTEGER i, t
t = 0
FOR i = 0 TO NGOODS - 1
IF i < 13 THEN t = t + cargo(i)
NEXT i
HoldUsed = t
END FUNCTION
SUB BuyOne(i AS INTEGER)
IF mkStock(i) <= 0 THEN EXIT SUB
IF cashTenths < mkPrice(i) THEN EXIT SUB
IF i < 13 AND HoldUsed() >= holdSize THEN EXIT SUB
cashTenths = cashTenths - mkPrice(i)
cargo(i) = cargo(i) + 1
mkStock(i) = mkStock(i) - 1
END SUB
SUB SellOne(i AS INTEGER)
IF cargo(i) <= 0 THEN EXIT SUB
cashTenths = cashTenths + mkPrice(i)
cargo(i) = cargo(i) - 1
mkStock(i) = mkStock(i) + 1
END SUB
FUNCTION TradeMax(i AS INTEGER, buying AS INTEGER) AS INTEGER
LOCAL INTEGER m
IF buying = 0 THEN
TradeMax = cargo(i)
EXIT FUNCTION
ENDIF
m = mkStock(i)
IF mkPrice(i) > 0 THEN
IF cashTenths \ mkPrice(i) < m THEN m = cashTenths \ mkPrice(i)
ENDIF
IF i < 13 THEN
IF holdSize - HoldUsed() < m THEN m = holdSize - HoldUsed()
ENDIF
IF m < 0 THEN m = 0
TradeMax = m
END FUNCTION
SUB TradeAmount(i AS INTEGER, buying AS INTEGER)
LOCAL INTEGER most, n, k
LOCAL t$ LENGTH 4
LOCAL p$ LENGTH 40
most = TradeMax(i, buying)
IF most <= 0 THEN Sfx SFX_BOOP : EXIT SUB
IF buying THEN p$ = "Buy how many" ELSE p$ = "Sell how many"
p$ = p$ + " (max " + STR$(most) + ")? "
t$ = ""
DO
MarketScreen i
TEXT VCX, SCRH - 9, p$ + t$ + "_", "CT", 7, 1, cYellow
FRAMEBUFFER COPY F, N
k = DockKey()
IF k = 27 THEN EXIT SUB
IF k = 13 THEN EXIT DO
IF k = 8 THEN
IF LEN(t$) > 0 THEN t$ = LEFT$(t$, LEN(t$) - 1)
ELSEIF k >= 48 AND k <= 57 THEN
IF VAL(t$ + CHR$(k)) <= most THEN t$ = t$ + CHR$(k) ELSE Sfx SFX_BOOP
ENDIF
LOOP
n = VAL(t$)
IF n <= 0 THEN EXIT SUB
FOR k = 1 TO n
IF buying THEN BuyOne i ELSE SellOne i
NEXT k
Sfx SFX_BEEP
END SUB
SUB EquipScreen(sel AS INTEGER)
LOCAL INTEGER i, y, c
CLS
GotoSystem gGal, homeSys
SysData
TEXT VCX, 3, "EQUIP SHIP", "CT", 7, 1, cWhite
LINE 0, 15, SCRW - 1, 15, 1, cWhite
y = 22
TEXT 20, y, "Fuel", "LT", 7, 1, cWhite
TEXT 220, y, PriceStr$((70 - pFuel) * 2) + " Cr", "LT", 7, 1, cYellow
y = y + 11
FOR i = 0 TO NEQUIP - 1
IF eqTech(i) <= sysTech + 1 THEN
c = cWhite
IF eqOwned(i) THEN c = cDim
IF i = sel THEN
c = cYellow
BOX 16, y - 1, 292, 9, 0, cSel, cSel
ENDIF
TEXT 20, y, eqName$(i), "LT", 7, 1, c
TEXT 220, y, STR$(eqPrice(i)) + " Cr", "LT", 7, 1, c
y = y + 9
ENDIF
NEXT i
TEXT 20, y + 5, "Cash: " + STR$(cashTenths / 10) + " Cr", "LT", 7, 1, cWhite
END SUB
SUB BuyEquip(i AS INTEGER)
LOCAL INTEGER v
IF eqTech(i) > sysTech + 1 THEN EXIT SUB
IF cashTenths < eqPrice(i) * 10 THEN EXIT SUB
IF i = EQ_PULSE OR i = EQ_BEAM OR i = EQ_MINING OR i = EQ_MILITARY THEN
v = AskView()
IF v < 0 THEN EXIT SUB
IF i = EQ_PULSE THEN
IF lasView(v) <> 0 THEN Sfx SFX_BOOP : EXIT SUB
lasView(v) = LAS_PULSE
ELSE
IF i = EQ_BEAM AND lasView(v) = LAS_BEAM THEN Sfx SFX_BOOP : EXIT SUB
IF i = EQ_MINING AND lasView(v) = LAS_MINING THEN Sfx SFX_BOOP : EXIT SUB
IF i = EQ_MILITARY AND lasView(v) = LAS_MILITARY THEN Sfx SFX_BOOP : EXIT SUB
IF lasView(v) = LAS_PULSE THEN cashTenths = cashTenths + eqPrice(EQ_PULSE) * 10
IF i = EQ_BEAM THEN lasView(v) = LAS_BEAM
IF i = EQ_MINING THEN lasView(v) = LAS_MINING
IF i = EQ_MILITARY THEN lasView(v) = LAS_MILITARY
ENDIF
cashTenths = cashTenths - eqPrice(i) * 10
EXIT SUB
ENDIF
IF eqOwned(i) THEN EXIT SUB
IF i = 0 THEN
IF pMissl >= 4 THEN EXIT SUB
cashTenths = cashTenths - eqPrice(i) * 10
pMissl = pMissl + 1
EXIT SUB
ENDIF
cashTenths = cashTenths - eqPrice(i) * 10
eqOwned(i) = 1
SELECT CASE i
CASE 1 : holdSize = 35
CASE EQ_ENERGY : energyUnit = 1
END SELECT
END SUB
FUNCTION AskView() AS INTEGER
LOCAL INTEGER k
demoAsk = 1
DO
BOX 44, 92, 232, 52, 1, cWhite, cBlack
TEXT VCX, 102, "WHICH MOUNT?", "CT", 7, 1, cWhite
TEXT VCX, 122, "F1 fore  F2 aft  F3 left  F4 right", "CT", 7, 1, cYellow
FRAMEBUFFER COPY F, N
k = DockKey()
IF k = 27 THEN demoAsk = 0 : AskView = -1 : EXIT FUNCTION
LOOP UNTIL k >= 145 AND k <= 148
demoAsk = 0
AskView = k - 145
END FUNCTION
SUB BuyFuel
LOCAL INTEGER cost
cost = (70 - pFuel) * 2
IF cost <= 0 THEN EXIT SUB
IF cashTenths < cost THEN
pFuel = pFuel + cashTenths \ 2
cashTenths = cashTenths MOD 2
ELSE
cashTenths = cashTenths - cost
pFuel = 70
ENDIF
END SUB
SUB SaveCommander(f$)
LOCAL INTEGER i, fn
fn = 1
OPEN f$ FOR OUTPUT AS #fn
PRINT #fn, "elite-commander 3"
PRINT #fn, gGal
PRINT #fn, homeSys
PRINT #fn, cashTenths
PRINT #fn, pFuel
PRINT #fn, holdSize
PRINT #fn, kills
PRINT #fn, legal
PRINT #fn, pMissl
FOR i = 0 TO 3 : PRINT #fn, lasView(i) : NEXT i
PRINT #fn, energyUnit
PRINT #fn, mission
FOR i = 0 TO NGOODS - 1 : PRINT #fn, cargo(i) : NEXT i
FOR i = 0 TO NEQUIP - 1 : PRINT #fn, eqOwned(i) : NEXT i
CLOSE #fn
END SUB
FUNCTION LoadCommander(f$) AS INTEGER
LOCAL INTEGER i, fn
LOCAL hd$
LoadCommander = 0
IF DIR$(f$, FILE) = "" THEN EXIT FUNCTION
fn = 1
OPEN f$ FOR INPUT AS #fn
LINE INPUT #fn, hd$
IF hd$ <> "elite-commander 2" AND hd$ <> "elite-commander 3" THEN
CLOSE #fn
EXIT FUNCTION
ENDIF
INPUT #fn, gGal
INPUT #fn, homeSys
INPUT #fn, cashTenths
INPUT #fn, pFuel
INPUT #fn, holdSize
INPUT #fn, kills
INPUT #fn, legal
INPUT #fn, pMissl
FOR i = 0 TO 3 : INPUT #fn, lasView(i) : NEXT i
INPUT #fn, energyUnit
mission = 0
IF hd$ = "elite-commander 3" THEN INPUT #fn, mission
FOR i = 0 TO NGOODS - 1 : INPUT #fn, cargo(i) : NEXT i
FOR i = 0 TO NEQUIP - 1 : INPUT #fn, eqOwned(i) : NEXT i
CLOSE #fn
selSys = homeSys
GotoSystem gGal, homeSys
SysData
homeX = sysX : homeY = sysY * 2
curX = homeX : curY = homeY
mkByte = 0
MakeMarket sysEco, mkByte
LoadCommander = 1
END FUNCTION
dat_equip:
DATA "Missile",30,1
DATA "Large Cargo Bay",400,4
DATA "E.C.M. System",600,3
DATA "Extra Pulse Lasers",400,4
DATA "Extra Beam Lasers",1000,4
DATA "Fuel Scoops",525,5
DATA "Escape Pod",600,6
DATA "Energy Bomb",900,7
DATA "Energy Unit",1500,8
DATA "Docking Computer",1500,9
DATA "Galactic Hyperdrive",5000,10
DATA "Extra Military Lasers",6000,10
DATA "Extra Mining Lasers",800,10
FUNCTION TitleScreen() AS INTEGER
LOCAL INTEGER k
DO
DrawTitle
k = WaitKey(TITLEWAIT)
IF k <> 72 AND k <> 104 THEN
TitleScreen = k
EXIT FUNCTION
ENDIF
ControlsScreen
LOOP
END FUNCTION
SUB DrawTitle
CLS
IF DIR$(titlePic$, FILE) <> "" THEN
LOAD JPG titlePic$
ELSE
TEXT VCX, 40, "E L I T E", "CT", 1, 4, cWhite
LINE 24, 100, SCRW - 25, 100, 1, cCyan
TEXT VCX, 130, "after Bell and Braben, 1984", "CT", 7, 1, cWhite
LINE 24, 190, SCRW - 25, 190, 1, cCyan
TEXT VCX, 202, "PRESS ANY KEY TO PLAY", "CT", 7, 1, cYellow
TEXT VCX, 218, "H FOR THE CONTROLS", "CT", 7, 1, cCyan
ENDIF
FRAMEBUFFER COPY F, N
END SUB
SUB DrawControls
LOCAL INTEGER y
CLS
TEXT VCX, 1, "FLIGHT", "CT", 7, 1, cWhite
LINE 0, 11, SCRW - 1, 11, 1, cCyan
y = 14
KeyLine y, "Roll", "< >  or left/right" : y = y + 9
KeyLine y, "Pitch", "S X  or up/down" : y = y + 9
KeyLine y, "Speed", "SPACE faster, / slower" : y = y + 9
KeyLine y, "Fire", "A" : y = y + 9
KeyLine y, "Missile", "T locks on, M fires" : y = y + 9
KeyLine y, "E.C.M.", "E" : y = y + 9
KeyLine y, "Docking computer", "C" : y = y + 9
KeyLine y, "Hyperspace", "H, outside the zone" : y = y + 9
KeyLine y, "In-system jump", "J, with nothing about" : y = y + 9
KeyLine y, "Galactic jump", "G, if one is fitted" : y = y + 9
KeyLine y, "Energy bomb", "TAB" : y = y + 9
KeyLine y, "Escape pod", "ESC, if one is fitted" : y = y + 9
KeyLine y, "Views", "F1 fore, F2 aft" : y = y + 9
KeyLine y, "", "F3 left, F4 right" : y = y + 12
TEXT VCX, y, "SCREENS, FLYING OR DOCKED", "CT", 7, 1, cWhite
y = y + 11
KeyLine y, "F5 Galactic chart", "F8 Market prices" : y = y + 9
KeyLine y, "F6 Short range", "F9 Status" : y = y + 9
KeyLine y, "F7 System data", "F10 Inventory" : y = y + 12
TEXT VCX, y, "DOCKED", "CT", 7, 1, cWhite
y = y + 11
KeyLine y, "F1 Launch", "F2 buy, F3 sell" : y = y + 9
KeyLine y, "F4 Equip ship", "SPACE buys one" : y = y + 9
KeyLine y, "F fills the tank", "S save, L load" : y = y + 9
TEXT VCX, SCRH - 9, "any key goes back", "CT", 7, 1, cDim
FRAMEBUFFER COPY F, N
END SUB
SUB ControlsScreen
LOCAL INTEGER k
DrawControls
k = WaitKey(0)
END SUB
SUB KeyLine(y AS INTEGER, lb$, v$)
TEXT 14, y, lb$, "LT", 7, 1, cYellow
TEXT 150, y, v$, "LT", 7, 1, cWhite
END SUB
FUNCTION WaitKey(ms AS INTEGER) AS INTEGER
LOCAL FLOAT t
LOCAL k$ LENGTH 2
t = TIMER + ms
DO
k$ = INKEY$
IF k$ <> "" THEN WaitKey = ASC(k$) : EXIT FUNCTION
LOOP UNTIL ms > 0 AND TIMER > t
WaitKey = 0
END FUNCTION
SUB RunGame
quitGame = 0
frames = 0
tFlight = 0
DO
IF docked THEN
RunDocked
ELSE
RunFlight
ENDIF
LOOP UNTIL quitGame OR dead
END SUB
SUB RunFlight
LOCAL FLOAT t0
t0 = TIMER
ResetTick
DO
NextTick
ReadKeys
IF kQuit THEN
IF eqOwned(EQ_POD) THEN
EscapePod
EXIT DO
ELSE
quitGame = 1
EXIT DO
ENDIF
ENDIF
UpdatePlayer
IF kFire THEN FireLaser
IF kTarget THEN TargetMissile
IF kMissile THEN LaunchMissile
IF kECM THEN FireECM
IF kDock THEN
IF eqOwned(EQ_DOCK) THEN dockComp = 1 - dockComp ELSE Sfx SFX_BOOP
ENDIF
IF dockComp THEN DockingComputer
IF kJump THEN JumpAway
IF kBomb THEN EnergyBomb
IF kHop THEN InSystemJump
IF kGal THEN GalacticJump
IF kChart > 0 THEN
tStage = TIMER : IF ecmActive > 0 THEN Sfx SFX_ECMOFF
InfoScreen kChart
t0 = t0 + TIMER - tStage
ResetTick : IF ecmActive > 0 THEN Sfx SFX_ECM
ENDIF
IF tickWhole THEN
IF lasTimer > 0 THEN lasTimer = lasTimer - 1
IF lasFlash > 0 THEN lasFlash = lasFlash - 1
ENDIF
tStage = TIMER
MoveShips
Contact
Missiles
DockCheck
IF tickWhole THEN
Tactics
ECMService
Recharge
EnergyWarning
Altitude
CabinTemp
StationCheck
StationPolice
StationTraffic
HyperCount
SpawnTraffic
mcnt = (mcnt + 1) AND 255
ENDIF
prof(5) = prof(5) + TIMER - tStage
DrawFrame
IF demoMode THEN DemoCaption
FRAMEBUFFER COPY F, N
IF kPause THEN PauseGame
frames = frames + 1
LOOP UNTIL dead OR docked
tFlight = tFlight + TIMER - t0
IF docked THEN
HangarScreen
MissionCheck
ENDIF
END SUB
SUB PauseGame
LOCAL INTEGER k
DO
TEXT VCX, VIEWH - 12, "PAUSED", "CT", 7, 1, cWhite
FRAMEBUFFER COPY F, N
k = WaitKey(0)
IF k <> 68 AND k <> 100 THEN EXIT DO
tick = 0
DrawFrame
FRAMEBUFFER COPY F, N
shotNo = shotNo + 1
SAVE IMAGE homeDir$ + "SCREEN" + STR$(shotNo) + ".BMP"
LOOP
ResetTick
END SUB
SUB JumpAway
IF hypCount > 0 THEN EXIT SUB
IF inSafe THEN EXIT SUB
IF selSys = homeSys THEN EXIT SUB
IF CanReach(selSys) = 0 THEN EXIT SUB
hypCount = 15
hypTick = 15
Sfx SFX_BEEP
END SUB
SUB HyperCount
IF hypCount = 0 THEN EXIT SUB
hypTick = hypTick - 1
IF hypTick > 0 THEN EXIT SUB
hypTick = 5
hypCount = hypCount - 1
IF hypCount > 0 THEN
Sfx SFX_BEEP
EXIT SUB
ENDIF
IF inSafe OR CanReach(selSys) = 0 THEN
Message "HYPERSPACE ABORTED"
Sfx SFX_BOOP
EXIT SUB
ENDIF
Hyperspace selSys
END SUB
SUB RunDocked
LOCAL INTEGER k, dirty
dscreen = SCR_STATUS
dsel = 0
dirty = 1
DO
IF dirty THEN
DrawDocked
FRAMEBUFFER COPY F, N
dirty = 0
ENDIF
k = DockKey()
dirty = 1
SELECT CASE k
CASE 27
quitGame = 1
EXIT SUB
CASE 145
LaunchTunnel
docked = 0
dockComp = 0
msLock = -1
LaunchState
EXIT SUB
CASE 146 : dscreen = SCR_MARKET : dbuy = 1 : dsel = 0
CASE 147 : dscreen = SCR_MARKET : dbuy = 0 : dsel = 0
CASE 148 : dscreen = SCR_EQUIP  : dsel = 0
CASE 149 : dscreen = SCR_LONG
CASE 150 : dscreen = SCR_SHORT
CASE 151 : dscreen = SCR_DATA
CASE 152 : dscreen = SCR_MARKET : dbuy = 1 : dsel = 0
CASE 153 : dscreen = SCR_STATUS
CASE 154 : dscreen = SCR_INVENT
CASE 128 : chartStep = 1 : DockUp
CASE 129 : chartStep = 1 : DockDown
CASE 130 : chartStep = 1 : DockLeft
CASE 131 : chartStep = 1 : DockRight
CASE 164 : chartStep = CHARTFAST : DockUp
CASE 161 : chartStep = CHARTFAST : DockDown
CASE 162 : chartStep = CHARTFAST : DockLeft
CASE 163 : chartStep = CHARTFAST : DockRight
CASE 32
DockAct
CASE 13
IF dscreen = SCR_MARKET THEN
TradeAmount dsel, dbuy
ELSE
DockAct
ENDIF
CASE 70, 102
IF dscreen = SCR_EQUIP THEN
BuyFuel
ELSEIF dscreen = SCR_LONG OR dscreen = SCR_SHORT THEN
FindByName
ENDIF
CASE 83, 115
SaveCommander cmdrFile$
dscreen = SCR_STATUS
CASE 76, 108
IF LoadCommander(cmdrFile$) THEN dscreen = SCR_STATUS
CASE ELSE
dirty = 0
END SELECT
LOOP
END SUB
SUB DockAct
SELECT CASE dscreen
CASE SCR_MARKET
IF dbuy THEN BuyOne dsel ELSE SellOne dsel
CASE SCR_EQUIP
BuyShopItem
END SELECT
END SUB
FUNCTION DockKey() AS INTEGER
IF demoMode THEN DockKey = DemoKey() : EXIT FUNCTION
DockKey = WaitKey(0)
END FUNCTION
FUNCTION AskText$(p$, most AS INTEGER)
LOCAL INTEGER k
LOCAL t$ LENGTH 20
t$ = ""
DO
DrawDocked
BOX 0, SCRH - 12, SCRW, 12, 0, cBlack, cBlack
TEXT VCX, SCRH - 9, p$ + t$ + "_", "CT", 7, 1, cYellow
FRAMEBUFFER COPY F, N
k = DockKey()
IF k = 27 THEN AskText$ = "" : EXIT FUNCTION
IF k = 13 THEN EXIT DO
IF k = 8 THEN
IF LEN(t$) > 0 THEN t$ = LEFT$(t$, LEN(t$) - 1)
ELSEIF k >= 32 AND k < 127 THEN
IF LEN(t$) < most THEN t$ = t$ + CHR$(k)
ENDIF
LOOP
AskText$ = t$
END FUNCTION
SUB DrawDocked
SELECT CASE dscreen
CASE SCR_STATUS
StatusScreen
DockFooter "F1 launch   S save   L load"
CASE SCR_INVENT
InventoryScreen
DockFooter "F1 launch   F2 buy   F3 sell"
CASE SCR_MARKET
MarketScreen dsel
IF dbuy THEN
DockFooter "up/down choose  SPACE one  RETURN how many  F3 sell"
ELSE
DockFooter "up/down choose  SPACE one  RETURN how many  F2 buy"
ENDIF
CASE SCR_EQUIP
EquipScreen dsel
DockFooter "up/down choose   SPACE buy   F fill the tank"
CASE SCR_LONG
ChartLong
DockFooter "arrows move  shift+arrow faster  F find  F7 data"
CASE SCR_SHORT
ChartShort
DockFooter "arrows move  shift+arrow faster  F find  F7 data"
CASE SCR_DATA
SysDataScreen
DockFooter "F5 galactic   F6 short range   F7 data   F1 launch"
END SELECT
END SUB
SUB DockFooter(t$)
IF demoMode THEN
TEXT VCX, SCRH - 9, "DEMONSTRATION - PRESS ANY KEY TO PLAY", "CT", 7, 1, cDim
ELSE
TEXT VCX, SCRH - 9, t$, "CT", 7, 1, cDim
ENDIF
END SUB
SUB DockUp
IF dscreen = SCR_MARKET OR dscreen = SCR_EQUIP THEN
dsel = dsel - 1
IF dsel < 0 THEN dsel = 0
ELSE
curY = curY - 4 * chartStep : ChartMoved
ENDIF
END SUB
SUB DockDown
LOCAL INTEGER nrows
IF dscreen = SCR_MARKET THEN
nrows = NGOODS - 1
ELSEIF dscreen = SCR_EQUIP THEN
nrows = ShopCount() - 1
ELSE
curY = curY + 4 * chartStep : ChartMoved
EXIT SUB
ENDIF
dsel = dsel + 1
IF dsel > nrows THEN dsel = nrows
IF dsel < 0 THEN dsel = 0
END SUB
SUB DockLeft
IF dscreen <> SCR_MARKET AND dscreen <> SCR_EQUIP THEN
curX = curX - 2 * chartStep : ChartMoved
ENDIF
END SUB
SUB DockRight
IF dscreen <> SCR_MARKET AND dscreen <> SCR_EQUIP THEN
curX = curX + 2 * chartStep : ChartMoved
ENDIF
END SUB
SUB ChartMoved
IF curX < 0 THEN curX = 0
IF curX > 255 THEN curX = 255
IF curY < 0 THEN curY = 0
IF curY > 255 THEN curY = 255
FindSystem curX, curY
END SUB
FUNCTION ShopCount() AS INTEGER
LOCAL INTEGER i, k
GotoSystem gGal, homeSys
SysData
k = 0
FOR i = 0 TO NEQUIP - 1
IF eqTech(i) <= sysTech + 1 THEN k = k + 1
NEXT i
ShopCount = k
END FUNCTION
SUB BuyShopItem
LOCAL INTEGER i, k
GotoSystem gGal, homeSys
SysData
k = 0
FOR i = 0 TO NEQUIP - 1
IF eqTech(i) <= sysTech + 1 THEN
IF k = dsel THEN BuyEquip i : EXIT SUB
k = k + 1
ENDIF
NEXT i
END SUB
SUB InfoScreen(which AS INTEGER)
LOCAL INTEGER k, w, dirty
w = which
dirty = 1
DO
IF dirty THEN
SELECT CASE w
CASE 1 : ChartLong
CASE 2 : ChartShort
CASE 3 : SysDataScreen
CASE 4 : MarketScreen(-1)
CASE 5 : StatusScreen
CASE 6 : InventoryScreen
END SELECT
DockFooter "F5-F10 screens   arrows move   F1-F4 back"
FRAMEBUFFER COPY F, N
dirty = 0
ENDIF
k = DockKey()
dirty = 1
SELECT CASE k
CASE 128 : curY = curY - 4 : ChartMoved
CASE 129 : curY = curY + 4 : ChartMoved
CASE 130 : curX = curX - 2 : ChartMoved
CASE 131 : curX = curX + 2 : ChartMoved
CASE 149 TO 154 : w = k - 148
CASE 145 TO 148 : vw = k - 145 : EXIT SUB
CASE 13, 27 : EXIT SUB
CASE ELSE : dirty = 0
END SELECT
LOOP
END SUB
SUB NewGame
NewCommander
ClearSlots
docked = 1
dscreen = SCR_STATUS
dbuy = 1
END SUB
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
cashTenths = 150000
demoScrn = DEMOREAD
demoPhase = 0
demoRock = -999
demoScoop = 0
demoMined = 0
eqOwned(EQ_DOCK) = 1
ClearSlots
docked = 1
dscreen = SCR_STATUS
dbuy = 1
DemoPickTarget
DrawControls
IF DemoHold(DEMOHELP, 0) = 27 THEN demoStop = 1 : EXIT DO
IF demoMode = 0 THEN EXIT DO
RunGame
LOOP UNTIL demoStop OR DEMOLOOP = 0
demoMode = 0
END SUB
FUNCTION DemoKey() AS INTEGER
LOCAL INTEGER k
IF docked = 0 THEN
DemoKey = DemoHold(demoScrn, 13)
EXIT FUNCTION
ENDIF
IF demoStep >= dkCount THEN
DemoKey = DemoHold(DEMOREAD, 27)
EXIT FUNCTION
ENDIF
k = DemoHold(dkWait(demoStep), dkKey(demoStep))
IF demoMode = 0 THEN DemoKey = k : EXIT FUNCTION
demoStep = demoStep + 1
IF k = 145 AND demoAsk = 0 THEN demoLeg = demoLeg + 1 : demoTick = 0
DemoKey = k
END FUNCTION
FUNCTION DemoHold(ms AS INTEGER, k AS INTEGER) AS INTEGER
LOCAL FLOAT t
LOCAL kb$ LENGTH 2
t = TIMER + ms
DO
kb$ = INKEY$
IF kb$ <> "" THEN
demoStop = 1
demoMode = 0
demoCap$ = ""
quitGame = 1
IF kb$ <> CHR$(27) THEN demoTakeover = 1
DemoHold = 27
EXIT FUNCTION
ENDIF
LOOP UNTIL TIMER > t
DemoHold = k
END FUNCTION
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
SELECT CASE demoLeg
CASE 0, 1 : DemoLeg1
CASE 2    : DemoLeg2
CASE ELSE : DemoLeg3
END SELECT
END SUB
SUB DemoLeg1
SELECT CASE demoTick
CASE 1 TO 55    : kFaster = 1
CASE 90 TO 145  : kRollR = 1
CASE 185 TO 240 : kRollL = 1
CASE 300        : vw = 1 : demoCap$ = "REAR VIEW: LAVE STATION BEHIND US"
CASE 620        : vw = 2 : demoCap$ = "LEFT VIEW"
CASE 940        : vw = 3 : demoCap$ = "RIGHT VIEW"
CASE 1260       : vw = 0 : demoCap$ = ""
CASE 1310       : demoTgt = DemoSpawn(T_ANACONDA, 250, 120, 3000, 16, 0, 0)
demoCap$ = "AN ANACONDA: THE BIGGEST TRADER"
CASE 1560       : demoCap$ = ""
CASE 1600       : demoTgt = DemoSpawn(T_VIPER, -700, 200, 2200, 20, 128 OR 48, 180)
demoCap$ = "POLICE: THEY HAVE SEEN THE SLAVES"
CASE 2120       : demoCap$ = "AN ASTEROID"
demoTgt = DemoSpawn(T_ASTEROID, 150, -100, 2600, 0, 0, 180)
IF demoTgt >= 0 THEN sPit(demoTgt) = 127
CASE 2150       : demoCap$ = "MISSILE LOCKED"
CASE 2200       : kMissile = 1 : demoCap$ = "MISSILE AWAY"
CASE 2330       : demoCap$ = ""
CASE 2580       : DemoIncoming
demoCap$ = "INCOMING MISSILE"
CASE 2690       : kECM = 1 : demoCap$ = "E.C.M."
CASE 2780       : demoCap$ = ""
CASE 2810       : kChart = 4
CASE 2840       : kChart = 1
CASE 2870       : demoScrn = 8000 : kChart = 3
CASE 2871       : demoScrn = DEMOREAD
CASE 2910       : DemoPickTarget
inSafe = 0
demoCap$ = "CLEAR OF THE SAFE ZONE"
CASE 2960       : kJump = 1 : demoCap$ = "HYPERSPACE: FIFTEEN AND COUNTING"
END SELECT
IF demoTick > 2965 AND hypCount = 0 THEN demoLeg = 2 : demoTick = 0 : demoCap$ = ""
IF demoTick > 1330 AND demoTick < 1550 THEN DemoAim demoTgt, 0
IF demoTick > 1620 AND demoTick < 2100 THEN DemoAim DemoNearestFoe(), 1
IF demoTick > 2130 AND demoTick < 2199 THEN DemoAim demoTgt, 0
IF demoTick > 2150 AND demoTick < 2199 THEN kTarget = 1
IF demoTick > 2205 AND demoTick < 2320 THEN DemoAim demoTgt, 0
IF demoTick > 3700 THEN kQuit = 1
END SUB
SUB DemoLeg2
SELECT CASE demoTick
CASE 1          : demoCap$ = "ARRIVED AT " + SysName$()
CASE 2 TO 55    : kFaster = 1
CASE 90         : vw = 1 : demoCap$ = "THE SUN, BEHIND US"
CASE 190        : vw = 0 : demoCap$ = ""
CASE 240        : DemoPack 2
demoCap$ = "PIRATES, DRAWN FROM THE PACK OF EIGHT"
CASE 620        : DemoPack 2
demoCap$ = "AND TWO MORE"
CASE 900        : demoCap$ = ""
CASE 940        : DemoCloseOnStation
demoCap$ = "THE STATION IS IN RANGE"
CASE 950        : kDock = 1 : demoCap$ = "DOCKING COMPUTER ENGAGED"
CASE 1150       : demoCap$ = ""
END SELECT
IF demoTick > 260 AND demoTick < 900 THEN DemoAim DemoNearestFoe(), 1
IF demoTick > 5200 THEN kQuit = 1
END SUB
SUB DemoLeg3
LOCAL INTEGER mined
SELECT CASE demoTick
CASE 1          : demoCap$ = "A MINING LASER ON THE FORE MOUNT"
demoPhase = 0
demoRock = -999
demoScoop = 0
demoMined = cargo(12) + cargo(15)
CASE 2 TO 45    : kFaster = 1
CASE 150        : demoCap$ = ""
END SELECT
IF demoTick < 160 THEN EXIT SUB
mined = 0
IF cargo(12) + cargo(15) > demoMined THEN mined = 1
IF demoPhase = 0 THEN
IF mined OR demoTick > 2400 THEN
demoPhase = 1
demoTgt = demoTick
DemoCloseOnStation
IF mined THEN demoCap$ = "ORE ABOARD" ELSE demoCap$ = ""
ELSE
DemoWorkRock
ENDIF
EXIT SUB
ENDIF
IF demoTick = demoTgt + 10 THEN kDock = 1
IF demoTick = demoTgt + 160 THEN demoCap$ = "DOCKING COMPUTER ENGAGED"
IF demoTick = demoTgt + 360 THEN demoCap$ = ""
IF demoTick > 5200 THEN kQuit = 1
END SUB
SUB DemoWorkRock
LOCAL INTEGER n, i
n = DemoNearestOf(T_ASTEROID)
IF n >= 0 THEN
demoCap$ = "AN ASTEROID, AND THE LASER THAT MINES IT"
DemoAim n, 1
EXIT SUB
ENDIF
n = DemoNearestOf(T_BOULDER)
IF n >= 0 THEN
demoCap$ = "BOULDERS: BREAK THEM AGAIN"
DemoAim n, 1
EXIT SUB
ENDIF
n = DemoNearestOf(T_SPLINTER)
IF n >= 0 THEN
IF demoScoop = 0 THEN
demoScoop = demoTick
demoCap$ = "SPLINTERS: MINERALS OR GEM-STONES"
i = 0
FOR n = 2 TO nUsed - 1
IF sTyp(n) = T_SPLINTER THEN
sX(n) = i * 70 - 70
sY(n) = -120
sZ(n) = 1400 + i * 500
sSpd(n) = 0
i = i + 1
ENDIF
NEXT n
EXIT SUB
ENDIF
DemoScoopAim DemoNearestOf(T_SPLINTER)
EXIT SUB
ENDIF
IF demoTick > demoRock + 90 THEN
demoRock = demoTick
demoScoop = 0
n = DemoSpawn(T_ASTEROID, 100, -60, 2800, 0, 0, 180)
IF n >= 0 THEN sPit(n) = 127
ENDIF
END SUB
SUB DemoAim(n AS INTEGER, fire AS INTEGER)
LOCAL FLOAT d, ux, uy, uz
IF n < 0 OR n >= nUsed THEN EXIT SUB
IF sTyp(n) = 0 OR sExp(n) > 0 THEN EXIT SUB
d = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
IF d < 1 THEN EXIT SUB
ux = sX(n) / d : uy = sY(n) / d : uz = sZ(n) / d
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
IF uz < 0 AND ABS(uy) < 0.05 THEN kUp = 1
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
IF fire THEN
IF uz > 0 AND sX(n)*sX(n) + sY(n)*sY(n) < bArea(sBp(n)) THEN kFire = 1
ENDIF
END SUB
SUB DemoScoopAim(n AS INTEGER)
LOCAL FLOAT d, ux, uy, uz
IF n < 0 OR n >= nUsed THEN EXIT SUB
IF sTyp(n) = 0 OR sExp(n) > 0 THEN EXIT SUB
d = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
IF d < 1 THEN EXIT SUB
ux = sX(n) / d : uy = sY(n) / d : uz = sZ(n) / d
IF ux > 0.02 THEN
kRollR = 1
ELSEIF ux < -0.02 THEN
kRollL = 1
ENDIF
IF uy < -0.10 THEN
kUp = 1
ELSEIF uy > -0.04 THEN
kDn = 1
ENDIF
IF uz < 0 AND ABS(uy) < 0.05 THEN kUp = 1
IF d > 900 THEN
IF dSpeed < 22 THEN kFaster = 1
ELSE
IF dSpeed > 10 THEN kSlower = 1
IF dSpeed < 8 THEN kFaster = 1
ENDIF
END SUB
FUNCTION DemoNearestOf(t AS INTEGER) AS INTEGER
LOCAL INTEGER n, best
LOCAL FLOAT d, bd
best = -1 : bd = 1e12
FOR n = 2 TO nUsed - 1
IF sTyp(n) = t AND sExp(n) = 0 THEN
d = sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n)
IF d < bd THEN bd = d : best = n
ENDIF
NEXT n
DemoNearestOf = best
END FUNCTION
FUNCTION DemoNearestFoe() AS INTEGER
LOCAL INTEGER n, best
LOCAL FLOAT d, bd
best = -1 : bd = 1e12
FOR n = 2 TO nUsed - 1
IF sTyp(n) <> 0 AND sBp(n) >= 0 AND sExp(n) = 0 AND (sAI(n) AND 128) <> 0 THEN
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
MATH Q_EULER RAD(hdg), 0, 0, qA() : qA(4) = 1
n = NewShip(t, x, y, z, qA())
IF n >= 0 THEN sSpd(n) = spd : sAI(n) = ai
DemoSpawn = n
END FUNCTION
SUB DemoPack(cnt AS INTEGER)
LOCAL INTEGER i, n
FOR i = 0 TO cnt - 1
n = DemoSpawn(PackShip(), (i - 1) * 550, 150 - (i AND 1) * 300, 2400 + i * 400, 22, 128 OR 56, 180)
NEXT i
END SUB
SUB DemoIncoming
LOCAL INTEGER n
MATH Q_EULER RAD(180), 0, 0, qA() : qA(4) = 1
n = NewShip(T_MISSILE, 700, 0, 2400, qA())
IF n >= 0 THEN
sSpd(n) = bSpd(sBp(n))
sAI(n) = 128 OR 126
sTgt(n) = -2
ENDIF
END SUB
SUB DemoCloseOnStation
IF sTyp(SLOT_PLANET) = 0 THEN EXIT SUB
pRoll = JCENTRE : pPitch = JCENTRE
dSpeed = 8
IF sTyp(SLOT_STAR) = T_STATION THEN KillShip SLOT_STAR
sX(SLOT_PLANET) = 0
sY(SLOT_PLANET) = 0
sZ(SLOT_PLANET) = 2 * PRADIUS + 3000
mcnt = 0
END SUB
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
SUB DemoCaption
IF demoCap$ = "" THEN EXIT SUB
TEXT VCX, VIEWH - 26, demoCap$, "CT", 7, 1, cDim
END SUB
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
DATA 152,3500
DATA 146,3000
DATA 13,1200
DATA 49,400
DATA 50,400
DATA 13,1600
DATA 129,400
DATA 129,300
DATA 129,400
DATA 32,400
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,900
DATA 148,2500
DATA 32,1300
DATA 129,700
DATA 32,1300
DATA 129,700
DATA 32,1300
DATA 129,700
DATA 129,700
DATA 32,700
DATA 145,1500
DATA 129,700
DATA 32,1500
DATA 149,3000
DATA 131,400
DATA 131,400
DATA 129,600
DATA 162,500
DATA 164,500
DATA 163,500
DATA 161,900
DATA 70,1200
DATA 90,300
DATA 65,250
DATA 79,250
DATA 78,250
DATA 67,250
DATA 69,600
DATA 13,2000
DATA 151,6500
DATA 150,3000
DATA 154,3000
DATA 153,3000
DATA 145,3500
DATA 147,4500
DATA 13,1200
DATA 49,400
DATA 50,400
DATA 13,1600
DATA 129,400
DATA 129,300
DATA 129,400
DATA 32,400
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,900
DATA 148,2500
DATA 129,200
DATA 129,200
DATA 129,200
DATA 129,200
DATA 129,200
DATA 129,200
DATA 129,200
DATA 129,200
DATA 129,200
DATA 129,200
DATA 129,200
DATA 129,200
DATA 129,400
DATA 129,900
DATA 32,1500
DATA 145,1800
DATA 128,1000
DATA 32,1500
DATA 146,1800
DATA 145,3500
DATA 154,4000
DATA 153,5000
DATA -1,0
