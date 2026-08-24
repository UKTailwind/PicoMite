

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

' Place the atoms on screen.
' This is done in three passes.  Positions must be settled for ALL atoms
' before any of them is shown, because SPRITE SHOW saves whatever is under
' the sprite as that sprite's background.  Show one atom on top of another
' and the lower atom becomes part of the upper one's saved background, so
' it gets stamped back onto the screen every time the upper atom moves -
' that is what leaves a permanent trail.

' Pass 1 - claim the grid positions that are clear of the boxes.
' The grid step guarantees grid positions can never overlap each other.
' Positions that hit a box are deferred, marked with x() = -1.
k=1
For i=MM.HRES\9 To MM.HRES\9*8 Step MM.HRES\9
  For j=MM.VRES\9 To MM.VRES\9*8 Step MM.VRES\9
    If Not inside_box(i, j, 9) Then
      x(k)=i
      y(k)=j
    Else
      x(k)=-1
      y(k)=-1
    EndIf
    k=k+1
  Next j
Next i

' Pass 2 - find a home for each deferred atom, avoiding the boxes AND
' every atom already placed.  Deferring the whole pass until the grid is
' known is what stops a random atom landing on a grid cell that has not
' been filled in yet.
For k=1 To 64
  If x(k) = -1 Then
    Do
      x(k) = Rnd*(MM.HRES-9)
      y(k) = Rnd*(MM.VRES-9)
    Loop Until inside_box(x(k), y(k), 9) = 0 And hits_atom(k, x(k), y(k), 9) = 0
  EndIf
Next k

' Pass 3 - nothing overlaps now, so a plain SPRITE SHOW is safe
For k=1 To 64
  Sprite show k,x(k),y(k),1
  vector k,direction(k), 0, x(k), y(k) 'load up the vector move
Next k

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

' Check whether a 'size' square at px,py would overlap any atom that has
' already been given a position.  Atoms still awaiting one are marked -1
' and are skipped.
Function hits_atom(me As integer, px As integer, py As integer, size As integer) As integer
  Local integer a
  For a = 1 To 64
    If a <> me Then
      If x(a) >= 0 Then
        If px + size > x(a) And px < x(a) + size Then
          If py + size > y(a) And py < y(a) + size Then
            hits_atom = 1
            Exit Function
          EndIf
        EndIf
      EndIf
    EndIf
  Next a
  hits_atom = 0
End Function

' True if 'me' can be shown at px,py: on screen, and not overlapping another
' atom.  Any move of more than one pixel must pass this before SPRITE SHOW.
' The engine flags a sprite-to-sprite collision as soon as two sprites touch,
' one pixel before they actually overlap, so single pixel travel is always
' deflected in time.  A multi pixel jump can clear that guard band in one go
' and land on top of another atom, which is what creates a trail.
Function place_ok(me As integer, px As integer, py As integer, aw As integer, ah As integer) As integer
  place_ok = 0
  If px < 0 Then Exit Function
  If py < 0 Then Exit Function
  If px > MM.HRES - aw Then Exit Function
  If py > MM.VRES - ah Then Exit Function
  If hits_atom(me, px, py, aw) Then Exit Function
  place_ok = 1
End Function

