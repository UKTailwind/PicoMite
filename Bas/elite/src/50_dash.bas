' =====================================================================
'  The dashboard and the 3D scanner
'
'  PROVISIONAL LAYOUT.  The arrangement follows the original - speed,
'  roll and dive/climb on the left with the four energy banks beneath,
'  the scanner and compass in the middle, the six status bars on the
'  right - but the exact pixel geometry is still to be reconciled with
'  the source.
'
'  The whole screen is cleared each frame, because the planet's disc and
'  the outermost stardust reach below the space view and the drawing
'  primitives clip to the screen rather than to a region.  The dashboard
'  is therefore laid down again over the top of the space view, fixed
'  artwork first.
' =====================================================================

' The fixed artwork is drawn once and then photographed straight out of the
' framebuffer into an array.  Every later call puts it back with a single
' word-aligned memory copy of the 64 rows, which costs a fraction of what
' redrawing thirteen labels and outlines does.  At 4 bits per pixel the
' strip is (SCRH-DASHY) * SCRW / 2 bytes, hence the divide by 16 to count
' 64-bit words.
'
' fadd caches the write buffer's address, so this is only valid while the
' framebuffer stays the write target - which it does for the whole game.
SUB DashStatic
  STATIC INTEGER wordcount = (SCRH - DASHY) * SCRW \ 16
  STATIC INTEGER store(wordcount - 1)
  STATIC INTEGER addr = 0, fadd = 0
  IF fadd = 0 THEN
    LOCAL INTEGER i
    addr = PEEK(VARADDR store())
    fadd = MM.INFO(WRITEBUFF) + DASHY * SCRW / 2
    BOX 0, DASHY, SCRW, SCRH - DASHY, 0, RGB(BLACK), RGB(BLACK)
    LINE 0, DASHY, SCRW - 1, DASHY, 1, RGB(CYAN)
    BarFrame LX, DASHY + 4,  "SP"
    BarFrame LX, DASHY + 12, "RL"
    BarFrame LX, DASHY + 20, "DC"
    FOR i = 0 TO 3
      BarFrame LX, DASHY + 32 + i * 7, STR$(i + 1) + " "
    NEXT i
    BarFrame RX, DASHY + 4,  "FS"
    BarFrame RX, DASHY + 12, "AS"
    BarFrame RX, DASHY + 20, "FU"
    BarFrame RX, DASHY + 28, "CT"
    BarFrame RX, DASHY + 36, "LT"
    BarFrame RX, DASHY + 44, "AL"
    MEMORY COPY INTEGER fadd, addr, wordcount
  ELSE
    MEMORY COPY INTEGER addr, fadd, wordcount
  ENDIF
  ARRAY SET -1, lastBar()
END SUB

SUB BarFrame(x AS INTEGER, y AS INTEGER, lb$)
  TEXT x - 15, y - 1, lb$, "LT", 7, 1, RGB(WHITE)
  BOX x, y, BARW, BARH, 1, RGB(GRAY), -1
END SUB

SUB DrawDash
  LOCAL INTEGER i, e
  ' The space view's planet and stardust overrun this strip, so the fixed
  ' artwork goes down again each frame before anything that moves.
  DashStatic
  Bar 0, LX, DASHY + 4, dSpeed / MAXSPEED, RGB(YELLOW)
  Pointer 1, LX, DASHY + 12, alp2 * alp1 / 31.0
  Pointer 2, LX, DASHY + 20, bet2 * bet1 / 8.0
  FOR i = 0 TO 3
    e = pEnergy - i * 64
    IF e < 0 THEN e = 0
    IF e > 64 THEN e = 64
    Bar 3 + i, LX, DASHY + 32 + i * 7, e / 64.0, RGB(YELLOW)
  NEXT i
  Bar 7,  RX, DASHY + 4,  pFsh / 255.0, RGB(GREEN)
  Bar 8,  RX, DASHY + 12, pAsh / 255.0, RGB(GREEN)
  Bar 9,  RX, DASHY + 20, pFuel / 70.0, RGB(YELLOW)
  Bar 10, RX, DASHY + 28, pCabT / 255.0, RGB(MAGENTA)
  Bar 11, RX, DASHY + 36, pLasT / 255.0, RGB(MAGENTA)
  Bar 12, RX, DASHY + 44, pAltit / 255.0, RGB(GREEN)
  DrawScanner
  DrawCompass
