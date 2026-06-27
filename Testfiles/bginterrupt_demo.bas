Option explicit
Option default none
Option console serial
Option base 1
MODE 2
FRAMEBUFFER create
FRAMEBUFFER write f
CLS

' Static Object Interrupt Demo
' Demonstrates both SPRITE STINTERRUPT and SPRITE INTERRUPT
' - Static objects for walls/goal (STINTERRUPT)
' - Sprite collisions for collectible coins (INTERRUPT)

Dim integer player_x = 20, player_y = 20
Dim integer player_sprite = 1
Dim integer score = 0, lives = 3
Dim integer last_st_hit = 0, sprite_hit = 0
Dim integer coin_collected = 0
Dim integer i
Dim string msg$ = ""

' Coin positions (center x, center y)
Dim integer coin_x(5) = (100, 220, 160, 100, 220)
Dim integer coin_y(5) = (75, 75, 145, 175, 175)

' Create player sprite (green square with white border)
Box 0, 0, 20, 20, 1, RGB(white), RGB(green)
Sprite read player_sprite, 0, 0, 20, 20
CLS

' Create coin sprite (yellow circle) - 15x15 with circle centered
Circle 7, 7, 6, 1, 1, RGB(yellow), RGB(yellow)
Sprite read 2, 0, 0, 15, 15
CLS

' Make copies of coin sprite for the other 4 coins
Sprite copy 2, 3, 4

' Draw the game area
Box 0, 0, MM.HRES, MM.VRES, 1, RGB(white)

' Draw visible obstacles (red danger zones)
Box 50, 50, 40, 40, 2, RGB(red), RGB(red)
Box 230, 50, 40, 40, 2, RGB(red), RGB(red)
Box 140, 100, 40, 40, 2, RGB(red), RGB(red)
Box 50, 150, 40, 40, 2, RGB(red), RGB(red)
Box 230, 150, 40, 40, 2, RGB(red), RGB(red)

' Draw safe zone (green goal)
Box 140, 200, 40, 30, 2, RGB(green), RGB(cyan)
Text 145, 208, "GOAL", L, 1, 1, RGB(black)

' Define static objects for the danger zones (1-5)
Sprite static 1, 50, 50, 40, 40    ' Danger 1
Sprite static 2, 230, 50, 40, 40   ' Danger 2
Sprite static 3, 140, 100, 40, 40  ' Danger 3
Sprite static 4, 50, 150, 40, 40   ' Danger 4
Sprite static 5, 230, 150, 40, 40  ' Danger 5

' Define static object for goal zone (6)
Sprite static 6, 140, 200, 40, 30  ' Goal

' Show the coin sprites (sprites 2-6) - center at coin_x, coin_y
For i = 1 To 5
  Sprite show i + 1, coin_x(i) - 7, coin_y(i) - 7, 1
Next i

' Show the player sprite
Sprite show player_sprite, player_x, player_y, 1

' Small pause to let collision state settle
Pause 50

' Set up interrupt handlers AFTER sprites are positioned
Sprite stinterrupt st_collision    ' For walls and goal
Sprite interrupt coin_collision    ' For coin collection

' Display instructions
Text 5, 5, "Arrows: move. Collect coins, avoid red!", L, 1, 1, RGB(white), RGB(black)
Text 5, 220, "Score: 0  Lives: 3", L, 1, 1, RGB(white), RGB(black)

FRAMEBUFFER copy f, n

' Main game loop
Dim string key$
Dim integer need_redraw = 0
Do
  key$ = Inkey$
  
  If key$ <> "" Then
    need_redraw = 1
    
    ' Move based on key press
    Select Case Asc(key$)
      Case 128 ' Up
        player_y = player_y - 5
        If player_y < 5 Then player_y = 5
      Case 129 ' Down
        player_y = player_y + 5
        If player_y > MM.VRES - 25 Then player_y = MM.VRES - 25
      Case 130 ' Left
        player_x = player_x - 5
        If player_x < 5 Then player_x = 5
      Case 131 ' Right
        player_x = player_x + 5
        If player_x > MM.HRES - 25 Then player_x = MM.HRES - 25
    End Select
  EndIf
  
  ' Hide collected coin sprite
  If coin_collected > 0 Then
    Sprite hide safe coin_collected + 1
    coin_collected = 0
    need_redraw = 1
  EndIf
  
  If need_redraw Then
    need_redraw = 0
    
    ' Hide sprite, update position, show sprite
    Sprite hide player_sprite
    Sprite show player_sprite, player_x, player_y, 1
    
    ' Update status display
    Box 5, 220, 310, 12, 0, RGB(black), RGB(black)
    Text 5, 220, "Score: " + Str$(score) + "  Lives: " + Str$(lives), L, 1, 1, RGB(white), RGB(black)
    
    ' Handle message display - show in status area, stays until next move
    If msg$ <> "" Then
      Text 200, 220, msg$, L, 1, 1, RGB(yellow), RGB(black)
      msg$ = ""
    EndIf
    
    FRAMEBUFFER copy f, n
  EndIf
  
  Pause 10
Loop Until lives <= 0 Or score >= 100

' Game over
Sprite hide player_sprite
Box 100, 100, 120, 40, 2, RGB(white), RGB(black)
If lives <= 0 Then
  Text 120, 115, "GAME OVER!", L, 1, 1, RGB(red), RGB(black)
Else
  Text 130, 115, "YOU WIN!", L, 1, 1, RGB(green), RGB(black)
EndIf
FRAMEBUFFER copy f, n

End

' Static object collision interrupt - handles walls and goal
Sub st_collision
  sprite_hit = Sprite(ST, COLLISION)
  last_st_hit = Sprite(ST, OBJECT)
  
  ' Only react if it was the player sprite that hit the ST object
  If sprite_hit <> player_sprite Then Exit Sub
  
  If last_st_hit >= 1 And last_st_hit <= 5 Then
    ' Hit a danger zone - lose a life
    lives = lives - 1
    msg$ = "OUCH! -1 Life"
    ' Reset player position
    player_x = 20
    player_y = 20
    
  ElseIf last_st_hit = 6 Then
    ' Reached the goal!
    score = score + 50
    msg$ = "GOAL! +50!"
  EndIf
End Sub

' Sprite collision interrupt - handles coin collection
Sub coin_collision
  Local integer hit_sprite, col_count, c, other
  hit_sprite = Sprite(S)
  
  ' Only process if player sprite triggered the collision
  If hit_sprite = player_sprite Then
    col_count = Sprite(C, player_sprite)
    For c = 1 To col_count
      other = Sprite(C, player_sprite, c)
      ' Check if it's a coin sprite (2-6)
      If other >= 2 And other <= 6 Then
        score = score + 10
        msg$ = "+10 Points!"
        coin_collected = other - 1  ' Flag which coin to hide (1-5)
        Exit For
      EndIf
    Next c
  EndIf
End Sub
