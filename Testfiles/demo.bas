Play Modfile "b:mario.mod"
MODE 2
Dim integer transparent=0
Dim integer white=1:Map(white)=&hffffff
Dim integer silver=2:Map(silver)=&hc0c0c0
Dim integer grey=3:Map(grey)=&h808080
Dim integer black=4:Map(black)=&h000000
Dim integer marron=5:Map(marron)=&hb72504
Dim integer red=6:Map(red)=&hff0000
Dim integer brown=7:Map(brown)=&h684632
Dim integer orange=8:Map(orange)=&heeaf36
Dim integer yellow=9:Map(yellow)=&hffff00
Dim integer darkgreen=10:Map(darkgreen)=&h046017
Dim integer green=11:Map(green)=&h138200
Dim integer lime=12:Map(lime)=&h73f218
Dim integer blue=13:Map(blue)=&h0000ff
Dim integer azul=14:Map(azul)=&h6185f8
Dim integer violet=15:Map(violet)=&h4b0082
Map set
Map(0)=&h6185f8
Map set

Color Map(white),(black)
Restore calci
For n=1 To 5
CLS
For j=1 To 23
Read a$
For i=1 To Len(a$)
Select Case Mid$(a$,i,1)
  Case "W":Pixel i,j,Map(white)
  Case "w":Pixel i,j,Map(silver)
  Case "D":Pixel i,j,Map(grey)
  Case "d":Pixel i,j,Map(black)
  Case "r":Pixel i,j,Map(brown)
  Case "R":Pixel i,j,Map(red)
  Case "o":Pixel i,j,Map(marron)
  Case "O":Pixel i,j,Map(orange)
  Case "y":Pixel i,j,Map(yellow)
  Case "g":Pixel i,j,Map(darkgreen)
  Case "G":Pixel i,j,Map(green)
  Case "l":Pixel i,j,Map(lime)
  Case "b":Pixel i,j,Map(blue)
  Case "B":Pixel i,j,Map(azul)
  Case "v":Pixel i,j,Map(violet)
  Case Else
End Select
Sprite read n,1,1,14,23
Next
Next
Next

Restore calci
For n=6 To 10
CLS
For j=1 To 23
Read a$
For i=1 To Len(a$)
Select Case Mid$(a$,i,1)
  Case "W":Pixel i,j,Map(white)
  Case "w":Pixel i,j,Map(silver)
  Case "D":Pixel i,j,Map(grey)
  Case "d":Pixel i,j,Map(black)
  Case "r":Pixel i,j,Map(marron)
  Case "R":Pixel i,j,Map(red)
  Case "o":Pixel i,j,Map(brown)
  Case "O":Pixel i,j,Map(orange)
  Case "y":Pixel i,j,Map(violet)
  Case "g":Pixel i,j,Map(darkgreen)
  Case "G":Pixel i,j,Map(green)
  Case "l":Pixel i,j,Map(lime)
  Case "b":Pixel i,j,Map(blue)
  Case "B":Pixel i,j,Map(azul)
  Case "v":Pixel i,j,Map(yellow)
  Case Else
End Select
Sprite read n,1,1,14,23
Next
Next
Next

CLS
'FRAMEBUFFER create f
'FRAMEBUFFER write f
Restore tiles
For n=1 To 7
Read a$
For j=0 To 7
Read a$

For i=1 To Len(a$)
Select Case Mid$(a$,i,1)
  Case "W":Pixel i+n*8-9,j,Map(white)
  Case "w":Pixel i+n*8-9,j,Map(silver)
  Case "D":Pixel i+n*8-9,j,Map(grey)
  Case "d":Pixel i+n*8-9,j,Map(black)
  Case "r":Pixel i+n*8-9,j,Map(brown)
  Case "R":Pixel i+n*8-9,j,Map(red)
  Case "o":Pixel i+n*8-9,j,Map(marron)
  Case "O":Pixel i+n*8-9,j,Map(orange)
  Case "y":Pixel i+n*8-9,j,Map(yellow)
  Case "g":Pixel i+n*8-9,j,Map(darkgreen)
  Case "G":Pixel i+n*8-9,j,Map(green)
  Case "l":Pixel i+n*8-9,j,Map(lime)
  Case "b":Pixel i+n*8-9,j,Map(blue)
  Case "B":Pixel i+n*8-9,j,Map(azul)
  Case "v":Pixel i+n*8-9,j,Map(violet)
  Case Else
