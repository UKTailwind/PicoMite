' FrameTest.bas - Extended test program for the FRAME command
' Tests all FRAME subcommands: CREATE, BOX, PRINT, CLS, CURSOR,
' PANEL, WRITE, CLEAR, CLOSE, and legacy text output.
'
' Run on PicoMite with a display or serial terminal (TeraTerm etc.)
' Press any key to advance between test sections.
' Test descriptions print at row 30 (below frame area).

OPTION EXPLICIT
OPTION CONSOLE SERIAL
DIM k$, j%, cell%, cell2%

'=============================================================
' TEST 1: CREATE and CLOSE
'=============================================================
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 1: FRAME CREATE / CLOSE"
FRAME CREATE
PAUSE 500
FRAME CLOSE
PRINT "  CREATE and CLOSE: PASS"
PAUSE 500

'=============================================================
' TEST 2: Simple BOX (1x1 grid = single box, 1 panel)
'=============================================================
FRAME CREATE
FRAME CURSOR ON
FRAME BOX 2, 1, 30, 10
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 2: Simple BOX (single panel)"
PRINT "  Drew single box at (2,1) 30x10"
PRINT "  Panel 1 created. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 3: PRINT into panel (no wrap - clipping)
'=============================================================
FRAME CLS 1
FRAME PRINT 1, "Hello Panel 1!"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 3: PRINT (no wrap)"
PRINT "  Wrote 'Hello Panel 1!' Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 4: PRINT with clipping (long text, no wrap)
'=============================================================
FRAME CLS 1
FRAME PRINT 1, "This is a very long string that should be clipped at the right edge of the panel and not wrap to the next line"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 4: PRINT clipping"
PRINT "  Long text should be clipped. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 5: PRINT with WRAP
'=============================================================
FRAME CLS 1
FRAME PRINT 1, "This text should wrap within the panel boundaries nicely.", , WRAP
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 5: PRINT with WRAP"
PRINT "  Text should wrap inside panel. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 6: PRINT with colour
'=============================================================
FRAME CLS 1
FRAME PRINT 1, "RED TEXT", RGB(RED)
FRAME PRINT 1, " GREEN", RGB(GREEN)
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 6: PRINT with colour"
PRINT "  Red then green text. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 7: CLS single panel
'=============================================================
FRAME CLS 1
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 7: CLS panel"
PRINT "  Panel 1 cleared. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 8: CLEAR (full frame reset, destroys panels)
'=============================================================
FRAME CLEAR
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 8: FRAME CLEAR"
PRINT "  Entire frame cleared, panels gone. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 9: 2x1 grid (horizontal split = 2 panels side by side)
'=============================================================
FRAME BOX 0, 0, 60, 12, 2, 1
FRAME PRINT 1, "Left Panel"
FRAME PRINT 2, "Right Panel"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 9: BOX 2x1 grid"
PRINT "  Two side-by-side panels. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 10: 1x2 grid (vertical split = 2 panels stacked)
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 40, 20, 1, 2
FRAME PRINT 1, "Top Panel"
FRAME PRINT 2, "Bottom Panel"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 10: BOX 1x2 grid"
PRINT "  Two stacked panels. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 11: 2x2 grid (quad layout = 4 panels)
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 60, 20, 2, 2
FRAME PRINT 1, "Panel 1 (TL)"
FRAME PRINT 2, "Panel 2 (TR)"
FRAME PRINT 3, "Panel 3 (BL)"
FRAME PRINT 4, "Panel 4 (BR)"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 11: BOX 2x2 grid"
PRINT "  Four panels in quad layout. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 12: 3x3 grid (9 panels)
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 60, 24, 3, 3
DIM i%
FOR i% = 1 TO 9
  FRAME PRINT i%, "P" + STR$(i%)
