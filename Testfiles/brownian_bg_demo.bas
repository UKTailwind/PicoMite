

Option explicit
Option default none
Option console serial
'MODE 2
FRAMEBUFFER create
FRAMEBUFFER write f
CLS

'brownian motion demo using sprites with static object collisions
Dim integer x(64),y(64),c(64)
Dim float direction(64)
Dim integer i,j,k, collision=0
Dim string q$

' Create the atom sprites
For i=1 To 64
  direction(i)=Rnd*360 'establish the starting direction for each atom
  c(i)=RGB(Rnd*255,Rnd*255,Rnd*255) 'give each atom a colour
  Circle 10,10,4,1,,RGB(white),c(i) 'draw the atom
  Sprite read i,6,6,9,9 'read it in as a sprite
Next i
CLS

' Load background image
'Load jpg "b:/img320"
cls rgb(myrtle)

' Draw screen border
Box 0,0,MM.HRES,MM.VRES

' Draw red obstacle boxes and define them as static objects
' Box 1 - top left area
Box 60,40,50,50,3,RGB(red),RGB(red)
Sprite static 1, 60, 40, 50, 50

' Box 2 - top right area  
Box 210,40,50,50,3,RGB(red),RGB(red)
Sprite static 2, 210, 40, 50, 50

' Box 3 - center
Box 135,95,50,50,3,RGB(red),RGB(red)
Sprite static 3, 135, 95, 50, 50

' Box 4 - bottom left area
Box 60,150,50,50,3,RGB(red),RGB(red)
Sprite static 4, 60, 150, 50, 50

' Box 5 - bottom right area
Box 210,150,50,50,3,RGB(red),RGB(red)
Sprite static 5, 210, 150, 50, 50

' Place the atoms on screen
k=1
For i=MM.HRES\9 To MM.HRES\9*8 Step MM.HRES\9
  For j=MM.VRES\9 To MM.VRES\9*8 Step MM.VRES\9
    ' Skip positions that would overlap with boxes
    If Not inside_box(i, j, 9) Then
      Sprite show k,i,j,1
      x(k)=i
      y(k)=j
      vector k,direction(k), 0, x(k), y(k) 'load up the vector move
    Else
      ' Find a random valid position that doesn't overlap boxes
      Do
        x(k) = Rnd*(MM.HRES-9)
        y(k) = Rnd*(MM.VRES-9)
      Loop Until Not inside_box(x(k), y(k), 9)
      Sprite show k,x(k),y(k),1
      vector k,direction(k), 0, x(k), y(k)
    EndIf
    k=k+1
  Next j
Next i

' Main animation loop
Do
  For i=1 To 64
    vector i, direction(i), 1, x(i), y(i)
    Sprite show i,x(i),y(i),1
    ' Check for sprite collisions OR background object collisions
    If sprite(S,i)<>-1 Then
      break_collision i
    EndIf
  Next i

  FRAMEBUFFER copy f,n
  Print Timer
  Timer = 0
Loop

' Check if a position is inside any of the static boxes
Function inside_box(px As integer, py As integer, size As integer) As integer
  Local integer b
  For b = 1 To 5
    If Sprite(ST, b, A) Then  ' If static object is active
      If px + size > Sprite(ST, b, X) And px < Sprite(ST, b, X) + Sprite(ST, b, W) Then
        If py + size > Sprite(ST, b, Y) And py < Sprite(ST, b, Y) + Sprite(ST, b, H) Then
          inside_box = 1
          Exit Function
        EndIf
      EndIf
    EndIf
  Next b
  inside_box = 0
End Function