End Select
Next
Next
Blit read n,n*8-8,0,8,8
Next
FRAMEBUFFER create
FRAMEBUFFER layer

Restore world
For x=0 To 320\8
Read a$
If a$="" Then Exit For
For y=0 To 240\8

Select Case Mid$(a$,y*2+1,2)
  Case "g ":FRAMEBUFFER write f:Blit write 1,x*8,MM.VRES-8-y*8
  Case "| ":FRAMEBUFFER write f:Blit write 2,x*8,MM.VRES-8-y*8
  Case "\ ":FRAMEBUFFER write f:Blit write 3,x*8,MM.VRES-8-y*8
  Case "/ ":FRAMEBUFFER write f:Blit write 4,x*8,MM.VRES-8-y*8
  Case "e ":FRAMEBUFFER write f:Blit write 5,x*8,MM.VRES-8-y*8
  Case "b1"
    FRAMEBUFFER write n:Blit write 6,x*8,MM.VRES-8-y*8
    FRAMEBUFFER write f:Blit write 2,x*8,MM.VRES-8-y*8
  Case "b2"
    FRAMEBUFFER write n:Blit write 7,x*8,MM.VRES-8-y*8
    FRAMEBUFFER write f:Blit write 2,x*8,MM.VRES-8-y*8
End Select
xxx:
Next
Next
FRAMEBUFFER write n
'right arm retracted
'left arm back swing forwar
'right leg front
calci:
Data "  yrrrr   "
Data "  ryrrrr  "
Data "  yrrwd"
Data "  rrOOO"
Data "  rrORR"
Data "  rrOO "
Data " rrvvv"
Data " r vvv"
Data "  vvvvv"
Data " vvvvvvv"
Data "vvvvvvvv"
Data "vvvvvv vv"
Data "  vOOv OO"
Data "   vvv"
Data "   bbb"
Data "   bbb"
Data "  bbbb"
Data "  bbbbb"
Data " bbb bbb"
Data " bbb bbb"
Data "bbb   bbb"
Data "oooo  oooo"
Data ""

Data "  yrrrr   "
Data "  ryrrrr  "
Data "  yrrwd"
Data "  rrOOO"
Data "  rrORR"
Data " rrrOO "
Data " rrvvv"
Data "   vvv"
Data "   vvv"
Data "  vvvv"
Data " vvvvvv"
Data " vvvvvO"
Data "   vOO"
Data "   vvv"
Data "   bbb"
Data "   bbb"
Data "  bbbb"
Data "  bbbbb"
Data " bbbbbb"
Data " bbbbbb"
Data " ooobbb"
Data "    oooo"
Data ""

Data "  yrrrr   "
Data "  ryrrrr  "
Data "  yrrwd"
Data "  rrOOO"
Data "  rrORR"
Data "  rrOO "
Data " rrvvv"
Data " r vvv"
Data "   vvv"
Data "  vvvv"
Data "  vvvv"
Data "  vvvv"
Data "   vvOO"
Data "   vvv"
Data "   bbb"
Data "   bbb"
Data "   bbb"
Data "   bbb "
Data "   bbb "
Data "   bbb "
Data "   bbb"
Data "   oooo"
Data ""

Data "  yrrrr   "
Data "  ryrrrr  "
Data "  yrrwd"
Data "  rrOOO"
Data "  rrORR"
Data " rrrOO "
Data " rrvvv"
Data "   vvv"
Data "  vvvv"
Data " vvvvv"
Data " vvvvv"
Data "  vvvv"
Data "  OvvvOO"
Data "   vvv"
Data "   bbb"
Data "   bbb"
Data "   bbb"
Data "   bbbb "
Data "   bbbbb "
Data "   bbbbb "
Data "   bbbooo"
Data "   oooo"
Data ""

Data "  yrrrr   "
Data "  ryrrrr  "
Data "  yrrwd"
Data "  rrOOO"
Data "  rrORR"
Data "  rrOO "
Data " rrvvv"
Data " r vvv"
Data "  vvvvv"
Data " vvvvvvv"
Data "vvvvvvvv"
Data "vvvvvv vv"
Data " OOvvv OO"
Data "   vvv"
Data "   bbb"
Data "   bbb"
Data "  bbbb"
Data "  bbbbb"
Data " bbb bbb"
Data " bbb bbb"
Data "bbb   bbb"
Data "oooo  oooo"
Data ""

