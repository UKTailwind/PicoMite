' VBufTest.bas - Test program for FRAME VBUF (virtual buffer) and FRAME SCROLL
'
' Tests:
'   1. FRAME VBUF creation and FRAME(VW), FRAME(VH) queries
'   2. FRAME PRINT into virtual buffer (content wider/taller than panel)
'   3. FRAME SCROLL to pan the viewport across the virtual buffer
'   4. FRAME CLS on a vbuf panel
'   5. FRAME(SX), FRAME(SY) queries
'   6. Interactive scrolling demo with arrow keys
'
' Run on PicoMite with a display or serial terminal.
' Press any key to advance between test sections.

OPTION EXPLICIT
OPTION CONSOLE SERIAL
DIM k$, i%, j%, vw%, vh%, sx%, sy%

'=============================================================
' Utility: wait for keypress via FRAME(INKEY)
'=============================================================
SUB WaitKey
  LOCAL k$
  DO : k$ = FRAME(INKEY) : LOOP UNTIL k$ <> ""
END SUB

'=============================================================
' Utility: clear status area below the frame
'=============================================================
SUB ClearStatus
  PRINT @(0, 20*MM.FONTHEIGHT) SPACE$(70)
  PRINT SPACE$(70)
  PRINT SPACE$(70)
  PRINT SPACE$(70)
END SUB

'=============================================================
' TEST 1: FRAME VBUF creation and queries
'=============================================================
ClearStatus
PRINT @(0, 20*MM.FONTHEIGHT) "TEST 1: FRAME VBUF creation and VW/VH queries"

FRAME CREATE
FRAME CURSOR ON
FRAME BOX 1, 1, 30, 10
FRAME VBUF 1, 60, 30
FRAME WRITE

vw% = FRAME(VW 1)
vh% = FRAME(VH 1)
IF vw% = 60 AND vh% = 30 THEN
  PRINT "  VW=" STR$(vw%) " VH=" STR$(vh%) " : PASS"
ELSE
  PRINT "  VW=" STR$(vw%) " VH=" STR$(vh%) " : FAIL (expected 60, 30)"
ENDIF
WaitKey

'=============================================================
' TEST 2: FRAME PRINT into virtual buffer
'=============================================================
ClearStatus
PRINT @(0, 20*MM.FONTHEIGHT) "TEST 2: PRINT into vbuf (60x30, panel shows 28x8)"

' Fill vbuf with numbered lines
FOR i% = 0 TO 29
  FRAME PRINT 1, "Line " + STR$(i%) + ": " + STRING$(40, CHR$(65 + (i% MOD 26))) + CHR$(10)
NEXT i%
FRAME WRITE

PRINT "  30 lines written to 60x30 vbuf. Viewport shows first 8."
PRINT "  Press key to scroll down..."
WaitKey

'=============================================================
' TEST 3: FRAME SCROLL - vertical scrolling
'=============================================================
ClearStatus
PRINT @(0, 20*MM.FONTHEIGHT) "TEST 3: FRAME SCROLL down"

FRAME SCROLL 1, 0, 10
FRAME WRITE

sy% = FRAME(SY 1)
IF sy% = 10 THEN
  PRINT "  Scrolled to SY=10: PASS (should show lines 10-17)"
ELSE
  PRINT "  SY=" STR$(sy%) " : FAIL (expected 10)"
ENDIF
WaitKey

'=============================================================
' TEST 4: FRAME SCROLL - horizontal scrolling
'=============================================================
ClearStatus
PRINT @(0, 20*MM.FONTHEIGHT) "TEST 4: FRAME SCROLL right"

FRAME SCROLL 1, 15, 10
FRAME WRITE

sx% = FRAME(SX 1)
IF sx% = 15 THEN
  PRINT "  Scrolled to SX=15, SY=10: PASS"
ELSE
  PRINT "  SX=" STR$(sx%) " : FAIL (expected 15)"
ENDIF
WaitKey