NEXT i%
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 12: BOX 3x3 grid"
PRINT "  Nine panels labelled P1-P9. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 13: DOUBLE line style
'=============================================================
FRAME CLEAR
FRAME BOX 5, 2, 50, 16, 2, 2, RGB(WHITE), DOUBLE
FRAME PRINT 1, "Double lines!"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 13: DOUBLE line style"
PRINT "  Box with double-line borders. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 14: Multiple boxes (panels accumulate)
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 30, 10
FRAME BOX 35, 0, 30, 10
FRAME PRINT 1, "Box A"
FRAME PRINT 2, "Box B"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 14: Multiple independent boxes"
PRINT "  Two separate boxes, panels 1 and 2. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 15: PANEL - manual panel definition
'=============================================================
FRAME CLEAR
' Draw a custom border manually using legacy text
FRAME 0, 0, "+------------------------------+", RGB(YELLOW)
FOR i% = 1 TO 8
  FRAME 0, i%, "|                              |", RGB(YELLOW)
NEXT i%
FRAME 0, 9, "+------------------------------+", RGB(YELLOW)
' Define the interior as a panel
FRAME PANEL 1, 1, 1, 30, 8
FRAME PRINT 1, "Manual panel!", RGB(CYAN)
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 15: FRAME PANEL (manual)"
PRINT "  Custom panel inside hand-drawn border. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 16: CURSOR positioning + serial cursor ON/OFF
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 40, 10
FRAME PRINT 1, "Cursor test"
FRAME CURSOR 1, 5, 2
FRAME PRINT 1, "HERE"
FRAME CURSOR ON
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 16: CURSOR ON - serial cursor at panel 1 pos after 'HERE'"
PRINT "  Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP
FRAME CURSOR OFF
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "  CURSOR OFF - serial cursor hidden. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 17: Legacy text output (FRAME x, y, text$)
'=============================================================
FRAME CLEAR
FRAME CURSOR ON
FRAME 0, 0, "Direct text at (0,0)", RGB(WHITE)
FRAME 10, 2, "Text at (10,2)", RGB(GREEN)
FRAME 0, 4, "Colour test", RGB(RED)
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 17: Legacy text output"
PRINT "  Three lines of direct text. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 18: CLS entire frame (preserves panels)
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 40, 10
FRAME PRINT 1, "Before CLS"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 18: CLS entire frame (before)"
FRAME WRITE
PAUSE 500
FRAME CLS
FRAME PRINT 1, "After CLS"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 18: CLS entire frame (after)"
PRINT "  Frame cleared but panel still works. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 20: Wrap with long word-fill test
'=============================================================
FRAME CLEAR
FRAME BOX 2, 1, 24, 14
FRAME PRINT 1, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz", , WRAP
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 20: Wrap fill test"
PRINT "  A-Z, 0-9, a-z wrapping. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 21: Panel print cursor persistence across calls
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 40, 10
FRAME PRINT 1, "First "
FRAME PRINT 1, "Second "
FRAME PRINT 1, "Third"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 21: Cursor persistence"
PRINT "  'First Second Third' on same line. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 22: Newline handling in PRINT
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 40, 12
FRAME PRINT 1, "Line A" + CHR$(10) + "Line B" + CHR$(10) + "Line C"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 22: Newline in PRINT"
PRINT "  Three lines via CHR$(10). Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 23: Stress test - many panels
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 78, 24, 6, 4
FOR i% = 1 TO 24
  FRAME PRINT i%, STR$(i%)
