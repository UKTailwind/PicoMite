'============================================================
' RGB332 video player  (companion to: vid2rgb121.py --rgb332)
'
' Each frame is raw RGB332, exactly W*H bytes, no header. MEMORY
' INPUT reads a frame straight into the LIVE display buffer
' (MM.INFO(WRITEBUFF)) - no decode, no copy. Fastest possible path;
' expect some tearing since it writes to the scanned-out buffer.
'
' MODE 5 on a 640x480 base gives a 320x240 RGB332 surface:
'     OPTION RESOLUTION 640x480[,315000]   (one-off, persists)
'============================================================
OPTION EXPLICIT

CONST F_HANDLER = 1
CONST T_ESC     = 27

DIM STRING vidFile = "sample-2.r332"
DIM STRING audFile = ""               ' set to e.g. "sample-2.aud" if present
DIM FLOAT  fps     = 22               ' MUST match the converter's --fps
DIM FLOAT  frameMs = 1000 / fps
DIM INTEGER vDelay = 0

CONST VW = 320, VH = 240
CONST FRAMEBYTES = VW * VH            ' 76800 bytes per RGB332 frame

MODE 5
CLS
DIM INTEGER scr% = MM.INFO(WRITEBUFF) ' live display buffer base

DIM INTEGER frames = 0
OPEN vidFile FOR INPUT AS #F_HANDLER
IF audFile <> "" THEN PLAY WAV audFile
PAUSE vDelay

TIMER = 0
DO
  IF EOF(#F_HANDLER) THEN EXIT DO
  MEMORY INPUT F_HANDLER, FRAMEBYTES, scr%   ' frame -> straight to screen
  frames = frames + 1
  DO WHILE TIMER < frames * frameMs          ' pace to fps (A/V sync)
  LOOP
  IF ASC(INKEY$) = T_ESC THEN EXIT DO
LOOP

CLOSE #F_HANDLER
PLAY STOP
PRINT "Played "; frames; " frames in "; STR$(TIMER/1000); " s"
END
