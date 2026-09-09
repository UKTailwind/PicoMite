' bench.bas - Elite Phase 0 frame-time harness for the PC3
' Measures what one Cobra Mk III costs per frame in each Draw3D mode,
' the fixed per-frame costs (CLS, FRAMEBUFFER COPY), object create/close,
' and a synthetic MVEIT-sized BASIC update for eight ships.
' Run at OPTION CPUSPEED 378000 in MODE 2 with a framebuffer.
OPTION EXPLICIT
OPTION BASE 0

DIM FLOAT v(2, 27), q(4), t0, t1, tcls, tcopy, tplain
DIM INTEGER fc(16), f(59), col(1), ec(16), fl(16)
DIM INTEGER n, ships, i, mode, k, z, hl, nset(3)
nset(0) = 1 : nset(1) = 2 : nset(2) = 4 : nset(3) = 8

PRINT "Elite Phase 0 bench  MMBasic "; MM.VER; "  CPU "; MM.INFO(CPUSPEED); "  PSRAM "; MM.INFO(PSRAM SIZE)

' Cobra Mk III (blueprint units, unscaled): 28 vertices, 17 faces, 60 face-vertex entries
FOR i = 0 TO 27 : READ v(0, i), v(1, i), v(2, i) : NEXT i
FOR i = 0 TO 16 : READ fc(i) : NEXT i
FOR i = 0 TO 59 : READ f(i) : NEXT i
col(0) = RGB(WHITE) : col(1) = RGB(GRAY)
FOR i = 0 TO 16 : ec(i) = 0 : fl(i) = 1 : NEXT i
q(4) = 1

MODE 2
FRAMEBUFFER CREATE
FRAMEBUFFER WRITE F
Draw3D CAMERA 1, 320
PRINT "screen "; MM.HRES; "x"; MM.VRES; "  heap free "; MM.INFO(HEAP)

' ---- close range: 1 and 4 ships at z = 300 and 500 in each mode (framebuffer plain copy)
t0 = TIMER : FOR i = 1 TO 100 : CLS : NEXT i : tcls = (TIMER - t0) / 100
t0 = TIMER : FOR i = 1 TO 100 : FRAMEBUFFER COPY F, N : NEXT i : tplain = (TIMER - t0) / 100
FOR mode = 0 TO 2
  FOR k = 0 TO 1
    ships = 1 + 3 * k
    FOR n = 1 TO ships
      IF mode = 1 THEN
        Draw3D CREATE n, 28, 17, 1, v(), fc(), f(), col(), ec(), fl()
      ELSE
        Draw3D CREATE n, 28, 17, 1, v(), fc(), f(), col(), ec()
      ENDIF
    NEXT n
    FOR z = 300 TO 500 STEP 200
      t0 = TIMER
      FOR i = 1 TO 50
        CLS
        FOR n = 1 TO ships
          MATH Q_EULER RAD(i * 3 + n * 40), RAD(i * 2), RAD(n * 10), q()
          Draw3D ROTATE q(), n
          IF mode = 2 THEN
            Draw3D WRITE n, (n - 2) * 40, 0, z, 0, 2
          ELSE
            Draw3D WRITE n, (n - 2) * 40, 0, z, 0, 0
          ENDIF
        NEXT n
        FRAMEBUFFER COPY F, N
      NEXT i
      t1 = (TIMER - t0) / 50
      PRINT "mode "; mode; " ships "; ships; " z "; z; "  frame "; STR$(t1, 4, 2); " ms  per-ship "; STR$((t1 - tcls - tplain) / ships, 4, 2); " ms"
    NEXT z
    FOR n = 1 TO ships : Draw3D CLOSE n : NEXT n
  NEXT k
NEXT mode
PRINT "heap free after hidden-line at z=300: "; MM.INFO(HEAP)
FRAMEBUFFER CLOSE
MODE 1
PRINT "bench2 done"
END

' Cobra Mk III vertices x,y,z
DATA 32,0,76, -32,0,76, 0,26,24, -120,-3,-8, 120,-3,-8, -88,16,-40, 88,16,-40
DATA 128,-8,-40, -128,-8,-40, 0,26,-40, -32,-24,-40, 32,-24,-40, -36,8,-40, -8,12,-40
DATA 8,12,-40, 36,8,-40, 36,-12,-40, 8,-16,-40, -8,-16,-40, -36,-12,-40, 0,0,76, 0,0,90
DATA -80,-6,-40, -80,6,-40, -88,0,-40, 80,6,-40, 88,0,-40, 80,-6,-40
' vertices per face
DATA 3,3,3,3,3,3,3,3,3,7,4,4,3,3,4,4,4
' face vertex lists
DATA 2,1,0, 2,5,1, 0,6,2, 5,3,1, 0,4,6, 2,9,5, 6,9,2, 5,8,3, 4,7,6
DATA 6,7,11,10,8,5,9, 12,13,18,19, 14,15,16,17, 22,24,23, 25,26,27
DATA 1,3,8,10, 0,1,10,11, 11,7,4,0
