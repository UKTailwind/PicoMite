' ctrltest.bas - Phase 4, second piece: the input layer and the pose dispatch.
'
' The control machine asks two questions every frame: what is the character
' doing now, and what is the player asking for.  This is both of those, without
' the answers that need the opponent or the world.
'
' The input model is the original's, and it is worth copying exactly:
'
'   jstkX   -1 forward, +1 back, 0 centred
'   jstkY   -1 up,      +1 down, 0 centred
'   btn     -1 down (pressed), +1 up
'
' plus a "fresh press" flag per direction and for the button.  A flag goes
' negative on the frame a press appears and is set to 1 once something has acted
' on it, so one press cannot be consumed twice.  Holding a key is a different
' input from pressing it, and several states care about the difference.
'
' The neat trick worth keeping: forward and back are mirrored when the character
' faces right, so every decision below reasons in forward/back terms and never
' in left/right.  One normalisation at the edge removes a facing test from every
' branch of the tree.
'
' Needs seq.dat, frames.dat and tables.idx from the converter.

Option Console Serial
Option Explicit
Option Default None

Const MAXB = 4096
Const GUARD = 400

Const OP_LOW = &HF1
Const OP_CHX = &HFB
Const OP_CHY = &HFA
Const OP_ABOUTFACE = &HFE
Const OP_GOTO = &HFF
Const OP_UP = &HFD
Const OP_DOWN = &HFC
Const OP_ACT = &HF9
Const OP_SETFALL = &HF8
Const OP_IFWTLESS = &HF7
Const OP_DIE = &HF6
Const OP_JARU = &HF5
Const OP_JARD = &HF4
Const OP_EFFECT = &HF3
Const OP_TAP = &HF2
Const OP_NEXTLEVEL = &HF1

' What the character is doing now, worked out from the previous frame number.
Const ST_STANDING = 0
Const ST_TURNING = 1
Const ST_STARTING = 2
Const ST_RUNNING = 3
Const ST_STJUMPUP = 4
Const ST_OTHER = 5

' Sequences this piece can start.  Numbers are the sequence table's own.
Const SEQ_STARTRUN = 1
Const SEQ_STAND = 2
Const SEQ_STANDJUMP = 3
Const SEQ_RUNJUMP = 4
Const SEQ_TURN = 5
Const SEQ_RUNSTOP = 13
Const SEQ_JUMPUP = 14
Const SEQ_STOOP = 50
Const SEQ_RUNNING = 84

Dim INTEGER packed(MAXB/8 + 8), seqb(MAXB), frmb(MAXB)
Dim INTEGER seqLen, seqCount, frmLen, frmCount, frmEntry
Dim INTEGER weightless
Dim INTEGER cPosn, cX, cY, cFace, cBlockX, cBlockY, cAction, cXVel, cYVel
Dim INTEGER cSeq, cScrn, cRepeat, cID, cSword, cLife
' Input: current, and the fresh-press flags
Dim INTEGER jstkX, jstkY, btn
Dim INTEGER clrF, clrB, clrU, clrD, clrBtn
Dim INTEGER pF, pB, pU, pD, pBtn          ' what was held last frame
Dim INTEGER seqTab(200)
Dim INTEGER i, fails
Dim STRING home

home = MM.Info(Path)
If home = "NONE" Then home = "A:/"

Print "--- control layer test"
ReadLayout
LoadBytes home + "seq.dat", seqLen, seqb()
LoadBytes home + "frames.dat", frmLen, frmb()
For i = 0 To seqCount - 1
  seqTab(i + 1) = seqb(i*2) Or (seqb(i*2 + 1) << 8)
Next i
Print "  "; Str$(seqCount); " sequences, "; Str$(frmCount); " frames"

fails = 0
TestFresh
TestFacing
TestDispatch
TestStanding

Print
If fails = 0 Then
  Print "PASS - input layer and pose dispatch behave as specified"
Else
  Print "FAIL - "; Str$(fails); " check(s) failed"
End If
Option Console Both
End

'-----------------------------------------------------------------------------
Sub Check(what As STRING, got As INTEGER, want As INTEGER)
  If got <> want Then
    Print "  FAIL "; what; ": got "; Str$(got); ", wanted "; Str$(want)
    fails = fails + 1
  End If
End Sub

'=============================================================================
' Input layer
'=============================================================================

' Set the raw input for this frame, then work out which presses are fresh.
' A press is fresh on the frame it first appears and stays available until
' something consumes it.
Sub SetInput(fwd As INTEGER, bk As INTEGER, up As INTEGER, dn As INTEGER, b As INTEGER)
  If fwd Then jstkX = -1 Else If bk Then jstkX = 1 Else jstkX = 0
  If up Then jstkY = -1 Else If dn Then jstkY = 1 Else jstkY = 0
  If b Then btn = -1 Else btn = 1

  If fwd And Not pF Then clrF = -1
  If bk And Not pB Then clrB = -1
  If up And Not pU Then clrU = -1
  If dn And Not pD Then clrD = -1
  If b And Not pBtn Then clrBtn = -1

  ' A direction that is no longer held has no press left to give.
  If Not fwd Then clrF = 1
  If Not bk Then clrB = 1
  If Not up Then clrU = 1
  If Not dn Then clrD = 1
  If Not b Then clrBtn = 1

  pF = fwd : pB = bk : pU = up : pD = dn : pBtn = b
End Sub

' Forward and back are relative to the character, so mirror them when he faces
' right.  Called once on the way in and once on the way out, which leaves the
' stored flags in the player's frame of reference.
Sub FaceJstk
  Local INTEGER t
  jstkX = -jstkX
  t = clrF : clrF = clrB : clrB = t
