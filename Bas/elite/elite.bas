' Elite for the PicoMite, built by elite_tools/build.py from src/*.bas.
' Comments and indentation are stripped to fit program memory - read src/.
OPTION TRACECACHE ON 80
OPTION CACHE DEBUG ON
OPTION CACHE SUB DrawStardust, DrawScanner
OPTION EXPLICIT
OPTION BASE 0
OPTION DEFAULT NONE
CONST SCRW = 320, SCRH = 240
CONST VIEWH = 176
CONST VCX = 160, VCY = 88
CONST DASHY = 176
CONST VPLANE = 256
CONST DEMOFRAMES = 0
CONST DEMOSCENE = 1
CONST PANY = VCY - (SCRH \ 2 - 1)
CONST NSLOT = 12
CONST NBP = 12
CONST SLOT_PLANET = 0
CONST SLOT_STAR = 1
CONST PRADIUS = 24576
CONST FAROFF = 57344
CONST SAFEZONE = 49152
CONST T_PLANET = 128, T_SUN = 129, T_CRATER = 130
CONST T_SIDEWINDER = 1, T_VIPER = 2, T_MAMBA = 3, T_PYTHON = 4
CONST T_COBRA3 = 5, T_THARGOID = 6, T_TRADER = 7, T_STATION = 8
CONST T_MISSILE = 9, T_ASTEROID = 10, T_CANISTER = 11, T_THARGON = 12
CONST T_ESCAPE = 13
DIM INTEGER pRoll, pPitch, alp1, alp2, bet1, bet2, dSpeed
DIM FLOAT alpha, beta
DIM INTEGER pEnergy, pFsh, pAsh, pFuel, pCabT, pLasT, pAltit, pMissl
DIM INTEGER vw
DIM INTEGER mcnt
DIM INTEGER inSafe
DIM INTEGER sTyp(NSLOT-1), sBp(NSLOT-1), sObj(NSLOT-1)
DIM FLOAT sX(NSLOT-1), sY(NSLOT-1), sZ(NSLOT-1)
DIM FLOAT sQ(4, NSLOT-1)
DIM INTEGER sSpd(NSLOT-1), sAcc(NSLOT-1), sRol(NSLOT-1), sPit(NSLOT-1)
DIM INTEGER sEne(NSLOT-1), sAI(NSLOT-1), sFlg(NSLOT-1), sExp(NSLOT-1)
DIM INTEGER sTgt(NSLOT-1)
DIM INTEGER sMis(NSLOT-1)
DIM INTEGER nUsed
DIM bName$(NBP-1) LENGTH 16
DIM INTEGER bNv(NBP-1), bNf(NBP-1), bNfv(NBP-1), bNf0(NBP-1), bNv0(NBP-1)
DIM INTEGER bCan(NBP-1), bArea(NBP-1), bBty(NBP-1), bVis(NBP-1)
DIM INTEGER bEne(NBP-1), bSpd(NBP-1), bLas(NBP-1), bMis(NBP-1)
DIM INTEGER bGun(NBP-1), bExp(NBP-1), bSize(NBP-1)
DIM INTEGER tBp(13)
DIM FLOAT mV(2, 39), mNrm(2, 15)
DIM INTEGER mFc(31), mHost(31), mF(159), mEc(31), mFl(31)
DIM INTEGER col(6)
DIM INTEGER cGreen, cYellow, cWhite, cBlack, cCyan, cGrey, cRed
DIM INTEGER maxObj, objOwn(15)
DIM FLOAT qA(4), qB(4), qC(4), qV(4), qP(4), vwQ(4, 3)
DIM INTEGER kRollL, kRollR, kUp, kDn, kFaster, kSlower, kFire, kQuit
DIM INTEGER kView, kPause, kTarget, kMissile, kECM, kDock, kJump, kChart
DIM INTEGER kHeld
CONST KB_TARGET = 1, KB_MISSILE = 2, KB_ECM = 4, KB_DOCK = 8, KB_JUMP = 16
CONST KB_SCREEN = 32
CONST KB_SCREENS = 32+64+128+256+512+1024
DIM FLOAT frameMs, tFrame, tStage, tFlight
DIM INTEGER frames
DIM FLOAT prof(5)
DIM INTEGER gs0, gs1, gs2, gSys, gGal
DIM INTEGER sysX, sysY, sysGov, sysEco, sysTech, sysPop, sysProd, sysRad
DIM INTEGER homeX, homeY, homeSys, curX, curY, selSys, inWitch
CONST DIGRAPHS = "ALLEXEGEZACEBISOUSESARMAINDIREA?ERATENBERALAVETIEDORQUANTEISRION"
CONST LASPULSE = 4
DIM INTEGER lasTimer, lasPower, lasFlash, kills, dead, energyUnit, shots, hits
CONST DOCKRANGE = 280
CONST DOCKFACE = 0.896
CONST DOCKCONE = 0.927
CONST DOCKROLL = 0.833
CONST MSTURN = 0.22
CONST ECMFRAMES = 24
DIM INTEGER msLock, ecmActive, legal, docked, dockComp
DIM INTEGER spawnEV
CONST UNIT = 65536
CONST LAUNCHSPD = 12
CONST CHTOP = 24
CONST SRCX = 130
CONST SRCY = 90
CONST SRDX = 5
CONST SRDY = 2
CONST NEQUIP = 8
CONST EQ_SCOOPS = 4
DIM eqName$(NEQUIP-1) LENGTH 20
DIM INTEGER eqPrice(NEQUIP-1), eqTech(NEQUIP-1), eqOwned(NEQUIP-1)
CONST NGOODS = 17
DIM mkName$(NGOODS-1) LENGTH 14, mkUnit$(NGOODS-1) LENGTH 2
DIM INTEGER mkBase(NGOODS-1), mkFact(NGOODS-1), mkQty(NGOODS-1), mkMask(NGOODS-1)
DIM INTEGER mkPrice(NGOODS-1), mkStock(NGOODS-1), mkByte
DIM INTEGER cargo(NGOODS-1), holdSize, cashTenths
DIM INTEGER solidMode, showDot
DIM FLOAT tx, ty, tz
CONST JCENTRE = 128
CONST JROLLSTEP = 7
CONST JPITCHSTEP = 14
CONST JDAMPROLL = 2
CONST JDAMPPITCH = 1
CONST MAXSPEED = 40
CONST ANGSCALE = 256
CONST SELFROT = 0.0625
CONST NPCSPEED = 1.5
CONST TIDYEVERY = 16
CONST NEARZ = 32
CONST FARXY = 30000
CONST ZHI = 256
CONST VISFLOOR = 16
CONST VISCUT = 192
CONST NSTAR = 18
CONST NSEG = 16
DIM FLOAT ctab(NSEG-1), stab(NSEG-1), pgx(NSEG-1), pgy(NSEG-1)
DIM FLOAT stX(NSTAR-1), stY(NSTAR-1), stZ(NSTAR-1)
DIM INTEGER spx(4*NSTAR-1), spy(4*NSTAR-1), spc(4*NSTAR-1)
DIM INTEGER DLY(5), DRY(6)
DIM LLAB$(5) LENGTH 3, RLAB$(6) LENGTH 3
CONST DL = 20
CONST DR = 260
CONST DW = 40
CONST SCX = 155
CONST SCY = 204
CONST SCA = 86
CONST SCB = 18
CONST SCDOTX = 154
CONST SCXDIV = 204.8
CONST SCZDIV = 1024
CONST SCYDIV = 512
CONST SCTOP = 178
CONST SCBOT = 230
CONST CPX = 244
CONST CPY = 187
CONST CPR = 9
CONST PROFILE = 1
CONST MSGX = 90, MSGY = 160, MSGTIME = 1800
DIM msgText$ LENGTH 40
DIM FLOAT msgUntil
CONST SOUNDON = 1
CONST NSFX = 9
CONST SFX_LASER = 0, SFX_HIT = 1, SFX_BOOM = 2, SFX_BOOMT = 3, SFX_BEEP = 4
CONST SFX_BOOP = 5, SFX_LAUNCH = 6, SFX_HYPER = 7, SFX_ECM = 8
DIM INTEGER sfxCh(NSFX-1), sfxWv(NSFX-1), sfxF0(NSFX-1), sfxF1(NSFX-1)
DIM INTEGER sfxMs(NSFX-1), sfxVol(NSFX-1), sfxWb(NSFX-1)
DIM INTEGER chWv(4), chF0(4), chF1(4), chVol(4), chWb(4), chLast(4)
DIM FLOAT chT0(4), chT1(4)
CONST SCR_STATUS = 0, SCR_INVENT = 1, SCR_MARKET = 2, SCR_EQUIP = 3
CONST SCR_LONG = 4, SCR_SHORT = 5, SCR_DATA = 6
CONST CMDRFILE = "A:/cmdr.txt"
DIM INTEGER quitGame, dscreen, dsel, dbuy, titleKey
CONST DEMOPLAY = 1
CONST TITLEPIC = "A:/title.jpg"    ' drawn by elite_tools/titlescreen.py
CONST TITLEWAIT = 20000
CONST DEMOLOOP = 1
CONST DEMOREAD = 3000
DIM INTEGER demoMode, demoStop, demoStep, demoLeg, demoTick, demoTgt, demoTakeover
DIM INTEGER dkKey(127), dkWait(127), dkCount
DIM demoCap$ LENGTH 40
SetupScreen
LoadStats
ProbeObjects
SetupViews
EquipTable
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
IF DEMOSCENE = 3 THEN
DockedScreens
FRAMEBUFFER CLOSE
MODE 1
PRINT "docked screens done"
END
ELSEIF DEMOSCENE = 2 THEN
DockScene
ELSE
TestScene
ENDIF
frames = 0
tFrame = TIMER
DO
IF DEMOSCENE = 2 THEN DockInput frames ELSE DemoInput frames
IF kQuit OR dead OR docked THEN EXIT DO
UpdatePlayer
IF kFire THEN FireLaser
IF kTarget THEN TargetMissile
IF kMissile THEN LaunchMissile
IF kECM THEN FireECM
IF kDock THEN dockComp = 1 - dockComp
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
FRAMEBUFFER COPY F, N, B
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
MODE 1
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
tBp(11) = 9 : tBp(12) = 10 : tBp(13) = 11
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
mFl(j) = 1 + (mHost(j) MOD 6)
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
sExp(n) = 0 : sTgt(n) = -1
sMis(n) = 0
IF t < T_PLANET THEN
sBp(n) = tBp(t)
sEne(n) = bEne(sBp(n))
sMis(n) = bMis(sBp(n))
GetObject n
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
SUB SetupScreen
MODE 2
FRAMEBUFFER CREATE
FRAMEBUFFER WRITE F
Draw3D CAMERA 1, VPLANE, 0, 0, 0, PANY
col(0) = RGB(WHITE) : col(1) = RGB(GRAY) : col(2) = RGB(BLUE)
col(3) = RGB(GREEN) : col(4) = RGB(RED) : col(5) = RGB(MAGENTA)
col(6) = RGB(CYAN)
cGreen = RGB(GREEN) : cYellow = RGB(YELLOW) : cWhite = RGB(WHITE)
cBlack = RGB(BLACK) : cCyan = RGB(CYAN) : cGrey = RGB(64, 64, 64)
cRed = RGB(RED)
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
SUB SetupViews
LOCAL INTEGER i
MATH Q_EULER 0, 0, 0, qA()      : FOR i = 0 TO 4 : vwQ(i, 0) = qA(i) : NEXT i
MATH Q_CREATE RAD(180), 0, 1, 0, qA() : FOR i = 0 TO 4 : vwQ(i, 1) = qA(i) : NEXT i
MATH Q_CREATE RAD(90), 0, 1, 0, qA()  : FOR i = 0 TO 4 : vwQ(i, 2) = qA(i) : NEXT i
MATH Q_CREATE RAD(-90), 0, 1, 0, qA() : FOR i = 0 TO 4 : vwQ(i, 3) = qA(i) : NEXT i
END SUB
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
kChart = 0
IF (hnew AND KB_SCREENS) <> 0 THEN
FOR i = 0 TO 5
IF (hnew AND (KB_SCREEN << i)) <> 0 THEN kChart = i + 1 : EXIT FOR
NEXT i
ENDIF
END SUB
SUB UpdatePlayer
LOCAL INTEGER d
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
LOCAL FLOAT k
MATH Q_CREATE -alpha, 0, 0, 1, qA()
MATH Q_CREATE beta, 1, 0, 0, qB()
MATH Q_MULT qB(), qA(), qP()
n = 0
DO WHILE n < nUsed
IF sTyp(n) = 0 THEN
n = n + 1
ELSE
IF sBp(n) >= 0 AND sSpd(n) <> 0 THEN
NoseVec n
sX(n) = sX(n) + qV(1) * qV(4) * sSpd(n) * NPCSPEED
sY(n) = sY(n) + qV(2) * qV(4) * sSpd(n) * NPCSPEED
sZ(n) = sZ(n) + qV(3) * qV(4) * sSpd(n) * NPCSPEED
ENDIF
IF sAcc(n) <> 0 THEN
mag = sSpd(n) + sAcc(n)
IF mag < 0 OR mag > 127 THEN mag = 0
IF mag > bSpd(sBp(n)) THEN mag = bSpd(sBp(n))
sSpd(n) = mag
sAcc(n) = 0
ENDIF
k = sY(n) - alpha * sX(n)
sZ(n) = sZ(n) + beta * k
sY(n) = k - beta * sZ(n)
sX(n) = sX(n) + alpha * sY(n)
sZ(n) = sZ(n) - dSpeed
IF sBp(n) >= 0 THEN
MATH SLICE sQ(), , n, qA()
MATH Q_MULT qP(), qA(), qC()
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
LOCAL INTEGER n, px, py, zb
FOR n = 0 TO nUsed - 1
IF sTyp(n) <> 0 AND sBp(n) >= 0 THEN
ViewXform n
IF tz > NEARZ THEN
zb = tz \ ZHI
IF zb < VISCUT AND ABS(tx) < tz AND ABS(ty) < tz THEN
IF zb >= VISFLOOR AND zb > bVis(sBp(n)) THEN
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
SUB SpaceFurniture
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
POLYGON NSEG, pgx(), pgy(), cWhite
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
IF have THEN LINE lx, ly, px, py, 1, cWhite
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
LOCAL FLOAT q, x, y, z, a, b, h, qb, d, dsg, ratsg
a = alp2 * alp1
b = bet2 * bet1
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
SUB LoadSounds
LOCAL INTEGER i
RESTORE dat_sfx
FOR i = 0 TO NSFX - 1
READ sfxCh(i), sfxWv(i), sfxF0(i), sfxF1(i), sfxMs(i), sfxVol(i), sfxWb(i)
NEXT i
FOR i = 1 TO 4 : chT1(i) = 0 : chLast(i) = -1 : NEXT i
END SUB
SUB Sfx(n AS INTEGER)
LOCAL INTEGER c
IF SOUNDON = 0 THEN EXIT SUB
c = sfxCh(n)
chWv(c) = sfxWv(n)
chF0(c) = sfxF0(n) : chF1(c) = sfxF1(n)
chVol(c) = sfxVol(n) : chWb(c) = sfxWb(n)
chT0(c) = TIMER
chT1(c) = TIMER + sfxMs(n)
chLast(c) = chF0(c)
PlayCh c, chWv(c), chF0(c), chVol(c)
END SUB
SUB SfxStop(n AS INTEGER)
LOCAL INTEGER c
IF SOUNDON = 0 THEN EXIT SUB
c = sfxCh(n)
IF chT1(c) = 0 THEN EXIT SUB
chT1(c) = 0
PLAY SOUND c, B, O
END SUB
SUB SoundService
LOCAL INTEGER c, f
LOCAL FLOAT t, k
IF SOUNDON = 0 THEN EXIT SUB
t = TIMER
FOR c = 1 TO 4
IF chT1(c) > 0 THEN
IF t >= chT1(c) THEN
chT1(c) = 0
PLAY SOUND c, B, O
ELSE
k = (t - chT0(c)) / (chT1(c) - chT0(c))
f = chF0(c) + (chF1(c) - chF0(c)) * k
IF chWb(c) THEN
IF (INT(t / 45) AND 1) = 1 THEN f = f * 3 \ 4
ENDIF
IF f < 1 THEN f = 1
IF f <> chLast(c) THEN
PlayCh c, chWv(c), f, chVol(c)
chLast(c) = f
ENDIF
ENDIF
ENDIF
NEXT c
END SUB
SUB PlayCh(c AS INTEGER, w AS INTEGER, f AS INTEGER, v AS INTEGER)
SELECT CASE w
CASE 0 : PLAY SOUND c, B, Q, f, v
CASE 1 : PLAY SOUND c, B, N, f, v
CASE ELSE : PLAY SOUND c, B, P, f, v
END SELECT
END SUB
SUB SoundOff
LOCAL INTEGER c
FOR c = 1 TO 4
chT1(c) = 0
PLAY SOUND c, B, O
NEXT c
PLAY STOP
END SUB
dat_sfx:
DATA 1, 0, 900, 122, 800, 12, 0
DATA 1, 0, 230, 150, 400, 15, 0
DATA 2, 1, 3000, 200, 1300, 18, 0
DATA 3, 0, 3891, 400, 1200, 10, 0
DATA 3, 0, 1839, 1839, 50, 15, 0
DATA 3, 0, 145, 145, 400, 18, 0
DATA 2, 1, 800, 200, 600, 15, 0
DATA 2, 2, 200, 2400, 800, 15, 0
DATA 4, 0, 1997, 1997, 1200, 12, 1
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
STATIC INTEGER wordcount = (SCRH - DASHY) * SCRW \ 16
STATIC INTEGER store(wordcount - 1)
STATIC INTEGER addr = 0, fadd = 0
IF fadd = 0 THEN
LOCAL INTEGER i
addr = PEEK(VARADDR store())
fadd = MM.INFO(WRITEBUFF) + DASHY * SCRW / 2
BOX 0, DASHY, SCRW, SCRH - DASHY, 0, cBlack, cBlack
LINE 0, DASHY, SCRW - 1, DASHY, 1, cCyan
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
BOX x + 1, y, DW, 4, 0, cBlack, cBlack
BOX x + 1 + v * 2.5, y, 3, 4, 0, cYellow, cYellow
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
c = cGreen
IF sTyp(n) = T_MISSILE THEN c = cYellow
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
TEXT 110, 8, v$, "LT", 7, 1, cWhite
END SUB
SUB NewCommander
LOCAL INTEGER i
pRoll = JCENTRE : pPitch = JCENTRE
pEnergy = 255 : pFsh = 255 : pAsh = 255 : pFuel = 70
pCabT = 30 : pLasT = 0 : pAltit = 200 : pMissl = 3
cashTenths = 1000 : holdSize = 20
lasPower = 15 : lasTimer = 0 : lasFlash = 0
kills = 0 : dead = 0 : energyUnit = 0 : legal = 0
shots = 0 : hits = 0
docked = 0 : dockComp = 0 : msLock = -1
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
SUB SaveShot(f AS INTEGER)
SAVE IMAGE "A:/fly" + STR$(f) + ".bmp"
END SUB
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
kJump = 0 : kChart = 0
END SUB
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
FOR i = 1 TO 5 : BuyOne 0 : NEXT i
FOR i = 1 TO 3 : BuyOne 12 : NEXT i
eqOwned(2) = 1
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
END SUB
SUB DataLine(y AS INTEGER, lb$, v$)
TEXT 20, y, lb$ + ":", "LT", 7, 1, cWhite
TEXT 150, y, v$, "LT", 7, 1, cYellow
END SUB
SUB ArriveInSystem
LOCAL INTEGER n, pz, sz, sx, ptype
ClearSlots
SysData
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
SUB FireLaser
LOCAL INTEGER n, best, bestz, dmg
IF lasTimer > 0 THEN EXIT SUB
IF pLasT >= 242 THEN EXIT SUB
IF lasPower = 0 THEN EXIT SUB
pLasT = pLasT + 8
IF pLasT > 255 THEN pLasT = 255
lasFlash = 2
IF lasPower >= 128 THEN lasTimer = 0 ELSE lasTimer = LASPULSE
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
dmg = lasPower AND 127
sEne(best) = sEne(best) - dmg
IF sAI(best) < 128 THEN sAI(best) = sAI(best) OR 128
IF sEne(best) <= 0 THEN
IF sTyp(best) = T_STATION THEN
sEne(best) = bEne(sBp(best))
AngerStation
ELSE
Explode best
ENDIF
ENDIF
END SUB
SUB Explode(n AS INTEGER)
sExp(n) = 18
Sfx SFX_BOOM
Sfx SFX_BOOMT
sSpd(n) = 0
sAI(n) = 0
kills = kills + 1
cashTenths = cashTenths + bBty(sBp(n))
IF bBty(sBp(n)) > 0 THEN Message STR$(bBty(sBp(n)) / 10) + " CR"
NoteKill n
EjectCargo n
DropObject n
END SUB
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
SUB Tactics
LOCAL INTEGER n, dmg
LOCAL FLOAT d, cnt, nx, ny, nz
FOR n = 2 TO nUsed - 1
IF sTyp(n) <> 0 AND sBp(n) >= 0 AND sExp(n) = 0 THEN
IF (sAI(n) AND 128) <> 0 THEN
IF inSafe THEN
IF sTyp(n) < T_COBRA3 THEN
IF sTyp(n) <> T_VIPER THEN sAI(n) = sAI(n) AND 129
ENDIF
ENDIF
IF ((mcnt XOR n) AND 7) = 0 THEN
d = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
IF d > 1 THEN
NoseVec n
nx = qV(1) * qV(4) : ny = qV(2) * qV(4) : nz = qV(3) * qV(4)
cnt = (-sX(n) * nx - sY(n) * ny - sZ(n) * nz) / d
IF d < 8192 AND cnt > 0.917 AND (sAI(n) AND 126) <> 0 THEN
dmg = bLas(sBp(n)) * 2
IF cnt > 0.972 THEN
HitPlayer dmg
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
NEXT n
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
SUB HitPlayer(dmg AS INTEGER)
LOCAL INTEGER dleft
dleft = dmg
Sfx SFX_HIT
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
HomeOn n, dx, dy, dz, d
ENDIF
ENDIF
ENDIF
NEXT n
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
SUB HomeOn(n AS INTEGER, dx AS FLOAT, dy AS FLOAT, dz AS FLOAT, d AS FLOAT)
LOCAL FLOAT nx, ny, nz, ax, ay, az, m
IF d < 1 THEN EXIT SUB
NoseVec n
nx = qV(1) * qV(4) : ny = qV(2) * qV(4) : nz = qV(3) * qV(4)
nx = nx + (dx / d - nx) * MSTURN
ny = ny + (dy / d - ny) * MSTURN
nz = nz + (dz / d - nz) * MSTURN
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
LOCAL INTEGER n
IF ecmActive > 0 THEN EXIT SUB
ecmActive = ECMFRAMES
Sfx SFX_ECM
FOR n = 2 TO nUsed - 1
IF sTyp(n) = T_MISSILE AND sExp(n) = 0 THEN
IF sTgt(n) = -2 THEN Message "MISSILE JAMMED"
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
pEnergy = pEnergy - 1
IF pEnergy < 0 THEN pEnergy = 0
IF ecmActive = 0 THEN SfxStop SFX_ECM
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
IF sTyp(n) = T_VIPER THEN legal = legal OR 64
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
HitPlayer 10
ELSE
pEnergy = 0
dead = 1
ENDIF
END SUB
SUB DoDock
docked = 1
dSpeed = 0
dockComp = 0
msLock = -1
pEnergy = 255 : pFsh = 255 : pAsh = 255
pLasT = 0 : pCabT = 30 : pAltit = 200
pRoll = JCENTRE : pPitch = JCENTRE
ClearSlots
END SUB
SUB DockingComputer
LOCAL INTEGER n
LOCAL FLOAT d, ux, uy, uz, rx, ry
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
IF ABS(rx) < DOCKROLL THEN
IF rx * ry > 0 THEN pRoll = 255 ELSE pRoll = 1
ENDIF
ENDIF
IF d > 4000 THEN
dSpeed = 20
ELSEIF d > 1200 THEN
dSpeed = 8
ELSE
dSpeed = 3
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
FRAMEBUFFER COPY F, N, B
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
n = NewFacing(T_COBRA3, x, y, z, 180)
IF n >= 0 THEN
sAI(n) = 0
sSpd(n) = 16 + INT(RND * 16)
sRol(n) = INT(RND * 128)
ENDIF
SpawnBenign = 1
EXIT FUNCTION
ENDIF
IF inSafe THEN SpawnBenign = 1 : EXIT FUNCTION
t = T_ASTEROID
IF INT(RND * 256) < 5 THEN t = T_CANISTER
n = NewFacing(t, x, y, z, 180)
IF n >= 0 THEN
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
t = INT(RND * 4) OR 1
n = Aggressor(t, 0)
NEXT i
EXIT SUB
ENDIF
spawnEV = spawnEV + 1
t = (r AND 3) + 3
ai = 192
IF INT(RND * 256) >= 200 THEN ai = ai OR 1
IF t = T_THARGOID THEN
IF INT(RND * 256) >= 200 THEN
IF Aggressor(T_THARGOID, ai) >= 0 THEN n = Aggressor(T_THARGON, 129)
ENDIF
EXIT SUB
ENDIF
n = Aggressor(t, ai)
END SUB
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
HitPlayer 32
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
END FUNCTION
SUB ScoopIt(n AS INTEGER, t AS INTEGER)
LOCAL INTEGER item
SELECT CASE t
CASE T_ESCAPE  : item = 3
CASE T_THARGON : item = 16
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
IF eqOwned(4) THEN TEXT 30, y, "Fuel Scoops", "LT", 7, 1, cYellow : y = y + 10
IF eqOwned(6) THEN TEXT 30, y, "Docking Computer", "LT", 7, 1, cYellow : y = y + 10
IF lasPower >= 128 THEN TEXT 30, y, "Beam Laser", "LT", 7, 1, cYellow : y = y + 10
END SUB
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
IF y = 53 THEN TEXT 20, y, "Nothing in the hold.", "LT", 7, 1, cGrey
END SUB
SUB MarketScreen(sel AS INTEGER)
LOCAL INTEGER i, y, c
CLS
GotoSystem gGal, homeSys
SysData
TEXT VCX, 3, SysName$() + " MARKET PRICES", "CT", 7, 1, cWhite
LINE 0, 15, SCRW - 1, 15, 1, cWhite
TEXT 20, 18, "PRODUCT", "LT", 7, 1, cGrey
TEXT 150, 18, "UNIT", "LT", 7, 1, cGrey
TEXT 190, 18, "PRICE", "LT", 7, 1, cGrey
TEXT 245, 18, "FOR SALE", "LT", 7, 1, cGrey
TEXT 300, 18, "HELD", "LT", 7, 1, cGrey
y = 29
FOR i = 0 TO NGOODS - 1
c = cWhite
IF i = sel THEN
c = cYellow
BOX 16, y - 1, 292, 9, 0, RGB(32, 32, 64), RGB(32, 32, 64)
ENDIF
TEXT 20, y, mkName$(i), "LT", 7, 1, c
TEXT 150, y, mkUnit$(i), "LT", 7, 1, c
TEXT 190, y, PriceStr$(mkPrice(i)), "LT", 7, 1, c
TEXT 250, y, STR$(mkStock(i)), "LT", 7, 1, c
TEXT 302, y, STR$(cargo(i)), "LT", 7, 1, c
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
IF eqOwned(i) THEN c = cGrey
IF i = sel THEN
c = cYellow
BOX 16, y - 1, 292, 9, 0, RGB(32, 32, 64), RGB(32, 32, 64)
ENDIF
TEXT 20, y, eqName$(i), "LT", 7, 1, c
TEXT 220, y, STR$(eqPrice(i)) + " Cr", "LT", 7, 1, c
y = y + 9
ENDIF
NEXT i
TEXT 20, y + 5, "Cash: " + STR$(cashTenths / 10) + " Cr", "LT", 7, 1, cWhite
END SUB
SUB BuyEquip(i AS INTEGER)
IF eqOwned(i) THEN EXIT SUB
IF eqTech(i) > sysTech + 1 THEN EXIT SUB
IF cashTenths < eqPrice(i) * 10 THEN EXIT SUB
cashTenths = cashTenths - eqPrice(i) * 10
eqOwned(i) = 1
SELECT CASE i
CASE 0 : IF pMissl < 4 THEN pMissl = pMissl + 1 : eqOwned(0) = 0
CASE 1 : holdSize = 35
CASE 3 : lasPower = 143 OR 128
CASE 5 : energyUnit = 1
END SELECT
END SUB
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
PRINT #fn, "elite-commander 1"
PRINT #fn, gGal
PRINT #fn, homeSys
PRINT #fn, cashTenths
PRINT #fn, pFuel
PRINT #fn, holdSize
PRINT #fn, kills
PRINT #fn, legal
PRINT #fn, pMissl
PRINT #fn, lasPower
PRINT #fn, energyUnit
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
IF LEFT$(hd$, 16) <> "elite-commander " THEN
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
INPUT #fn, lasPower
INPUT #fn, energyUnit
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
DATA "Beam Laser",1000,4
DATA "Fuel Scoops",525,5
DATA "Energy Unit",1500,8
DATA "Docking Computer",1500,9
DATA "Galactic Hyperdrive",5000,10
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
IF DIR$(TITLEPIC, FILE) <> "" THEN
LOAD JPG TITLEPIC
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
SUB ControlsScreen
LOCAL INTEGER y, k
CLS
TEXT VCX, 2, "FLIGHT", "CT", 7, 1, cWhite
LINE 0, 13, SCRW - 1, 13, 1, cCyan
y = 17
KeyLine y, "Roll", "< >  or left/right" : y = y + 10
KeyLine y, "Pitch", "S X  or up/down" : y = y + 10
KeyLine y, "Speed", "SPACE faster, / slower" : y = y + 10
KeyLine y, "Fire", "A" : y = y + 10
KeyLine y, "Missile", "T locks on, M fires" : y = y + 10
KeyLine y, "E.C.M.", "E" : y = y + 10
KeyLine y, "Docking computer", "C" : y = y + 10
KeyLine y, "Hyperspace", "H, outside the zone" : y = y + 10
KeyLine y, "Views", "F1 fore, F2 aft" : y = y + 10
KeyLine y, "", "F3 left, F4 right" : y = y + 14
TEXT VCX, y, "SCREENS, FLYING OR DOCKED", "CT", 7, 1, cWhite
y = y + 12
KeyLine y, "F5 Galactic chart", "F8 Market prices" : y = y + 10
KeyLine y, "F6 Short range", "F9 Status" : y = y + 10
KeyLine y, "F7 System data", "F10 Inventory" : y = y + 14
TEXT VCX, y, "DOCKED", "CT", 7, 1, cWhite
y = y + 12
KeyLine y, "F1 Launch", "F2 buy, F3 sell" : y = y + 10
KeyLine y, "F4 Equip ship", "SPACE buys one" : y = y + 10
KeyLine y, "F fills the tank", "S save, L load" : y = y + 10
TEXT VCX, SCRH - 10, "any key goes back", "CT", 7, 1, cGrey
FRAMEBUFFER COPY F, N
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
SoundService
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
DO
ReadKeys
IF kQuit THEN quitGame = 1 : EXIT DO
UpdatePlayer
IF kFire THEN FireLaser
IF kTarget THEN TargetMissile
IF kMissile THEN LaunchMissile
IF kECM THEN FireECM
IF kDock THEN dockComp = 1 - dockComp
IF dockComp THEN DockingComputer
IF kJump THEN JumpAway
IF kChart > 0 THEN
tStage = TIMER
InfoScreen kChart
t0 = t0 + TIMER - tStage
ENDIF
IF lasTimer > 0 THEN lasTimer = lasTimer - 1
IF lasFlash > 0 THEN lasFlash = lasFlash - 1
tStage = TIMER
MoveShips
Contact
Missiles
Tactics
ECMService
Recharge
EnergyWarning
Altitude
CabinTemp
StationCheck
StationPolice
SpawnTraffic
DockCheck
prof(5) = prof(5) + TIMER - tStage
DrawFrame
IF demoMode THEN DemoCaption
FRAMEBUFFER COPY F, N, B
IF kPause THEN PauseGame
mcnt = (mcnt + 1) AND 255
frames = frames + 1
SoundService
LOOP UNTIL dead OR docked
tFlight = tFlight + TIMER - t0
END SUB
SUB PauseGame
LOCAL INTEGER k
TEXT VCX, VIEWH - 12, "PAUSED", "CT", 7, 1, cWhite
FRAMEBUFFER COPY F, N
k = WaitKey(0)
END SUB
SUB JumpAway
IF inSafe THEN EXIT SUB
IF selSys = homeSys THEN EXIT SUB
IF CanReach(selSys) = 0 THEN EXIT SUB
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
CASE 128 : DockUp
CASE 129 : DockDown
CASE 130 : DockLeft
CASE 131 : DockRight
CASE 32, 13
DockAct
CASE 70, 102
IF dscreen = SCR_EQUIP THEN BuyFuel
CASE 83, 115
SaveCommander CMDRFILE
dscreen = SCR_STATUS
CASE 76, 108
IF LoadCommander(CMDRFILE) THEN dscreen = SCR_STATUS
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
DockFooter "up/down choose   SPACE buy one   F3 sell"
ELSE
DockFooter "up/down choose   SPACE sell one   F2 buy"
ENDIF
CASE SCR_EQUIP
EquipScreen dsel
DockFooter "up/down choose   SPACE buy   F fill the tank"
CASE SCR_LONG
ChartLong
DockFooter "arrows move the cursor   F7 data   F1 launch"
CASE SCR_SHORT
ChartShort
DockFooter "arrows move the cursor   F7 data   F1 launch"
CASE SCR_DATA
SysDataScreen
DockFooter "F5 galactic chart   F6 short range   F1 launch"
END SELECT
END SUB
SUB DockFooter(t$)
IF demoMode THEN
TEXT VCX, SCRH - 9, "DEMONSTRATION - PRESS ANY KEY TO PLAY", "CT", 7, 1, cGrey
ELSE
TEXT VCX, SCRH - 9, t$, "CT", 7, 1, cGrey
ENDIF
END SUB
SUB DockUp
IF dscreen = SCR_MARKET OR dscreen = SCR_EQUIP THEN
dsel = dsel - 1
IF dsel < 0 THEN dsel = 0
ELSE
curY = curY - 4 : ChartMoved
ENDIF
END SUB
SUB DockDown
LOCAL INTEGER nrows
IF dscreen = SCR_MARKET THEN
nrows = NGOODS - 1
ELSEIF dscreen = SCR_EQUIP THEN
nrows = ShopCount() - 1
ELSE
curY = curY + 4 : ChartMoved
EXIT SUB
ENDIF
dsel = dsel + 1
IF dsel > nrows THEN dsel = nrows
IF dsel < 0 THEN dsel = 0
END SUB
SUB DockLeft
IF dscreen <> SCR_MARKET AND dscreen <> SCR_EQUIP THEN
curX = curX - 2 : ChartMoved
ENDIF
END SUB
SUB DockRight
IF dscreen <> SCR_MARKET AND dscreen <> SCR_EQUIP THEN
curX = curX + 2 : ChartMoved
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
cashTenths = 25000
ClearSlots
docked = 1
dscreen = SCR_STATUS
dbuy = 1
DemoPickTarget
RunGame
LOOP UNTIL demoStop OR DEMOLOOP = 0
demoMode = 0
END SUB
FUNCTION DemoKey() AS INTEGER
LOCAL INTEGER k
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
IF k = 145 THEN demoLeg = demoLeg + 1 : demoTick = 0
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
demoTick = demoTick + 1
IF demoLeg <= 1 THEN DemoLeg1 ELSE DemoLeg2
END SUB
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
CASE 2230       : kChart = 4
CASE 2260       : kChart = 1
CASE 2290       : kChart = 3
CASE 2330       : DemoPickTarget
inSafe = 0
demoCap$ = "CLEAR OF THE SAFE ZONE"
CASE 2380       : kJump = 1 : demoCap$ = "HYPERSPACE"
CASE 2400       : demoLeg = 2 : demoTick = 0 : demoCap$ = ""
END SELECT
IF demoTick > 530 AND demoTick < 900 THEN DemoAim demoTgt, 1
IF demoTick > 950 AND demoTick < 1500 THEN DemoAim DemoNearestFoe(), 1
IF demoTick > 1550 AND demoTick < 1809 THEN DemoAim demoTgt, 0
IF demoTick > 1740 AND demoTick < 1809 THEN kTarget = 1
IF demoTick > 1815 AND demoTick < 1950 THEN DemoAim demoTgt, 0
IF demoTick > 3000 THEN kQuit = 1
END SUB
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
CASE 950        : kDock = 1 : demoCap$ = "DOCKING COMPUTER ENGAGED"
CASE 1150       : demoCap$ = ""
END SELECT
IF demoTick > 260 AND demoTick < 900 THEN DemoAim DemoNearestFoe(), 1
IF demoTick > 3600 THEN kQuit = 1
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
MATH Q_EULER RAD(hdg), 0, 0, qA() : qA(4) = 1
n = NewShip(t, x, y, z, qA())
IF n >= 0 THEN sSpd(n) = spd : sAI(n) = ai
DemoSpawn = n
END FUNCTION
SUB DemoIncoming
LOCAL INTEGER n
MATH Q_EULER RAD(180), 0, 0, qA() : qA(4) = 1
n = NewShip(T_MISSILE, 1800, 0, 6500, qA())
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
sX(SLOT_PLANET) = 0
sY(SLOT_PLANET) = 0
sZ(SLOT_PLANET) = 2 * PRADIUS + 7000
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
TEXT VCX, VIEWH - 26, demoCap$, "CT", 7, 1, cGrey
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
DATA 32,400
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,600
DATA 129,400
DATA 129,300
DATA 129,400
DATA 32,400
DATA 32,300
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
DATA 32,1800
DATA 149,3000
DATA 131,400
DATA 131,400
DATA 131,400
DATA 131,400
DATA 129,700
DATA 151,3000
DATA 150,3000
DATA 154,3000
DATA 153,3000
DATA 145,3500
DATA 147,4500
DATA 32,400
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,600
DATA 129,400
DATA 129,300
DATA 129,400
DATA 32,400
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,300
DATA 32,900
DATA 154,3500
DATA 153,5000
DATA -1,0
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