END SUB

' A bar is only touched when its length has actually changed.
SUB Bar(id AS INTEGER, x AS INTEGER, y AS INTEGER, frac AS FLOAT, c AS INTEGER)
  LOCAL INTEGER w
  w = frac * (BARW - 2)
  IF w < 0 THEN w = 0
  IF w > BARW - 2 THEN w = BARW - 2
  IF w <> lastBar(id) THEN
    lastBar(id) = w
    BOX x + 1, y + 1, BARW - 2, BARH - 2, 0, RGB(BLACK), RGB(BLACK)
    IF w > 0 THEN BOX x + 1, y + 1, w, BARH - 2, 0, c, c
  ENDIF
END SUB

' Roll and dive/climb are centre-zero: a marker that slides either side
' of the middle rather than a bar that fills from one end.
SUB Pointer(id AS INTEGER, x AS INTEGER, y AS INTEGER, frac AS FLOAT)
  LOCAL INTEGER px
  px = BARW \ 2 + frac * (BARW \ 2 - 3)
  IF px < 1 THEN px = 1
  IF px > BARW - 4 THEN px = BARW - 4
  IF px <> lastBar(id) THEN
    lastBar(id) = px
    BOX x + 1, y + 1, BARW - 2, BARH - 2, 0, RGB(BLACK), RGB(BLACK)
    BOX x + px, y + 1, 2, BARH - 2, 0, RGB(YELLOW), RGB(YELLOW)
  ENDIF
END SUB

' The scanner.  Each contact is a dot with a stick down to the plane of
' the ellipse, so the ellipse reads as the plane the player is flying in
' and the stick shows how far above or below it the contact sits.
SUB DrawScanner
  LOCAL INTEGER n, px, py, base, c
  BOX SCX - SCA, SCY - SCB - 7, 2 * SCA, 2 * SCB + 14, 0, cBlack, cBlack
  ' CIRCLE takes the vertical radius; its aspect is width over height.
  CIRCLE SCX, SCY, SCB, 1, SCA / SCB, cCyan, -1
  FOR n = 0 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 THEN
      IF ABS(sX(n)) < 16384 AND ABS(sY(n)) < 16384 AND ABS(sZ(n)) < 16384 THEN
        px = SCX + sX(n) / 234
        base = SCY - sZ(n) / 1024
        py = base - sY(n) / 700
        IF px > SCX - SCA AND px < SCX + SCA AND py > SCY - SCB - 7 AND py < SCY + SCB + 7 THEN
          c = cGreen
          IF sTyp(n) = T_MISSILE THEN c = cYellow
          IF sTyp(n) = T_STATION THEN c = cWhite
          LINE px, base, px, py, 1, c
          BOX px - 1, py - 1, 3, 3, 0, c, c
        ENDIF
      ENDIF
    ENDIF
  NEXT n
END SUB

' The compass points at the station inside the safe zone and at the
' planet outside it, and is hollow when the target is behind us.
SUB DrawCompass
  LOCAL INTEGER n, px, py
  LOCAL FLOAT m
  n = SLOT_PLANET
  IF inSafe AND sTyp(SLOT_STAR) = T_STATION THEN n = SLOT_STAR
  IF sTyp(n) = 0 THEN EXIT SUB
  m = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
  IF m < 1 THEN EXIT SUB
  BOX CPX - 11, CPY - 11, 23, 23, 0, RGB(BLACK), RGB(BLACK)
  CIRCLE CPX, CPY, 10, 1, 1, RGB(64, 64, 64), -1
  px = CPX + 9 * sX(n) / m
  py = CPY - 9 * sY(n) / m
  IF sZ(n) >= 0 THEN
    BOX px - 1, py - 1, 3, 3, 0, RGB(GREEN), RGB(GREEN)
  ELSE
    BOX px - 1, py - 1, 3, 3, 1, RGB(GREEN), -1
  ENDIF
END SUB

SUB ViewName
  LOCAL v$
  SELECT CASE vw
    CASE 0 : v$ = "FRONT VIEW"
    CASE 1 : v$ = "REAR VIEW"
    CASE 2 : v$ = "LEFT VIEW"
    CASE 3 : v$ = "RIGHT VIEW"
  END SELECT
  TEXT VCX, 2, v$, "CT", 7, 1, RGB(WHITE)
END SUB