'=============================================================
' TEST 5: FRAME SCROLL back to origin
'=============================================================
ClearStatus
PRINT @(0, 20*MM.FONTHEIGHT) "TEST 5: FRAME SCROLL back to 0,0"

FRAME SCROLL 1, 0, 0
FRAME WRITE

sx% = FRAME(SX 1)
sy% = FRAME(SY 1)
IF sx% = 0 AND sy% = 0 THEN
  PRINT "  SX=0, SY=0: PASS"
ELSE
  PRINT "  SX=" STR$(sx%) " SY=" STR$(sy%) " : FAIL"
ENDIF
WaitKey

'=============================================================
' TEST 6: FRAME CLS on vbuf panel
'=============================================================
ClearStatus
PRINT @(0, 20*MM.FONTHEIGHT) "TEST 6: FRAME CLS on vbuf panel"

FRAME CLS 1
FRAME WRITE

' Check VW/VH still intact after CLS
vw% = FRAME(VW 1)
vh% = FRAME(VH 1)
IF vw% = 60 AND vh% = 30 THEN
  PRINT "  CLS cleared vbuf, VW/VH preserved: PASS"
ELSE
  PRINT "  VW=" STR$(vw%) " VH=" STR$(vh%) " : FAIL"
ENDIF
WaitKey

'=============================================================
' TEST 7: Re-fill and interactive scroll demo
'=============================================================
ClearStatus
PRINT @(0, 20*MM.FONTHEIGHT) "TEST 7: Interactive scroll (arrows/WASD, Q=quit)"

' Re-fill the vbuf with a grid pattern
FRAME CLS 1
FOR j% = 0 TO 29
  LOCAL line$
  line$ = ""
  FOR i% = 0 TO 59
    ' Create a visible grid: coordinates encoded
    IF (i% MOD 10) = 0 AND (j% MOD 5) = 0 THEN
      line$ = line$ + "+"
    ELSEIF (i% MOD 10) = 0 THEN
      line$ = line$ + "|"
    ELSEIF (j% MOD 5) = 0 THEN
      line$ = line$ + "-"
    ELSE
      line$ = line$ + "."
    ENDIF
  NEXT i%
  FRAME PRINT 1, line$ + CHR$(10)
NEXT j%
FRAME SCROLL 1, 0, 0
FRAME WRITE

' Interactive scrolling
sx% = 0
sy% = 0
DIM pw%, ph%, maxsx%, maxsy%
pw% = FRAME(PW 1)
ph% = FRAME(PH 1)
maxsx% = 60 - pw%
maxsy% = 30 - ph%

DO
  k$ = FRAME(INKEY)
  IF k$ <> "" THEN
    IF ASC(k$) = 128 OR k$ = "w" OR k$ = "W" THEN
      ' Up
      IF sy% > 0 THEN sy% = sy% - 1
    ELSEIF ASC(k$) = 129 OR k$ = "s" OR k$ = "S" THEN
      ' Down
      IF sy% < maxsy% THEN sy% = sy% + 1
    ELSEIF ASC(k$) = 130 OR k$ = "a" OR k$ = "A" THEN
      ' Left
      IF sx% > 0 THEN sx% = sx% - 1
    ELSEIF ASC(k$) = 131 OR k$ = "d" OR k$ = "D" THEN
      ' Right
      IF sx% < maxsx% THEN sx% = sx% + 1
    ELSEIF k$ = "q" OR k$ = "Q" THEN
      EXIT DO
    ENDIF
    FRAME SCROLL 1, sx%, sy%
    FRAME WRITE
    ' Show position below frame
    PRINT @(0, 20*MM.FONTHEIGHT) "  SX=" STR$(sx%) " SY=" STR$(sy%) "   "
  ENDIF
LOOP

'=============================================================
' Clean up
'=============================================================
ClearStatus
PRINT @(0, 20*MM.FONTHEIGHT) "All VBUF tests complete."
FRAME CLOSE
