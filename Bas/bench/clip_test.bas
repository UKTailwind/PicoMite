' clip_test.bas - shapes wholly or partly off the screen must leave the screen's edges alone (V8 ClipRectangle)
' Every display's DrawRectangle used to clamp the two ends of a span apart, so a BOX off one edge, or the rows of a
' filled TRIANGLE or CIRCLE running off it, drew a line of pixels along that edge. Runs in each MODE of a VGA or
' HDMI build, or once on the panel of an LCD build.
OPTION EXPLICIT
OPTION DEFAULT INTEGER
DIM mds(4) = (1, 2, 3, 4, 5), mi, md, W, H, bg, fg, n, bad, allbad, vga
vga = (INSTR(MM.DEVICE$, "VGA") > 0 OR INSTR(MM.DEVICE$, "HDMI") > 0 OR INSTR(MM.DEVICE$, "PC3") > 0)
IF vga THEN
  FOR mi = 0 TO 4
    md = mds(mi)
    ON ERROR SKIP 1
    MODE md
    IF MM.ERRNO THEN
      PRINT "MODE"; md; ": not available"
    ELSE
      runtests
      PRINT "MODE"; md; " ("; STR$(W); "x"; STR$(H); "):"; n; " checks,"; bad; " wrong"
    ENDIF
  NEXT
  MODE 1
ELSE
  runtests
  PRINT "panel ("; STR$(W); "x"; STR$(H); "):"; n; " checks,"; bad; " wrong"
  CLS
ENDIF
IF allbad = 0 THEN PRINT "CLIP TEST PASSED" ELSE PRINT "CLIP TEST FAILED:"; allbad
END

SUB runtests
  LOCAL y, x
  W = MM.HRES : H = MM.VRES : n = 0 : bad = 0
  CLS RGB(BLACK)
  bg = PIXEL(W \ 2, H \ 2)
  PIXEL W \ 2, H \ 2, RGB(WHITE) : fg = PIXEL(W \ 2, H \ 2) : PIXEL W \ 2, H \ 2, RGB(BLACK)
  ' wholly off each edge: nothing at all
  BOX -40, 20, 30, 30, 1, RGB(WHITE), RGB(WHITE)
  FOR y = 20 TO 49 : want 0, y, bg, "BOX off the left" : NEXT
  BOX W + 5, 20, 30, 30, 1, RGB(WHITE), RGB(WHITE)
  FOR y = 20 TO 49 : want W - 1, y, bg, "BOX off the right" : NEXT
  BOX 20, -40, 30, 30, 1, RGB(WHITE), RGB(WHITE)
  FOR x = 20 TO 49 : want x, 0, bg, "BOX off the top" : NEXT
  BOX 20, H + 5, 30, 30, 1, RGB(WHITE), RGB(WHITE)
  FOR x = 20 TO 49 : want x, H - 1, bg, "BOX off the bottom" : NEXT
  TRIANGLE -60, H \ 2, -10, H \ 2 - 20, -10, H \ 2 + 20, RGB(WHITE), RGB(WHITE)
  FOR y = H \ 2 - 20 TO H \ 2 + 20 : want 0, y, bg, "TRIANGLE off the left" : NEXT
  CIRCLE W + 30, H \ 2, 20, 1, 1, RGB(WHITE), RGB(WHITE)
  FOR y = H \ 2 - 20 TO H \ 2 + 20 : want W - 1, y, bg, "CIRCLE off the right" : NEXT
  LINE -50, 30, -10, 30, 1, RGB(WHITE)
  want 0, 30, bg, "LINE off the left"
  ' partly off: the part on the screen is drawn, and nothing beyond it
  BOX -5, 60, 20, 10, 0, RGB(WHITE), RGB(WHITE)
  FOR x = 0 TO 14 : want x, 60, fg, "BOX partly off: its part" : NEXT
  want 15, 60, bg, "BOX partly off: beyond it"
  want 0, 59, bg, "BOX partly off: above it"
  want 0, 70, bg, "BOX partly off: below it"
  CIRCLE -5, H - 30, 12, 1, 1, RGB(WHITE), RGB(WHITE)
  want 0, H - 30, fg, "CIRCLE partly off: its middle"
  want 0, H - 30 - 14, bg, "CIRCLE partly off: above it"
  want 0, H - 30 + 14, bg, "CIRCLE partly off: below it"
  TRIANGLE W - 10, 100, W + 40, 80, W + 40, 120, RGB(WHITE), RGB(WHITE)
  want W - 1, 100, fg, "TRIANGLE partly off: its tip row"
  want W - 1, 84, bg, "TRIANGLE partly off: a row wholly off"
  want W - 1, 116, bg, "TRIANGLE partly off: another"
  allbad = allbad + bad
END SUB

SUB want px, py, v, what$
  n = n + 1
  IF PIXEL(px, py) <> v THEN
    bad = bad + 1
    IF bad <= 5 THEN PRINT "  "; what$; " at"; px; ","; py; ": got &H"; HEX$(PIXEL(px, py)); ", wanted &H"; HEX$(v)
  ENDIF
END SUB
