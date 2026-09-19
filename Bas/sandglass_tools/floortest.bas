' floortest.bas - Phase 4, third piece: the floor test.
'
' Every frame the engine asks whether the character is standing on something,
' falling towards it, or passing through it.  This is that decision, run over
' every cell of every screen of a real level at a spread of heights, and
' checksummed so it can be compared with the host reference exactly.
'
' Two things read from the original rather than assumed:
'
'   The block under his feet is his own cell, because the row index names the
'   floor he stands on, not the space he occupies.
'
'   A solid block counts as "no floor".  That looks wrong until you see it is
'   handled separately: you cannot stand on a solid block, you are pushed out
'   to one side of it.
'
' Y grows downwards, so reaching the floor plane means Y has caught up with the
' floor line of the row below.
'
' Needs levels.dat, blocks.dat and tables.idx from the converter.

Option Console Serial
Option Explicit
Option Default None

Const LEVELBYTES = 2304
Const ROWS = 3
Const COLS = 10
Const SCREENS = 24

' Action classes, from the original's floor test.
Const ACT_FALLING = 4
Const ACT_BUMPED = 5
Const ACT_THREE = 3

' What happened, as a small number so it can go in the checksum.
Const R_HANG = 0
Const R_BUMPED = 1
Const R_OTHER = 2
Const R_FALLON = 3
Const R_LAND = 4
Const R_THROUGH = 5
Const R_INBLOCK = 6
Const R_GROUND = 7
Const R_OFFMAP = 8

Const EXP_FALLON = 1440
Const EXP_INBLOCK = 414
Const EXP_LAND = 210
Const EXP_THROUGH = 1536
Const EXP_SUM = 3324012

Dim INTEGER packed(5000), level(LEVELBYTES), blocks(512)
Dim INTEGER floory(5), noFloor(31)
Dim INTEGER gFloorY, blockTypes
Dim INTEGER cPosn, cX, cY, cFace, cBlockX, cBlockY, cAction, cScrn
Dim INTEGER ck, i, scrn, by, bx, d, what
Dim INTEGER nHang, nBumped, nOther, nFallon, nLand, nThrough, nInblock, nGround, nOffmap
Dim INTEGER dys(4)
Dim STRING home
Dim FLOAT t0, tRun

home = MM.Info(Path)
If home = "NONE" Then home = "A:/"

Print "--- floor test"
ReadLayout
LoadBytes home + "blocks.dat", 400, blocks()
LoadLevel 0

For i = 0 To 4
  floory(i) = blocks(gFloorY + i)
Next i
Print "  floor lines "; Str$(floory(0)); " "; Str$(floory(1)); " "; Str$(floory(2)); " "; Str$(floory(3)); " "; Str$(floory(4))

' Block types you cannot stand on.  A solid block is here because standing on
' it is not how it is handled.
For i = 0 To 31 : noFloor(i) = 0 : Next i
noFloor(0) = 1 : noFloor(9) = 1 : noFloor(12) = 1 : noFloor(20) = 1
noFloor(26) = 1 : noFloor(27) = 1 : noFloor(28) = 1 : noFloor(29) = 1

dys(0) = -20 : dys(1) = -1 : dys(2) = 0 : dys(3) = 1 : dys(4) = 40

ck = 0
nHang=0 : nBumped=0 : nOther=0 : nFallon=0 : nLand=0
nThrough=0 : nInblock=0 : nGround=0 : nOffmap=0

t0 = Timer
For scrn = 0 To SCREENS - 1
  For by = 0 To ROWS - 1
    For bx = 0 To COLS - 1
      For d = 0 To 4
        cPosn = 0 : cFace = &HFF
        cScrn = scrn : cBlockX = bx : cBlockY = by
        cAction = ACT_FALLING
        cY = (floory(by + 1) + dys(d)) And &HFF
        what = CheckFloor()
        Tally what
        Mix cY : Mix cBlockY : Mix what
      Next d
    Next bx
  Next by
Next scrn
tRun = Timer - t0

