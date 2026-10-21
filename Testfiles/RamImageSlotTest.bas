' RamImageSlotTest.bas - exercises the RAM image slots (RP2350 with PSRAM).
'
' Image slots 1-3 are flash; on an RP2350 with PSRAM slots 4-8 are RAM slots
' 1-5.  FLASH LOAD IMAGE, BLIT FLASH, TILEMAP and MM.INFO(FLASH ADDRESS) all
' take the same slot number.  Self-contained: draws its own tileset, loads it
' into flash slot 1 and RAM slot 4, checks the two copies are byte-identical,
' draws from both and compares pixels, runs a tilemap off the RAM slot, and
' checks the error paths.  Needs MODE 2, a drive, and OPTION PSRAM.
' The console goes to serial while it runs: on an HDMI board the display
' console prints into the write buffer and would scroll what is being read back.
Option EXPLICIT
Option BASE 0

Const TW = 16, TH = 8, TPR = 8
Const IMGW = TPR * TW, IMGH = TH
Const RAMSLOT = 4, RAMSLOT2 = 5

Dim integer fails, tests, i, n, a, b, w, h, c, r, fslot, e(15)
Dim float t0

If MM.Info(PSRAM SIZE) = 0 Then
  Print "No PSRAM - RAM image slots need OPTION PSRAM"
  End
EndIf

Option CONSOLE SERIAL
MODE 2
CLS
FRAMEBUFFER CREATE
MakeTileset

' ---- the same file into a RAM slot and, if one is free, a flash slot.
' Flash slots already in use are left alone (they may hold someone's game).
fslot = 0
For i = 1 To 2
  If fslot = 0 And Peek(WORD MM.Info(FLASH ADDRESS i)) = &HFFFFFFFF Then fslot = i
Next i
If fslot = 0 Then Print "no free flash slot: flash-vs-RAM comparisons skipped"
RAM ERASE RAMSLOT - 3
Flash LOAD IMAGE RAMSLOT, "ristest_tiles.bmp"
b = MM.Info(FLASH ADDRESS RAMSLOT)
Check "RAM slot address is in PSRAM", (b >= &H11000000) And (b < &H12000000), 1
Check "header width", Peek(WORD b), IMGW
Check "header height", Peek(WORD b + 4), IMGH
' the packed picture itself: row 0, tile 1 is red = RGB121 8 in both nibbles
Check "packed red tile byte", Peek(BYTE b + 8 + TW \ 2), &H88
Check "packed black tile byte", Peek(BYTE b + 8), 0
If fslot Then
  Flash LOAD IMAGE fslot, "ristest_tiles.bmp"
  a = MM.Info(FLASH ADDRESS fslot)
  n = 0
  For i = 0 To 8 + ((IMGW + 1) \ 2) * IMGH - 1 Step 4
    If Peek(WORD a + i) <> Peek(WORD b + i) Then n = n + 1
  Next i
  Check "flash and RAM copies differ in words", n, 0
EndIf

' ---- overwrite protection and the O flag
tests = tests + 1
On Error Skip 1
Flash LOAD IMAGE RAMSLOT, "ristest_tiles.bmp"
If MM.ErrNo <> 0 And Instr(MM.ErrMsg$, "Already programmed") > 0 Then
  Print "ok:   second load refused ->" + MM.ErrMsg$
Else
  Print "FAIL: second load into a used RAM slot was not refused"
  fails = fails + 1
EndIf
On Error Clear
Flash LOAD IMAGE RAMSLOT, "ristest_tiles.bmp", O
Check "reload with O keeps the header", Peek(WORD b), IMGW

' ---- RAM SAVE must not trample an image slot
tests = tests + 1
On Error Skip 1
RAM SAVE RAMSLOT - 3
If MM.ErrNo <> 0 Then
  Print "ok:   RAM SAVE onto an image slot refused ->" + MM.ErrMsg$
Else
  Print "FAIL: RAM SAVE overwrote an image slot"
  fails = fails + 1
EndIf
On Error Clear

' ---- BLIT FLASH from both slots draws the same pixels
FRAMEBUFFER WRITE F
CLS
Blit FLASH RAMSLOT, F, 0, 0, 0, IMGH + 4, IMGW, IMGH
For i = 0 To 7
  Check "tile " + Str$(i) + " colour from RAM", Pixel(i * TW + 4, IMGH + 4 + 3), RGB(255 * (i And 1), 255 * ((i And 2) \ 2), 255 * ((i And 4) \ 4))
Next i
If fslot Then
  Blit FLASH fslot, F, 0, 0, 0, 0, IMGW, IMGH
  n = 0
  For r = 0 To IMGH - 1
    For c = 0 To IMGW - 1
      If Pixel(c, r) <> Pixel(c, r + IMGH + 4) Then n = n + 1
    Next c
  Next r
  Check "BLIT FLASH pixels differing flash vs RAM", n, 0
EndIf
' transparency argument works from a RAM slot
CLS RGB(BLUE)
Blit FLASH RAMSLOT, F, 0, 0, 0, 0, IMGW, IMGH, 0
Check "transparent black keeps the background", Pixel(2, 2), RGB(BLUE)
Check "opaque pixel still drawn", Pixel(TW + 4, 3), RGB(RED)

