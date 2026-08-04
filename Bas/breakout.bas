'=============================================
' BREAKOUT - A Tilemap-Based Brick Breaker
'
' Uses TILEMAP for the brick field and
' TILEMAP sprites for the ball and paddle.
' Procedurally generates tileset BMP.
'
' Controls: Left/Right arrows to move paddle
'           Space to launch ball
'           Q to quit
'
' Requires: MODE 2 (320x240 RGB121), SD card
'=============================================
Option EXPLICIT
Option BASE 0
SetPin gp8,din,pullup
SetPin gp9,din,pullup
SetPin gp10,din,pullup
SetPin gp11,din,pullup
SetPin gp12,din,pullup
SetPin gp13,din,pullup
SetPin gp14,din,pullup
SetPin gp15,din,pullup


' ---- Display constants ----
Const SCR_W = 320
Const SCR_H = 240

' ---- Tile dimensions ----
Const TW = 16           ' tile width
Const TH = 8            ' tile height (bricks are wide and short)
Const TPR = 8           ' tiles per row in tileset image

' ---- Map dimensions ----
Const COLS = 20          ' 20 cols x 16 = 320 pixels = screen width
Const ROWS = 30          ' 30 rows x 8 = 240 pixels = screen height

' ---- Tile indices ----
Const T_EMPTY  = 0
Const T_RED    = 1       ' 7 points
Const T_YELLOW = 2       ' 5 points
Const T_GREEN  = 3       ' 3 points
Const T_BLUE   = 4       ' 1 point
Const T_WALL   = 5       ' indestructible border
Const T_BALL   = 6       ' ball sprite tile
Const T_PADDLE = 7       ' paddle segment tile
Const T_PADL   = 8       ' paddle left end

' ---- Attribute bits ----
Const A_BRICK = &b0001   ' breakable brick
Const A_WALL  = &b0010   ' solid wall (unbreakable)
Const A_SOLID = &b0011   ' anything solid

' ---- RGB121 colours ----
Const C_BLACK  = RGB(0,0,0)
Const C_RED    = RGB(255,0,0)
Const C_YELLOW = RGB(255,255,0)
Const C_GREEN  = RGB(0,255,0)
Const C_BLUE   = RGB(0,0,255)
Const C_WHITE  = RGB(255,255,255)
Const C_GREY   = RGB(128,128,128)
Const C_COBALT = RGB(0,0,255)
Const C_CYAN   = RGB(0,255,255)
Const C_BROWN  = RGB(255,255,0)

' ---- Game state ----
Dim score, lives, level, bricks_left
Dim ball_x!, ball_y!     ' ball position (sub-pixel float)
Dim ball_dx!, ball_dy!   ' ball velocity
Dim pad_x                ' paddle left edge (pixel)
Dim pad_w                ' paddle width in pixels
Dim launched             ' has ball been launched?
Dim k$
Dim r, c, ps
Dim new_x!, new_y!
Dim bx, by, hit_t, tcol, trow, hit_a
Dim prev_bx, prev_by, ptcol, ptrow
Dim wprev_bx, wprev_by
Dim edge_t
Dim hit_pos!, pad_centre!

' ---- Speed/difficulty ----
Const BALL_SPEED! = 4.0
Const PAD_SPEED = 10
Const PAD_W_TILES = 5    ' paddle width in tiles
Const PAD_ROW = 28       ' row where paddle sits
Const BRICK_START_ROW = 4 ' first row of bricks
Const BRICK_ROWS = 8     ' rows of bricks

' ============================================
' Setup
' ============================================
Print "Generating tileset..."
GenerateTileset
Flash LOAD IMAGE 1, "breakout_tiles.bmp", O
FRAMEBUFFER CREATE

' ============================================
' Title Screen
' ============================================
TitleScreen:
FRAMEBUFFER WRITE F
CLS C_BLACK
Text SCR_W\2, 60, "BREAKOUT", "CM", 7, 2, C_RED
Text SCR_W\2, 110, "Left and Right Buttons to move", "CM", 1, 1, C_WHITE
Text SCR_W\2, 130, "A to launch ball", "CM", 1, 1, C_WHITE
Text SCR_W\2, 150, "B to quit", "CM", 1, 1, C_GREY
Text SCR_W\2, 190, "Press SELECT to start", "CM", 1, 1, C_YELLOW
FRAMEBUFFER COPY F, N
Do : k$ = GetPress() : Loop Until k$ = " " Or UCase$(k$) = "Q"
If UCase$(k$) = "Q" Then GoTo Cleanup

' ============================================
' New Game
' ============================================
score = 0
lives = 3
level = 1