Print
Print "  fallon  "; Str$(nFallon); "  expected "; Str$(EXP_FALLON)
Print "  inblock "; Str$(nInblock); "  expected "; Str$(EXP_INBLOCK)
Print "  land    "; Str$(nLand); "  expected "; Str$(EXP_LAND)
Print "  through "; Str$(nThrough); "  expected "; Str$(EXP_THROUGH)
Print "  checksum "; Str$(ck); "  expected "; Str$(EXP_SUM)
Print "  time     "; Str$(tRun, 0, 1); " ms for 3600 tests"

Print
If nFallon = EXP_FALLON And nInblock = EXP_INBLOCK And nLand = EXP_LAND And nThrough = EXP_THROUGH And ck = EXP_SUM Then
  Print "PASS - the floor test agrees with the host reference exactly"
Else
  Print "FAIL - the engine and the reference disagree"
End If

Option Console Both
End

'-----------------------------------------------------------------------------
' Standing, falling, or going through?  Y grows downwards.
Function CheckFloor() As INTEGER
  Local INTEGER idx, t
  If cAction = 2 Or cAction = 6 Then CheckFloor = R_HANG : Exit Function
  If cAction = ACT_BUMPED Then
    If cPosn = 109 Or cPosn = 185 Then CheckFloor = R_GROUND Else CheckFloor = R_BUMPED
    Exit Function
  End If
  If cAction = ACT_THREE Then
    If cPosn >= 102 And cPosn <= 105 Then CheckFloor = R_FALLON Else CheckFloor = R_OTHER
    Exit Function
  End If
  If cAction = ACT_FALLING Then
    idx = cBlockY + 1
    If idx < 0 Or idx > 4 Then CheckFloor = R_OFFMAP : Exit Function
    If cY < floory(idx) Then CheckFloor = R_FALLON : Exit Function
    t = BlockAt(cScrn, cBlockX, cBlockY)
    If t = 20 Then CheckFloor = R_INBLOCK : Exit Function
    If noFloor(t) = 0 Then
      cY = floory(idx)
      CheckFloor = R_LAND
      Exit Function
    End If
    cBlockY = (cBlockY + 1) And &HFF
    CheckFloor = R_THROUGH
    Exit Function
  End If
  CheckFloor = R_GROUND
End Function

'-----------------------------------------------------------------------------
' Off the edge reads as empty.  Reaching into the next screen is screen linking
' and is not done yet, here or in the renderer.
Function BlockAt(scrn As INTEGER, bx As INTEGER, by As INTEGER) As INTEGER
  If bx < 0 Or bx >= COLS Or by < 0 Or by >= ROWS Then BlockAt = 0 : Exit Function
  BlockAt = level(scrn * 30 + by * COLS + bx) And &H1F
End Function

'-----------------------------------------------------------------------------
Sub Tally(w As INTEGER)
  Select Case w
    Case R_HANG    : nHang = nHang + 1
    Case R_BUMPED  : nBumped = nBumped + 1
    Case R_OTHER   : nOther = nOther + 1
    Case R_FALLON  : nFallon = nFallon + 1
    Case R_LAND    : nLand = nLand + 1
    Case R_THROUGH : nThrough = nThrough + 1
    Case R_INBLOCK : nInblock = nInblock + 1
    Case R_GROUND  : nGround = nGround + 1
    Case R_OFFMAP  : nOffmap = nOffmap + 1
  End Select
End Sub

Sub Mix(v As INTEGER)
  ck = (ck * 31 + v) And &HFFFFFF
End Sub

'-----------------------------------------------------------------------------
Sub LoadLevel(n As INTEGER)
  Open home + "levels.dat" For Input As #1
  Seek #1, n * LEVELBYTES + 1
  Memory Input #1, LEVELBYTES, packed()
  Close #1
  Memory Unpack packed(), level(), LEVELBYTES, 8
End Sub

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
          Case "geom_FloorY" : gFloorY = v
          Case "block_types" : blockTypes = v
        End Select
      End If
    End If
  Loop
  Close #2
End Sub
