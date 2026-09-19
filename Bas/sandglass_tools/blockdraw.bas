' blockdraw.bas - Phase 3.  Compose a screen from a level blueprint.
'
' A block is not one picture.  It is a back section, a middle section, a
' front-of-middle section, a floor section and a foreground piece that draws
' over the character, each with its own image number and vertical offset.  This
' walks the thirty blocks of one screen, resolves every section through the real
' piece tables and the real screen geometry, and draws them.
'
' What this measures is the per-block loop, which Phase 0 showed is where the
' frame budget actually goes: more of the cost is BASIC issuing the calls than
' the drawing itself.
'
' Needs blocks.dat, levels.dat, art.bin and tables.idx from the converter.
' Artwork is read from whatever sheet is in the image slot; for a timing run
' that can be the pattern sheet selftest.py makes, since cost depends on the
' sizes and the count, not on what the pixels are.

Option Console Serial
Option Explicit
Option Default None

Const SLOT = 4
Const ROWS = 3
Const COLS = 10
Const BLOCKW = 28              ' 4 bytes of 7 pixels
Const SCRW = 280
Const SCRH = 192
Const ORIGINX = 20             ' 280 wide centred in 320
Const ORIGINY = 24             ' 192 tall centred in 240
Const LEVELBYTES = 2304
Const FRAMES = 25

' Which table a piece number names: under $80 it is the first background
' table, at or above it is the second, less $80.  Verified against every
' piece table: no value falls outside its table's image count.
Const BGSPLIT = &H80

' Table numbers are read from tables.idx by name, never assumed: which tables
' are present depends on the converter's --set, so the numbering shifts.

' Every section of every block type, resolved once at load time.  Phase 0 showed
' the interpreter issuing the work costs more than the drawing, so the draw loop
' must not walk a lookup chain per section: it reads a flat row and blits.
' A block type has at most one piece per section, and the sections come from
' different blocks: the C section belongs to the block below and to the left,
' the B section to the block to the left, and D and A to the block itself.  So
' the tables are per section, indexed by block type, and the draw loop picks the
' table and the type separately.  Drawing order is C, B, D, A, then the
' foreground piece, which goes in a later pass because it covers the character.
'
' One dimension throughout: a two-dimensional read costs an index multiply and a
' second bounds check, and there are eight per blit.
Const S_C = 0
Const S_B = 1
Const S_D = 2
Const S_A = 3
Const S_F = 4
Const NSECT = 5
Const SECMAX = 5 * 32
' The screen's block types, padded with an empty column on the left and an empty
' row below, so a neighbour off the edge reads as empty with no test at all.
' Real block (r, c) lives at TP(r * TPW + c + 1).
Const TPW = 12
Dim INTEGER tp(4 * TPW)

Dim INTEGER secOn(SECMAX)                 ' is there a piece at all
Dim INTEGER secSheet(SECMAX), secSX(SECMAX), secSY(SECMAX)
Dim INTEGER secW(SECMAX), secH(SECMAX)
Dim INTEGER secDX(SECMAX), secDY(SECMAX)

Dim INTEGER packed(5000)
Dim INTEGER blocks(512), level(LEVELBYTES), art(12000)
Dim INTEGER artFirst(20), artCount(20), artFacings(20), artBase
Dim INTEGER pMaskA, pPieceA, pPieceAY, pMaskB, pPieceB, pPieceBY
Dim INTEGER pStripe, pPieceC, pPieceD, pFrontI, pFrontY, pFrontX
Dim INTEGER gBlockBot, gBlockTop, gFloorY, gBlockAy, gBlockEdge
Dim INTEGER blockTypes, nDrawn, nSkipped, sheetW, sheetH, artLen
Dim INTEGER T_BG1, T_BG2, realArt, nSheets
Dim INTEGER i, r, c, scr, lvl
Dim STRING home
Dim FLOAT t0, tDraw

home = MM.Info(Path)
If home = "NONE" Then home = "A:/"

Print "--- block renderer"
ReadLayout
LoadBytes home + "blocks.dat", 400, blocks()
LoadArt
LoadLevel 1                                   ' level 1

Mode 2
FrameBuffer Create
CLS

' Real artwork if it is there: sheet n goes into image slot SLOT+n-1.  Otherwise
' the pattern sheet, which is enough to time the loop, since cost follows the
' sizes and the count rather than the pixels.
LoadSheets
nSkipped = 0
BuildSections
Print "  sections resolved for "; Str$(blockTypes); " block types, "; Str$(nSkipped); " empty"

