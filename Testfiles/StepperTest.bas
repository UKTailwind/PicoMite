' StepperTest.bas
' MMBasic test harness for the PicoMite STEPPER subsystem.
'
' This script builds up from simplest single-axis motion to complex
' consecutive multi-axis moves and arcs.
'
' IMPORTANT:
' - Edit the pin assignments and axis tuning values below for your hardware.
' - Feedrate F is specified in mm/min (G-code convention).
' - The stepper subsystem starts in TEST mode after STEPPER INIT.
' - Use MM.CODE to avoid "G-code buffer full" errors.
'
' ---------------------------
' User configuration
' ---------------------------

' Pin numbers:
' Use either physical pin numbers or GPxx numbers depending on your build.
' The firmware accepts either a physical pin number or GPxx.
'
' Example placeholders (change these):
Const X_STEP = 2
Const X_DIR  = 3
Const X_EN   = 4

Const Y_STEP = 5
Const Y_DIR  = 6
Const Y_EN   = 7

Const Z_STEP = 8
Const Z_DIR  = 9
Const Z_EN   = 10

' Axis tuning (change to match your mechanics)
Const X_STEPS_PER_MM = 80
Const Y_STEPS_PER_MM = 80
Const Z_STEPS_PER_MM = 400

' Max velocity (mm/s) and max acceleration (mm/s^2)
Const X_VMAX = 50
Const Y_VMAX = 50
Const Z_VMAX = 10

Const X_AMAX = 200
Const Y_AMAX = 200
Const Z_AMAX = 80

' Soft limits (mm)
Const X_MIN = 0
Const X_MAX = 200
Const Y_MIN = 0
Const Y_MAX = 200
Const Z_MIN = 0
Const Z_MAX = 50

' ---------------------------
' Helpers
' ---------------------------

Sub WaitForBufferSpace(n%)
  ' Wait until at least n% free slots are available
  Do While MM.CODE < n%
    Pause 10
  Loop
End Sub

Sub QueueG0X(x!)
  Call WaitForBufferSpace(1)
  StePPer GCode G0, X, x!
End Sub

Sub QueueG1XY(x!, y!, fmmmin!)
  Call WaitForBufferSpace(1)
  StePPer GCode G1, X, x!, Y, y!, F, fmmmin!
End Sub

Sub QueueG1X(x!, fmmmin!)
  Call WaitForBufferSpace(1)
  StePPer GCode G1, X, x!, F, fmmmin!
End Sub

Sub QueueG1Z(z!, fmmmin!)
  Call WaitForBufferSpace(1)
  StePPer GCode G1, Z, z!, F, fmmmin!
End Sub

Sub QueueG2XY_IJ(x!, y!, i!, j!, fmmmin!)
  Call WaitForBufferSpace(1)
  StePPer GCode G2, X, x!, Y, y!, I, i!, J, j!, F, fmmmin!
End Sub

Sub QueueG3XY_IJ(x!, y!, i!, j!, fmmmin!)
  Call WaitForBufferSpace(1)
  StePPer GCode G3, X, x!, Y, y!, I, i!, J, j!, F, fmmmin!
End Sub

Sub RunAndWait(ms%)
  ' Start execution, then wait a conservative amount of time.
  ' There is currently no numeric status pseudo-variable exposed for motion_active,
  ' so we use a time-based wait.
  StePPer RUN
  Pause ms%
  StePPer TEST
End Sub

Sub ShowSection(t$)
  Print
  Print "---------------------------"
  Print t$
  Print "---------------------------"
End Sub

' ---------------------------
' Setup
' ---------------------------

Call ShowSection("Setup")

StePPer INIT

' Configure axes
StePPer AXIS X, X_STEP, X_DIR, X_EN, 0, X_STEPS_PER_MM, X_VMAX, X_AMAX
StePPer AXIS Y, Y_STEP, Y_DIR, Y_EN, 0, Y_STEPS_PER_MM, Y_VMAX, Y_AMAX
StePPer AXIS Z, Z_STEP, Z_DIR, Z_EN, 0, Z_STEPS_PER_MM, Z_VMAX, Z_AMAX

' Enable drivers
StePPer ENABLE ALL, 1

' Set limits
StePPer LIMITS X, X_MIN, X_MAX
StePPer LIMITS Y, Y_MIN, Y_MAX
StePPer LIMITS Z, Z_MIN, Z_MAX

' Set known starting position
StePPer POSITION X, 0
StePPer POSITION Y, 0
StePPer POSITION Z, 0

StePPer STATUS
Print "MM.CODE free slots = "; MM.CODE

' ---------------------------
' Test 0: S-curve A/B (G0/G1 only)
' ---------------------------

Call ShowSection("Test 0: S-curve A/B (same path, SCURVE 0 then 1)")
Print "Note: Arcs (G2/G3 segments) use existing profile initially."
Print "If STEPPER JERK errors, increase JERK_MM_S3."

Const JERK_MM_S3 = 1000000

' IMPORTANT: STEPPER JERK must be issued after STEPPER AXIS so steps/mm are known.
StePPer JERK JERK_MM_S3

' A/B path: absolute square that returns to start (same start/end position)
StePPer GCODE G90

Print "Run A: trapezoid (SCURVE 0)"
StePPer SCURVE 0
StePPer CLEAR
Call QueueG1XY(30, 30, 1800)
Call QueueG1XY(60, 30, 1800)
Call QueueG1XY(60, 60, 1800)
Call QueueG1XY(30, 60, 1800)
Call QueueG1XY(30, 30, 1800)
Call RunAndWait(8000)