' Vector movement subroutine
Sub vector(myobj As integer, angle As float, distance As float, x_new As integer, y_new As integer)
  Static float y_move(64), x_move(64)
  Static float x_last(64), y_last(64)
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
  Local integer bx, by, bw, bh, aw, ah
  Local integer pl, pr, pt, pb, pushx, pushy, moved
  Local float current_angle=direction(atom)

  aw = Sprite(W, atom)
  ah = Sprite(H, atom)

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
      ' Bounce off a static object.
      '
      ' The old code chose the bounce axis by comparing the atom centre with
      ' the box centre, and only ever reflected the direction - it never
      ' separated the atom from the box.  Reflection on its own cannot
      ' recover from an overlap: the atom is still touching next frame, so it
      ' collides again and reflects again, and two reflections about the same
      ' axis cancel out.  That is the atom stuck creeping along a box edge.
      ' Near a corner the centre test picks the wrong axis, which walks the
      ' atom further in until it is completely inside the box - and once
      ' fully inside, nothing in the routine can ever get it out.
      '
      ' Fix: work out the minimum translation vector (how far the atom must
      ' move on each axis to clear the box), then actually push it out along
      ' the shallower axis and reflect about that same axis.
      bx = Sprite(ST, bg_hit, X)
      by = Sprite(ST, bg_hit, Y)
      bw = Sprite(ST, bg_hit, W)
      bh = Sprite(ST, bg_hit, H)

      pl = (x(atom) + aw) - bx      ' travel needed to clear out to the left
      pr = (bx + bw) - x(atom)      ' travel needed to clear out to the right
      pt = (y(atom) + ah) - by      ' travel needed to clear upwards
      pb = (by + bh) - y(atom)      ' travel needed to clear downwards

      If pl < pr Then pushx = -pl Else pushx = pr
      If pt < pb Then pushy = -pt Else pushy = pb

      ' A push moves the atom further than the one pixel per frame that
      ' normal travel uses, so it can skip straight past the engine's
      ' touching-but-not-overlapping guard band and land ON another atom.
      ' That captures the other atom into this sprite's saved background and
      ' leaves a permanent trail, so every destination must be checked.
      ' Shallower axis first, then the other one.
      moved = 0
      If Abs(pushx) <= Abs(pushy) Then
        If place_ok(atom, x(atom)+pushx, y(atom), aw, ah) Then
          x(atom) = x(atom) + pushx
          current_angle = 360 - current_angle
          moved = 1
        ElseIf place_ok(atom, x(atom), y(atom)+pushy, aw, ah) Then
          y(atom) = y(atom) + pushy
          current_angle = ((540 - current_angle) Mod 360)
          moved = 1
        EndIf
      Else
        If place_ok(atom, x(atom), y(atom)+pushy, aw, ah) Then
          y(atom) = y(atom) + pushy
          current_angle = ((540 - current_angle) Mod 360)
          moved = 1
        ElseIf place_ok(atom, x(atom)+pushx, y(atom), aw, ah) Then
          x(atom) = x(atom) + pushx
          current_angle = 360 - current_angle
          moved = 1
        EndIf
      EndIf

      If moved = 0 Then
        ' Boxed in by other atoms this frame - just reflect and try the
        ' separation again next frame, rather than land on top of one.
        If Abs(pushx) <= Abs(pushy) Then
          current_angle = 360 - current_angle
        Else
          current_angle = ((540 - current_angle) Mod 360)
        EndIf
      EndIf

      ' Re-seed the vector sub with the corrected position.  Without this its
      ' internal float position still holds the pre-push spot and the very
      ' next move puts the atom straight back inside the box.
      vector atom, current_angle, 0, x(atom), y(atom)
    Else
      ' Collision with another sprite or corner
      current_angle = current_angle + 180
    EndIf
  EndIf
  
  direction(atom) = current_angle
  vector atom, direction(atom), j, x(atom), y(atom) 'break the collision
  Sprite show atom, x(atom), y(atom), 1
  
  ' If the simple bounce didn't work, try a random bounce.
  ' The <>0 matters: AND is bitwise, so without it an even collision value
  ' (e.g. edges=2, top) ANDed with the 1 from j<10 gives 0 and this whole
  ' recovery loop is silently skipped.
  Do While ((sprite(t,atom) Or sprite(e,atom)) <> 0) And (j < 10)
    Do
      direction(atom) = Rnd*360
      vector atom, direction(atom), j, x(atom), y(atom)
      j = j + 1
    Loop Until place_ok(atom, x(atom), y(atom), aw, ah) And inside_box(x(atom), y(atom), aw) = 0
    Sprite show atom, x(atom), y(atom), 1
  Loop
  
  ' If that didn't work then place the atom randomly (avoiding boxes)
  Do While (sprite(t,atom) Or sprite(e,atom))
    direction(atom) = Rnd*360
    Do
      x(atom) = Rnd*(MM.HRES-aw)
      y(atom) = Rnd*(MM.VRES-ah)
    Loop Until inside_box(x(atom), y(atom), aw) = 0 And hits_atom(atom, x(atom), y(atom), aw) = 0
    vector atom, direction(atom), 0, x(atom), y(atom)
    Sprite show atom, x(atom), y(atom), 1
  Loop
End Sub
