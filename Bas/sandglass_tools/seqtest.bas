' seqtest.bas - Phase 2.  The animation sequence interpreter, on its own.
'
' Every movement in the game is a short byte-code program.  This is the machine
' that runs them, exercised with no world around it: it walks all of the
' sequences and counts what it executed, and the counts must agree exactly with
' what the host-side walker got from the same data.  If an operand count is
' wrong the walk steps into the middle of an instruction and the totals diverge
' or it runs off the end, so agreement is real evidence.
'
' Needs seq.dat, frames.dat and tables.idx from the converter in the same
' directory.  Carries no data of its own.

Option Console Serial
Option Explicit
Option Default None

Const MAXB = 4096                ' both tables are well under this
Const EXP_STEPS = 2402           ' what the host walker counted
Const EXP_FRAMES = 1358

' Opcodes.  They are small negative numbers in the source, so they arrive as
' high byte values.  Anything below the lowest is a frame number.
Const OP_LOW = &HF1
Const OP_GOTO = &HFF
Const OP_IFWTLESS = &HF7
Const OP_DIE = &HF6
Const OP_NEXTLEVEL = &HF1

Dim INTEGER packed(MAXB/8 + 8)
Dim INTEGER seqb(MAXB), frmb(MAXB), vis(MAXB)
Dim INTEGER opnd(255)
Dim INTEGER seqLen, seqCount, frmLen, frmCount, frmEntry
Dim INTEGER frmAlt1, frmAlt2, frmSword, seqFirst
Dim INTEGER wSteps, wFrames, walked, problems
Dim INTEGER i, start, bad
Dim STRING wErr, home
Dim FLOAT t0, tWalk

home = MM.Info(Path)
If home = "NONE" Then home = "A:/"

Print "--- sequence interpreter test"
Print "data   "; home

ReadLayout
Print "layout seq "; Str$(seqLen); " bytes / "; Str$(seqCount); " sequences,";
Print " frames "; Str$(frmLen); " bytes / "; Str$(frmCount); " entries"

LoadBytes home + "seq.dat", seqLen, seqb()
LoadBytes home + "frames.dat", frmLen, frmb()

SetOperands

' ---- walk every sequence ---------------------------------------------------
problems = 0 : walked = 0 : wSteps = 0 : wFrames = 0 : bad = 0
t0 = Timer
For i = 0 To seqCount - 1
  start = seqb(i*2) Or (seqb(i*2 + 1) << 8)
  If start < seqCount*2 Or start >= seqLen Then
    Print "sequence "; Str$(i+1); " starts at "; Str$(start); ", outside the data"
    bad = bad + 1
  Else
    wErr = ""
    Walk start, i + 1
    walked = walked + 1
    If wErr <> "" Then
      Print "sequence "; Str$(i+1); " ("; Str$(start); "): "; wErr
      problems = problems + 1
    End If
  End If
Next i
tWalk = Timer - t0
problems = problems + bad

Print
Print "walked "; Str$(walked); " of "; Str$(seqCount); " sequences in "; Str$(tWalk,0,2); " ms"
Print "steps  "; Str$(wSteps); "  expected "; Str$(EXP_STEPS)
Print "frames "; Str$(wFrames); "  expected "; Str$(EXP_FRAMES)

If wSteps <> EXP_STEPS Then problems = problems + 1
If wFrames <> EXP_FRAMES Then problems = problems + 1

' ---- the frame table -------------------------------------------------------
CheckFrames

Print
If problems = 0 Then
  Print "PASS - the interpreter agrees with the host walker exactly"
Else
  Print "FAIL - "; Str$(problems); " problem(s)"
End If

Option Console Both
End

'-----------------------------------------------------------------------------
Sub Walk(start As INTEGER, stamp As INTEGER)
  Local INTEGER pc, op, n, target
  pc = start
  Do
    If pc < 0 Or pc >= seqLen Then wErr = "ran off the end" : Exit Sub
    If vis(pc) = stamp Then Exit Sub            ' a cycle: an idle loop
    vis(pc) = stamp
    op = seqb(pc)
    wSteps = wSteps + 1
    If op < OP_LOW Then
      wFrames = wFrames + 1
      pc = pc + 1
    Else
      n = opnd(op)
      If pc + 1 + n > seqLen Then wErr = "operands run past the end" : Exit Sub
      Select Case op
        Case OP_GOTO
          target = seqb(pc+1) Or (seqb(pc+2) << 8)
          If target >= seqLen Then wErr = "goto target out of range" : Exit Sub
          pc = target
        Case OP_IFWTLESS
          target = seqb(pc+1) Or (seqb(pc+2) << 8)
          If target >= seqLen Then wErr = "ifwtless target out of range" : Exit Sub
          pc = pc + 3                            ' the not-taken path
        Case OP_DIE, OP_NEXTLEVEL
          Exit Sub
        Case Else
          pc = pc + 1 + n
      End Select
    End If
  Loop
End Sub

'-----------------------------------------------------------------------------
Sub SetOperands
  Local INTEGER i
  For i = 0 To 255 : opnd(i) = 0 : Next i
  opnd(&HFF) = 2      ' goto        target
  opnd(&HFE) = 0      ' aboutface
  opnd(&HFD) = 0      ' up
  opnd(&HFC) = 0      ' down
  opnd(&HFB) = 1      ' chx         dx
  opnd(&HFA) = 1      ' chy         dy
  opnd(&HF9) = 1      ' act         action class
  opnd(&HF8) = 2      ' setfall     dx, dy
  opnd(&HF7) = 2      ' ifwtless    target
  opnd(&HF6) = 0      ' die
  opnd(&HF5) = 0      ' jaru: a flag; the byte after it is a frame
  opnd(&HF4) = 0      ' jard
  opnd(&HF3) = 1      ' effect      number
  opnd(&HF2) = 1      ' tap         number
  opnd(&HF1) = 0      ' nextlevel
End Sub

'-----------------------------------------------------------------------------
Sub CheckFrames
  Local INTEGER i, img, distinct, seen(255)
  For i = 0 To 255 : seen(i) = 0 : Next i
  distinct = 0
  For i = 0 To frmCount - 1
    img = frmb(i * frmEntry)
    If seen(img) = 0 Then seen(img) = 1 : distinct = distinct + 1
  Next i
  Print "frames "; Str$(frmCount); " entries of "; Str$(frmEntry); " bytes, ";
  Print Str$(distinct); " distinct images"
End Sub

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
          Case "seq_len"        : seqLen = v
          Case "seq_count"      : seqCount = v
          Case "seq_first"      : seqFirst = v
          Case "frames_len"     : frmLen = v
          Case "frames_count"   : frmCount = v
          Case "frames_entry"   : frmEntry = v
          Case "frames_altset1" : frmAlt1 = v
          Case "frames_altset2" : frmAlt2 = v
          Case "frames_swordtab": frmSword = v
        End Select
      End If
    End If
  Loop
  Close #2
End Sub
