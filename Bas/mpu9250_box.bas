' ===========================================================================
'  MPU-9250 + SensorFusion 3D box demo  (PicoMite / PicoMiteVGA / HDMI)
' ---------------------------------------------------------------------------
'  Reads the 9-axis MPU-9250 (MPU-6500 accel/gyro + AK8963 magnetometer)
'  over I2C, fuses the data with  MATH SENSORFUSION MAHONY, and steers a
'  3D rectangular box drawn with the built-in 3D engine.
'
'  Wiring (I2C):  SDA, SCL to the Pico's configured I2C pins, plus 3V3/GND.
'                 AD0 low  -> MPU address &H68.
'  Requires a configured display (LCD panel, VGA or HDMI).
'  Run from the editor with F2, or  RUN "examples/mpu9250_box.bas".
'  Press any key to quit.
' ===========================================================================
Option EXPLICIT
Option BASE 0
Option ANGLE RADIANS            ' SensorFusion gyro in/Euler out are radians here

' ---- I2C device addresses ----
Const MPU = &H69               ' MPU-9250 accel/gyro
Const AK  = &H0C               ' AK8963 magnetometer (reached via bypass)

' ---- raw -> physical scale factors ----
Const GSCALE = 250.0 / 32768.0 ' gyro  +/-250 dps  -> dps per LSB
Const ASCALE = 2.0   / 32768.0 ' accel +/-2 g      -> g   per LSB
Const MSCALE = 0.15            ' mag 16-bit        -> uT  per LSB (pre-ASA)
Const D2R    = Pi / 180.0

' ---- 3D box geometry (8 vertices, 6 quad faces, 0-based indices) ----
Dim FLOAT   vert(2,7)                 ' 8 vertices * (x,y,z)
Dim INTEGER facecnt(5), faces(23)    ' verts-per-face, face vertex lists
Dim INTEGER col(6), lc(5), fc(5)     ' colour palette, edge idx, fill idx

' ---- globals shared with the SUBs ----
Dim FLOAT magadj(2)            ' per-axis magnetometer sensitivity adjustment
Dim FLOAT gbias(2)            ' gyro zero-rate bias (rad/s)
Dim FLOAT pitch, rollv, yaw   ' fused Euler angles (radians)
Dim FLOAT quat(4)            ' rotation quaternion [w,x,y,z,m] for the 3D engine
Dim integer i
MODE 2
FRAMEBUFFER create
FRAMEBUFFER write f
InitMPU
InitMag
CalibrateGyro
BuildBox

CLS
Draw3D CAMERA 1, 500                          ' camera 1: viewplane / focal length
Draw3D CREATE 1, 8, 6, 1, vert(), facecnt(), faces(), col(), lc(), fc()
i=0
Do
  ReadAndFuse                                 ' updates pitch, rollv, yaw
  Print @(0,0)i:Inc i
  EulerToQuat -rollv, pitch, yaw, quat()       ' Euler -> rotation quaternion
  Draw3D ROTATE quat(), 1                      ' rotate from the pristine vertices
  Draw3D SHOW 1, 0, 0, 250, 1                  ' draw object 1 at depth 250
  Text 0, 0, "P=" + Str$(pitch/D2R,4,0) + " R=" + Str$(rollv/D2R,4,0) + " Y=" + Str$(yaw/D2R,4,0) + "   "
  FRAMEBUFFER copy f,n
  Pause 15
Loop Until Inkey$ <> ""

Draw3D CLOSE 1
CLS
End

' ===========================================================================
'  Sensor read + fusion
' ===========================================================================
Sub ReadAndFuse
  Local INTEGER b(13), m(6)
  Local FLOAT ax, ay, az, gx, gy, gz, mx, my, mz, t

  ' accel(6) + temp(2) + gyro(6), big-endian, starting at ACCEL_XOUT_H (&H3B)
  ReadRegs MPU, &H3B, 14, b()
  ax = s16%(b(0),  b(1))  * ASCALE
  ay = s16%(b(2),  b(3))  * ASCALE
  az = s16%(b(4),  b(5))  * ASCALE
  gx = s16%(b(8),  b(9))  * GSCALE * D2R - gbias(0)
  gy = s16%(b(10), b(11)) * GSCALE * D2R - gbias(1)
  gz = s16%(b(12), b(13)) * GSCALE * D2R - gbias(2)

  ' AK8963 mag is little-endian: HXL,HXH,HYL,HYH,HZL,HZH,ST2 (ST2 ends the read)
  ReadRegs AK, &H03, 7, m()
  mx = s16%(m(1), m(0)) * MSCALE * magadj(0)
  my = s16%(m(3), m(2)) * MSCALE * magadj(1)
  mz = s16%(m(5), m(4)) * MSCALE * magadj(2)
  ' Remap AK8963 axes into the accel/gyro body frame:
  '   mag X = body Y, mag Y = body X, mag Z = -body Z
  t = mx : mx = my : my = t : mz = -mz

  Math SENSORFUSION MAHONY ax, ay, az, gx, gy, gz, mx, my, mz, pitch, rollv, yaw