End Sub

'=============================================================================
' What is the character doing now?  Worked out from the previous frame number,
' the way the original does it.
'=============================================================================
Function PoseState(posn As INTEGER) As INTEGER
  If posn = 15 Then PoseState = ST_STANDING : Exit Function
  If posn = 48 Then PoseState = ST_TURNING : Exit Function
  If posn >= 50 And posn < 53 Then PoseState = ST_STANDING : Exit Function
  If posn < 4 Then PoseState = ST_STARTING : Exit Function
  If posn >= 67 And posn < 70 Then PoseState = ST_STJUMPUP : Exit Function
  If posn < 15 Then PoseState = ST_RUNNING : Exit Function
  PoseState = ST_OTHER
End Function

'=============================================================================
' Standing, movement only.  The branches that need a sword, an opponent or the
' floor under his feet are not here; they belong with the guards and the traps.
' Returns the sequence to start, or 0 to carry on with the current one.
'=============================================================================
Function StandCtrl() As INTEGER
  StandCtrl = 0
  If btn < 0 Then
    ' Button down is the careful set: back is a measured step, up is a
    ' standing jump straight up.
    If clrB < 0 Then clrB = 1 : StandCtrl = SEQ_TURN : Exit Function
    If clrU < 0 Then clrU = 1 : StandCtrl = SEQ_JUMPUP : Exit Function
    Exit Function
  End If
  If jstkY > 0 Then StandCtrl = SEQ_STOOP : Exit Function
  If jstkY < 0 Then StandCtrl = SEQ_STANDJUMP : Exit Function
  If jstkX < 0 Then StandCtrl = SEQ_STARTRUN : Exit Function
  If jstkX > 0 Then StandCtrl = SEQ_TURN : Exit Function
End Function

'=============================================================================
' Tests
'=============================================================================
Sub TestFresh
  Print "fresh-press flags"
  ClearInput
  SetInput 1,0,0,0,0                      ' forward pressed
  Check "fresh forward", clrF, -1
  Check "jstkX forward", jstkX, -1
  clrF = 1                                ' something consumed it
  SetInput 1,0,0,0,0                      ' still held
  Check "held forward is not fresh again", clrF, 1
  SetInput 0,0,0,0,0                      ' released
  Check "released forward", clrF, 1
  SetInput 1,0,0,0,0                      ' pressed again
  Check "re-pressed forward is fresh", clrF, -1
End Sub

Sub TestFacing
  Print "facing normalisation"
  ClearInput
  SetInput 1,0,0,0,0
  cFace = 0                               ' facing right
  FaceJstk
  Check "mirrored jstkX", jstkX, 1
  Check "mirrored fresh flag moved to back", clrB, -1
  FaceJstk
  Check "mirroring twice restores jstkX", jstkX, -1
  Check "mirroring twice restores the flag", clrF, -1
End Sub

Sub TestDispatch
  Print "pose dispatch"
  Check "frame 15 is standing", PoseState(15), ST_STANDING
  Check "frame 48 is turning", PoseState(48), ST_TURNING
  Check "frame 51 is standing", PoseState(51), ST_STANDING
  Check "frame 2 is starting to run", PoseState(2), ST_STARTING
  Check "frame 68 is jumping up", PoseState(68), ST_STJUMPUP
  Check "frame 10 is running", PoseState(10), ST_RUNNING
  Check "frame 200 is something else", PoseState(200), ST_OTHER
End Sub

Sub TestStanding
  Print "standing decisions"
  ClearInput : SetInput 1,0,0,0,0
  Check "forward starts a run", StandCtrl(), SEQ_STARTRUN
  ClearInput : SetInput 0,1,0,0,0
  Check "back turns round", StandCtrl(), SEQ_TURN
  ClearInput : SetInput 0,0,1,0,0
  Check "up is a standing jump", StandCtrl(), SEQ_STANDJUMP
  ClearInput : SetInput 0,0,0,1,0
  Check "down crouches", StandCtrl(), SEQ_STOOP
  ClearInput : SetInput 0,0,0,0,0
  Check "nothing held stays standing", StandCtrl(), 0
  ClearInput : SetInput 0,0,1,0,1
  Check "button and up jumps straight up", StandCtrl(), SEQ_JUMPUP
  ' and the press is consumed, so holding it does not fire twice
  Check "the press was consumed", clrU, 1
End Sub

Sub ClearInput
  jstkX = 0 : jstkY = 0 : btn = 1
  clrF = 1 : clrB = 1 : clrU = 1 : clrD = 1 : clrBtn = 1
  pF = 0 : pB = 0 : pU = 0 : pD = 0 : pBtn = 0
  cFace = &HFF
End Sub

'=============================================================================
Sub LoadBytes(path As STRING, n As INTEGER, dst() As INTEGER)
  Open path For Input As #1
  Memory Input #1, n, packed()
  Close #1
  Memory Unpack packed(), dst(), n, 8
End Sub

Sub ReadLayout
  Local STRING l, k
  Local INTEGER i, v
  Open home + "tables.idx" For Input As #2
  Do While Not Eof(#2)
    Line Input #2, l
    If l <> "" And Left$(l, 1) <> "#" Then
      i = Instr(l, " ")
      If i > 1 Then
        k = Left$(l, i - 1)
        v = Val(Mid$(l, i + 1))
        Select Case k
          Case "seq_len"      : seqLen = v
          Case "seq_count"    : seqCount = v
          Case "frames_len"   : frmLen = v
          Case "frames_count" : frmCount = v
          Case "frames_entry" : frmEntry = v
        End Select
      End If
    End If
  Loop
  Close #2
End Sub
