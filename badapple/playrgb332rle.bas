'============================================================
' RGB332 RLE video player  (companion to: vid2rgb121.py --rgb332-rle)
'
' Each frame is read straight into RAM with MEMORY INPUT, then the
' BLIT MEMORY332 firmware command decodes the count+value RLE onto
' the LIVE screen (no framebuffer, no copy). ~3x less SD I/O than
' raw RGB332, so full frame rate plays in real time.
'
' .r332c frame layout on disk:
'     uint32  blobLen          (little-endian)
'     uint16  w                \  BLIT MEMORY332 header
'     uint16  h                /
'     [count 1..255][value] ...   runs, fill w*h pixels
'
' MODE 5 on a 640x480 base gives a 320x240 RGB332 surface:
'     OPTION RESOLUTION 640x480[,315000]   (one-off, persists)
'============================================================
OPTION EXPLICIT

CONST F_HANDLER = 1
CONST T_ESC     = 27

DIM STRING vidFile = "sample-2.r332c"
DIM STRING audFile = ""               ' set to e.g. "sample-2.aud" if present
DIM FLOAT  fps     = 22               ' MUST match the converter's --fps
DIM FLOAT  frameMs = 1000 / fps
DIM INTEGER vDelay = 0

' --- per-frame RAM buffer (raw integer array, not a long string) ---
' Holds one frame blob (4-byte header + RLE). Worst case ~2*W*H+4; for
' 320x240 that is ~154 KB, so 19300 ints = 154.4 KB covers any frame.
DIM INTEGER buf%(19300)
DIM INTEGER baddr% = PEEK(VARADDR buf%())
DIM INTEGER lenbuf%(1)

MODE 5
CLS

DIM INTEGER blobLen, frames = 0
OPEN vidFile FOR INPUT AS #F_HANDLER
IF audFile <> "" THEN PLAY WAV audFile
PAUSE vDelay

TIMER = 0
DO
  IF EOF(#F_HANDLER) THEN EXIT DO

  ' --- 4-byte little-endian blob length, straight into RAM ---
  lenbuf%(0) = 0
  MEMORY INPUT F_HANDLER, 4, lenbuf%()
  blobLen = lenbuf%(0)

  ' --- whole frame blob in one read, decoded onto screen ---
  MEMORY INPUT F_HANDLER, blobLen, buf%()
  BLIT MEMORY332 baddr%, 0, 0
  frames = frames + 1

  DO WHILE TIMER < frames * frameMs    ' pace to fps (A/V sync)
  LOOP
  IF ASC(INKEY$) = T_ESC THEN EXIT DO
LOOP

CLOSE #F_HANDLER
PLAY STOP
PRINT "Played "; frames; " frames in "; STR$(TIMER/1000); " s"
END