End Sub

' Euler angles (radians) -> unit rotation quaternion for  3D ROTATE
Sub EulerToQuat(p As FLOAT, r As FLOAT, y As FLOAT, q() As FLOAT)
  Local FLOAT cy, sy, cp, sp, cr, sr
  cy = Cos(y*0.5) : sy = Sin(y*0.5)
  cp = Cos(p*0.5) : sp = Sin(p*0.5)
  cr = Cos(r*0.5) : sr = Sin(r*0.5)
  q(0) = cr*cp*cy + sr*sp*sy            ' w
  q(1) = sr*cp*cy - cr*sp*sy            ' x
  q(2) = cr*sp*cy + sr*cp*sy            ' y
  q(3) = cr*cp*sy - sr*sp*cy            ' z
  q(4) = 1.0                            ' magnitude (unit)
End Sub

' ===========================================================================
'  Device initialisation
' ===========================================================================
Sub InitMPU
'  SetPin gp0,gp1,i2c
  Local INTEGER who
'  I2C OPEN 400, 1000
  WReg MPU, &H6B, &H80          ' PWR_MGMT_1: device reset
  Pause 100
  WReg MPU, &H6B, &H01          ' wake, auto-select PLL clock
  Pause 10
  who = RdReg%(MPU, &H75)       ' WHO_AM_I (0x71 = 9250, 0x73 = 9255)
  If who <> &H71 And who <> &H73 Then
    Print "MPU-9250 not found, WHO_AM_I=&H" + HEX$(who)
    End
  EndIf
  WReg MPU, &H1B, &H00          ' GYRO_CONFIG : +/-250 dps
  WReg MPU, &H1C, &H00          ' ACCEL_CONFIG: +/-2 g
  WReg MPU, &H1A, &H03          ' CONFIG: DLPF ~44 Hz
  WReg MPU, &H19, &H04          ' SMPLRT_DIV: 200 Hz
  Pause 10
End Sub

Sub InitMag
  Local INTEGER a(2)
  WReg MPU, &H37, &H02          ' INT_PIN_CFG: BYPASS_EN (expose AK8963)
  WReg MPU, &H6A, &H00          ' USER_CTRL : disable internal I2C master
  Pause 10
  WReg AK, &H0A, &H00 : Pause 10 ' CNTL1: power down
  WReg AK, &H0A, &H1F : Pause 10 ' CNTL1: fuse ROM access
  ReadRegs AK, &H10, 3, a()      ' ASAX/ASAY/ASAZ sensitivity values
  magadj(0) = (a(0)-128)/256.0 + 1.0
  magadj(1) = (a(1)-128)/256.0 + 1.0
  magadj(2) = (a(2)-128)/256.0 + 1.0
  WReg AK, &H0A, &H00 : Pause 10 ' power down
  WReg AK, &H0A, &H16 : Pause 10 ' CNTL1: 16-bit, continuous mode 2 (100 Hz)
End Sub

' Average the gyro at rest to remove its zero-rate bias
Sub CalibrateGyro
  Local INTEGER b(5), i
  Local FLOAT sx, sy, sz
  Print "Calibrating gyro - keep the sensor still..."
  sx = 0 : sy = 0 : sz = 0
  For i = 1 To 200
    ReadRegs MPU, &H43, 6, b()  ' GYRO_XOUT_H ...
    sx = sx + s16%(b(0), b(1))
    sy = sy + s16%(b(2), b(3))
    sz = sz + s16%(b(4), b(5))
    Pause 5
  Next
  gbias(0) = sx/200.0 * GSCALE * D2R
  gbias(1) = sy/200.0 * GSCALE * D2R
  gbias(2) = sz/200.0 * GSCALE * D2R
  Print "Calibration done"
  CLS
End Sub

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
Sub WReg(adr As INTEGER, reg As INTEGER, val As INTEGER)
  I2C WRITE adr, 0, 2, reg, val
End Sub

Function RdReg%(adr As INTEGER, reg As INTEGER)
  Local INTEGER b(1)              ' min 2-element array; only b(0) is used
  I2C WRITE adr, 1, 1, reg        ' repeated start (no stop)
  I2C READ  adr, 0, 1, b()
  RdReg% = b(0)
End Function

Sub ReadRegs(adr As INTEGER, reg As INTEGER, n As INTEGER, b() As INTEGER)
  I2C WRITE adr, 1, 1, reg        ' repeated start (no stop)
  I2C READ  adr, 0, n, b()
End Sub

' combine a big-endian (hi,lo) byte pair into a signed 16-bit value
Function s16%(hi As INTEGER, lo As INTEGER)
  Local INTEGER v
  v = (hi And &HFF) * 256 + (lo And &HFF)
  If v > 32767 Then v = v - 65536
  s16% = v
End Function