NEXT i%
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 23: 6x4 grid (24 panels)"
PRINT "  All panels numbered. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 24: Panel auto-scroll with WRAP
'=============================================================
FRAME CLEAR
FRAME BOX 2, 1, 30, 6
FRAME PRINT 1, "Line 1" + CHR$(10)
FRAME PRINT 1, "Line 2" + CHR$(10)
FRAME PRINT 1, "Line 3" + CHR$(10)
FRAME PRINT 1, "Line 4" + CHR$(10)
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 24: Auto-scroll (before)"
PRINT "  4 lines in 4-row panel. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP
' Now add more lines - should auto-scroll
FRAME PRINT 1, "Line 5" + CHR$(10), , WRAP
FRAME PRINT 1, "Line 6" + CHR$(10), , WRAP
FRAME PRINT 1, "Line 7", , WRAP
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 24: Auto-scroll (after)"
PRINT "  Lines 4-7 visible, 1-3 scrolled off. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 25: Auto-scroll via long wrapping text
'=============================================================
FRAME CLEAR
FRAME BOX 2, 1, 20, 5
FRAME PRINT 1, "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789 THE END", , WRAP
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 25: Auto-scroll via wrap overflow"
PRINT "  Long text wraps and scrolls. 'THE END' visible. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 26: No scroll without WRAP
'=============================================================
FRAME CLEAR
FRAME BOX 2, 1, 30, 4
FRAME PRINT 1, "Row 1" + CHR$(10) + "Row 2"
' Panel is 2 rows high; try to overflow without WRAP
FRAME PRINT 1, CHR$(10) + "Row 3" + CHR$(10) + "Row 4"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 26: No scroll without WRAP"
PRINT "  Only Row 1 and Row 2 visible (overflow discarded). Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 27: FRAME CURSOR panel positioning
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 40, 10, 2, 1
' Panel 1 is left half, Panel 2 is right half
FRAME PRINT 1, "Start"
FRAME CURSOR 1, 0, 2
FRAME PRINT 1, "Row 2 Col 0"
FRAME CURSOR 1, 5, 0
FRAME PRINT 1, "After"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 27: FRAME CURSOR panel_id, x, y"
PRINT "  Panel 1: 'StartAfter' row 0, 'Row 2 Col 0' row 2. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 28: Panel cursor + scroll combined
'=============================================================
FRAME CLEAR
FRAME BOX 2, 1, 36, 6
' Fill panel to the bottom
FOR i% = 1 TO 4
  FRAME PRINT 1, "Line " + STR$(i%) + CHR$(10), , WRAP
NEXT i%
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 28: Cursor + scroll combined (before)"
PRINT "  4 lines filling panel. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP
' Reposition cursor to middle line and overwrite
FRAME CURSOR 1, 10, 1
FRAME PRINT 1, "<INSERTED>"
' Then add lines that cause scrolling
FRAME CURSOR 1, 0, 4
FRAME PRINT 1, "New line 5" + CHR$(10), , WRAP
FRAME PRINT 1, "New line 6" + CHR$(10), , WRAP
FRAME PRINT 1, "New line 7", , WRAP
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 28: Cursor + scroll combined (after)"
PRINT "  Scrolled with '<INSERTED>' visible. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 29: Scrolling log panel (continuous)
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 40, 8
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 29: Scrolling log panel"
PRINT "  Watch panel scroll 20 log entries..."
FOR j% = 1 TO 20
  FRAME PRINT 1, "Log entry #" + STR$(j%) + CHR$(10), , WRAP
  FRAME WRITE
  PAUSE 300
NEXT j%
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "  Entries 17-20 visible. Press a key..."
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 29b: Basic overlay create, show, hide
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 60, 20
FRAME PRINT 1, "Main panel content underneath"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 29b: Overlay basic"
PRINT "  Main panel visible. Press a key to show overlay..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

FRAME OVERLAY 10, 27, 7
FRAME PRINT 10, "== Pop-Up Overlay =="
FRAME PRINT 10, CHR$(10) + "Hello from overlay!"
FRAME PRINT 10, CHR$(10) + "Panel ID = 10"
FRAME SHOW 10, 15, 7
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "  Overlay shown at (15,7). Press a key to hide..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

FRAME HIDE 10
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "  Overlay hidden, main restored. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 30: Overlay with colour
'=============================================================
FRAME CLS 10
FRAME PRINT 10, "RED OVERLAY", RGB(RED)
FRAME PRINT 10, CHR$(10)
FRAME PRINT 10, "GREEN TEXT", RGB(GREEN)
FRAME SHOW 10, 10, 5
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 30: Overlay with colour"
PRINT "  Red and green text in overlay. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP
FRAME HIDE 10

'=============================================================
' TEST 31: Multiple overlapping overlays (z-order)
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 78, 24
FRAME PRINT 1, "Background panel"

