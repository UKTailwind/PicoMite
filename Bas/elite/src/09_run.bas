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
  IF kQuit THEN EXIT DO
  UpdatePlayer
  tStage = TIMER
  MoveShips
  prof(5) = prof(5) + TIMER - tStage
  DrawFrame
  FRAMEBUFFER COPY F, N, B
  mcnt = (mcnt + 1) AND 255
  frames = frames + 1
  IF DEMOFRAMES > 0 THEN
    IF frames = 30 OR frames = 110 OR frames = 190 OR frames = 250 THEN SaveShot frames
    IF frames >= DEMOFRAMES THEN EXIT DO
  ENDIF
LOOP
frameMs = (TIMER - tFrame) / frames

CloseAll
FRAMEBUFFER CLOSE
MODE 1
PRINT "frames"; frames; "  average"; STR$(frameMs, 4, 2); " ms per frame"
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
