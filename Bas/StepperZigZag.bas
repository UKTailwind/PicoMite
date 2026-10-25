' StepperZigZag.bas - the 170 degree zigzag in exact-stop (G61) and blended (G64) modes,
' reporting time and end position against the expected target.
' X: STEP GP2, DIR GP3.  Y: STEP GP4, DIR GP5.  No motors or wiring needed.
' Correct junction speeds slow each 170 degree turn to about 1.5 mm/s: about 1.95 s in total.
Option Explicit
Option Default Float
Dim t%, k%, rep%, x, y, a, md$

For rep% = 1 To 4
  If rep% Mod 2 = 1 Then md$ = "G61" Else md$ = "G64"
  STEPPER INIT 0.5, 64
  STEPPER AXIS X, GP2, GP3, , 0, 80, 6000, 500
  STEPPER AXIS Y, GP4, GP5, , 0, 80, 6000, 500
  STEPPER POSITION X, 50
  STEPPER POSITION Y, 50
  STEPPER RUN
  STEPPER GCODE G90
  If md$ = "G61" Then
    STEPPER GCODE G61
  Else
    STEPPER GCODE G64
  EndIf
  x = 50 : y = 50
  t% = Timer
  For k% = 1 To 10
    If k% Mod 2 = 1 Then a = Rad(5) Else a = Rad(175)
    x = x + 5 * Cos(a) : y = y + 5 * Sin(a)
    STEPPER GCODE G1, X, x, Y, y, F, 3000
  Next
  Do : Loop Until Peek(STEPPER ACTIVE) = 0 And Peek(STEPPER BUFFER) = 64
  Print md$; " zigzag "; Str$((Timer - t%) / 1000, 0, 2); " s, end X,Y="; Str$(Peek(STEPPER X), 0, 4); ","; Str$(Peek(STEPPER Y), 0, 4); "  expected "; Str$(x, 0, 4); ","; Str$(y, 0, 4)
  STEPPER CLOSE
Next