' ---- draw one screen, timed -------------------------------------------------
scr = 1
t0 = Timer
BuildTypeGrid scr
For i = 1 To FRAMES
  nDrawn = 0
  DrawScreen scr
  ' characters would be drawn here
  DrawFront scr
  FrameBuffer Copy F, N
Next i
tDraw = (Timer - t0) / FRAMES

Print
Print "screen "; Str$(scr); " of level 1"
Print "  sections drawn   "; Str$(nDrawn)
Print "  per frame        "; Str$(tDraw, 0, 3); " ms of the 83 ms budget"
If nDrawn > 0 Then
  Print "  per section      "; Str$(tDraw * 1000 / nDrawn, 0, 1); " us"
End If

' ---- how heavy is the worst screen in the game? -----------------------------
Dim INTEGER worst, worstScr, tot
worst = 0 : worstScr = 0 : tot = 0
For lvl = 0 To 14
  LoadLevel lvl
  For scr = 0 To 23
    nDrawn = 0 : nSkipped = 0
    CountScreen scr
    tot = tot + nDrawn
    If nDrawn > worst Then worst = nDrawn : worstScr = scr
  Next scr
Next lvl
Print
Print "across all 15 levels: "; Str$(tot); " sections, busiest screen "; Str$(worst)
Print "busiest screen would cost about "; Str$(tDraw * worst / 46, 0, 1); " ms"

FrameBuffer Close
Option Console Both
End

'-----------------------------------------------------------------------------
' The draw loop does no resolution at all: every section was worked out once by
' BuildSections, so this reads a row and blits.  Phase 0 measured the drawing at
' about 40 us a blit and the first version of this loop at 2.3 ms a section, so
' the lookup chain was over fifty times the cost of the drawing it fed.
' Build the padded type grid for one screen.  Thirty reads once, instead of the
' masking and edge tests being repeated inside the draw loop, where they cost
' more than the drawing.
Sub BuildTypeGrid(scr As INTEGER)
  Local INTEGER r, c, t, base, row
  For r = 0 To 3
    For c = 0 To TPW - 1
      tp(r * TPW + c) = 0
    Next c
  Next r
  For r = 0 To ROWS - 1
    base = scr * 30 + r * COLS
    row = r * TPW
    For c = 0 To COLS - 1
      t = level(base + c) And &H1F
      If t >= blockTypes Then t = 0
      tp(row + c + 1) = t
    Next c
  Next r
End Sub

'-----------------------------------------------------------------------------
' Background pass: C, B, D, A.  The foreground piece is a separate pass because
' it draws over the character.
'
' Which block each section comes from is the part that is easy to get wrong: D
' and A belong to the block itself, B to the block on its left, and C to the
' block below and to the left.  Reading them all from the block itself is what
' the first version did, and it is wrong.
'
' Off the edges, neighbours are treated as empty.  The original reaches into the
' screen to the left and the screen below for them, which is screen linking and
' is not done yet.
Sub DrawScreen(scr As INTEGER)
  Local INTEGER r, c, dy, x, yb, k, row, i
  For r = 0 To ROWS - 1
    ' BlockBot holds unsigned screen positions (2, 65, 128, 191, 254), not
    ' signed offsets.  Only the per-piece offsets are signed.
    dy = blocks(gBlockBot + r + 1)
    yb = ORIGINY + dy
    row = r * TPW
    For c = 0 To COLS - 1
      x = ORIGINX + c * BLOCKW
      i = row + c
      k = tp(i + TPW)             ' C section, from below and to the left
      If secOn(k) Then Blit Flash secSheet(k),F,secSX(k),secSY(k),x+secDX(k),yb+secDY(k),secW(k),secH(k),0 : nDrawn=nDrawn+1
      k = 32 + tp(i)              ' B section, from the block on the left
      If secOn(k) Then Blit Flash secSheet(k),F,secSX(k),secSY(k),x+secDX(k),yb+secDY(k),secW(k),secH(k),0 : nDrawn=nDrawn+1
      k = 64 + tp(i + 1)          ' D section, this block
      If secOn(k) Then Blit Flash secSheet(k),F,secSX(k),secSY(k),x+secDX(k),yb+secDY(k),secW(k),secH(k),0 : nDrawn=nDrawn+1
      k = 96 + tp(i + 1)          ' A section, this block
      If secOn(k) Then Blit Flash secSheet(k),F,secSX(k),secSY(k),x+secDX(k),yb+secDY(k),secW(k),secH(k),0 : nDrawn=nDrawn+1
    Next c
  Next r