FRAME OVERLAY 20, 22, 6, RGB(YELLOW)
FRAME PRINT 20, "Overlay A (ID 20)"
FRAME PRINT 20, CHR$(10) + "First shown"

FRAME OVERLAY 21, 22, 6, RGB(CYAN), DOUBLE
FRAME PRINT 21, "Overlay B (ID 21)"
FRAME PRINT 21, CHR$(10) + "Last shown=on top"

FRAME SHOW 20, 5, 5
FRAME SHOW 21, 15, 7
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 31: Overlapping overlays"
PRINT "  B on top (shown last). Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

' Re-show A to bring it to top
FRAME SHOW 20, 5, 5
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "  A re-shown, now on top. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

FRAME HIDE 20
FRAME HIDE 21
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "  Both hidden. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 32: Overlay move (SHOW at different position)
'=============================================================
FRAME OVERLAY 30, 19, 5
FRAME PRINT 30, "Moving overlay!"
FRAME SHOW 30, 5, 3
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 32: Overlay move"
PRINT "  Overlay at (5,3). Press a key to move..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

FRAME SHOW 30, 40, 10
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "  Moved to (40,10). Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP
FRAME HIDE 30

'=============================================================
' TEST 33: Overlay with wrap and auto-scroll
'=============================================================
FRAME CLS 1
FRAME PRINT 1, "Main panel text"
FRAME OVERLAY 40, 27, 6
FRAME SHOW 40, 20, 8
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 33: Overlay wrap + scroll"
PRINT "  Watch overlay scroll 10 lines..."
FOR j% = 1 TO 10
  FRAME PRINT 40, "Scroll line " + STR$(j%) + CHR$(10), , WRAP
  FRAME WRITE
  PAUSE 300
NEXT j%
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "  Lines 7-10 visible. Press a key..."
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP
FRAME HIDE 40
FRAME WRITE

'=============================================================
' TEST 34: Overlay cursor positioning
'=============================================================
FRAME OVERLAY 50, 22, 7
FRAME PRINT 50, "Line 0"
FRAME CURSOR 50, 0, 3
FRAME PRINT 50, "Line 3"
FRAME CURSOR 50, 10, 0
FRAME PRINT 50, "<insert>"
FRAME SHOW 50, 25, 5
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 34: Overlay cursor positioning"
PRINT "  'Line 0<insert>' row 0, 'Line 3' row 3. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP
FRAME HIDE 50
FRAME WRITE

'=============================================================
' TEST 35: FRAME COLOUR - panel background colour
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 40, 10
FRAME COLOUR 1, RGB(WHITE), RGB(BLUE)
FRAME CLS 1
FRAME PRINT 1, "White on Blue!"
FRAME PRINT 1, CHR$(10) + "Background filled"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 35: FRAME COLOUR (bg)"
PRINT "  White text on blue background. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 36: Highlight bar via COLOUR change
'=============================================================
FRAME CLS 1
FRAME COLOUR 1, RGB(WHITE), RGB(BLUE)
FRAME CLS 1
FRAME PRINT 1, " Normal item 1"
FRAME PRINT 1, CHR$(10)
FRAME COLOUR 1, RGB(BLACK), RGB(YELLOW)
FRAME PRINT 1, " >> Selected <<  "
FRAME COLOUR 1, RGB(WHITE), RGB(BLUE)
FRAME PRINT 1, CHR$(10) + " Normal item 3"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 36: Highlight bar"
PRINT "  Yellow highlight on item 2. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 37: FRAME TITLE on box panel
'=============================================================
FRAME CLEAR
FRAME BOX 5, 2, 40, 12
FRAME TITLE 1, "My Panel"
FRAME PRINT 1, "Panel with title above"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 37: FRAME TITLE (single)"
PRINT "  Title centred in top border. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 38: FRAME TITLE on double-line box
'=============================================================
FRAME CLEAR
FRAME BOX 5, 2, 40, 12, 1, 1, RGB(WHITE), DOUBLE
FRAME TITLE 1, "Double Title", RGB(YELLOW)
FRAME PRINT 1, "Double-line box with title"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 38: FRAME TITLE (double)"
PRINT "  Yellow title in double border. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 39: FRAME TITLE on overlay
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 78, 24
FRAME PRINT 1, "Main content"
FRAME OVERLAY 10, 30, 8, RGB(CYAN)
FRAME TITLE 10, "Overlay Title", RGB(WHITE)
FRAME PRINT 10, "Titled overlay popup"
FRAME SHOW 10, 20, 6
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 39: FRAME TITLE on overlay"
PRINT "  Overlay with title in border. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP
FRAME HIDE 10

