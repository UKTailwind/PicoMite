'============================================================
' RGB121 framebuffer video player  (companion to vid2rgb121.py)
'
' Fast path: each frame is read straight from the file into a RAM
' buffer with MEMORY INPUT (no INPUT$/LONGSTRING loop), then BLIT
' MEMORY decodes the RLE nibbles directly onto the LIVE screen
' (no framebuffer, no FRAMEBUFFER COPY).
'
' .vc frame layout on disk:
'     uint32  blobLen          (little-endian)
'     uint16  width | 0x8000    \  BLIT MEMORY header
'     uint16  height            /
'     bytes   rle...            (colour<<4)|count, count 1..15
'
' On HDMI/USB the base display must be 640x480; MODE 2 makes it a
' 320x240, 16-colour RGB121 surface:
'     OPTION RESOLUTION 640x480[,315000]   (one-off, persists)
'============================================================
OPTION EXPLICIT

CONST F_HANDLER = 1
CONST T_ESC     = 27

DIM STRING vidFile = "sample-2.vc"
DIM STRING audFile = "sample-2.aud"   ' set to "" if the clip has no audio
DIM FLOAT  fps     = 22               ' MUST match the converter's --fps
DIM FLOAT  frameMs = 1000 / fps
DIM INTEGER vDelay = 0                ' ms A/V sync nudge

' --- per-frame RAM buffer (NOT a long string: raw integer array) -----
' Holds one compressed frame blob (header + RLE). 320x240 worst case
' is ~76 KB; 9700 ints = 77.6 KB headroom.
DIM INTEGER buf%(9700)
DIM INTEGER baddr% = PEEK(VARADDR buf%())   ' frame data address for BLIT MEMORY
DIM INTEGER lenbuf%(1)                        ' 16 bytes scratch for the length word

' --- screen: MODE 2 draws straight to the live 320x240 RGB121 display
MODE 2
CLS

DIM INTEGER blobLen, frames = 0

OPEN vidFile FOR INPUT AS #F_HANDLER
IF audFile <> "" THEN PLAY WAV audFile
PAUSE vDelay

TIMER = 0
DO
  IF EOF(#F_HANDLER) THEN EXIT DO

  ' --- read the 4-byte little-endian blob length straight into RAM ---
  lenbuf%(0) = 0
  MEMORY INPUT F_HANDLER, 4, lenbuf%()
  blobLen = lenbuf%(0)                 ' low 4 bytes = length, upper pre-zeroed

  ' --- read the whole frame blob in one go, then decode onto screen ---
  MEMORY INPUT F_HANDLER, blobLen, buf%()
  BLIT MEMORY baddr%, 0, 0
  frames = frames + 1

  ' --- pace to the encode fps so audio stays in sync (drift-free) ---
  DO WHILE TIMER < frames * frameMs
  LOOP

  IF ASC(INKEY$) = T_ESC THEN EXIT DO
LOOP

CLOSE #F_HANDLER
PLAY STOP
PRINT "Played "; frames; " frames in "; STR$(TIMER/1000); " s"
END