End Sub

'-----------------------------------------------------------------------------
' Foreground pass.  Runs after the characters would be drawn, which is what
' makes a character walk behind a pillar.
Sub DrawFront(scr As INTEGER)
  Local INTEGER r, c, dy, x, yb, k, row
  For r = 0 To ROWS - 1
    dy = blocks(gBlockBot + r + 1)
    yb = ORIGINY + dy
    row = r * TPW
    For c = 0 To COLS - 1
      k = 128 + tp(row + c + 1)
      If secOn(k) Then
        x = ORIGINX + c * BLOCKW
        Blit Flash secSheet(k),F,secSX(k),secSY(k),x+secDX(k),yb+secDY(k),secW(k),secH(k),0
        nDrawn = nDrawn + 1
      End If
    Next c
  Next r
End Sub

'-----------------------------------------------------------------------------
' Count the sections a screen would draw, without drawing them.
Sub CountScreen(scr As INTEGER)
  Local INTEGER r, c, t, k
  For r = 0 To ROWS - 1
    For c = 0 To COLS - 1
      t = level(scr * 30 + r * COLS + c) And &H1F
      If t >= blockTypes Then t = 0
      If blocks(pPieceA + t) <> 0 Then nDrawn = nDrawn + 1
      If blocks(pPieceB + t) <> 0 Then nDrawn = nDrawn + 1
      If blocks(pPieceC + t) <> 0 Then nDrawn = nDrawn + 1
      If blocks(pPieceD + t) <> 0 Then nDrawn = nDrawn + 1
      If blocks(pFrontI + t) <> 0 Then nDrawn = nDrawn + 1
    Next c
  Next r
End Sub

'-----------------------------------------------------------------------------
' Resolve every section of every block type, once.
'
' A block's five sections are a back, a middle, a front-of-middle, a floor and a
' foreground piece.  The back and middle sit relative to the block bottom less
' three; the floor pair sit on the block bottom; the foreground piece follows
' the back section and carries its own x and y offsets.  Everything is
' normalised here to one base, the block bottom, and to the top-left corner a
' blit wants rather than the lower-left the original works in.
Sub BuildSections
  Local INTEGER t
  For t = 0 To blockTypes - 1
    AddSection S_C, t, blocks(pPieceC + t), 0, 0
    AddSection S_B, t, blocks(pPieceB + t), 0, -3 + Sgn8(blocks(pPieceBY + t))
    AddSection S_D, t, blocks(pPieceD + t), 0, 0
    AddSection S_A, t, blocks(pPieceA + t), 0, -3 + Sgn8(blocks(pPieceAY + t))
    AddSection S_F, t, blocks(pFrontI + t), Sgn8(blocks(pFrontX + t)), -3 + Sgn8(blocks(pFrontY + t))
  Next t
End Sub

'-----------------------------------------------------------------------------
' One section.  A piece number of zero means there is nothing there.
Sub AddSection(kind As INTEGER, t As INTEGER, num As INTEGER, xoff As INTEGER, yoff As INTEGER)
  Local INTEGER tab, img, rec, sx, sy, w, h, k
  k = kind * 32 + t
  secOn(k) = 0
  If num = 0 Then nSkipped = nSkipped + 1 : Exit Sub
  If num < BGSPLIT Then
    tab = T_BG1 : img = num
  Else
    tab = T_BG2 : img = num - BGSPLIT
  End If
  If img < 1 Or img > artCount(tab) Then
    Print "  block "; Str$(t); " names image "; Str$(img); " of table "; Str$(tab); " - out of range"
    nSkipped = nSkipped + 1 : Exit Sub
  End If
  rec = (artFirst(tab) + (img - 1) * artFacings(tab)) * 9 + artBase
  sx = art(rec+1) Or (art(rec+2) << 8)
  sy = art(rec+3) Or (art(rec+4) << 8)
  w  = art(rec+5) Or (art(rec+6) << 8)
  h  = art(rec+7) Or (art(rec+8) << 8)
  If w < 1 Or h < 1 Then nSkipped = nSkipped + 1 : Exit Sub
  ' Clamp the source into whatever sheet is actually loaded, so a timing run
  ' against the pattern sheet still blits representative sizes.
  If Not realArt Then
    If sx + w > sheetW Then sx = sheetW - w
    If sy + h > sheetH Then sy = sheetH - h
    If sx < 0 Or sy < 0 Then nSkipped = nSkipped + 1 : Exit Sub
  End If
  secOn(k) = 1
  If realArt Then secSheet(k) = SLOT + art(rec) - 1 Else secSheet(k) = SLOT
  secSX(k) = sx : secSY(k) = sy
  secW(k) = w   : secH(k) = h
  secDX(k) = xoff
  ' lower-left to top-left, folded in now so the draw loop does no arithmetic
  secDY(k) = yoff - h + 1
