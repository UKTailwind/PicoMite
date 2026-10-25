' StepperPullTest.bas - check the STEPPER limit and E-STOP inputs keep their pull resistor,
' and the enable and spindle outputs start at their inactive level.
' All test pins are unconnected (they float), so the pad pull decides an input's level.
' Reads RP2350 PADS_BANK0 (PUE bit 3, PDE bit 2) and SIO GPIO_IN directly for inputs, and
' SIO GPIO_OUT / GPIO_OE for outputs (output pads have their input buffer off, so GPIO_IN
' reads 0 there).
Option Explicit
Option Default Integer

Function Pad$(g)
  Local v, inb
  v = Peek(WORD &H40038004 + 4 * g)
  inb = (Peek(WORD &HD0000004) >> g) And 1
  Pad$ = "GP" + Str$(g) + ": pull-up=" + Str$((v >> 3) And 1) + " pull-down=" + Str$((v >> 2) And 1) + " level=" + Str$(inb)
End Function

Function OutLvl$(g)
  Local o, e
  If g < 32 Then
    o = (Peek(WORD &HD0000010) >> g) And 1 : e = (Peek(WORD &HD0000030) >> g) And 1
  Else
    o = (Peek(WORD &HD0000014) >> (g - 32)) And 1 : e = (Peek(WORD &HD0000034) >> (g - 32)) And 1
  EndIf
  OutLvl$ = "GP" + Str$(g) + ": output=" + Str$(e) + " drives " + Str$(o)
End Function

Print "Test A: E-STOP on GP6, X enable on GP7, active-low limits on GP0, GP1, GP26"
STEPPER INIT 0.5, 32, GP6
Print "  E-STOP "; Pad$(6)
STEPPER AXIS X, GP2, GP3, GP7, 0, 80, 6000, 500
Print "  Enable after AXIS      "; OutLvl$(7); "  (expect 1: drivers disabled)"
STEPPER ENABLE X, 1
Print "  Enable after ENABLE 1  "; OutLvl$(7); "  (expect 0)"
STEPPER ENABLE X, 0
Print "  Enable after ENABLE 0  "; OutLvl$(7); "  (expect 1)"
STEPPER HWLIMITS GP0, GP1, GP26
Print "  X_MIN "; Pad$(0)
Print "  Y_MIN "; Pad$(1)
Print "  Z_MIN "; Pad$(26)
STEPPER POSITION X, 0
STEPPER RUN
STEPPER GCODE G90
STEPPER GCODE G1, X, 20, F, 3000
Pause 1500
STEPPER STATUS
STEPPER CLOSE

Print "Test B: active-high limits (invert mask 7) on GP0, GP1, GP26"
STEPPER INIT
STEPPER AXIS X, GP2, GP3, , 0, 80, 6000, 500
STEPPER HWLIMITS GP0, GP1, GP26, 0, 0, 0, 7
Print "  X_MIN "; Pad$(0)
Print "  Y_MIN "; Pad$(1)
Print "  Z_MIN "; Pad$(26)
STEPPER POSITION X, 0
STEPPER RUN
STEPPER GCODE G90
STEPPER GCODE G1, X, 20, F, 3000
Pause 1500
STEPPER STATUS
STEPPER CLOSE

Print "Test C: G28 X with an unconnected X_MIN on GP0 (should run to 'limit not reached')"
STEPPER INIT
STEPPER AXIS X, GP2, GP3, , 0, 2, 60000, 1000
STEPPER HWLIMITS GP0, GP1, GP26
STEPPER POSITION X, 0
On Error Skip
STEPPER GCODE G28, X, 1
Print "  G28 ended with: "; MM.ErrMsg$
STEPPER CLOSE

Print "Test D: spindle output on GP38, active-high then inverted (active-low)"
' M03/M05 are buffered and run in order with motion, so an axis is set up, its
' position made known and the stepper armed first
STEPPER INIT
STEPPER AXIS X, GP2, GP3, , 0, 80, 6000, 500
STEPPER POSITION X, 0
STEPPER SPINDLE GP38
Print "  Active-high after SPINDLE "; OutLvl$(38); "  (expect 0: off)"
STEPPER RUN
STEPPER GCODE M03
Pause 50
Print "  Active-high after M03     "; OutLvl$(38); "  (expect 1)"
STEPPER GCODE M05
Pause 50
Print "  Active-high after M05     "; OutLvl$(38); "  (expect 0)"
STEPPER CLOSE
STEPPER INIT
STEPPER AXIS X, GP2, GP3, , 0, 80, 6000, 500
STEPPER POSITION X, 0
STEPPER SPINDLE GP38, 1
Print "  Inverted after SPINDLE    "; OutLvl$(38); "  (expect 1: off)"
STEPPER RUN
STEPPER GCODE M03
Pause 50
Print "  Inverted after M03        "; OutLvl$(38); "  (expect 0)"
STEPPER GCODE M05
Pause 50
Print "  Inverted after M05        "; OutLvl$(38); "  (expect 1)"
STEPPER CLOSE
Print "Done"