tiles:
Data "g"
Data "GGGGGGGG"
Data "GGGGGGGG"
Data "GGGGGGGG"
Data "GGGGGGGG"
Data "GGGGGGGG"
Data "GGGGGGGG"
Data "GGGGGGGG"
Data "GGGGGGGG"

Data "|"
Data ""
Data ""
Data ""
Data ""
Data ""
Data ""
Data ""
Data "GGgGGgGG"

Data "\"
Data "       G"
Data "      gG"
Data "     GGG"
Data "    gGGG"
Data "   GGGGG"
Data "  gGGGGG"
Data " GGGGGGG"
Data "GGGGGGGG"
Data "/"
Data "G"
Data "Gg"
Data "GGG"
Data "GGGg"
Data "GGGGG"
Data "GGGGGg"
Data "GGGGGGG"
Data "GGGGGGGG"
'earth
Data "e"
Data "rrrrrDrr"
Data "rrDrrror"
Data "rrrrorrr"
Data "rdrrorrD"
Data "rrrrdrrr"
Data "rorDrror"
Data "rrrrrDrr"
Data "rdrdrrrd"
'bush 1 b1
Data "b1"
Data ""
Data "       g "
Data "      gg"
Data "    gggg"
Data "  gggggg"
Data " ggggggg"
Data "gggggggg"
Data "GGgGGgGG"

Data "b2"
Data ""
Data "g"
Data "gg"
Data "gggg"
Data "gggggg"
Data "ggggggg"
Data "gggggggg"
Data "GGgGGgGG"

'.

'Print sprite(h,1)
'End
FRAMEBUFFER write f

'1 is boundary method, 2 is pixel exact
mode1=1
mode2=1


Do
y1=10
y2=170
prevn=1
For n=1 To 300 Step 1
Sprite show safe(n\3) Mod 5+1,00+n,y1,0,0,1
Sprite show safe(n\3) Mod 5+6,00+n,y2,0,0,1




If mode1=1 Then
If sprite(b,(n\3) Mod 5+1)=0 Then
  y1=y1+1
Else
  bottomcoll=sprite(b,(n\3) Mod 5+1,3)
  If bottomcoll=1 Then
    'nothing todo we slide on the floor
  Else If bottomcoll=2 Then
    mode2=2
    'we are hitting a ramp now we need to switch to pixel exact
  End If
End If
End If

If mode1=2 Then
If sprite(b,(n\3) Mod 5+1)=0 Then
  y1=y1+1
Else
   If sprite(b,(n\3) mode 5+1,3)=1 Then mode1=1
  y1=y1-sprite(b,(n\3) Mod 5+1,7)
End If
End If


If mode2=1 Then
If sprite(b,(n\3) Mod 5+6)=0 Then
  y2=y2+1
Else
  bottomcoll=sprite(b,(n\3) Mod 5+6,3)
  If bottomcoll=1 Then
    'nothing todo we slide on the floor
  Else If bottomcoll=2 Then
    mode2=2
    'we are hitting a ramp now we need to switch to pixel exact
  End If
End If
End If

If mode2=2 Then
If sprite(b,(n\3) Mod 5+6)=0 Then
  y2=y2+1
Else
   If sprite(b,(n\3) Mod 5+6,3)=1 Then mode2=1
  y2=y2-sprite(b,(n\3) Mod 5+6,7)
End If
End If


If y1<0 Then y1=0
If y2<0 Then y2=170
If y2>239 Then y2=170


Print @(20,0) sprite(b,(n\3)Mod 5+1)" "sprite(b,(n\3)Mod 5+6)" "y1" "y2" "mode1" "mode2
Print @(200,0) "____________________"
Print @(200,0) sprite(b,(n\3) Mod 5+1,0)"   "sprite(b,(n\3) Mod 5+6,0)"  "
Print @(200,10) "____________________"
Print @(200,10) sprite(b,(n\3) Mod 5+1,1)"   "sprite(b,(n\3) Mod 5+6,1)"  "
Print @(200,20) "____________________"
Print @(200,20) sprite(b,(n\3) Mod 5+1,2)"   "sprite(b,(n\3) Mod 5+6,2)"  "
Print @(200,30) "____________________"
Print @(200,30) sprite(b,(n\3) Mod 5+1,3)"   "sprite(b,(n\3) Mod 5+6,3)"  "


