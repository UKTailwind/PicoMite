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

' ---- fixed per-frame costs
t0 = TIMER : FOR i = 1 TO 100 : CLS : NEXT i : tcls = (TIMER - t0) / 100
t0 = TIMER : FOR i = 1 TO 100 : FRAMEBUFFER COPY F, N, B : NEXT i : tcopy = (TIMER - t0) / 100
t0 = TIMER : FOR i = 1 TO 100 : FRAMEBUFFER COPY F, N : NEXT i : tplain = (TIMER - t0) / 100
PRINT "CLS "; STR$(tcls, 3, 2); " ms   COPY F,N,B "; STR$(tcopy, 3, 2); " ms   COPY F,N "; STR$(tplain, 3, 2); " ms"

' ---- object create + close
t0 = TIMER
FOR i = 1 TO 50
  Draw3D CREATE 1, 28, 17, 1, v(), fc(), f(), col(), ec()
  Draw3D CLOSE 1
NEXT i
PRINT "CREATE+CLOSE "; STR$((TIMER - t0) / 50, 3, 2); " ms"

' ---- synthetic MVEIT-sized update, 8 ships: 3 Q_ROTATE, 1 Q_MULT, ~20 scalar statements each
DIM FLOAT ps(4), r(4), rinv(4), so(4), tmp(4), nose(4), fwd(4), up(4), roofv(4)
DIM FLOAT sp, ax, mv, cnt, dz, dist
DIM INTEGER rollc, pitchc, mcnt
MATH Q_VECTOR 100, 50, 900, ps()
MATH Q_CREATE RAD(1), 0, 0, 1, r()
MATH Q_INVERT r(), rinv()
MATH Q_EULER 0.3, 0.2, 0.1, so()
MATH Q_VECTOR 0, 0, 1, fwd()
MATH Q_VECTOR 0, 1, 0, up()
sp = 20 : mv = 30 : rollc = 5 : pitchc = 3 : mcnt = 0
t0 = TIMER
FOR i = 1 TO 100
  mcnt = (mcnt + 1) AND 255
  FOR n = 1 TO 8
    MATH Q_ROTATE rinv(), ps(), tmp()
    MATH Q_MULT rinv(), so(), tmp()
    MATH Q_ROTATE so(), fwd(), nose()
    MATH Q_ROTATE so(), up(), roofv()
    sp = sp + ax
    IF sp > 28 THEN sp = 28
    IF sp < 1 THEN sp = 1
    ax = 0
    ps(1) = ps(1) + nose(1) * sp * 1.5
    ps(2) = ps(2) + nose(2) * sp * 1.5
    ps(3) = ps(3) + nose(3) * sp * 1.5 - mv
    IF rollc <> 0 AND rollc <> 127 THEN rollc = rollc - 1
    IF pitchc <> 0 AND pitchc <> 127 THEN pitchc = pitchc - 1
    IF ((mcnt XOR n) AND 7) = 0 THEN
      cnt = nose(1) * ps(1) + nose(2) * ps(2) + nose(3) * ps(3)
      dist = SQR(ps(1) * ps(1) + ps(2) * ps(2) + ps(3) * ps(3))
      IF dist > 0 THEN cnt = cnt / dist
      IF cnt > 0.9 THEN ax = 3 ELSE IF cnt < -0.7 THEN ax = -1
    ENDIF
    dz = ps(3)
    IF dz < 0 THEN ps(3) = 900
    IF ABS(ps(1)) > 57344 OR ABS(ps(2)) > 57344 THEN ps(1) = 100 : ps(2) = 50
  NEXT n
NEXT i
PRINT "synthetic MVEIT, 8 ships "; STR$((TIMER - t0) / 100, 3, 2); " ms/frame"

' ---- is hidden-line mode available on this build/target?
hl = 1
Draw3D CREATE 1, 28, 17, 1, v(), fc(), f(), col(), ec()
ON ERROR SKIP 1
Draw3D WRITE 1, 0, 0, 900, 0, 2
IF MM.ERRNO <> 0 THEN PRINT "hidden-line unavailable: "; MM.ERRMSG$ : hl = 0
ON ERROR CLEAR
Draw3D CLOSE 1

' ---- render modes: 0 wireframe, 1 solid, 2 hidden-line; 1/2/4/8 ships; near and far
FOR mode = 0 TO 1 + hl
  FOR k = 0 TO 3
    ships = nset(k)
    FOR n = 1 TO ships
      IF mode = 1 THEN
        Draw3D CREATE n, 28, 17, 1, v(), fc(), f(), col(), ec(), fl()
      ELSE
        Draw3D CREATE n, 28, 17, 1, v(), fc(), f(), col(), ec()
      ENDIF
    NEXT n
    FOR z = 900 TO 2500 STEP 1600
      t0 = TIMER
      FOR i = 1 TO 50
        CLS
        FOR n = 1 TO ships
          MATH Q_EULER RAD(i * 3 + n * 40), RAD(i * 2), RAD(n * 10), q()
          Draw3D ROTATE q(), n
          IF mode = 2 THEN
            Draw3D WRITE n, (n - 4) * 60, 0, z, 0, 2
          ELSE
            Draw3D WRITE n, (n - 4) * 60, 0, z, 0, 0
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

' ---- sound path accepted?
ON ERROR SKIP 1
PLAY SOUND 1, B, S, 440, 20
IF MM.ERRNO <> 0 THEN PRINT "PLAY SOUND error: "; MM.ERRMSG$ ELSE PRINT "PLAY SOUND accepted"
ON ERROR CLEAR
PAUSE 300
PLAY STOP

FRAMEBUFFER CLOSE
MODE 1
PRINT "bench done"
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
