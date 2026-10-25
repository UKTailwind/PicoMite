' StepperPullTest.bas - check the STEPPER limit and E-STOP inputs keep their pull resistor.
' All test pins are unconnected (they float), so the pad pull decides the level.
' Reads RP2350 PADS_BANK0 (PUE bit 3, PDE bit 2) and SIO GPIO_IN directly.
' STEPPER INIT will not start alongside I2S audio: use OPTION AUDIO DISABLE first if set.
Option Explicit
Option Default Integer

Function Pad$(g)
  Local v, inb
  v = Peek(WORD &H40038004 + 4 * g)
  inb = (Peek(WORD &HD0000004) >> g) And 1
  Pad$ = "GP" + Str$(g) + ": pull-up=" + Str$((v >> 3) And 1) + " pull-down=" + Str$((v >> 2) And 1) + " level=" + Str$(inb)
End Function

Print "Test A: E-STOP on GP6, X enable on GP7, active-low limits on GP0, GP1, GP26"
STEPPER INIT 0.5, 32, GP6
Print "  E-STOP "; Pad$(6)
STEPPER AXIS X, GP2, GP3, GP7, 0, 80, 6000, 500
Print "  Enable "; Pad$(7); "  (1 = drivers disabled)"
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
Print "Done"