Print @(200,50) "____________________"
Print @(200,50) sprite(b,(n\3) Mod 5+1,4)"   "sprite(b,(n\3) Mod 5+6,4)"  "
Print @(200,60) "____________________"
Print @(200,60) sprite(b,(n\3) Mod 5+1,5)"   "sprite(b,(n\3) Mod 5+6,5)"  "
Print @(200,70) "____________________"
Print @(200,70) sprite(b,(n\3) Mod 5+1,6)"   "sprite(b,(n\3) Mod 5+6,6)"  "
Print @(200,80) "____________________"
Print @(200,80) sprite(b,(n\3) Mod 5+1,7)"   "sprite(b,(n\3) Mod 5+6,7)"  "
FRAMEBUFFER copy f,l,b
Pause 100
If sprite(X,(n\3) Mod 5+6)<>10000 Then Sprite hide safe(n\3) Mod 5+6
If sprite(X,(n\3) Mod 5+1)<>10000 Then Sprite hide safe(n\3) Mod 5+1
prev=n
Next
y1=10
y2=170

For n=300 To 0 Step -2
Sprite show safe(n\3) Mod 5+1,00+n,y1,0,1,1
Sprite show safe(n\3) Mod 5+6,00+n,y2,0,1,1
Print @(20,0) sprite(b,(n\3) Mod 5+1)" "sprite(b,(n\3) Mod 5+6)" "MM.HRES" "MM.VRES
If sprite(b,(n\3) Mod 5+1)=0 Then y1=y1+1
If sprite(b,(n\3) Mod 5+6)=0 Then y2=y2+1

FRAMEBUFFER copy f,l,b

Sprite hide safe(n\3) Mod 5+6

Sprite hide safe(n\3) Mod 5+1

Next

Loop
'Print sprite(b)
'Print sprite(B,1)
'Print sprite(b,1,1)
world:
Data "e e g | . . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e g | . . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e g | . . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e g | . . . . . e g . . . . . . e . . . . . . . . . . . . "
Data "e e g | . . . . . e g . . . . . . e . . . . . . . . . . . . "
Data "e e g | . . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e g b1. . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e g b2. . . . . e g . . . . . . . . . . . . e g . . . . . "
Data "e e g | . . . . . e g . . . . . . . . . . . . e g . . . . . "
Data "e e g | . . . . . e g . . . . . . . . . . . . e g . . . . . "'10
Data "e e g | . . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e g | . . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e g | . . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e g \ . . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e g g | . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . . . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . . . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . . . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . e . . . . . e g . . . . . "'20
Data "e e e g | . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e e g b1. . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e e g b2. . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . . . . . . . e g . . . . . "'30
Data "e e e g | . . . . e g . . . . . . . . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e e g | . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e e g | . . . . . . . . . . . . e . . . . . e g . . . . . "
Data "e e e / . . . . . . . . . . . . . e . . . . . e g . . . . . "
Data "e e / . . . . . . . . . . . . . . e . . . . . e g . . . . . "
Data "e e | . . . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e | . . . . . . e g . . . . . . e . . . . . e g . . . . . "'40
Data "e e | . . . . . . e g . . . . . . . . . . . . . . . . . . . "
Data "e e | . . . . . . e g . . . . . . . . . . . . . . . . . . . "
Data "e e | . . . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e | . . . . . . e g . . . . . . e . . . . . e g . . . . . "'40
Data "e e | . . . . . . e g . . . . . . . . . . . . . . . . . . . "
Data "e e | . . . . . . e g . . . . . . . . . . . . . . . . . . . "
Data "e e | . . . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e | . . . . . . e g . . . . . . e . . . . . e g . . . . . "'40
Data "e e | . . . . . . e g . . . . . . . . . . . . . . . . . . . "
Data "e e | . . . . . . e g . . . . . . . . . . . . . . . . . . . "
Data "e e | . . . . . . e g . . . . . . e . . . . . e g . . . . . "
Data "e e | . . . . . . e g . . . . . . e . . . . . e g . . . . . "'40
Data "e e | . . . . . . e g . . . . . . . . . . . . . . . . . . . "
Data "e e | . . . . . . e g . . . . . . . . . . . . . . . . . . . "
Data ""