NewLevel:
' Build the map
Tilemap CLOSE
Tilemap CREATE mapdata, 1, 1, TW, TH, TPR, COLS, ROWS
Tilemap ATTR tileattrs, 1, 8
BALL_SPEED=BALL_SPEED*1.25
' Count bricks
bricks_left = 0
For r = BRICK_START_ROW To BRICK_START_ROW + BRICK_ROWS - 1
 For c = 1 To COLS - 2
   If Tilemap(TILE 1, c * TW + 1, r * TH + 1) > 0 Then
     If (Tilemap(ATTR 1, Tilemap(TILE 1, c * TW + 1, r * TH + 1)) And A_BRICK)
Then
       bricks_left = bricks_left + 1
     End If
   End If
 Next c
Next r

' Create sprites: ball and paddle segments
Tilemap SPRITE CREATE 1, 1, T_BALL, SCR_W\2, (PAD_ROW - 1) * TH

' Paddle sprites (5 segments)
pad_x = (SCR_W - PAD_W_TILES * TW) \ 2
For ps = 1 To PAD_W_TILES
 Tilemap SPRITE CREATE ps + 1, 1, T_PADDLE, pad_x + (ps - 1) * TW, PAD_ROW * TH
Next ps

' Reset ball
ResetBall:
launched = 0
ball_dx! = BALL_SPEED!
ball_dy! = -BALL_SPEED!
ball_x! = pad_x + (PAD_W_TILES * TW) \ 2 - TW \ 2
ball_y! = (PAD_ROW - 1) * TH

' ============================================
' Main Game Loop
' ============================================
GameLoop:
Do
 FRAMEBUFFER WRITE F
 CLS C_BLACK

 ' ---- Input ----
 k$ = GetPress()
 If k$ = Chr$(130) Then         ' Left arrow
   pad_x = pad_x - PAD_SPEED
   If pad_x < TW Then pad_x = TW
 End If
 If k$ = Chr$(131) Then         ' Right arrow
   pad_x = pad_x + PAD_SPEED
   If pad_x > SCR_W - PAD_W_TILES * TW - TW Then pad_x = SCR_W - PAD_W_TILES *
TW - TW
 End If
 If k$ = "L" And launched = 0 Then launched = 1
 If UCase$(k$) = "Q" Then GoTo Cleanup

 ' ---- Update paddle sprites ----
 For ps = 1 To PAD_W_TILES
   Tilemap SPRITE MOVE ps + 1, pad_x + (ps - 1) * TW, PAD_ROW * TH
 Next ps

 ' ---- Ball logic ----
 If launched = 0 Then
   ' Ball sits on paddle
   ball_x! = pad_x + (PAD_W_TILES * TW) \ 2 - TW \ 2
   ball_y! = (PAD_ROW - 1) * TH
 Else
   ' Move ball
   new_x! = ball_x! + ball_dx!
   new_y! = ball_y! + ball_dy!

   ' ---- Wall collisions ----
   ' Left wall
   If new_x! < TW Then
     new_x! = TW
     ball_dx! = -ball_dx!
   End If
   ' Right wall
   If new_x! > SCR_W - 2 * TW Then
     new_x! = SCR_W - 2 * TW
     ball_dx! = -ball_dx!
   End If
   ' Top wall
   If new_y! < TH Then
     new_y! = TH
     ball_dy! = -ball_dy!
   End If

   ' ---- Brick collision ----
   ' Check ball centre against tilemap
   bx = Int(new_x!) + TW \ 2
   by = Int(new_y!) + TH \ 2
   hit_t = Tilemap(TILE 1, bx, by)
   If hit_t > 0 Then
     hit_a = Tilemap(ATTR 1, hit_t)
     If (hit_a And A_BRICK) Then
       ' Score based on brick colour
       Select Case hit_t
         Case T_RED    : score = score + 7
         Case T_YELLOW : score = score + 5
         Case T_GREEN  : score = score + 3
         Case T_BLUE   : score = score + 1
       End Select
       ' Remove brick
       tcol = bx \ TW
       trow = by \ TH
       Tilemap SET 1, tcol, trow, T_EMPTY
       bricks_left = bricks_left - 1

       ' Bounce: determine which face was hit
       prev_bx = Int(ball_x!) + TW \ 2
       prev_by = Int(ball_y!) + TH \ 2
       ptcol = prev_bx \ TW
       ptrow = prev_by \ TH
       If ptcol <> tcol Then ball_dx! = -ball_dx!
       If ptrow <> trow Then ball_dy! = -ball_dy!
       If ptcol = tcol And ptrow = trow Then
         ball_dy! = -ball_dy!
       End If
     ElseIf (hit_a And A_WALL) Then
       ' Bounce off wall
       wprev_bx = Int(ball_x!) + TW \ 2
       wprev_by = Int(ball_y!) + TH \ 2
       If (wprev_bx \ TW) <> (bx \ TW) Then ball_dx! = -ball_dx!
       If (wprev_by \ TH) <> (by \ TH) Then ball_dy! = -ball_dy!
       If (wprev_bx \ TW) = (bx \ TW) And (wprev_by \ TH) = (by \ TH) Then
         ball_dy! = -ball_dy!
       End If
     End If
   End If

   ' Also check ball edges for bricks (corners)
   ' Top edge
   edge_t = Tilemap(TILE 1, bx, Int(new_y!))
   If edge_t > 0 Then
     If (Tilemap(ATTR 1, edge_t) And A_BRICK) Then
       tcol = bx \ TW
       trow = Int(new_y!) \ TH
       Select Case edge_t
         Case T_RED    : score = score + 7
         Case T_YELLOW : score = score + 5
         Case T_GREEN  : score = score + 3
         Case T_BLUE   : score = score + 1
       End Select
       Tilemap SET 1, tcol, trow, T_EMPTY
       bricks_left = bricks_left - 1
       ball_dy! = -ball_dy!
     End If
   End If

   ' ---- Paddle collision ----
   If ball_dy! > 0 Then  ' only when moving down
     If Int(new_y!) + TH >= PAD_ROW * TH And Int(new_y!) + TH <= PAD_ROW * TH +