'=============================================================
' TEST 40: FRAME HLINE - panel divider
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 40, 12
FRAME PRINT 1, "Header text"
FRAME HLINE 1, 1
FRAME CURSOR 1, 0, 2
FRAME PRINT 1, "Body below divider"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 40: FRAME HLINE"
PRINT "  Horizontal divider at row 1. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 41: FRAME HLINE double in double box
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 40, 12, 1, 1, RGB(WHITE), DOUBLE
FRAME PRINT 1, "Double Header"
FRAME HLINE 1, 1, RGB(WHITE), DOUBLE
FRAME CURSOR 1, 0, 2
FRAME PRINT 1, "Double body text"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 41: HLINE double in double box"
PRINT "  Matching double-line divider. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 42: FRAME DESTROY overlay
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 60, 20
FRAME PRINT 1, "Main panel"
FRAME OVERLAY 10, 25, 5
FRAME PRINT 10, "Temporary overlay"
FRAME SHOW 10, 15, 6
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 42: FRAME DESTROY"
PRINT "  Overlay visible. Press a key to destroy..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP
FRAME HIDE 10
FRAME DESTROY 10
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "  Overlay destroyed and freed. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 43: Opaque overlay background
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 60, 20
FRAME PRINT 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
FRAME PRINT 1, CHR$(10) + "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
FRAME PRINT 1, CHR$(10) + "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
FRAME PRINT 1, CHR$(10) + "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
FRAME PRINT 1, CHR$(10) + "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE"
FRAME OVERLAY 10, 20, 5
FRAME PRINT 10, "Opaque!"
FRAME SHOW 10, 10, 3
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 43: Opaque overlay"
PRINT "  Overlay hides text behind it. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP
FRAME HIDE 10

'=============================================================
' TEST 44: Overlay with coloured background
'=============================================================
FRAME CLS 1
FRAME PRINT 1, "Main content visible around overlay"
FRAME OVERLAY 11, 30, 8, RGB(WHITE)
FRAME COLOUR 11, RGB(WHITE), RGB(RED)
FRAME CLS 11
FRAME TITLE 11, "Alert!", RGB(WHITE)
FRAME PRINT 11, "Red background overlay"
FRAME PRINT 11, CHR$(10) + "with coloured bg"
FRAME SHOW 11, 12, 5
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 44: Coloured overlay bg"
PRINT "  Red bg overlay with title. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP
FRAME HIDE 11
FRAME WRITE