' Vector movement subroutine
Sub vector(myobj As integer, angle As float, distance As float, x_new As integer, y_new As integer)
  Static float y_move(64), x_move(64)
  Static float x_last(69), y_last(64)
  Static float last_angle(64)
  
  If distance=0 Then
    x_last(myobj)=x_new
    y_last(myobj)=y_new
  EndIf
  If angle<>last_angle(myobj) Then
    y_move(myobj)=-Cos(Rad(angle))
    x_move(myobj)=Sin(Rad(angle))
    last_angle(myobj)=angle
  EndIf
  x_last(myobj) = x_last(myobj) + distance * x_move(myobj)
  y_last(myobj) = y_last(myobj) + distance * y_move(myobj)
  x_new=Cint(x_last(myobj))
  y_new=Cint(y_last(myobj))
End Sub

' Handle collisions - break them by bouncing
Sub break_collision(atom As integer)
  Local integer j=1, col, bg_hit, hit
  Local integer bx, by, bw, bh, ax, ay
  Local float current_angle=direction(atom)
  Local float dx, dy
  
  ' Check what type of collision occurred
  If sprite(e,atom)=1 Then
    ' Collision with left of screen
    current_angle=360-current_angle
  ElseIf sprite(e,atom)=2 Then
    ' Collision with top of screen
    current_angle=((540-current_angle) Mod 360)
  ElseIf sprite(e,atom)=4 Then
    ' Collision with right of screen
    current_angle=360-current_angle
  ElseIf sprite(e,atom)=8 Then
    ' Collision with bottom of screen
    current_angle=((540-current_angle) Mod 360)
  Else
    ' Check for static object collision
    bg_hit = 0
    For col = 1 To Sprite(C, atom)
      hit = Sprite(C, atom, col)
      If hit >= &H80 And hit < &HF0 Then
        ' Static object collision (codes 0x80-0xBF)
        bg_hit = hit And &H3F  ' Extract object number
        Exit For
      EndIf
    Next col
    
    If bg_hit > 0 Then
      ' Bounce off static object - determine which side was hit
      bx = Sprite(ST, bg_hit, X)
      by = Sprite(ST, bg_hit, Y)
      bw = Sprite(ST, bg_hit, W)
      bh = Sprite(ST, bg_hit, H)
      ax = x(atom) + Sprite(W, atom)\2
      ay = y(atom) + Sprite(H, atom)\2
      
      ' Determine if collision is more horizontal or vertical
      dx = Abs(ax - (bx + bw\2))
      dy = Abs(ay - (by + bh\2))
      
      If dx / bw > dy / bh Then
        ' Horizontal bounce (hit left or right side)
        current_angle = 360 - current_angle
      Else
        ' Vertical bounce (hit top or bottom)
        current_angle = ((540 - current_angle) Mod 360)
      EndIf
    Else
      ' Collision with another sprite or corner
      current_angle = current_angle + 180
    EndIf
  EndIf
  
  direction(atom) = current_angle
  vector atom, direction(atom), j, x(atom), y(atom) 'break the collision
  Sprite show atom, x(atom), y(atom), 1
  
  ' If the simple bounce didn't work, try a random bounce
  Do While (sprite(t,atom) Or sprite(e,atom)) And j<10
    Do
      direction(atom) = Rnd*360
      vector atom, direction(atom), j, x(atom), y(atom)
      j = j + 1
    Loop Until x(atom)>=0 And x(atom)<=MM.HRES-sprite(w,atom) And y(atom)>=0 And y(atom)<=MM.VRES-sprite(h,atom)
    Sprite show atom, x(atom), y(atom), 1
  Loop
  
  ' If that didn't work then place the atom randomly (avoiding boxes)
  Do While (sprite(t,atom) Or sprite(e,atom))
    direction(atom) = Rnd*360
    Do
      x(atom) = Rnd*(MM.HRES-sprite(w,atom))
      y(atom) = Rnd*(MM.VRES-sprite(h,atom))
    Loop Until Not inside_box(x(atom), y(atom), sprite(w,atom))
    vector atom, direction(atom), 0, x(atom), y(atom)
    Sprite show atom, x(atom), y(atom), 1
  Loop
End Sub
