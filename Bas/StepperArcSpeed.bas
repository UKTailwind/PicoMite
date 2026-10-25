' StepperArcSpeed.bas - time a circle drawn as one G2, as 36 short G2 arcs, and as 36 G1
' chords, against the ideal time at the requested feed. Short moves only run near full speed
' when consecutive moves (including arcs) blend at their junctions.
' X: STEP GP2, DIR GP3.  Y: STEP GP4, DIR GP5.  No motors or wiring needed.
' Times run from queueing to PEEK(STEPPER ACTIVE) = 0, so they include planning.
Option Explicit
Option Default Float
Dim t%, k%, acc, f, r, cx, cy, px, py, nx, ny, ideal

Sub Setup
  STEPPER INIT 0.5, 64
  STEPPER AXIS X, GP2, GP3, , 0, 80, 6000, acc
  STEPPER AXIS Y, GP4, GP5, , 0, 80, 6000, acc
  STEPPER POSITION X, cx + r
  STEPPER POSITION Y, cy
  STEPPER RUN
  STEPPER GCODE G90
  STEPPER GCODE G64
  t% = Timer
End Sub

Sub Finish(label$)
  Local e
  Do : Loop Until (Peek(STEPPER ACTIVE) = 0 And Peek(STEPPER BUFFER) = 64) Or Timer - t% > 60000
  e = (Timer - t%) / 1000
  Print label$; ": "; Str$(e, 0, 2); " s  (ideal "; Str$(ideal, 0, 2); " s, "; Str$(ideal / e * 100, 0, 0); "% of requested speed)  end X,Y=";
  Print Str$(Peek(STEPPER X), 0, 3); ","; Str$(Peek(STEPPER Y), 0, 3)
  STEPPER CLOSE
End Sub

Sub DoTest(radius, feed, accel)
  r = radius : f = feed : acc = accel : cx = 50 : cy = 50
  ideal = 2 * Pi * r / (f / 60)
  Print "R="; r; "mm  F="; f; "mm/min  accel="; acc; "mm/s^2"

  Setup
  STEPPER GCODE G2, X, cx + r, Y, cy, I, -r, J, 0, F, f
  Finish "  one G2 circle      "

  Setup
  px = cx + r : py = cy
  For k% = 1 To 36
    nx = cx + r * Cos(Rad(-10 * k%)) : ny = cy + r * Sin(Rad(-10 * k%))
    STEPPER GCODE G2, X, nx, Y, ny, I, cx - px, J, cy - py, F, f
    px = nx : py = ny
  Next
  Finish "  36 x 10deg G2 arcs "

  Setup
  For k% = 1 To 36
    nx = cx + r * Cos(Rad(-10 * k%)) : ny = cy + r * Sin(Rad(-10 * k%))
    STEPPER GCODE G1, X, nx, Y, ny, F, f
  Next
  Finish "  36 G1 chords (G64)"
End Sub

DoTest 10, 1200, 500
DoTest 10, 3000, 500
DoTest 2, 1200, 500
Print "Done"
