' blitspike.bas - Phase 0 spike.
'
' Two questions, both answered on the board rather than estimated:
'   1. does a sheet written by sheets.py load into a RAM image slot and blit
'      back pixel-for-pixel?
'   2. what does a frame's worth of background blitting actually cost?
'
' The sheet it reads is the synthetic one selftest.py writes, so this test
' carries no game data at all.  Expected colour of sheet pixel (x,y) is
' ((x>>3) + (y>>3)) AND 15.
'
' Run with the sheet in the same directory: RUN "A:/sg/blitspike.bas"

Option Console Serial
Option Explicit
Option Default None

Const SLOT = 4                  ' RAM slot 1 on a board with PSRAM
Const SHEETW = 512
Const SHEETH = 574

' Measured from the released background tables: 178 pieces, median 21 x 12,
' mean area 461 px, largest 112 x 64.
Const PIECEW = 21
Const PIECEH = 12
Const BIGW = 112
Const BIGH = 64
Const BLITS = 150               ' 30 blocks of up to 5 sections
Const FRAMES = 50

Dim INTEGER ref(15), fails, i, f, c, x, y, sx, sy, dx, dy
Dim FLOAT t0, tloop, tblit, tbig, tcopy
Dim STRING home$, msg$

home$ = MM.Info(Path)
If home$ = "NONE" Then home$ = "A:/"

Print "--- blit spike, "; MM.Device$; " ", MM.Ver
Print "cpu    "; MM.Info(CpuSpeed)
Print "sheet  "; home$ + "testshet.bmp"

Mode 2
FrameBuffer Create
CLS

' ---- load ------------------------------------------------------------------
t0 = Timer
Flash Load Image SLOT, home$ + "testshet.bmp", O
Print "load   "; Str$(Timer - t0, 0, 1); " ms into slot "; Str$(SLOT)
Print "image  "; Str$(MM.Info(Flash Address SLOT))

' ---- what Pixel() reports for each colour index -----------------------------
' The Pixel statement takes an RGB colour, not an RGB121 index, so the index
' cannot be learned by drawing it.  These are the sixteen MODE 2 colours from
' the firmware's own table (graphics/Draw.c colours[16]).
For c = 0 To 15
  Read ref(c)
Next c
Data &H000000, &H0000FF, &H004000, &H0040FF
Data &H008000, &H0080FF, &H00FF00, &H00FFFF
Data &HFF0000, &HFF00FF, &HFF4000, &HFF40FF
Data &HFF8000, &HFF80FF, &HFFFF00, &HFFFFFF

' ---- correctness: blit a known patch to the display and read it back -------
sx = 64 : sy = 32
Blit Flash SLOT, N, sx, sy, 100, 100, 48, 48
fails = 0
For i = 0 To 47 Step 7
  For f = 0 To 47 Step 5
    x = sx + i : y = sy + f
    c = ((x >> 3) + (y >> 3)) And 15
    If Pixel(100 + i, 100 + f) <> ref(c) Then fails = fails + 1
  Next f
Next i
If fails = 0 Then
  Print "verify OK - every sampled pixel matched the pattern"
Else
  Print "verify FAILED on "; Str$(fails); " sampled pixels"
End If

' ---- cost of the loop alone ------------------------------------------------
CLS
t0 = Timer
For f = 1 To FRAMES
  sx = 0 : sy = 0 : dx = 0 : dy = 0
  For i = 1 To BLITS
    sx = sx + 23 : If sx > 300 Then sx = 0
    dx = dx + 17 : If dx > 240 Then dx = 0
    dy = dy + 13 : If dy > 160 Then dy = 0
  Next i
Next f
tloop = (Timer - t0) / FRAMES

' ---- loop plus a frame of typical background pieces ------------------------
t0 = Timer
For f = 1 To FRAMES
  sx = 0 : sy = 0 : dx = 0 : dy = 0
  For i = 1 To BLITS
    sx = sx + 23 : If sx > 300 Then sx = 0
    dx = dx + 17 : If dx > 240 Then dx = 0
    dy = dy + 13 : If dy > 160 Then dy = 0
    Blit Flash SLOT, F, sx, sy, dx, dy, PIECEW, PIECEH
  Next i
Next f
tblit = (Timer - t0) / FRAMES

' ---- same count at the largest piece the game owns -------------------------
t0 = Timer
For f = 1 To FRAMES
  sx = 0 : sy = 0 : dx = 0 : dy = 0
  For i = 1 To BLITS
    sx = sx + 23 : If sx > 300 Then sx = 0
    dx = dx + 17 : If dx > 160 Then dx = 0
    dy = dy + 13 : If dy > 120 Then dy = 0
    Blit Flash SLOT, F, sx, sy, dx, dy, BIGW, BIGH
  Next i
Next f
tbig = (Timer - t0) / FRAMES

' ---- the present ------------------------------------------------------------
t0 = Timer
For f = 1 To FRAMES
  FrameBuffer Copy F, N
Next f
tcopy = (Timer - t0) / FRAMES

Print
Print "per frame, "; Str$(BLITS); " blits, average of "; Str$(FRAMES)
Print "  loop only        "; Str$(tloop, 0, 3); " ms"
Print "  + "; Str$(PIECEW); "x"; Str$(PIECEH); " blits    "; Str$(tblit, 0, 3); " ms   blits alone "; Str$(tblit - tloop, 0, 3); " ms"
Print "  + "; Str$(BIGW); "x"; Str$(BIGH); " blits  "; Str$(tbig, 0, 3); " ms   blits alone "; Str$(tbig - tloop, 0, 3); " ms"
Print "  framebuffer copy "; Str$(tcopy, 0, 3); " ms"
Print
Print "typical frame      "; Str$(tblit + tcopy, 0, 3); " ms of the 83 ms budget"
Print "worst case         "; Str$(tbig + tcopy, 0, 3); " ms"

FrameBuffer Close
Option Console Both
End
