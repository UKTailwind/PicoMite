' =====================================================================
'  Start up and the frame loop
' =====================================================================
SetupScreen
LoadStats
ProbeObjects
SetupViews
IF DEMOSCENE = 2 THEN
  DockScene
ELSE
  TestScene
ENDIF

frames = 0
tFrame = TIMER
DO
  IF DEMOFRAMES > 0 THEN
    IF DEMOSCENE = 2 THEN DockInput frames ELSE DemoInput frames
  ELSE
    ReadKeys
  ENDIF
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
  Missiles
  Tactics
  ECMService
  Recharge
  StationCheck
  StationPolice
  DockCheck
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
PRINT "docked"; docked; "  docking computer"; dockComp
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
