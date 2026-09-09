' =====================================================================
'  The dashboard and the 3D scanner
'
'  PROVISIONAL LAYOUT.  The bar positions here follow the original's
'  arrangement - speed, roll and dive/climb on the left with the four
'  energy banks beneath, the scanner and compass in the middle, and the
'  six status bars on the right - but the exact pixel geometry is still
'  to be reconciled with the source.
'
'  BBC dashboard coordinates convert as x * 1.25 and y - 16, because the
'  original's dashboard is the bottom 64 rows of a 256 row screen and
'  ours is the bottom 64 rows of a 240 row screen.
' =====================================================================

CONST BARW = 58                    ' the drawn part of an indicator bar
CONST BARH = 5
CONST LX = 22                      ' left column bars start here
CONST RX = 254                     ' right column bars start here
CONST SCX = 155                    ' scanner centre
CONST SCY = 206
CONST SCA = 84                     ' scanner semi-axes
CONST SCB = 17
CONST CPX = 292                    ' compass centre
CONST CPY = 194

SUB DrawDash
  LOCAL INTEGER i, e
  LINE 0, DASHY, SCRW - 1, DASHY, 1, RGB(CYAN)

  ' --- left column: the flight indicators
  Bar LX, DASHY + 4,  "SP", dSpeed / MAXSPEED, RGB(YELLOW)
  Pointer LX, DASHY + 12, "RL", alp2 * alp1 / 31.0
  Pointer LX, DASHY + 20, "DC", bet2 * bet1 / 8.0

  ' --- the four energy banks, filled from the bottom up
  FOR i = 0 TO 3
    e = pEnergy - i * 64
    IF e < 0 THEN e = 0
    IF e > 64 THEN e = 64
    Bar LX, DASHY + 34 + i * 7, STR$(i + 1) + " ", e / 64.0, RGB(YELLOW)
  NEXT i

  ' --- right column: the status indicators
  Bar RX, DASHY + 4,  "FS", pFsh / 255.0, RGB(GREEN)
  Bar RX, DASHY + 12, "AS", pAsh / 255.0, RGB(GREEN)
  Bar RX, DASHY + 20, "FU", pFuel / 70.0, RGB(YELLOW)
  Bar RX, DASHY + 28, "CT", pCabT / 255.0, RGB(MAGENTA)
  Bar RX, DASHY + 36, "LT", pLasT / 255.0, RGB(MAGENTA)
  Bar RX, DASHY + 44, "AL", pAltit / 255.0, RGB(GREEN)

  DrawScanner
  DrawCompass
  ViewName
END SUB

' An indicator: two character label, then a bar that fills left to right.
SUB Bar(x AS INTEGER, y AS INTEGER, lb$, frac AS FLOAT, c AS INTEGER)
  LOCAL INTEGER w
  TEXT x - 16, y - 1, lb$, "LT", 7, 1, RGB(WHITE)
  w = frac * BARW
  IF w < 0 THEN w = 0
  IF w > BARW THEN w = BARW
  BOX x, y, BARW, BARH, 1, RGB(GRAY), -1
  IF w > 1 THEN BOX x + 1, y + 1, w - 1, BARH - 2, 0, c, c
END SUB

' Roll and dive/climb are centre-zero: a marker that slides either side
' of the middle rather than a bar that fills.
SUB Pointer(x AS INTEGER, y AS INTEGER, lb$, frac AS FLOAT)
  LOCAL INTEGER px
  TEXT x - 16, y - 1, lb$, "LT", 7, 1, RGB(WHITE)
  BOX x, y, BARW, BARH, 1, RGB(GRAY), -1
  px = x + BARW \ 2 + frac * (BARW \ 2 - 2)
  IF px < x + 1 THEN px = x + 1
  IF px > x + BARW - 3 THEN px = x + BARW - 3
  BOX px, y + 1, 2, BARH - 2, 0, RGB(YELLOW), RGB(YELLOW)
END SUB

' The 3D scanner.  Ships close enough to register appear as a dot with a
' vertical stick down to the plane of the ellipse, so the ellipse reads
' as the plane the player is flying in and the stick shows how far above
' or below it each contact is.
SUB DrawScanner
  LOCAL INTEGER n, px, py, base, c
  CIRCLE SCX, SCY, SCA, 1, SCB / SCA, RGB(CYAN), -1
  LINE SCX - SCA, SCY, SCX + SCA, SCY, 1, RGB(64, 64, 64)
  FOR n = 0 TO nUsed - 1
    IF sTyp(n) <> 0 AND sBp(n) >= 0 THEN
      IF ABS(sX(n)) < 16384 AND ABS(sY(n)) < 16384 AND ABS(sZ(n)) < 16384 THEN
        px = SCX + sX(n) / 204.8
        base = SCY + sZ(n) / -1024
        py = base - sY(n) / 512
        IF px > SCX - SCA AND px < SCX + SCA THEN
          c = RGB(GREEN)
          IF sTyp(n) = T_MISSILE THEN c = RGB(YELLOW)
          IF sTyp(n) = T_STATION THEN c = RGB(WHITE)
          LINE px, base, px, py, 1, c
          BOX px - 1, py - 1, 3, 3, 0, c, c
        ENDIF
      ENDIF
    ENDIF
  NEXT n
END SUB

' The compass points at the station inside the safe zone and at the
' planet outside it; it is hollow when the target is behind us.
SUB DrawCompass
  LOCAL INTEGER n, px, py
  LOCAL FLOAT m
  n = SLOT_PLANET
  IF inSafe AND sTyp(SLOT_STAR) = T_STATION THEN n = SLOT_STAR
  IF sTyp(n) = 0 THEN EXIT SUB
  m = SQR(sX(n)*sX(n) + sY(n)*sY(n) + sZ(n)*sZ(n))
  IF m < 1 THEN EXIT SUB
  CIRCLE CPX, CPY, 12, 1, 1, RGB(64, 64, 64), -1
  px = CPX + 11 * sX(n) / m
  py = CPY - 11 * sY(n) / m
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