' ---- a tilemap fed from the RAM slot
Tilemap CLOSE
Tilemap CREATE mapdata, 2, RAMSLOT, TW, TH, TPR, 8, 2
CLS
Tilemap DRAW 2, F, 0, 0, 0, 2 * TH + 4, 8 * TW, 2 * TH
Check "tile 8 (white) from the RAM tilemap, row 0", Pixel(7 * TW + 4, 2 * TH + 4 + 3), RGB(WHITE)
Check "tile 8 (white) from the RAM tilemap, row 1", Pixel(0 * TW + 4, 3 * TH + 4 + 3), RGB(WHITE)
Check "tile 2 (red) from the RAM tilemap, row 1", Pixel(6 * TW + 4, 3 * TH + 4 + 3), RGB(RED)
If fslot Then
  Tilemap CREATE mapdata, 1, fslot, TW, TH, TPR, 8, 2
  Tilemap DRAW 1, F, 0, 0, 0, 0, 8 * TW, 2 * TH
  n = 0
  For r = 0 To 2 * TH - 1
    For c = 0 To 8 * TW - 1
      If Pixel(c, r) <> Pixel(c, r + 2 * TH + 4) Then n = n + 1
    Next c
  Next r
  Check "TILEMAP DRAW pixels differing flash vs RAM", n, 0
EndIf

' ---- empty and out-of-range slots
ExpectError "BLIT FLASH from an empty RAM slot", "Invalid Image", RAMSLOT2
tests = tests + 1
On Error Skip 1
Tilemap CREATE mapdata, 3, RAMSLOT2, TW, TH, TPR, 8, 2
If MM.ErrNo <> 0 And Instr(MM.ErrMsg$, "No image") > 0 Then
  Print "ok:   TILEMAP on an empty RAM slot ->" + MM.ErrMsg$
Else
  Print "FAIL: TILEMAP CREATE on an empty RAM slot did not error"
  fails = fails + 1
EndIf
On Error Clear
tests = tests + 1
On Error Skip 1
Flash LOAD IMAGE 9, "ristest_tiles.bmp"
If MM.ErrNo <> 0 Then
  Print "ok:   slot 9 refused ->" + MM.ErrMsg$
Else
  Print "FAIL: slot 9 accepted"
  fails = fails + 1
EndIf
On Error Clear

' ---- RAM ERASE empties the image slot
RAM ERASE RAMSLOT - 3
ExpectError "BLIT FLASH after RAM ERASE", "Invalid Image", RAMSLOT
Check "erased header", Peek(WORD b), 0

' ---- a big image: the whole 320x240 display into a RAM slot, and timing
CLS
For i = 0 To 15
  Box i * 20, 0, 20, 240, 0, Map(i), Map(i)
Next i
For i = 0 To 15
  e(i) = Pixel(i * 20 + 5, 100)
Next i
Save IMAGE "ristest_big.bmp"
t0 = Timer
Flash LOAD IMAGE RAMSLOT, "ristest_big.bmp", O
Print "320x240 load into RAM took" + Str$(Int(Timer - t0)) + " ms"
CLS
Blit FLASH RAMSLOT, F, 0, 0, 0, 0, 320, 240
For i = 0 To 15
  Check "big image column " + Str$(i), Pixel(i * 20 + 5, 100), e(i)
Next i
Check "big image bottom right", Pixel(15 * 20 + 5, 239), e(15)

' ---- what the LIST shows
Print "RAM LIST says:"
RAM LIST

If fslot Then Flash ERASE fslot
FRAMEBUFFER COPY F, N
FRAMEBUFFER WRITE N
Print "RAM IMAGE SLOTS:" + Str$(tests - fails) + " of" + Str$(tests) + " checks passed"
If fails = 0 Then Print "PASS" Else Print "FAIL"
Tilemap CLOSE
Option CONSOLE BOTH
End

Sub Check(what$, got As integer, want As integer)
  tests = tests + 1
  If got = want Then
    Print "ok:   " + what$ + " =" + Str$(got)
  Else
    Print "FAIL: " + what$ + " got" + Str$(got) + " want" + Str$(want)
    fails = fails + 1
  EndIf
End Sub

' BLIT FLASH from slot s expecting an error containing must$
Sub ExpectError(what$, must$, s As integer)
  Local m$
  tests = tests + 1
  On Error Skip 1
  Blit FLASH s, F, 0, 0, 0, 0, TW, TH
  m$ = MM.ErrMsg$
  If MM.ErrNo = 0 Then
    Print "FAIL: " + what$ + " - no error raised"
    fails = fails + 1
  ElseIf must$ <> "" And Instr(m$, must$) = 0 Then
    Print "FAIL: " + what$ + " - got '" + m$ + "'"
    fails = fails + 1
  Else
    Print "ok:   " + what$ + " ->" + m$
  EndIf
  On Error Clear
End Sub

' eight 16 x 8 tiles in a row: black, then the seven primary colours
Sub MakeTileset
  Local i
  CLS RGB(BLACK)
  For i = 1 To 7
    Box i * TW, 0, TW, TH, 0, RGB(255 * (i And 1), 255 * ((i And 2) \ 2), 255 * ((i And 4) \ 4)), RGB(255 * (i And 1), 255 * ((i And 2) \ 2), 255 * ((i And 4) \ 4))
  Next i
  Save IMAGE "ristest_tiles.bmp", 0, 0, TPR * TW, TH
End Sub

' 8 x 2: every tile once, then reversed
mapdata:
Data 1,2,3,4,5,6,7,8
Data 8,7,6,5,4,3,2,1
