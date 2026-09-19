' FRAMEBUFFER MERGE test for VGA/HDMI builds (RP2350): every RESOLUTION x MODE,
' composites 2 over F into N with a transparent colour and checks pixels exactly.
' Run on a PicoMiteHDMI* board; prints OK/FAIL per combination and a fails count.
' Mode 1 at 1024x600 reports the tile colour 6BFF for a set bit - that is PIXEL, not MERGE.
DIM r$(5) = ("640x480", "720x400", "800x600", "848x480", "800x480", "1024x600")
DIM INTEGER fails = 0
FOR ri = 0 TO 5
  ON ERROR SKIP 1
  EXECUTE "RESOLUTION " + r$(ri)
  en = MM.ERRNO : em$ = MM.ERRMSG$ : ON ERROR CLEAR
  IF INSTR(em$, "Unknown command") THEN
    ' VGA builds have no RESOLUTION command: test the fixed resolution once
    ri = 5 : r$(ri) = STR$(MM.HRES) + "x" + STR$(MM.VRES) : en = 0
  ENDIF
  IF en <> 0 THEN
    PRINT r$(ri); " : "; em$
  ELSE
    PAUSE 300
    FOR m = 1 TO 5
      ON ERROR SKIP 1
      MODE m
      IF MM.ERRNO <> 0 THEN
        PRINT r$(ri); " mode"; m; " : "; MM.ERRMSG$
        ON ERROR CLEAR
      ELSE
        DoMode m, r$(ri)
      ENDIF
    NEXT m
  ENDIF
NEXT ri
ON ERROR SKIP 1
RESOLUTION 640x480, 315000
ON ERROR CLEAR
MODE 2
FRAMEBUFFER CLOSE
PRINT "SWEEP DONE fails="; fails
END

SUB DoMode m, res$
  LOCAL w, h, t1, t2, tc$, tcol, ok
  LOCAL INTEGER pL, pR, pC, pB
  FRAMEBUFFER CLOSE
  FRAMEBUFFER CREATE
  FRAMEBUFFER CREATE 2
  w = MM.HRES : h = MM.VRES
  SELECT CASE m
    CASE 1 : tc$ = ""
    CASE 2, 3 : tc$ = "7" : tcol = MAP(7)
    CASE 4 : tc$ = "RGB(CYAN)" : tcol = RGB(CYAN)
    CASE 5 : tc$ = "31" : tcol = RGB(CYAN)
  END SELECT
  COLOUR RGB(WHITE), RGB(BLACK)
  IF m = 1 THEN
    ' 1 bpp: F has the left half set, 2 has a circle set; merge ORs them
    FRAMEBUFFER WRITE F : CLS : BOX 0, 0, w/2, h, 1, RGB(WHITE), RGB(WHITE)
    FRAMEBUFFER WRITE 2 : CLS : CIRCLE w/2, h/2, h/4, 1, , RGB(WHITE), RGB(WHITE)
  ELSE
    FRAMEBUFFER WRITE F : CLS RGB(BLUE)
    BOX 0, 0, w/2, h, 1, RGB(YELLOW), RGB(YELLOW)
    FRAMEBUFFER WRITE 2 : CLS tcol
    CIRCLE w/2, h/2, h/4, 1, , RGB(RED), RGB(RED)
    BOX w/8, h*2/3, w/4, h/5, 1, RGB(WHITE), RGB(GREEN)
  ENDIF
  FRAMEBUFFER WRITE N : CLS
  TIMER = 0
  IF tc$ = "" THEN
    FRAMEBUFFER MERGE
  ELSE
    EXECUTE "FRAMEBUFFER MERGE " + tc$
  ENDIF
  t1 = TIMER
  TIMER = 0
  IF tc$ = "" THEN
    FRAMEBUFFER MERGE , B
  ELSE
    EXECUTE "FRAMEBUFFER MERGE " + tc$ + ", B"
  ENDIF
  t2 = TIMER
  pL = PIXEL(5, 5*h/6) : pR = PIXEL(w-5, 5) : pC = PIXEL(w/2, h/2) : pB = PIXEL(w/8+10, h*2/3+10)
  IF m = 1 THEN
    ok = (pL = RGB(WHITE)) AND (pR = RGB(BLACK)) AND (pC = RGB(WHITE)) AND (pB = RGB(WHITE))
  ELSE
    ' modes 4/5 quantise the colours, so compare against what CLS/BOX actually stored
    FRAMEBUFFER WRITE F
    ok = (pL = PIXEL(5, 5*h/6)) AND (pR = PIXEL(w-5, 5))
    FRAMEBUFFER WRITE 2
    ok = ok AND (pC = PIXEL(w/2, h/2)) AND (pB = PIXEL(w/8+10, h*2/3+10))
    FRAMEBUFFER WRITE N
  ENDIF
  IF NOT ok THEN fails = fails + 1
  PRINT res$; " mode"; m; " "; w; "x"; h; " size"; w*h; " merge "; t1; " ms  merge,B "; t2; " ms  "; CHOICE(ok, "OK", "FAIL " + HEX$(pL) + " " + HEX$(pR) + " " + HEX$(pC) + " " + HEX$(pB))
END SUB
