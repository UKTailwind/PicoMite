' landtest.bas - Phase 4, fourth piece: how a fall ends.
'
' When a fall reaches a floor the engine has to decide what it cost.  Two
' velocity thresholds separate a landing that costs nothing from one that hurts
' and one that kills, but two cases are decided before velocity is looked at at
' all: being dead already on the way down, and landing on live spikes.
'
' Order is the whole of it.  Checking velocity first would let a gentle drop
' onto spikes be survivable, and would give a corpse a soft landing.
'
' Exhaustive over every block type underfoot, every velocity through the range,
' and both the alive and the spikes-armed flags, then checksummed so it can be
' compared with the host reference exactly.

Option Console Serial
Option Explicit
Option Default None

' Below the first it costs nothing, below the second it hurts, at or above it
' kills.
Const OOF_VELOCITY = 22
Const DEATH_VELOCITY = 33
Const SPIKES = 2

Const LAND_SOFT = 0
Const LAND_MED = 1
Const LAND_HARD = 2
Const LAND_IMPALE = 3

Const EXP_SOFT = 1298
Const EXP_MED = 649
Const EXP_HARD = 2891
Const EXP_IMPALE = 82
Const EXP_SUM = 10558295

Dim INTEGER ck, nSoft, nMed, nHard, nImpale
Dim INTEGER underfoot, yvel, alive, lethal, r
Dim FLOAT t0, tRun

Print "--- landing test"

ck = 0 : nSoft = 0 : nMed = 0 : nHard = 0 : nImpale = 0
t0 = Timer
For underfoot = 0 To 29
  For yvel = 0 To 40
    For alive = 1 To 0 Step -1
      For lethal = 1 To 0 Step -1
        r = Landing(yvel, underfoot, alive, lethal)
        Select Case r
          Case LAND_SOFT   : nSoft = nSoft + 1
          Case LAND_MED    : nMed = nMed + 1
          Case LAND_HARD   : nHard = nHard + 1
          Case LAND_IMPALE : nImpale = nImpale + 1
        End Select
        ck = (ck * 31 + r) And &HFFFFFF
      Next lethal
    Next alive
  Next yvel
Next underfoot
tRun = Timer - t0

Print "  soft   "; Str$(nSoft); "  expected "; Str$(EXP_SOFT)
Print "  med    "; Str$(nMed); "  expected "; Str$(EXP_MED)
Print "  hard   "; Str$(nHard); "  expected "; Str$(EXP_HARD)
Print "  impale "; Str$(nImpale); "  expected "; Str$(EXP_IMPALE)
Print "  checksum "; Str$(ck); "  expected "; Str$(EXP_SUM)
Print "  time     "; Str$(tRun, 0, 1); " ms for 4920 cases"

Print
If nSoft = EXP_SOFT And nMed = EXP_MED And nHard = EXP_HARD And nImpale = EXP_IMPALE And ck = EXP_SUM Then
  Print "PASS - the landing agrees with the host reference exactly"
Else
  Print "FAIL - the engine and the reference disagree"
End If
End

'-----------------------------------------------------------------------------
' Spikes and being dead already both decide the outcome before velocity does.
Function Landing(yvel As INTEGER, underfoot As INTEGER, alive As INTEGER, lethal As INTEGER) As INTEGER
  If underfoot = SPIKES And lethal Then Landing = LAND_IMPALE : Exit Function
  If alive = 0 Then Landing = LAND_HARD : Exit Function
  If yvel < OOF_VELOCITY Then Landing = LAND_SOFT : Exit Function
  If yvel < DEATH_VELOCITY Then Landing = LAND_MED : Exit Function
  Landing = LAND_HARD
End Function