End Sub

'-----------------------------------------------------------------------------
Function Sgn8(v As INTEGER) As INTEGER
  If v > 127 Then Sgn8 = v - 256 Else Sgn8 = v
End Function

'-----------------------------------------------------------------------------
' Load whatever artwork is present.  The converted sheets if they are there,
' otherwise the generated pattern sheet, which is enough for a timing run.
Sub LoadSheets
  Local INTEGER n
  Local STRING f
  realArt = 0 : nSheets = 0
  For n = 1 To 4
    f = home + "sheet" + Str$(n) + ".bmp"
    If Dir$(f, File) <> "" Then
      Flash Load Image SLOT + n - 1, f, O
      nSheets = nSheets + 1
    End If
  Next n
  If nSheets > 0 Then
    realArt = 1
    Print "  artwork  "; Str$(nSheets); " converted sheet(s), slots "; Str$(SLOT); "-"; Str$(SLOT+nSheets-1)
  Else
    Flash Load Image SLOT, home + "testshet.bmp", O
    sheetW = 512 : sheetH = 256
    Print "  artwork  pattern sheet only - timing is valid, the picture is not"
  End If
End Sub

'-----------------------------------------------------------------------------
Sub LoadArt
  Local INTEGER n, tables, i, off
  n = artLen
  Open home + "art.bin" For Input As #1
  Memory Input #1, n, packed()
  Close #1
  Memory Unpack packed(), art(), n, 8
  tables = art(0) Or (art(1) << 8)
  artBase = 2 + tables * 7
  For i = 0 To tables - 1
    off = 2 + i * 7
    artCount(i + 1) = art(off) Or (art(off+1) << 8)
    artFacings(i + 1) = art(off+2)
    artFirst(i + 1) = art(off+3) Or (art(off+4) << 8) Or (art(off+5) << 16)
  Next i
  Print "  art.bin  "; Str$(tables); " tables, bg1 "; Str$(artCount(T_BG1));
  Print " images, bg2 "; Str$(artCount(T_BG2))
End Sub

'-----------------------------------------------------------------------------
Sub LoadLevel(n As INTEGER)
  Open home + "levels.dat" For Input As #1
  Seek #1, n * LEVELBYTES + 1
  Memory Input #1, LEVELBYTES, packed()
  Close #1
  Memory Unpack packed(), level(), LEVELBYTES, 8
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
          Case "block_maska"   : pMaskA = v
          Case "block_piecea"  : pPieceA = v
          Case "block_pieceay" : pPieceAY = v
          Case "block_maskb"   : pMaskB = v
          Case "block_pieceb"  : pPieceB = v
          Case "block_pieceby" : pPieceBY = v
          Case "block_bstripe" : pStripe = v
          Case "block_piecec"  : pPieceC = v
          Case "block_pieced"  : pPieceD = v
          Case "block_fronti"  : pFrontI = v
          Case "block_fronty"  : pFrontY = v
          Case "block_frontx"  : pFrontX = v
          Case "block_types"   : blockTypes = v
          Case "geom_BlockBot" : gBlockBot = v
          Case "geom_BlockTop" : gBlockTop = v
          Case "geom_FloorY"   : gFloorY = v
          Case "geom_BlockAy"  : gBlockAy = v
          Case "geom_BlockEdge": gBlockEdge = v
          Case "art_len"       : artLen = v
          Case "tab_BGTAB1DUN" : T_BG1 = v
          Case "tab_BGTAB2DUN" : T_BG2 = v
        End Select
      End If
    End If
  Loop
  Close #2
End Sub
