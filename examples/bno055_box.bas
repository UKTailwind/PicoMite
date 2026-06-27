' ===========================================================================
'  BNO055 + SensorFusion 3D box demo  (PicoMite / PicoMiteVGA / HDMI)
' ---------------------------------------------------------------------------
'  Reads the BNO055 in AMG (non-fusion) mode - raw accelerometer, gyroscope
'  and magnetometer - fuses the data with  MATH SENSORFUSION MADGWICK, and
'  steers a 3D rectangular box drawn with the built-in 3D engine.
'
'  (The BNO055 also has onboard fusion; here we deliberately use its raw
'   AMG output so the firmware's SENSORFUSION command does the work.)
'
'  Wiring (I2C):  SDA, SCL to the Pico's configured I2C pins, plus 3V3/GND.
'                 ADR/COM3 low -> address &H28.
'  Requires a configured display (LCD panel, VGA or HDMI).
'  Run from the editor with F2, or  RUN "examples/bno055_box.bas".
'  Press any key to quit.
' ===========================================================================
OPTION EXPLICIT
OPTION BASE 0
OPTION ANGLE RADIANS            ' SensorFusion gyro in/Euler out are radians here

' ---- I2C device address ----
CONST BNO = &H28               ' BNO055 (ADR low); use &H29 if ADR high

' ---- raw -> physical scale factors (BNO055 default UNIT_SEL) ----
CONST ASCALE = 1.0 / 100.0     ' accel: 100 LSB = 1 m/s^2
CONST GSCALE = 1.0 / 16.0      ' gyro : 16  LSB = 1 dps
CONST MSCALE = 1.0 / 16.0      ' mag  : 16  LSB = 1 uT
CONST D2R    = PI / 180.0

' ---- 3D box geometry (8 vertices, 6 quad faces, 0-based indices) ----
DIM FLOAT   vert(2,7)                ' [x,y,z] x 8 vertices (engine wants a 2D array)
DIM INTEGER facecnt(5), faces(23)    ' verts-per-face, face vertex lists
DIM INTEGER col(6), lc(5), fc(5)     ' colour palette, edge idx, fill idx

' ---- globals shared with the SUBs ----
DIM FLOAT gbias(2)            ' gyro zero-rate bias (rad/s)
DIM FLOAT pitch, rollv, yaw   ' fused Euler angles (radians)
DIM FLOAT quat(4)            ' rotation quaternion [w,x,y,z,m] for the 3D engine
DIM INTEGER i                ' frame counter

MODE 2                       ' set a display mode (adjust for your panel/VGA/HDMI)
FRAMEBUFFER CREATE           ' off-screen buffer for flicker-free drawing
FRAMEBUFFER WRITE F          ' direct drawing into the framebuffer

InitBNO
CalibrateGyro
BuildBox

CLS
Draw3D CAMERA 1, 500                          ' camera 1: viewplane / focal length
Draw3D CREATE 1, 8, 6, 1, vert(), facecnt(), faces(), col(), lc(), fc()

i = 0
DO
  ReadAndFuse                                 ' updates pitch, rollv, yaw
  PRINT @(0,0) i : INC i                      ' frame counter
  EulerToQuat -rollv, pitch, yaw, quat()      ' Euler -> rotation quaternion
  Draw3D ROTATE quat(), 1                      ' rotate from the pristine vertices
  Draw3D SHOW 1, 0, 0, 250, 1                  ' draw object 1 at depth 250
  TEXT 0, 0, "P=" + STR$(pitch/D2R,4,0) + " R=" + STR$(rollv/D2R,4,0) + " Y=" + STR$(yaw/D2R,4,0) + "   "
  FRAMEBUFFER COPY F, N                        ' blit the buffer to the visible screen
  PAUSE 15
LOOP UNTIL INKEY$ <> ""

Draw3D CLOSE 1
CLS
END

' ===========================================================================
'  Sensor read + fusion
' ===========================================================================
SUB ReadAndFuse
  LOCAL INTEGER b(17)
  LOCAL FLOAT ax, ay, az, gx, gy, gz, mx, my, mz

  ' One burst: ACC(6) + MAG(6) + GYR(6) from ACC_DATA_X_LSB (&H08).
  ' All BNO055 data is little-endian (LSB first) and in one shared body frame,
  ' so no inter-sensor axis remap is needed.
  ReadRegs BNO, &H08, 18, b()
  ax = s16%(b(1),  b(0))  * ASCALE
  ay = s16%(b(3),  b(2))  * ASCALE
  az = s16%(b(5),  b(4))  * ASCALE
  mx = s16%(b(7),  b(6))  * MSCALE
  my = s16%(b(9),  b(8))  * MSCALE
  mz = s16%(b(11), b(10)) * MSCALE
  gx = s16%(b(13), b(12)) * GSCALE * D2R - gbias(0)
  gy = s16%(b(15), b(14)) * GSCALE * D2R - gbias(1)
  gz = s16%(b(17), b(16)) * GSCALE * D2R - gbias(2)

  MATH SENSORFUSION MADGWICK ax, ay, az, gx, gy, gz, mx, my, mz, pitch, rollv, yaw
END SUB

