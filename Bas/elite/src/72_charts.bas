' =====================================================================
'  The charts and the system data screen
'
'  Two views of the same generated galaxy.  The long range chart plots
'  all 256 systems of the current galaxy at their raw coordinates; the
'  short range chart shows only the neighbours, spread out four times
'  wider and twice taller so their names fit beside them.
'
'  Geometry from the original, converted x * 1.25 because our screen is
'  320 across where the BBC's was 256.  The galaxy is half as tall as it
'  is wide, so every chart halves the y coordinate.
'
'    long range   x = galaxy x,            y = galaxy y / 2 + 24
'    short range  x = 104 + dx * 4,        y = 90 + dy * 2
'                 shown only while |dx| < 20 and |dy| < 38
'
'  Distance is four times the root of dx squared plus half dy squared, in
'  tenths of a light year, and a full tank of 7.0 light years draws a
'  circle of radius 17 on the long range chart.
' =====================================================================

' Distance between two systems in tenths of a light year.  The y axis is
' halved because the galaxy is drawn half as tall as it is wide.
FUNCTION SysDist(x0 AS INTEGER, y0 AS INTEGER, x1 AS INTEGER, y1 AS INTEGER) AS INTEGER
  LOCAL INTEGER dx, dy
  dx = ABS(x1 - x0)
  dy = ABS(y1 - y0) \ 2
  ' The square root is taken as a whole number and only then multiplied by
  ' four.  Truncating after the multiply instead would put Lave to Zaonce
  ' at 5.7 light years rather than the 5.6 every player knows.
  SysDist = 4 * INT(SQR(dx * dx + dy * dy))
END FUNCTION

' Walk the current galaxy looking for the system nearest to (cx, cy) in
' raw galaxy coordinates, and leave the seeds sitting on it.
SUB FindSystem(cx AS INTEGER, cy AS INTEGER)
  LOCAL INTEGER i, best, bestd, d
  best = 0 : bestd = 32767
  SetGalaxy gGal
  FOR i = 0 TO 255
    SysData
    d = ABS(sysX - cx) + ABS(sysY * 2 - cy)
    IF d < bestd THEN bestd = d : best = i
    NextSystem
  NEXT i
  GotoSystem gGal, best
  SysData
  selSys = best
END SUB

' The whole galaxy: 256 dots, the reachable circle around where we are,
' and crosshairs on whatever the cursor has picked out.
SUB ChartLong
  LOCAL INTEGER i, px, py, r
  LOCAL nm$ LENGTH 10
  CLS
  TEXT VCX, 4, "GALACTIC CHART " + STR$(gGal), "CT", 7, 1, cWhite
  LINE 0, CHTOP - 5, SCRW - 1, CHTOP - 5, 1, cWhite
  LINE 0, CHTOP + 128, SCRW - 1, CHTOP + 128, 1, cWhite
  SetGalaxy gGal
  FOR i = 0 TO 255
    SysData
    px = sysX * 1.25
    py = sysY + CHTOP
    PIXEL px, py, cWhite
    NextSystem
  NEXT i
  ' The circle we can still reach on the fuel in the tank.
  r = pFuel \ 4
  CIRCLE curX * 1.25, curY \ 2 + CHTOP, r, 1, 1.25, cGreen, -1
  Crosshair curX * 1.25, curY \ 2 + CHTOP, 7
  GotoSystem gGal, selSys
  SysData
  nm$ = SysName$()
  TEXT 4, CHTOP + 134, nm$ + "   " + STR$(SysDist(homeX, homeY, sysX, sysY * 2) / 10) + " LY", "LT", 7, 1, cYellow
END SUB

' The neighbourhood, spread out enough to label.  Only systems close
' enough in both axes appear at all, and a name is only printed when its
' text row is still free, which is how the original stops labels piling
' on top of one another.
SUB ChartShort
  LOCAL INTEGER i, dx, dy, px, py, row, r
  LOCAL nm$ LENGTH 10
  LOCAL INTEGER used(21)
  CLS
  TEXT VCX, 4, "SHORT RANGE CHART", "CT", 7, 1, cWhite
  LINE 0, 19, SCRW - 1, 19, 1, cWhite
  ARRAY SET 0, used()
  SetGalaxy gGal
  FOR i = 0 TO 255
    SysData
    dx = sysX - curX
    dy = sysY * 2 - curY
    IF ABS(dx) < 20 AND ABS(dy) < 38 THEN
      px = SRCX + dx * SRDX
      py = SRCY + dy * SRDY
      ' A system's dot is a small circle whose size stands in for the
      ' point size the original varies with the seeds.
      CIRCLE px, py, 2, 1, 1, cWhite, cWhite
      row = py \ 8
      IF row >= 0 AND row <= 21 THEN
        IF used(row) = 0 THEN
          used(row) = 1
          nm$ = SysName$()
          TEXT px + 5, py - 3, nm$, "LT", 7, 1, cWhite
        ENDIF
      ENDIF
    ENDIF
    NextSystem
  NEXT i
  r = (pFuel \ 4) * SRDX
  CIRCLE SRCX, SRCY, r, 1, 1, cGreen, -1
  Crosshair SRCX, SRCY, 8
END SUB

SUB Crosshair(px AS INTEGER, py AS INTEGER, sz AS INTEGER)
  LINE px - sz, py, px + sz, py, 1, cWhite
  LINE px, py - sz, px, py + sz, 1, cWhite
END SUB

' Everything the game knows about the selected system, which is all
' derived from its seeds rather than stored anywhere.
SUB SysDataScreen
  LOCAL INTEGER y
  LOCAL nm$ LENGTH 10
  CLS
  GotoSystem gGal, selSys
  SysData
  nm$ = SysName$()
  TEXT VCX, 4, "DATA ON " + nm$, "CT", 7, 1, cWhite
  LINE 0, 19, SCRW - 1, 19, 1, cWhite
  y = 30
  DataLine y, "Distance", STR$(SysDist(homeX, homeY, sysX, sysY * 2) / 10) + " Light Years" : y = y + 12
  DataLine y, "Economy", EcoName$(sysEco) : y = y + 12
  DataLine y, "Government", GovName$(sysGov) : y = y + 12
  DataLine y, "Tech Level", STR$(sysTech + 1) : y = y + 12
  DataLine y, "Population", STR$(sysPop / 10) + " Billion" : y = y + 12
  DataLine y, "Productivity", STR$(sysProd) + " M CR" : y = y + 12
  DataLine y, "Radius", STR$(sysRad) + " km" : y = y + 12
END SUB

SUB DataLine(y AS INTEGER, lb$, v$)
  TEXT 20, y, lb$ + ":", "LT", 7, 1, cWhite
  TEXT 150, y, v$, "LT", 7, 1, cYellow
END SUB
