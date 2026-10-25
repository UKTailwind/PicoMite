' StepperTiming.bas - time the STEPPER STEP/DIR outputs with PIO 1, no wiring needed.
' PIO reads the GPIO pads the stepper code drives as SIO outputs.
' X: STEP GP2, DIR GP3.  Y: STEP GP4, DIR GP5.  Min limits GP0, GP1, GP26 (unconnected).
' PIO clock 10 MHz; the timing loops are 2 instructions, so 1 count = 0.2 us.
Option Explicit
Option Default Integer
Const P = 1, FK = 10000000   ' PIO clock in Hz (10 MHz)
Dim buf%(5119), v, rep, n, k, w, nib, bad, junk, mn, mx, sum

Print "Firmware "; MM.Ver

Sub PioLoad
  PIO Clear P
  ' SM0 @0: STEP high time
  PIO Program Line P, 0, &H2082 ' wait 1 gpio 2
  PIO Program Line P, 1, &HA02B ' mov x, ~null
  PIO Program Line P, 2, &H0043 ' jmp x--, 3
  PIO Program Line P, 3, &H00C2 ' jmp pin, 2
  PIO Program Line P, 4, &HA0C9 ' mov isr, ~x
  PIO Program Line P, 5, &H8020 ' push block
  ' SM1 @8: DIR (GP3) falling edge to next STEP (GP2) rising edge
  PIO Program Line P, 8, &H2083  ' wait 1 gpio 3
  PIO Program Line P, 9, &H2003  ' wait 0 gpio 3
  PIO Program Line P, 10, &HA02B ' mov x, ~null
  PIO Program Line P, 11, &H00CD ' jmp pin, 13
  PIO Program Line P, 12, &H004B ' jmp x--, 11
  PIO Program Line P, 13, &HA0C9 ' mov isr, ~x
  PIO Program Line P, 14, &H8020 ' push block
  ' SM2 @16: STEP,DIR sampled at each STEP rising and falling edge (autopush 4 bits)
  PIO Program Line P, 16, &H20A0 ' wait 1 pin 0
  PIO Program Line P, 17, &H4002 ' in pins, 2
  PIO Program Line P, 18, &H2020 ' wait 0 pin 0
  PIO Program Line P, 19, &H4002 ' in pins, 2
  PIO Init Machine P, 0, FK, Pio(pinctrl 0,0,0,GP2), Pio(execctrl GP2,0,5), Pio(shiftctrl 0,0,0,0,0,0,0,1), 0
  PIO Init Machine P, 1, FK, Pio(pinctrl 0,0,0,GP2), Pio(execctrl GP2,8,14), Pio(shiftctrl 0,0,0,0,0,0,0,1), 8
  PIO Init Machine P, 2, FK, Pio(pinctrl 0,0,0,GP2), Pio(execctrl GP2,16,19), Pio(shiftctrl 4,0,1,0,0,0,0,1), 16
End Sub

Function Word(k)
  If k Mod 2 = 0 Then Word = buf%(k \ 2) And &HFFFFFFFF Else Word = (buf%(k \ 2) >> 32) And &HFFFFFFFF
End Function

' Fill the DMA buffer with all-ones; no captured word can be &HFFFFFFFF
Sub DmaArm(sm, nw)
  For k = 0 To 5119 : buf%(k) = -1 : Next
  PIO DMA RX P, sm, nw, buf%()
End Sub

' Wait for the DMA to complete (or time out), return how many words arrived
Function DmaWait(maxwait)
  Local t
  t = Timer
  Do : Pause 200 : Loop Until MM.Info(PIO RX DMA) = 0 Or Timer - t > maxwait * 1000
  k = 0
  Do While k < 10240
    If Word(k) = &HFFFFFFFF Then Exit Do
    k = k + 1
  Loop
  DmaWait = k
End Function

Sub WidthStats(nw, label$)
  If nw <= 0 Then Print label$; ": no pulses captured" : Exit Sub
  mn = 1E9 : mx = 0 : sum = 0
  For k = 0 To nw - 1
    w = Word(k) : sum = sum + w
    If w < mn Then mn = w
    If w > mx Then mx = w
  Next
  Print label$; ": "; nw; " pulses, high time min "; Str$(mn * 0.2, 0, 1); " us, max "; Str$(mx * 0.2, 0, 1); " us, mean "; Str$(sum / nw * 0.2, 0, 2); " us"
End Sub

' ---------------- Test 1: G28 homing pulses ----------------
Print "Test 1: G28 homing (fast approach, limit never trips)"
STEPPER INIT
STEPPER AXIS X, GP2, GP3, , 0, 2, 60000, 1000
STEPPER POSITION X, 0
STEPPER RUN
STEPPER GCODE G90
STEPPER GCODE G1, X, 10, F, 6000   ' positive move leaves DIR high
Pause 1500
' Unconnected limit inputs: their pull-ups hold them inactive, so G28 runs to "limit not reached"
STEPPER HWLIMITS GP0, GP1, GP26
PioLoad
DmaArm 0, 200
PIO Start P, 0
PIO Start P, 1
On Error Skip
STEPPER GCODE G28, X, 1
Print "G28 ended with: "; MM.ErrMsg$
n = DmaWait(5)
PIO Read P, 1, 1, v
If v < 0 Then Print "DIR setup: no DIR edge seen" Else Print "Homing DIR fall to first STEP rise: "; Str$(v * 0.2, 0, 1); " us"
WidthStats n, "Homing STEP"
On Error Skip
PIO DMA RX OFF
STEPPER CLOSE

' ---------------- Test 2: DIR stable across every STEP pulse ----------------
Print "Test 2: linear reversals and four semicircle arcs (X reverses mid-arc)"
STEPPER INIT
STEPPER AXIS X, GP2, GP3, , 0, 80, 6000, 500
STEPPER AXIS Y, GP4, GP5, , 0, 80, 6000, 500
STEPPER POSITION X, 50
STEPPER POSITION Y, 60
PioLoad
DmaArm 2, 9600
PIO Start P, 2
STEPPER RUN
STEPPER GCODE G90
STEPPER GCODE G1, X, 60, F, 1200
STEPPER GCODE G1, X, 40
STEPPER GCODE G1, X, 50
For rep = 1 To 2
  STEPPER GCODE G2, X, 50, Y, 40, I, 0, J, -10
  STEPPER GCODE G2, X, 50, Y, 60, I, 0, J, 10
Next
n = DmaWait(30)
bad = 0 : junk = 0
For k = 0 To n - 1
  nib = Word(k) And 15
  If (nib And 4) = 0 Or (nib And 1) <> 0 Then junk = junk + 1
  If ((nib >> 3) And 1) <> ((nib >> 1) And 1) Then bad = bad + 1
Next
Print "X pulses seen: "; n; " (expected 9600)"
Print "Pulses where DIR changed while STEP was high: "; bad
Print "Malformed samples: "; junk
STEPPER STATUS
On Error Skip
PIO DMA RX OFF

' ---------------- Test 3: ISR pulse width ----------------
Print "Test 3: ISR step pulse width"
PioLoad
DmaArm 0, 200
PIO Start P, 0
STEPPER GCODE G1, X, 60, F, 1200
n = DmaWait(10)
WidthStats n, "ISR STEP"
On Error Skip
PIO DMA RX OFF
PIO Clear P
STEPPER CLOSE
Print "Done"
