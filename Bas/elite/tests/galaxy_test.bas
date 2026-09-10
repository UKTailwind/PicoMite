' galaxy_test.bas - does the board generate Elite's galaxy, exactly?
'
' Walks all eight galaxies and all 256 systems in each, reducing every
' field of every system to one number.  elite_tools/galaxy_ref.py computes
' the same number in Python from the same algorithm; if they agree, all
' 2048 systems agree, and the universe is the one Elite generates.
'
' Assemble with:  cat galaxy_test.bas ../src/70_galaxy.bas > run.bas
OPTION EXPLICIT
OPTION BASE 0
OPTION DEFAULT NONE

DIM INTEGER gs0, gs1, gs2, gSys, gGal
DIM INTEGER sysX, sysY, sysGov, sysEco, sysTech, sysPop, sysProd, sysRad
CONST DIGRAPHS = "ALLEXEGEZACEBISOUSESARMAINDIREA?ERATENBERALAVETIEDORQUANTEISRION"
CONST HMOD = 2147483647

DIM INTEGER g, i, k, h, t0
DIM nm$ LENGTH 10

PRINT "Elite galaxy check, 8 galaxies x 256 systems"
t0 = TIMER
h = 0
FOR g = 1 TO 8
  SetGalaxy g
  FOR i = 0 TO 255
    SysData
    nm$ = SysName$()
    h = (h * 31 + sysX) MOD HMOD
    h = (h * 31 + sysY) MOD HMOD
    h = (h * 31 + sysGov) MOD HMOD
    h = (h * 31 + sysEco) MOD HMOD
    h = (h * 31 + sysTech) MOD HMOD
    h = (h * 31 + sysPop) MOD HMOD
    h = (h * 31 + sysProd) MOD HMOD
    h = (h * 31 + sysRad) MOD HMOD
    FOR k = 1 TO LEN(nm$)
      h = (h * 31 + ASC(MID$(nm$, k, 1))) MOD HMOD
    NEXT k
    NextSystem
  NEXT i
  PRINT "  galaxy"; g; " done"
NEXT g
PRINT "checksum "; h
PRINT "expected 1418124007"
IF h = 1418124007 THEN PRINT "MATCH - the galaxy is Elite's" ELSE PRINT "** MISMATCH **"
PRINT "took"; (TIMER - t0) / 1000; " seconds"
PRINT

' And the systems everyone can check by eye.
PRINT "galaxy 1, the systems everyone knows:"
SetGalaxy 1
FOR i = 0 TO 255
  SysData
  nm$ = SysName$()
  IF nm$ = "LAVE" OR nm$ = "ZAONCE" OR nm$ = "DISO" OR nm$ = "RIEDQUAT" OR nm$ = "TIBEDIED" THEN
    PRINT "  "; i; " "; nm$; " ("; STR$(sysX); ","; STR$(sysY); ") ";
    PRINT GovName$(sysGov); ", "; EcoName$(sysEco);
    PRINT ", tech"; sysTech + 1; ", pop"; sysPop / 10; "bn, prod"; sysProd; "MCr, radius"; sysRad; "km"
  ENDIF
  NextSystem
NEXT i
PRINT "galaxy check done"
END
