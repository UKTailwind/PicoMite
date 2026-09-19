' animtest.bas - Phase 4, first piece: the frame advance.
'
' Phase 2 proved the sequence walk.  This proves what the sequences DO to a
' character: one frame advance runs the byte code until it reaches a frame
' number, updating the character on the way, and that frame number becomes the
' pose.  Every sequence is run for a fixed number of frames and the states are
' checksummed, so the result can be compared with the host reference exactly.
'
' Two things to keep straight, both read from the original's dispatcher:
'   chx moves the character and is adjusted for facing; chy is not.
'   The frame table's displacements are DRAWING offsets, applied when the sprite
'   is positioned.  They do not move the character.  Only chx and chy do.
'
' Facing is -1 (255) for left, which is how the artwork is drawn, and 0 for
' right, which is mirrored.  Moving forward while facing left decreases x.
'
' Needs seq.dat, frames.dat and tables.idx from the converter.

Option Console Serial
Option Explicit
Option Default None

Const MAXB = 4096
Const FRAMESPER = 24
Const EXP_ADVANCES = 2736
Const EXP_STALLED = 0
Const EXP_SUM = 1518061

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
Const GUARD = 400

Dim INTEGER packed(MAXB/8 + 8), seqb(MAXB), frmb(MAXB)
Dim INTEGER seqLen, seqCount, frmLen, frmCount, frmEntry
Dim INTEGER weightless
' The character record, laid out as the original does.
Dim INTEGER cPosn, cX, cY, cFace, cBlockX, cBlockY, cAction, cXVel, cYVel
Dim INTEGER cSeq, cScrn, cRepeat, cID, cSword, cLife
Dim INTEGER advances, stalled, ck, i, f, start, ok
Dim STRING home
Dim FLOAT t0, tRun

home = MM.Info(Path)
If home = "NONE" Then home = "A:/"

Print "--- frame advance test"
ReadLayout
LoadBytes home + "seq.dat", seqLen, seqb()
LoadBytes home + "frames.dat", frmLen, frmb()
Print "  seq "; Str$(seqLen); " bytes / "; Str$(seqCount); " sequences, frames "; Str$(frmCount)

weightless = 0
advances = 0 : stalled = 0 : ck = 0
t0 = Timer
For i = 0 To seqCount - 1
  start = seqb(i*2) Or (seqb(i*2 + 1) << 8)
  NewChar start
  For f = 1 To FRAMESPER
    ok = Advance()
    If ok = 0 Then
      stalled = stalled + 1
      Exit For
    End If
    advances = advances + 1
    Mix cPosn : Mix cX : Mix cY : Mix cFace : Mix cBlockY
    Mix cAction : Mix cXVel : Mix cYVel : Mix cSeq
  Next f
Next i
tRun = Timer - t0

Print
Print "frame advances "; Str$(advances); "  expected "; Str$(EXP_ADVANCES)
Print "stalled        "; Str$(stalled); "  expected "; Str$(EXP_STALLED)
Print "checksum       "; Str$(ck); "  expected "; Str$(EXP_SUM)
Print "time           "; Str$(tRun, 0, 2); " ms for "; Str$(advances); " advances"
If advances > 0 Then
  Print "per advance    "; Str$(tRun * 1000 / advances, 0, 1); " us"
End If

Print
If advances = EXP_ADVANCES And stalled = EXP_STALLED And ck = EXP_SUM Then
  Print "PASS - the frame advance agrees with the host reference exactly"
Else
  Print "FAIL - the engine and the reference disagree"
End If

Option Console Both
End

'-----------------------------------------------------------------------------
Sub Mix(v As INTEGER)
  ck = (ck * 31 + v) And &HFFFFFF
End Sub

'-----------------------------------------------------------------------------
Sub NewChar(s As INTEGER)
  cPosn = 0 : cX = 100 : cY = 100 : cFace = &HFF
  cBlockX = 0 : cBlockY = 1 : cAction = 0
  cXVel = 0 : cYVel = 0
  cSeq = s : cScrn = 1 : cRepeat = 0 : cID = 0 : cSword = 0 : cLife = 0
End Sub

'-----------------------------------------------------------------------------
' One frame advance.  Returns 1 when it reached a frame, 0 if it ran away.
Function Advance() As INTEGER
  Local INTEGER n, op, d
  For n = 1 To GUARD
    If cSeq < 0 Or cSeq >= seqLen Then Advance = 0 : Exit Function
    op = seqb(cSeq)
    cSeq = cSeq + 1

    If op < OP_LOW Then                 ' a frame number: this is the pose
      cPosn = op
      Advance = 1
      Exit Function
    End If

    Select Case op
      Case OP_CHX
        d = Sgn8(seqb(cSeq)) : cSeq = cSeq + 1
        If (cFace And &H80) Then cX = (cX - d) And &HFF Else cX = (cX + d) And &HFF
      Case OP_CHY
        d = Sgn8(seqb(cSeq)) : cSeq = cSeq + 1
        cY = (cY + d) And &HFF
      Case OP_ABOUTFACE
        cFace = cFace Xor &HFF
      Case OP_GOTO
        cSeq = seqb(cSeq) Or (seqb(cSeq + 1) << 8)
      Case OP_UP
        cBlockY = (cBlockY - 1) And &HFF
      Case OP_DOWN
        cBlockY = (cBlockY + 1) And &HFF
      Case OP_ACT
        cAction = seqb(cSeq) : cSeq = cSeq + 1
      Case OP_SETFALL
        cXVel = seqb(cSeq) : cYVel = seqb(cSeq + 1) : cSeq = cSeq + 2
      Case OP_IFWTLESS
        If weightless Then
          cSeq = seqb(cSeq) Or (seqb(cSeq + 1) << 8)
        Else
          cSeq = cSeq + 2
        End If
      Case OP_DIE, OP_JARD, OP_JARU, OP_NEXTLEVEL
        ' nothing to do here.  jaru takes NO operand: the byte after it is
        ' the next frame, and skipping it lost the touch-ceiling pose.
      Case OP_EFFECT, OP_TAP
        cSeq = cSeq + 1
      Case Else
        Advance = 0
        Exit Function
    End Select
  Next n
  Advance = 0
End Function

'-----------------------------------------------------------------------------
' Where the sprite goes for the current pose.  The frame's own displacement is
' applied here and nowhere else, then the result is doubled, which is how the
' 140-unit character space becomes 280 pixels.
Function DrawX() As INTEGER
  Local INTEGER f, dx, x
  f = (cPosn - 1) * frmEntry
  dx = Sgn8(frmb(f + 2))
  If (cFace And &H80) Then x = (cX - dx) And &HFF Else x = (cX + dx) And &HFF
  DrawX = ((x - 58) And &HFF) * 2
End Function

Function DrawY() As INTEGER
  Local INTEGER f
  f = (cPosn - 1) * frmEntry
  DrawY = (cY + Sgn8(frmb(f + 3))) And &HFF
End Function

'-----------------------------------------------------------------------------
Function Sgn8(v As INTEGER) As INTEGER
  If v > 127 Then Sgn8 = v - 256 Else Sgn8 = v
End Function

'-----------------------------------------------------------------------------
Sub LoadBytes(path As STRING, n As INTEGER, dst() As INTEGER)
  Open path For Input As #1
  Memory Input #1, n, packed()
  Close #1
  Memory Unpack packed(), dst(), n, 8
End Sub

'-----------------------------------------------------------------------------
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
