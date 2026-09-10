' chart_test.bas - draw the three galaxy screens and photograph them.
'
' Starts where a new commander does, at Lave in galaxy 1, and renders the
' long range chart, the short range chart and the system data screen.
'
' Assemble with:
'   cat chart_test.bas ../src/70_galaxy.bas ../src/72_charts.bas > run.bas
OPTION EXPLICIT
OPTION BASE 0
OPTION DEFAULT NONE

CONST SCRW = 320, SCRH = 240, VCX = 160
CONST CHTOP = 24
CONST SRCX = 130, SRCY = 90, SRDX = 5, SRDY = 2
CONST DIGRAPHS = "ALLEXEGEZACEBISOUSESARMAINDIREA?ERATENBERALAVETIEDORQUANTEISRION"

DIM INTEGER gs0, gs1, gs2, gSys, gGal
DIM INTEGER sysX, sysY, sysGov, sysEco, sysTech, sysPop, sysProd, sysRad
DIM INTEGER homeX, homeY, homeSys, curX, curY, selSys
DIM INTEGER pFuel
DIM INTEGER cWhite, cYellow, cGreen, cBlack, cCyan
DIM INTEGER i
DIM nm$ LENGTH 10

MODE 2
FRAMEBUFFER CREATE
FRAMEBUFFER WRITE F
cWhite = RGB(WHITE) : cYellow = RGB(YELLOW) : cGreen = RGB(GREEN)
cBlack = RGB(BLACK) : cCyan = RGB(CYAN)

' A new commander starts at Lave with a full tank.
gGal = 1
pFuel = 70
SetGalaxy 1
FOR i = 0 TO 255
  SysData
  IF SysName$() = "LAVE" THEN EXIT FOR
  NextSystem
NEXT i
homeSys = i : selSys = i
homeX = sysX : homeY = sysY * 2
curX = homeX : curY = homeY
PRINT "home is system"; homeSys; " "; SysName$(); " at ("; STR$(homeX); ","; STR$(homeY); ")"

ChartLong
FRAMEBUFFER COPY F, N
SAVE IMAGE "A:/chart_long.bmp"

ChartShort
FRAMEBUFFER COPY F, N
SAVE IMAGE "A:/chart_short.bmp"

' Pick a neighbour and show its data: nudge the cursor and find the
' nearest system to it.
curX = homeX + 6 : curY = homeY + 10
FindSystem curX, curY
PRINT "cursor picked system"; selSys; " "; SysName$()
SysDataScreen
FRAMEBUFFER COPY F, N
SAVE IMAGE "A:/chart_data.bmp"

' Distances from Lave to the systems every player knows.
SetGalaxy 1
FOR i = 0 TO 255
  SysData
  nm$ = SysName$()
  IF nm$ = "ZAONCE" OR nm$ = "DISO" OR nm$ = "REORTE" OR nm$ = "LEESTI" THEN
    PRINT "Lave to "; nm$; ": "; STR$(SysDist(homeX, homeY, sysX, sysY * 2) / 10); " LY"
  ENDIF
  NextSystem
NEXT i

FRAMEBUFFER CLOSE
MODE 1
PRINT "chart test done"
END