TH Then
       If Int(new_x!) + TW > pad_x And Int(new_x!) < pad_x + PAD_W_TILES * TW T
hen
         new_y! = PAD_ROW * TH - TH
         ball_dy! = -Abs(ball_dy!)

         ' Angle based on where ball hits paddle
         hit_pos! = (new_x! + TW \ 2 - pad_x) / (PAD_W_TILES * TW)
         ' hit_pos ranges 0..1, map to angle
         ball_dx! = (hit_pos! - 0.5) * BALL_SPEED! * 2
         ' Clamp horizontal speed
         If Abs(ball_dx!) > BALL_SPEED! * 0.9 Then
           ball_dx! = Sgn(ball_dx!) * BALL_SPEED! * 0.9
         End If
         ' Ensure minimum horizontal movement
         If Abs(ball_dx!) < 0.3 Then
           ball_dx! = Sgn(ball_dx!) * 0.3
           If ball_dx! = 0 Then ball_dx! = 0.3
         End If
         ' Maintain total speed
         ball_dy! = -Sqr(BALL_SPEED! * BALL_SPEED! - ball_dx! * ball_dx!)
       End If
     End If
   End If

   ' ---- Ball lost (bottom) ----
   If new_y! > SCR_H Then
     lives = lives - 1
     If lives <= 0 Then GoTo GameOver
     GoTo ResetBall
   End If

   ball_x! = new_x!
   ball_y! = new_y!
 End If

 ' ---- Update ball sprite ----
 Tilemap SPRITE MOVE 1, Int(ball_x!), Int(ball_y!)

 ' ---- Draw ----
 Tilemap DRAW 1, F, 0, 0, 0, 0, SCR_W, SCR_H
 Tilemap SPRITE DRAW F, 0

 ' HUD: score and lives
 Text 4, 1, "SCORE:" + Str$(score), "LT", 1, 1, C_WHITE
 Text SCR_W - 4, 1, "LIVES:" + Str$(lives), "RT", 1, 1, C_WHITE
 Text SCR_W \ 2, 1, "LVL:" + Str$(level), "CT", 1, 1, C_YELLOW

 FRAMEBUFFER COPY F, N

 ' ---- Level complete? ----
 If bricks_left <= 0 Then
   level = level + 1
   Tilemap SPRITE CLOSE
   GoTo NewLevel
 End If
Loop

' ============================================
' Game Over
' ============================================
GameOver:
FRAMEBUFFER WRITE F
CLS C_BLACK
Text SCR_W\2, 80, "GAME OVER", "CM", 7, 2, C_RED
Text SCR_W\2, 130, "Score: " + Str$(score), "CM", 1, 2, C_WHITE
Text SCR_W\2, 160, "Level: " + Str$(level), "CM", 1, 1, C_YELLOW
Text SCR_W\2, 200, "SPACE=Play Again  Q=Quit", "CM", 1, 1, C_GREY
FRAMEBUFFER COPY F, N
Do : k$ = GetPress() : Loop Until k$ = " " Or UCase$(k$) = "Q"
If k$ = " " Then GoTo TitleScreen