'=============================================================
' TEST 45: FRAME() function - basic queries
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 78, 24, 2, 1
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 45: FRAME() basic queries"
PRINT "  WIDTH=" + STR$(FRAME(WIDTH)) + " HEIGHT=" + STR$(FRAME(HEIGHT))
PRINT "  PANELS=" + STR$(FRAME(PANELS)) + " OVERLAYS=" + STR$(FRAME(OVERLAYS))
PRINT "  Panel 1: " + STR$(FRAME(PW 1)) + "x" + STR$(FRAME(PH 1))
PRINT "  Panel 2: " + STR$(FRAME(PW 2)) + "x" + STR$(FRAME(PH 2))
PRINT "  Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 46: FRAME() - cursor tracking and cell readback
'=============================================================
FRAME PRINT 1, "AB"
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 46: FRAME() cursor & cell readback"
PRINT "  After printing 'AB': PX=" + STR$(FRAME(PX 1)) + " PY=" + STR$(FRAME(PY 1))
cell% = FRAME(PCELL 1, 0, 0)
PRINT "  PCELL(1,0,0)=" + STR$(cell%) + " char=" + CHR$(cell% AND &HFF)
cell2% = FRAME(CELL 1, 1)
PRINT "  CELL(1,1)=" + STR$(cell2%) + " char=" + CHR$(cell2% AND &HFF)
PRINT "  Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 47: FRAME() - colour queries
'=============================================================
FRAME COLOUR 1, RGB(YELLOW), RGB(BLUE)
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 47: FRAME() colour queries"
PRINT "  Panel 1 FC=" + STR$(FRAME(FC 1)) + " BC=" + STR$(FRAME(BC 1))
PRINT "  ACTIVE(1)=" + STR$(FRAME(ACTIVE 1))
PRINT "  ACTIVE(2)=" + STR$(FRAME(ACTIVE 2))
PRINT "  Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 48: FRAME() - overlay visibility query
'=============================================================
FRAME OVERLAY 20, 20, 5, RGB(WHITE)
FRAME SHOW 20, 10, 5
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 48: FRAME() overlay visibility"
PRINT "  VISIBLE(20) after SHOW=" + STR$(FRAME(VISIBLE 20))
FRAME HIDE 20
PRINT "  VISIBLE(20) after HIDE=" + STR$(FRAME(VISIBLE 20))
PRINT "  OVERLAYS=" + STR$(FRAME(OVERLAYS))
FRAME DESTROY 20
PRINT "  OVERLAYS after DESTROY=" + STR$(FRAME(OVERLAYS))
PRINT "  Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 49: FRAME INPUT - basic string input in panel
'=============================================================
FRAME CLEAR
FRAME BOX 0, 0, 78, 24, 1, 1
FRAME COLOUR 1, RGB(WHITE), RGB(BLUE)
FRAME CLS 1
FRAME PRINT 1, "FRAME INPUT Test", RGB(YELLOW)
FRAME PRINT 1, CHR$(10)
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 49: FRAME INPUT - basic string input"
FRAME WRITE
DIM name$ LENGTH 40
FRAME INPUT 1, name$, "Enter your name: ", RGB(CYAN)
FRAME PRINT 1, CHR$(10)
FRAME PRINT 1, "Hello, " + name$ + "!", RGB(GREEN)
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 49: You entered: '" + name$ + "'. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 50: FRAME INPUT - numeric input
'=============================================================
FRAME CLS 1
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 50: FRAME INPUT - numeric input"
FRAME WRITE
DIM value%
FRAME INPUT 1, value%, "Enter a number: "
FRAME PRINT 1, CHR$(10)
FRAME PRINT 1, "You entered: " + STR$(value%), RGB(GREEN)
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 50: Value=" + STR$(value%) + ". Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP

'=============================================================
' TEST 51: FRAME INPUT - input in overlay
'=============================================================
FRAME CLS 1
FRAME PRINT 1, "Background content here..."
FRAME OVERLAY 15, 40, 6, RGB(WHITE)
FRAME COLOUR 15, RGB(WHITE), RGB(RED)
FRAME CLS 15
FRAME TITLE 15, "Dialog", RGB(WHITE)
FRAME SHOW 15, 10, 5
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 51: FRAME INPUT - input in overlay"
FRAME WRITE
DIM reply$ LENGTH 30
FRAME INPUT 15, reply$, "Response: ", RGB(YELLOW)
FRAME PRINT 15, CHR$(10) + "Got: " + reply$
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) "TEST 51: Reply: '" + reply$ + "'. Press a key..."
FRAME WRITE
k$ = FRAME(INKEY): DO WHILE k$ = "": k$ = FRAME(INKEY): LOOP
FRAME DESTROY 15

'=============================================================
' CLEANUP
'=============================================================
FRAME CLOSE
print @(0,30*mm.fontheight)space$(70):print space$(70):print space$(70):print space$(70)
PRINT @(0, 30*MM.FONTHEIGHT) ""
PRINT "All tests complete!"
END
