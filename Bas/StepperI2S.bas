' StepperI2S.bas - run the stepper alongside I2S audio and look for audio underruns.
' Plays a tone, then samples the I2S TX FIFO level (PC3: PIO2 SM1, FLEVEL bits 11:8) with
' no motion and while blended G1/G2 moves run. The audio interrupt keeps the FIFO at 6
' words; it reaching 0 means the interrupt was held off for over ~3 stereo frames (68us).
' BASIC cannot sample while it is inside STEPPER GCODE, so the time each command takes is
' measured too: junction blending runs with interrupts off, so a call shorter than ~68us
' cannot cause an underrun. X: STEP GP2, DIR GP3.  Y: STEP GP4, DIR GP5.  No wiring needed.
Option Explicit
Option Default Float
Const FLEVEL% = &H5040000C, SHIFT% = 8
Dim hist%(15), n%, k%, q%, lv%, t, worst, tot, x, y, px, py, nx, ny, md$, cmd%

Function Level%()
  Level% = (Peek(WORD FLEVEL%) >> SHIFT%) And 15
End Function

Sub ClearHist
  For k% = 0 To 15 : hist%(k%) = 0 : Next
  n% = 0
End Sub

Sub ShowHist(label$)
  Local mn% = 99
  For k% = 0 To 15
    If hist%(k%) > 0 And k% < mn% Then mn% = k%
  Next
  Print label$; ": "; n%; " samples, min level "; mn%; ", empty "; hist%(0); ", levels 0-8:";
  For k% = 0 To 8 : Print " "; hist%(k%); : Next
  Print
End Sub

Sub Setup(buf%)
  STEPPER INIT 0.5, buf%
  STEPPER AXIS X, GP2, GP3, , 0, 80, 6000, 500
  STEPPER AXIS Y, GP4, GP5, , 0, 80, 6000, 500
  STEPPER POSITION X, 60
  STEPPER POSITION Y, 50
  STEPPER GCODE G90
End Sub

' Queue move number q% of a lap of a R10 circle: even laps as 36 G2 arcs,
' odd laps as 36 G1 chords, all at F6000, returning to (60,50) at the end of each lap.
Sub QueueMove(q%)
  Local a = Rad(-10 * ((q% Mod 36) + 1))
  nx = 50 + 10 * Cos(a) : ny = 50 + 10 * Sin(a)
  If (q% \ 36) Mod 2 = 0 Then
    STEPPER GCODE G2, X, nx, Y, ny, I, 50 - px, J, 50 - py, F, 6000
  Else
    STEPPER GCODE G1, X, nx, Y, ny, F, 6000
  EndIf
  px = nx : py = ny
End Sub

PLAY TONE 1000, 1000
Pause 200

' 1. Tone only
ClearHist
t = Timer
Do
  lv% = Level%() : hist%(lv%) = hist%(lv%) + 1 : n% = n% + 1
Loop Until Timer - t > 2000
ShowHist "Tone only            "

' 2. Tone with blended moves running for 12 laps (about 10 s, so it can be listened to):
'    queue whenever there is room, sample the FIFO otherwise
Setup 64
STEPPER RUN
STEPPER GCODE G64
ClearHist
px = 60 : py = 50 : q% = 0
t = Timer
Do
  If q% < 432 And Peek(STEPPER BUFFER) > 0 Then
    QueueMove q% : q% = q% + 1
  Else
    lv% = Level%() : hist%(lv%) = hist%(lv%) + 1 : n% = n% + 1
  EndIf
Loop Until q% = 432 And Peek(STEPPER ACTIVE) = 0 And Peek(STEPPER BUFFER) = 64
Print "Moves took "; Str$((Timer - t) / 1000, 0, 2); " s, end X,Y="; Str$(Peek(STEPPER X), 0, 3); ","; Str$(Peek(STEPPER Y), 0, 3); " (expected 60.000,50.000)"
ShowHist "Tone + stepper moving"
STEPPER CLOSE

' 3. Time each STEPPER GCODE call, queued without running so every one blends (G64)
'    or none does (G61). The difference is the interrupts-off blending work.
For cmd% = 1 To 2
  If cmd% = 1 Then md$ = "G61" Else md$ = "G64"
  Setup 256
  If md$ = "G61" Then STEPPER GCODE G61 Else STEPPER GCODE G64
  px = 60 : py = 50 : worst = 0 : tot = 0
  For q% = 0 To 71
    t = Timer
    QueueMove q%
    t = Timer - t
    tot = tot + t
    If t > worst Then worst = t
  Next
  Print md$; " queueing: mean "; Str$(tot / 72 * 1000, 0, 0); " us, worst "; Str$(worst * 1000, 0, 0); " us per command"
  STEPPER CLOSE
Next

PLAY STOP
Print "Done"