' ============================================
' Cleanup
' ============================================
Cleanup:
Tilemap CLOSE
FRAMEBUFFER CLOSE
CLS
Print "Thanks for playing!"
Print "Final score: "; score
End

' ============================================
' SUBROUTINES
' ============================================

Sub GenerateTileset
 ' Create tileset: 8 tiles per row, 2 rows = 128x16 px
 ' Tile size: 16 x 8 pixels
 Local tx, ty

 CLS C_BLACK

 ' Tile 1: Red brick
 tx = 0 : ty = 0
 Box tx, ty, TW, TH, 0, C_RED, C_RED
 Box tx+1, ty+1, TW-2, TH-2, 1, C_WHITE

 ' Tile 2: Yellow brick
 tx = TW : ty = 0
 Box tx, ty, TW, TH, 0, C_YELLOW, C_YELLOW
 Box tx+1, ty+1, TW-2, TH-2, 1, C_WHITE

 ' Tile 3: Green brick
 tx = TW * 2 : ty = 0
 Box tx, ty, TW, TH, 0, C_GREEN, C_GREEN
 Box tx+1, ty+1, TW-2, TH-2, 1, C_WHITE

 ' Tile 4: Blue brick
 tx = TW * 3 : ty = 0
 Box tx, ty, TW, TH, 0, C_BLUE, C_BLUE
 Box tx+1, ty+1, TW-2, TH-2, 1, C_WHITE

 ' Tile 5: Wall (grey border)
 tx = TW * 4 : ty = 0
 Box tx, ty, TW, TH, 0, C_GREY, C_GREY
 Box tx+1, ty+1, TW-2, TH-2, 1, C_WHITE

 ' Tile 6: Ball (white square on black)
 tx = TW * 5 : ty = 0
 Box tx, ty, TW, TH, 0, C_BLACK, C_BLACK
 Box tx+TW\2-3, ty+TH\2-3, 6, 6, 0, C_WHITE, C_WHITE

 ' Tile 7: Paddle segment (cyan)
 tx = TW * 6 : ty = 0
 Box tx, ty, TW, TH, 0, C_CYAN, C_CYAN
 Box tx+1, ty+1, TW-2, TH-2, 1, C_WHITE

 ' Tile 8: Paddle left (same as paddle for now)
 tx = TW * 7 : ty = 0
 Box tx, ty, TW, TH, 0, C_CYAN, C_CYAN
 Box tx+1, ty+1, TW-2, TH-2, 1, C_WHITE

 Save IMAGE "breakout_tiles.bmp", 0, 0, TPR * TW, TH
End Sub

' ============================================
' MAP DATA: 20 cols x 30 rows = 600 values
' ============================================
mapdata:
' Row 0: top wall
Data 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5
' Row 1: side walls, HUD space
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 2: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 3: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 4: red bricks
Data 5,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,5
' Row 5: red bricks
Data 5,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,5
' Row 6: yellow bricks
Data 5,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,5
' Row 7: yellow bricks
Data 5,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,5
' Row 8: green bricks
Data 5,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,5
' Row 9: green bricks
Data 5,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,5
' Row 10: blue bricks
Data 5,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5
' Row 11: blue bricks
Data 5,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5
' Row 12: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 13: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 14: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 15: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 16: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 17: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 18: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 19: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 20: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 21: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 22: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 23: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 24: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 25: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 26: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 27: empty
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 28: empty (paddle row - paddle is a sprite)
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5
' Row 29: open bottom (ball death zone)
Data 5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5

' ============================================
' TILE ATTRIBUTES (8 tile types)
' ============================================
tileattrs:
Data 1      ' tile 1: red brick    - A_BRICK
Data 1      ' tile 2: yellow brick - A_BRICK
Data 1      ' tile 3: green brick  - A_BRICK
Data 1      ' tile 4: blue brick   - A_BRICK
Data 2      ' tile 5: wall         - A_WALL
Data 0      ' tile 6: ball         - none
Data 0      ' tile 7: paddle       - none
Data 0      ' tile 8: paddle left  - none
Function GetPress() As string
 Local x% = Port(GP8,8)
 Local y%=x% Xor &HFF
   Select Case y%
     Case 2
       GetPress=Chr$(130)
     Case 8
       GetPress=Chr$(131)
     Case 64
       GetPress="Q"
     Case 128
       GetPress="L"
     Case 16
       GetPress=" "
     Case Else
     GetPress=""
   End Select
End Function