Print "Run B: S-curve (SCURVE 1)"
StePPer SCURVE 1
StePPer CLEAR
Call QueueG1XY(30, 30, 1800)
Call QueueG1XY(60, 30, 1800)
Call QueueG1XY(60, 60, 1800)
Call QueueG1XY(30, 60, 1800)
Call QueueG1XY(30, 30, 1800)
Call RunAndWait(8000)

' ---------------------------
' Test 1: Single-axis rapid (G0)
' ---------------------------

Call ShowSection("Test 1: Single-axis rapid (G0)")

StePPer CLEAR
Call QueueG0X(10)
StePPer BUFFER
StePPer POLL
Call RunAndWait(1000)
StePPer STATUS

' ---------------------------
' Test 2: Single-axis feed (G1) with feedrate
' ---------------------------

Call ShowSection("Test 2: Single-axis feed (G1) + F")

StePPer CLEAR
Call QueueG1X(50, 1200)   ' 1200 mm/min
StePPer BUFFER
StePPer POLL
Call RunAndWait(3000)
StePPer STATUS

' ---------------------------
' Test 3: Incremental mode (G91) then back to absolute (G90)
' ---------------------------

Call ShowSection("Test 3: Incremental mode (G91) then absolute (G90)")

StePPer CLEAR
StePPer GCODE G90
Call QueueG1X(20, 1200)
StePPer GCODE G91
Call WaitForBufferSpace(1)
StePPer GCODE G1, X, 5, F, 600
Call WaitForBufferSpace(1)
StePPer GCODE G1, X, -5, F, 600
StePPer GCODE G90
StePPer BUFFER
StePPer POLL : StePPer POLL : StePPer POLL
Call RunAndWait(4000)
StePPer STATUS

' ---------------------------
' Test 4: Simple 2-axis diagonal move
' ---------------------------

Call ShowSection("Test 4: 2-axis coordinated move (G1 X,Y)")

StePPer CLEAR
Call QueueG1XY(30, 30, 1500)
StePPer BUFFER
StePPer POLL
Call RunAndWait(4000)
StePPer STATUS

' ---------------------------
' Test 5: Consecutive corners (for junction blending observation)
' ---------------------------

Call ShowSection("Test 5: Consecutive corners (blend starting from 3rd block)")
Print "Note: Local blending is applied when at least 2 blocks are already queued."

StePPer CLEAR
StePPer GCODE G90

' Queue a square path (4 segments). Blending will begin at the 2nd corner (3rd segment).
Call QueueG1XY(60, 30, 1800)
Call QueueG1XY(60, 60, 1800)
Call QueueG1XY(30, 60, 1800)
Call QueueG1XY(30, 30, 1800)

StePPer BUFFER
StePPer POLL : StePPer POLL : StePPer POLL : StePPer POLL
Call RunAndWait(8000)
StePPer STATUS

' ---------------------------
' Test 6: Z axis move (separate slower axis)
' ---------------------------

Call ShowSection("Test 6: Z axis move")

StePPer CLEAR
Call QueueG1Z(5, 300)     ' 300 mm/min
Call QueueG1Z(0, 300)
StePPer BUFFER
StePPer POLL : StePPer POLL
Call RunAndWait(6000)
StePPer STATUS

' ---------------------------
' Test 7: Arc tests (G2/G3) in XY
' ---------------------------

Call ShowSection("Test 7: Arc tests (G2/G3)")
Print "Arcs are linearised into segments; this will use multiple buffer slots."

StePPer CLEAR
StePPer GCODE G90

' Start from (30,30). Make a clockwise semicircle to (70,30) with centre at (50,30): I=+20, J=0.
Call QueueG1XY(30, 30, 1200)
Call QueueG2XY_IJ(70, 30, 20, 0, 1200)

' Then CCW semicircle back to start with centre at (50,30): I=-20, J=0.
Call QueueG3XY_IJ(30, 30, -20, 0, 1200)

StePPer BUFFER
StePPer POLL
Call RunAndWait(12000)
StePPer STATUS

' ---------------------------
' Test 8: Buffer fill / MM.CODE behaviour
' ---------------------------

Call ShowSection("Test 8: MM.CODE buffer space guarding")

StePPer CLEAR
Print "Initial free slots (MM.CODE) = "; MM.CODE

' Attempt to queue more items than buffer holds, but guard using MM.CODE.
' This should never trigger the firmware's "G-code buffer full" error.
For i% = 1 To 25
  Call WaitForBufferSpace(1)
  StePPer GCODE G1, X, (i% * 2), F, 600
Next i%

StePPer BUFFER
Print "Free slots after queuing (MM.CODE) = "; MM.CODE

' Inspect a few entries
StePPer POLL : StePPer POLL : StePPer POLL

' Run what is queued (may take a while)
Call RunAndWait(15000)
StePPer STATUS

' ---------------------------
' Test 9: ESTOP
' ---------------------------

Call ShowSection("Test 9: ESTOP")
Print "This will start a longer move then ESTOP shortly after."

StePPer CLEAR
StePPer GCODE G90
Call QueueG1X(150, 1200)
StePPer RUN
Pause 300
StePPer ESTOP
StePPer STATUS
Print "After ESTOP, MM.CODE free slots = "; MM.CODE

Print
Print "Done."
End