' Euler angles (radians) -> unit rotation quaternion for  3D ROTATE
SUB EulerToQuat(p AS FLOAT, r AS FLOAT, y AS FLOAT, q() AS FLOAT)
  LOCAL FLOAT cy, sy, cp, sp, cr, sr
  cy = COS(y*0.5) : sy = SIN(y*0.5)
  cp = COS(p*0.5) : sp = SIN(p*0.5)
  cr = COS(r*0.5) : sr = SIN(r*0.5)
  q(0) = cr*cp*cy + sr*sp*sy            ' w
  q(1) = sr*cp*cy - cr*sp*sy            ' x
  q(2) = cr*sp*cy + sr*cp*sy            ' y
  q(3) = cr*cp*sy - sr*sp*cy            ' z
  q(4) = 1.0                            ' magnitude (unit)
END SUB

' ===========================================================================
'  Device initialisation
' ===========================================================================
SUB InitBNO
  LOCAL INTEGER id, tries
  ' The BNO055 takes ~650 ms to boot - poll CHIP_ID (&H00, expect &HA0)
  tries = 0
  DO
    id = RdReg%(BNO, &H00)
    IF id = &HA0 THEN EXIT DO
    PAUSE 20 : tries = tries + 1
  LOOP UNTIL tries = 50
  IF id <> &HA0 THEN
    PRINT "BNO055 not found, CHIP_ID=&H" + HEX$(id)
    END
  ENDIF
  WReg BNO, &H3D, &H00 : PAUSE 25 ' OPR_MODE = CONFIGMODE
  WReg BNO, &H3F, &H20            ' SYS_TRIGGER: reset
  PAUSE 700                       ' wait out the power-on reset
  DO                              ' wait for the chip to come back
    id = RdReg%(BNO, &H00)
    IF id = &HA0 THEN EXIT DO
    PAUSE 20
  LOOP
  WReg BNO, &H3E, &H00 : PAUSE 10 ' PWR_MODE = NORMAL
  WReg BNO, &H07, &H00            ' PAGE_ID = 0
  WReg BNO, &H3F, &H00            ' SYS_TRIGGER: use internal oscillator
  WReg BNO, &H3B, &H00            ' UNIT_SEL: m/s^2, dps, uT
  PAUSE 10
  WReg BNO, &H3D, &H07 : PAUSE 20 ' OPR_MODE = AMG (raw accel+mag+gyro)
END SUB

' Average the gyro at rest to remove its zero-rate bias
SUB CalibrateGyro
  LOCAL INTEGER b(5), i
  LOCAL FLOAT sx, sy, sz
  PRINT "Calibrating gyro - keep the sensor still..."
  sx = 0 : sy = 0 : sz = 0
  FOR i = 1 TO 200
    ReadRegs BNO, &H14, 6, b()   ' GYR_DATA_X_LSB ... (little-endian)
    sx = sx + s16%(b(1), b(0))
    sy = sy + s16%(b(3), b(2))
    sz = sz + s16%(b(5), b(4))
    PAUSE 5
  NEXT
  gbias(0) = sx/200.0 * GSCALE * D2R
  gbias(1) = sy/200.0 * GSCALE * D2R
  gbias(2) = sz/200.0 * GSCALE * D2R
  PRINT "Calibration done"
  CLS
END SUB

' ===========================================================================
'  Box construction (half-extents 40 x 25 x 15) + per-face colours
' ===========================================================================
Sub BuildBox
  Local INTEGER i,j
  For i = 0 To 7 : For j=0 To 2: Read vert(j,i)    : Next i,j
  For i = 0 To 5  : Read facecnt(i) : Next
  For i = 0 To 23 : Read faces(i)   : Next
  col(0) = RGB(red)    : col(1) = RGB(green)   : col(2) = RGB(blue)
  col(3) = RGB(yellow) : col(4) = RGB(cyan)    : col(5) = RGB(magenta)
  col(6) = RGB(white)
  For i = 0 To 5 : fc(i) = i : lc(i) = 6 : Next  ' distinct fill, white edges
End Sub

' 8 vertices: x,y,z
Data -40,-25,-15,   40,-25,-15,   40, 25,-15,  -40, 25,-15
Data -40,-25, 15,   40,-25, 15,   40, 25, 15,  -40, 25, 15
' vertices per face (6 quads)
Data 4,4,4,4,4,4
' face vertex lists: front, back, left, right, top, bottom
Data 0,1,2,3,   4,5,6,7,   0,3,7,4,   1,2,6,5,   3,2,6,7,   0,1,5,4

' ===========================================================================
'  Small I2C helpers
' ===========================================================================
SUB WReg(adr AS INTEGER, reg AS INTEGER, val AS INTEGER)
  I2C WRITE adr, 0, 2, reg, val
END SUB

FUNCTION RdReg%(adr AS INTEGER, reg AS INTEGER)
  LOCAL INTEGER b(1)              ' min 2-element array; only b(0) is used
  I2C WRITE adr, 1, 1, reg        ' repeated start (no stop)
  I2C READ  adr, 0, 1, b()
  RdReg% = b(0)
END FUNCTION

SUB ReadRegs(adr AS INTEGER, reg AS INTEGER, n AS INTEGER, b() AS INTEGER)
  I2C WRITE adr, 1, 1, reg        ' repeated start (no stop)
  I2C READ  adr, 0, n, b()
END SUB

' combine a big-endian (hi,lo) byte pair into a signed 16-bit value
FUNCTION s16%(hi AS INTEGER, lo AS INTEGER)
  LOCAL INTEGER v
  v = (hi AND &HFF) * 256 + (lo AND &HFF)
  IF v > 32767 THEN v = v - 65536
  s16% = v
END FUNCTION
