' ===========================================================================
'  MPU-6050 (6-axis, NO magnetometer) + SensorFusion 3D box demo
' ---------------------------------------------------------------------------
'  Tests the IMU-only (no-magnetometer) path of  MATH SENSORFUSION MAHONY.
'  The mag fields in the command are left EMPTY, which selects the 6-axis
'  fusion (usemag = 0).  Roll and pitch are absolute (gravity-referenced);
'  YAW WILL SLOWLY DRIFT because there is no magnetic heading reference -
'  that is expected for a gyro/accel-only sensor.
'
'  Wiring (I2C):  SDA, SCL to the Pico's configured I2C pins, plus 3V3/GND.
'                 AD0 low -> address &H68 (use &H69 if AD0 high).
'  Requires a configured display (LCD panel, VGA or HDMI).
'  Run from the editor with F2, or  RUN "examples/mpu6050_box.bas".
'  Press any key to quit.
' ===========================================================================
OPTION EXPLICIT
OPTION BASE 0
OPTION ANGLE RADIANS            ' SensorFusion gyro in/Euler out are radians here

' ---- I2C device address ----
CONST MPU = &H68               ' MPU-6050 (AD0 low); use &H69 if AD0 high

' ---- raw -> physical scale factors (MPU-6050 default ranges) ----
CONST GSCALE = 250.0 / 32768.0 ' gyro  +/-250 dps -> dps per LSB
CONST ASCALE = 2.0   / 32768.0 ' accel +/-2 g     -> g   per LSB
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

InitMPU
CalibrateGyro
BuildBox

CLS
Draw3D CAMERA 1, 500                          ' camera 1: viewplane / focal length
Draw3D CREATE 1, 8, 6, 1, vert(), facecnt(), faces(), col(), lc(), fc()

i = 0
DO
  ReadAndFuse                                 ' updates pitch, rollv, yaw (6-axis)
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
'  Sensor read + 6-axis fusion (no magnetometer)
' ===========================================================================
SUB ReadAndFuse
  LOCAL INTEGER b(13)
  LOCAL FLOAT ax, ay, az, gx, gy, gz

  ' accel(6) + temp(2) + gyro(6), big-endian, starting at ACCEL_XOUT_H (&H3B)
  ReadRegs MPU, &H3B, 14, b()
  ax = s16%(b(0),  b(1))  * ASCALE
  ay = s16%(b(2),  b(3))  * ASCALE
  az = s16%(b(4),  b(5))  * ASCALE
  gx = s16%(b(8),  b(9))  * GSCALE * D2R - gbias(0)
  gy = s16%(b(10), b(11)) * GSCALE * D2R - gbias(1)
  gz = s16%(b(12), b(13)) * GSCALE * D2R - gbias(2)

  ' Mag fields left EMPTY -> SensorFusion uses the 6-axis (IMU-only) path
  MATH SENSORFUSION MAHONY ax, ay, az, gx, gy, gz, , , , pitch, rollv, yaw
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
SUB InitMPU
  SETPIN GP0, GP1, I2C          ' SDA=GP0, SCL=GP1 (change to your wiring)
  LOCAL INTEGER who
  I2C OPEN 400, 1000
  WReg MPU, &H6B, &H80          ' PWR_MGMT_1: device reset
  PAUSE 100
  WReg MPU, &H6B, &H01          ' wake, auto-select PLL clock
  PAUSE 10
  who = RdReg%(MPU, &H75)       ' WHO_AM_I (0x68 for MPU-6050)
  IF who <> &H68 THEN
    PRINT "MPU-6050 not found, WHO_AM_I=&H" + HEX$(who)
    END
  ENDIF
  WReg MPU, &H1B, &H00          ' GYRO_CONFIG : +/-250 dps
  WReg MPU, &H1C, &H00          ' ACCEL_CONFIG: +/-2 g
  WReg MPU, &H1A, &H03          ' CONFIG: DLPF ~44 Hz
  WReg MPU, &H19, &H04          ' SMPLRT_DIV: 200 Hz
  PAUSE 10
END SUB

' Average the gyro at rest to remove its zero-rate bias
SUB CalibrateGyro
  LOCAL INTEGER b(5), i
  LOCAL FLOAT sx, sy, sz
  PRINT "Calibrating gyro - keep the sensor still..."
  sx = 0 : sy = 0 : sz = 0
  FOR i = 1 TO 200
    ReadRegs MPU, &H43, 6, b()  ' GYRO_XOUT_H ...
    sx = sx + s16%(b(0), b(1))
    sy = sy + s16%(b(2), b(3))
    sz = sz + s16%(b(4), b(5))
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
SUB BuildBox
  LOCAL INTEGER i, j
  FOR i = 0 TO 7 : FOR j = 0 TO 2 : READ vert(j,i)   : NEXT i,j
  FOR i = 0 TO 5  : READ facecnt(i) : NEXT
  FOR i = 0 TO 23 : READ faces(i)   : NEXT
  col(0) = RGB(red)    : col(1) = RGB(green)   : col(2) = RGB(blue)
  col(3) = RGB(yellow) : col(4) = RGB(cyan)    : col(5) = RGB(magenta)
  col(6) = RGB(white)
  FOR i = 0 TO 5 : fc(i) = i : lc(i) = 6 : NEXT  ' distinct fill, white edges
END SUB

' 8 vertices: x,y,z
DATA -40,-25,-15,   40,-25,-15,   40, 25,-15,  -40, 25,-15
DATA -40,-25, 15,   40,-25, 15,   40, 25, 15,  -40, 25, 15
' vertices per face (6 quads)
DATA 4,4,4,4,4,4
' face vertex lists: front, back, left, right, top, bottom
DATA 0,1,2,3,   4,5,6,7,   0,3,7,4,   1,2,6,5,   3,2,6,7,   0,1,5,4

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
