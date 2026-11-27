' Threads version (PicoMite V8.00.00b5 or later): a chess clock runs as a thread (ClockThread) beside the game,
' and the library's search, a CSUB, lets it run every 128 nodes (the THREAD YIELD in the search below, which
' mmb2csub compiles to the firmware's CSubYield). The board redraw keeps each square's piece and colour, so a
' capture of a like piece is always redrawn.
' This version calls search() from a CSUB in the library. The LIBRARY LOAD
' below installs chesslib.bas there and restarts the program; once it is
' installed the line does nothing. chessiter.bas is the same program in plain
' BASIC.
Library LOAD "chesslib.bas", O
' TSCP Chess
' Tom Kerrigan's Simple Chess Program
' (TSCP) version 1.81c, 2/3/19
' Copyright 2019 Tom Kerrigan
' MMBasic conversion by Ceptimus

Option explicit:Option default integer:Option base 0
If Not Instr(MM.Info(lcdpanel),"MODE")) Then
FRAMEBUFFER create
FRAMEBUFFER write f
Else
MODE 1
EndIf
Colour RGB(white),RGB(black):CLS :Font 7
' the chess clock, a thread: started first, as a thread's stack must be in the processor's own memory, which the
' engine's arrays below would otherwise fill on a board with PSRAM
Dim clk!(1),clk_last!(1),clk_t0! 'each side's time booked, its last move's, and when the running clock started
Dim clk_side,clk_run,clk_redraw
Const C_W=0,C_B=1 'the clock's sides, as LI and DK below (defined here, before the thread starts)
Const vga=Instr(MM.Info(lcdpanel),"MODE")>0
Thread Start 1 Locals 24 Nest 4, ClockThread
Const autoplay=0

'changes to monitor STACK usage (require picomite 5.07.05RC5 or newer)
'SetTick 200,showstack

'Sub showstack
' Local a%,b%
' a%=MM.Info(stack)
' b%=MM.Info(heap)
' a%=a%-&h2003f800  'stack info from matherp
' Text 240,212,"stack free "+Str$(a%)
' Text 240,220,"heap free  "+Str$(b%)
'End Sub

'end of stack monitor changes ----------------------------------------

' BOARD
Sub init_board
Local i
Restore data_init_hue:For i=0 To 63:Read H(i):Next 'i
Restore data_init_piece:For i=0 To 63:Read P(i):Next 'i
side=LI
xside=DK
castle=15
ep=-1
fifty=0
ply=0
hply=0
set_hash ' init_hash() must be called before this function.
first_move(0)=0
End Sub

Sub init_hash
Local i,j,k
For i=0 To 1
  For j=0 To 5
    For k=0 To 63
      HP(i,j,k)=hash_rand()
    Next k
  Next j
Next i
hash_side=hash_rand()
For i=0 To 63
  hash_ep(i)=hash_rand()
Next i
End Sub

'could this be changed to a simple RND() ?
Function hash_rand()
Local i,r=0
For i=0 To 31:r=r Xor ((2^16)*Rnd())*(2^i):Next 'i
hash_rand=r
End Function

Sub set_hash
Local i
hash=0 'hash is global
For i=0 To 63
  If H(i)<>MT Then hash=hash Xor HP(H(i),P(i),i)
Next i
If side=DK Then hash=hash Xor hash_side
If ep<>-1 Then hash=hash Xor hash_ep(ep)
End Sub

Function in_check(s)
Local i
in_check=1 ' in case king not found (shouldn't happen)
For i=0 To 63
  If (P(i)=KING)And(H(i)=s) Then
    in_check=attack(i, s Xor 1)
    i=63
  EndIf
Next 'i
End Function

Function attack(sq,s)
Local i,j,n
attack=0 ' default to false
For i=0 To 63
  If H(i)=s Then
    If P(i)=PN Then
      If s=LI Then
        If ((i And 7)<>0)And(i-9=sq) Then
          attack=1:Exit For
        EndIf
        If ((i And 7)<>7)And(i-7=sq) Then
          attack=1:Exit For
        EndIf
      Else
        If ((i And 7)<>0)And(i+7=sq) Then
          attack=1:Exit For
        EndIf
        If ((i And 7)<>7)And(i+9=sq) Then
          attack=1:Exit For
        EndIf
      EndIf
    Else
      For j=0 To offsets(P(i))-1
        n=i
        Do
          n=mb(mb64(n)+offset(P(i),j))
          If n=-1 Then Exit
          If n=sq Then
            attack=1:j=offsets(P(i))-1
            i=63:Exit
          EndIf
          If H(n)<>MT Then Exit
          If slide(P(i))=0 Then Exit
        Loop
      Next 'j
    EndIf
  EndIf
Next i
End Function

Sub gen
Local i,j,n,x
first_move(ply+1)=first_move(ply)

For i=0 To 63
  If H(i)=side Then
    If P(i)=PN Then
      If side=LI Then
        If (i And 7) Then
          x=i-9
          If H(x)=DK Then gen_push(i,x,17)
        EndIf
        If (i And 7)<>7 Then
          x=i-7
          If H(x)=DK Then gen_push(i,x,17)
        EndIf
        x=i-8
        If H(x)=MT Then
          gen_push(i,x,16)
          If i>=48 Then
            Inc x,-8
            If H(x)=MT Then gen_push(i,x,24)
          EndIf
        EndIf
      Else
        If (i And 7) Then
          x=i+7
          If H(x)=LI Then gen_push(i,x,17)
        EndIf
        If (i And 7)<>7 Then
          x=i+9
          If H(x)=LI Then gen_push(i,x,17)
        EndIf
        x=i+8
        If H(x)=MT Then
          gen_push(i,x,16)
          If i<=15 Then
            Inc x,8
            If H(x)=MT Then gen_push(i,x,24)
          EndIf
        EndIf
      EndIf
    Else
      For j=0 To offsets(P(i))-1
        n=i
        Do
          n=mb(mb64(n)+offset(P(i),j))
          If n=-1 Then Exit
          If H(n)<>MT Then
            If H(n)=xside Then gen_push(i,n,1)
            Exit
          EndIf
          gen_push(i,n,0)
          If slide(P(i))=0 Then Exit
        Loop
      Next 'j
    EndIf
  EndIf
Next i

If side=LI Then
  If (castle And 1) Then gen_push(E1,G1,2)
  If (castle And 2) Then gen_push(E1,C1,2)
Else
  If (castle And 4) Then gen_push(E8,G8,2)
  If (castle And 8) Then gen_push(E8,C8,2)
EndIf

If ep<>-1 Then
  If side=LI Then
    x=ep+7
    If ((ep And 7)<>0)And(H(x)=LI)And(P(x)=PN) Then
      gen_push(x,ep,21)
    EndIf
    x=ep+9
    If ((ep And 7)<>7)And(H(x)=LI)And(P(x)=PN) Then
      gen_push(x,ep,21)
    EndIf
  Else
    x=ep-9
    If ((ep And 7)<>0)And(H(x)=DK)And(P(x)=PN) Then
      gen_push(x,ep,21)
    EndIf
    x=ep-7
    If ((ep And 7)<>7)And(H(x)=DK)And(P(x)=PN) Then
      gen_push(x,ep,21)
    EndIf
  EndIf
EndIf
End Sub

Sub gen_caps
Local i,j,n,x
first_move(ply+1)=first_move(ply)
For i=0 To 63
  If H(i)=side Then
    If P(i)=PN Then
      If side=LI Then
        If (i And 7) Then
          x=i-9
          If H(x)=DK Then gen_push(i,x,17)
        EndIf
        If (i And 7)<>7 Then
          x=i-7
          If H(x)=DK Then gen_push(i,x,17)
        EndIf
        If i<=15 Then
          x=i-8
          If H(x)=MT Then gen_push(i,x,16)
        EndIf
      EndIf
      If side=DK Then
        If (i And 7) Then
          x=i+7
          If H(x)=LI Then gen_push(i,x,17)
        EndIf
        If (i And 7)<>7 Then
          x=i+9
          If H(x)=LI Then gen_push(i,x,17)
        EndIf
        If i>=48 Then
          x=i+8
          If H(x)=MT Then gen_push(i,x,16)
        EndIf
      EndIf
    Else
      For j=0 To offsets(P(i))-1
        n=i
        Do
          n=mb(mb64(n)+offset(P(i),j))
          If n=-1 Then Exit
          If H(n)<>MT Then
            If H(n)=xside Then gen_push(i,n,1)
            Exit
          EndIf
          If slide(P(i))=0 Then Exit
        Loop
      Next 'j
    EndIf
  EndIf
Next i

If ep<>-1 Then
  If side=LI Then
    x=ep+7
    If ((ep And 7)<>0)And(H(x)=LI)And(P(x)=PN) Then
      gen_push(x,ep,21)
    EndIf
    x=ep+9
    If ((ep And 7)<>7)And(H(x)=LI)And(P(x)=PN) Then
      gen_push(x,ep,21)
    EndIf
  Else
    x=ep-9
    If ((ep And 7)<>0)And(H(x)=DK)And(P(x)=PN) Then
      gen_push(x,ep,21)
    EndIf
    x=ep-7
    If ((ep And 7)<>7)And(H(x)=DK)And(P(x)=PN) Then
      gen_push(x,ep,21)
    EndIf
  EndIf
EndIf
End Sub

Sub gen_push(from,too,bits)
Local g,q=0
If (bits And 16) Then
  If side=LI Then
    If too<=H8 Then gen_promote(from,too,bits):Exit Sub
  Else
    If too>=A1 Then gen_promote(from,too,bits):Exit Sub
  EndIf
EndIf
g=first_move(ply+1)
Inc first_move(ply+1),1
'Poke var q,0,from: Poke var q,1,too: Poke var q,2,0: Poke var q,3,bits
q=from Or(too<<8)Or(bits<<24)
If H(too)<>MT Then
  gd(g)=q Or((1000000+P(too)*10-P(from))<<32)
Else
  gd(g)=q Or(hi(from,too)<<32)
EndIf
End Sub

Sub gen_promote(from,too,bits)
Local i,g,p=0
For i=KT To QUEEN
  g=first_move(ply+1)
  Inc first_move(ply+1),1
' Poke var p,0,from: Poke var p,1,too: Poke var p,2,i: Poke var p,3,bits Or 32
  p=from Or(too<<8)Or(i<<16)Or((bits Or 32)<<24)
  gd(g)=p Or((1000000+i*10)<<32)
Next 'i
End Sub

Function makemove(m)
Local from,too
Local m_from=m And &HFF'Peek(var m,0)
Local m_too=m>>8 And &HFF'Peek(var m,1)
Local m_promote=m>>16 And &HFF'Peek(var m,2)
Local m_bits=m>>24 And &HFF'Peek(var m,3)
Local break=0
makemove=0

If (m_bits And 2) Then
  If in_check(side) Then Exit Function
  Select Case m_too
    Case 62
      If (H(F1)<>MT)Or(H(G1)<>MT) Then
        break=1
        ElseIf attack(F1, xside) Or attack(G1, xside) Then
        break=1
      EndIf
      from=H1
      too=F1
    Case 58
      If (H(B1)<>MT)Or(H(C1)<>MT)Or(H(D1)<>MT) Then
        break=1
        ElseIf attack(C1, xside) Or attack(D1, xside) Then
        break=1
      EndIf
      from=A1
      too=D1
    Case 6
      If (H(F8)<>MT)Or(H(G8)<>MT) Then
        break=1
        ElseIf attack(F8, xside) Or attack(G8, xside) Then
        break=1
      EndIf
      from=H8
      too=F8
    Case 2
      If (H(B8)<>MT)Or(H(C8)<>MT)Or(H(D8)<>MT) Then
        break=1
        ElseIf attack(C8, xside) Or attack(D8, xside) Then
        break=1
      EndIf
      from=A8
      too=D8
    Case Else  ' shouldn't get here
      from=-1
      too=-1
  End Select
  If break Then Exit Function
  H(too)=H(from)
  P(too)=P(from)
  H(from)=MT
  P(from)=MT
EndIf

hd(hply,0)=m Or(P(m_too)<<32)Or(castle<<36)Or((ep+1)<<40)Or(fifty<<48)
hd(hply,1)=hash
Inc ply,1
Inc hply,1

castle=castle And (castle_mask(m_from) And castle_mask(m_too))
If (m_bits And 8) Then
  If side=LI Then
    ep=m_too+8
  Else
    ep=m_too-8
  EndIf
Else
  ep=-1
EndIf

If (m_bits And 17) Then
  fifty=0
Else
  Inc fifty,1
EndIf

H(m_too)=side
If (m_bits And 32) Then
  P(m_too)=m_promote
Else
  P(m_too)=P(m_from)
EndIf
H(m_from)=MT
P(m_from)=MT

If (m_bits And 4) Then
  If side=LI Then
    H(m_too+8)=MT
    P(m_too+8)=MT
  Else
    H(m_too-8)=MT
    P(m_too-8)=MT
  EndIf
EndIf

side=side Xor 1
xside=xside Xor 1
If in_check(xside) Then takeback:Exit Function
set_hash
makemove=1
End Function

Sub takeback
Local m,m_from,m_too,m_bits,from,too

side=side Xor 1
xside=xside Xor 1
Inc ply,-1
Inc hply,-1
m=hd(hply,0)
'm_from=Peek(var m,0):m_too=Peek(var m,1):m_bits=Peek(var m,3)
m_from=m And &HFF:m_too=m>>8 And &HFF:m_bits=m>>24 And &HFF
castle=m>>36 And 15
ep=(m>>40 And 255)-1
fifty=m>>48
hash=hd(hply,1)
H(m_from)=side
If (m_bits And 32) Then
  P(m_from)=PN
Else
  P(m_from)=P(m_too)
EndIf
If (m>>32 And 15)=MT Then
  H(m_too)=MT
  P(m_too)=MT
Else
  H(m_too)=xside
  P(m_too)=m>>32 And 15
EndIf
If (m_bits And 2) Then
  Select Case m_too
    Case 62
      from=F1
      too=H1
    Case 58
      from=D1
      too=A1
    Case 6
      from=F8
      too=H8
    Case 2
      from=D8
      too=A8
    Case Else ' shouldn't get here
      from=-1
      too=-1
  End Select
  H(too)=side
  P(too)=ROOK
  H(from)=MT
  P(from)=MT
EndIf

If (m_bits And 4) Then
  If side=LI Then
    H(m_too+8)=xside
    P(m_too+8)=PN
  Else
    H(m_too-8)=xside
    P(m_too-8)=PN
  EndIf
EndIf
End Sub

' BOOK
Dim book_file=1 'set to 1 if opening book is present

Sub open_book
Local d$=MM.DEVICE$

On Error Skip
Open "book.txt" For input As#1
If MM.Errno Then
  oPrintR "Opening book missing."
Else
  Close #1
  book_file=1
EndIf
End Sub

Sub close_book
book_file=0
End Sub

Function book_move()
Local L$,book_line$
Local i,j,m,found
Local move(50)  'the possible book moves
Local count(50) 'the number of occurrences of each move
Local moves,total_count

moves=0
total_count=0
book_move=-1 'default to "no book move"
If book_file=0 Or hply>25 Then Exit Function
L$=""
For i=0 To hply-1
    L$=L$+move_str$(hd(i,0))+" "
Next 'i
Open "book.txt" For input As#1
Do While Not Eof(#1)
  Input #1, book_line$
  If book_match(L$, book_line$) Then
    m=parse_move(Mid$(book_line$, Len(L$)+1))
    If m<>-1 Then
      m=gd(m) And &HFFFFFFFF
      found=0
      For j=0 To moves-1
        If move(j)=m Then Inc count(j),1:found=1:j=moves
      Next 'j
      If found=0 Then
        move(moves)=m
        count(moves)=1
        Inc moves,1
      EndIf
      Inc total_count,1
    EndIf
  EndIf
Loop
Close #1
If moves=0 Then Exit Function

j=Int(Rnd()*total_count)
For i=0 To moves-1
  j=j-count(i)
  If j<0 Then book_move=move(i):i=moves
Next 'i
End Function

Function book_match(s1$, s2$)
book_match=0
If LEFT$(s2$, Len(s1$))=s1$ Then book_match=1
End Function

' DATA
Sub init_arrays
Local i,j


data_mb:
Data -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
Data -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
Data -1,  0,  1,  2,  3,  4,  5,  6,  7, -1
Data -1,  8,  9, 10, 11, 12, 13, 14, 15, -1
Data -1, 16, 17, 18, 19, 20, 21, 22, 23, -1
Data -1, 24, 25, 26, 27, 28, 29, 30, 31, -1
Data -1, 32, 33, 34, 35, 36, 37, 38, 39, -1
Data -1, 40, 41, 42, 43, 44, 45, 46, 47, -1
Data -1, 48, 49, 50, 51, 52, 53, 54, 55, -1
Data -1, 56, 57, 58, 59, 60, 61, 62, 63, -1
Data -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
Data -1, -1, -1, -1, -1, -1, -1, -1, -1, -1

data_mb64:
Data 21, 22, 23, 24, 25, 26, 27, 28
Data 31, 32, 33, 34, 35, 36, 37, 38
Data 41, 42, 43, 44, 45, 46, 47, 48
Data 51, 52, 53, 54, 55, 56, 57, 58
Data 61, 62, 63, 64, 65, 66, 67, 68
Data 71, 72, 73, 74, 75, 76, 77, 78
Data 81, 82, 83, 84, 85, 86, 87, 88
Data 91, 92, 93, 94, 95, 96, 97, 98

data_offset:
Data   0,   0,   0,   0,   0,   0,   0,   0
Data -21, -19, -12,  -8,   8,  12,  19,  21
Data -11,  -9,   9,  11,   0,   0,   0,   0
Data -10,  -1,   1,  10,   0,   0,   0,   0
Data -11, -10,  -9,  -1,   1,   9,  10,  11
Data -11, -10,  -9,  -1,   1,   9,  10,  11

data_castle_mask:
Data  7, 15, 15, 15,  3, 15, 15, 11
Data 15, 15, 15, 15, 15, 15, 15, 15
Data 15, 15, 15, 15, 15, 15, 15, 15
Data 15, 15, 15, 15, 15, 15, 15, 15
Data 15, 15, 15, 15, 15, 15, 15, 15
Data 15, 15, 15, 15, 15, 15, 15, 15
Data 15, 15, 15, 15, 15, 15, 15, 15
Data 13, 15, 15, 15, 12, 15, 15, 14


data_init_hue:
Data 1, 1, 1, 1, 1, 1, 1, 1
Data 1, 1, 1, 1, 1, 1, 1, 1
Data 6, 6, 6, 6, 6, 6, 6, 6
Data 6, 6, 6, 6, 6, 6, 6, 6
Data 6, 6, 6, 6, 6, 6, 6, 6
Data 6, 6, 6, 6, 6, 6, 6, 6
Data 0, 0, 0, 0, 0, 0, 0, 0
Data 0, 0, 0, 0, 0, 0, 0, 0

data_init_piece:
Data 3, 1, 2, 4, 5, 2, 1, 3
Data 0, 0, 0, 0, 0, 0, 0, 0
Data 6, 6, 6, 6, 6, 6, 6, 6
Data 6, 6, 6, 6, 6, 6, 6, 6
Data 6, 6, 6, 6, 6, 6, 6, 6
Data 6, 6, 6, 6, 6, 6, 6, 6
Data 0, 0, 0, 0, 0, 0, 0, 0
Data 3, 1, 2, 4, 5, 2, 1, 3


Restore data_mb:For i=0 To 119:Read mb(i):Next 'i
Restore data_mb64:For i=0 To 63:Read mb64(i):Next 'i
Restore data_offset

For i=0 To 5
  For j=0 To 7
    Read offset(i,j)
  Next 'j
Next 'i

Restore data_castle_mask:For i=0 To 63:Read castle_mask(i):Next 'i
Restore data_pawn_pcsq:For i=0 To 63:Read pawn_pcsq(i):Next 'i
Restore data_knight_pcsq:For i=0 To 63:Read knight_pcsq(i):Next 'i
Restore data_bishop_pcsq:For i=0 To 63:Read bishop_pcsq(i):Next 'i
Restore data_king_pcsq:For i=0 To 63:Read king_pcsq(i):Next 'i
Restore data_king_endgame_pcsq
For i=0 To 63:Read king_endgame_pcsq(i):Next 'i
Restore data_flip:For i=0 To 63:Read flip(i):Next 'i
End Sub

' DEFS
Const GEN_STACK  = 1120' 400 '1120 '208
Const MAX_PLY    = 32 ' as TSCP's C
Const HIST_STACK = 400
Const LI  = 0 'Light
Const DK  = 1 'Dark
Const PN  = 0 'Pawn
Const KT  = 1 'Knight
Const BISHOP = 2
Const ROOK   = 3
Const QUEEN  = 4
Const KING   = 5
Const MT = 6 'Empty
Const A1 =56
Const B1 =57
Const C1 =58
Const D1 =59
Const E1 =60
Const F1 =61
Const G1 =62
Const H1 =63
Const A8 = 0
Const B8 = 1
Const C8 = 2
Const D8 = 3
Const E8 = 4
Const F8 = 5
Const G8 = 6
Const H8 = 7

' EVAL
Const DPP  = 10 'Doubled Pawn Penalty
Const IPP  = 20 'Isolated Pawn Penalty
Const BPP  =  8 'Backwards Pawn Penalty
Const PPB  = 20 'Passed Pawn Bonus
Const RSOB = 10 'Rook Semi Open File Bonus
Const ROB  = 15 'Rook Open File Bonus
Const RSB  = 20 'Rook On Seventh Bonus

Dim piece_value(5)=(100,300,300,500,900,0)
Dim pawn_pcsq(63)
data_pawn_pcsq:
Data  0,   0,   0,   0,   0,   0,   0,   0
Data  5,  10,  15,  20,  20,  15,  10,   5
Data  4,   8,  12,  16,  16,  12,   8,   4
Data  3,   6,   9,  12,  12,   9,   6,   3
Data  2,   4,   6,   8,   8,   6,   4,   2
Data  1,   2,   3, -10, -10,   3,   2,   1
Data  0,   0,   0, -40, -40,   0,   0,   0
Data  0,   0,   0,   0,   0,   0,   0,   0

Dim knight_pcsq(63)
data_knight_pcsq:
Data -10, -10, -10, -10, -10, -10, -10, -10
Data -10,   0,   0,   0,   0,   0,   0, -10
Data -10,   0,   5,   5,   5,   5,   0, -10
Data -10,   0,   5,  10,  10,   5,   0, -10
Data -10,   0,   5,  10,  10,   5,   0, -10
Data -10,   0,   5,   5,   5,   5,   0, -10
Data -10,   0,   0,   0,   0,   0,   0, -10
Data -10, -30, -10, -10, -10, -10, -30, -10

Dim bishop_pcsq(63)
data_bishop_pcsq:
Data -10, -10, -10, -10, -10, -10, -10, -10
Data -10,   0,   0,   0,   0,   0,   0, -10
Data -10,   0,   5,   5,   5,   5,   0, -10
Data -10,   0,   5,  10,  10,   5,   0, -10
Data -10,   0,   5,  10,  10,   5,   0, -10
Data -10,   0,   5,   5,   5,   5,   0, -10
Data -10,   0,   0,   0,   0,   0,   0, -10
Data -10, -10, -20, -10, -10, -20, -10, -10

Dim king_pcsq(63)
data_king_pcsq:
Data -40, -40, -40, -40, -40, -40, -40, -40
Data -40, -40, -40, -40, -40, -40, -40, -40
Data -40, -40, -40, -40, -40, -40, -40, -40
Data -40, -40, -40, -40, -40, -40, -40, -40
Data -40, -40, -40, -40, -40, -40, -40, -40
Data -40, -40, -40, -40, -40, -40, -40, -40
Data -20, -20, -20, -20, -20, -20, -20, -20
Data   0,  20,  40, -20,   0, -20,  40,  20

Dim king_endgame_pcsq(63)
data_king_endgame_pcsq:
Data  0,  10,  20,  30,  30,  20,  10,   0
Data 10,  20,  30,  40,  40,  30,  20,  10
Data 20,  30,  40,  50,  50,  40,  30,  20
Data 30,  40,  50,  60,  60,  50,  40,  30
Data 30,  40,  50,  60,  60,  50,  40,  30
Data 20,  30,  40,  50,  50,  40,  30,  20
Data 10,  20,  30,  40,  40,  30,  20,  10
Data  0,  10,  20,  30,  30,  20,  10,   0

Dim flip(63)
data_flip:
Data 56,  57,  58,  59,  60,  61,  62,  63
Data 48,  49,  50,  51,  52,  53,  54,  55
Data 40,  41,  42,  43,  44,  45,  46,  47
Data 32,  33,  34,  35,  36,  37,  38,  39
Data 24,  25,  26,  27,  28,  29,  30,  31
Data 16,  17,  18,  19,  20,  21,  22,  23
Data  8,   9,  10,  11,  12,  13,  14,  15
Data  0,   1,   2,   3,   4,   5,   6,   7

Dim pawn_rank(1,9)
Dim piece_mat(1)
Dim pawn_mat(1)

Function eval_()
Local i,f,score(1)

For i=0 To 9
  pawn_rank(LI,i)=0
  pawn_rank(DK,i)=7
Next 'i
piece_mat(LI)=0
piece_mat(DK)=0
pawn_mat(LI)=0
pawn_mat(DK)=0
For i=0 To 63
  If H(i)<>MT Then
    If P(i)=PN Then
      Inc pawn_mat(H(i)),piece_value(PN)
      f=(i And 7)+1 ' add 1 because of the extra file in the array
      If H(i)=LI Then
        If pawn_rank(LI,f)<i\8 Then pawn_rank(LI,f)=i\8
      Else
        If pawn_rank(DK,f)>i\8 Then pawn_rank(DK,f)=i\8
      EndIf
    Else
      Inc piece_mat(H(i)),piece_value(P(i))
    EndIf
  EndIf
Next 'i

score(LI)=piece_mat(LI)+pawn_mat(LI)
score(DK)=piece_mat(DK)+pawn_mat(DK)
For i=0 To 63
  Do
    If H(i)<>MT Then Exit
    Inc i,1:If i>63 Then Exit
  Loop :If i>63 Then Exit For
  If H(i)=LI Then
    Select Case P(i)
      Case PN
        Inc score(LI),eval_light_pawn(i)
      Case KT
        Inc score(LI),knight_pcsq(i)
      Case BISHOP
        Inc score(LI),bishop_pcsq(i)
      Case ROOK
        If pawn_rank(LI,(i And 7)+1)=0 Then
          If pawn_rank(DK,(i And 7)+1)=7 Then
            Inc score(LI),ROB
          Else
            Inc score(LI),RSOB
          EndIf
        EndIf
        If i\8=1 Then
          Inc score(LI),RSB
        EndIf
      Case KING
        If piece_mat(DK)<=1200 Then
          Inc score(LI),king_endgame_pcsq(i)
        Else
          Inc score(LI),eval_light_king(i)
        EndIf
    End Select
  Else
    Select Case P(i)
      Case PN
        Inc score(DK),eval_dark_pawn(i)
      Case KT
        Inc score(DK),knight_pcsq(flip(i))
      Case BISHOP
        Inc score(DK),bishop_pcsq(flip(i))
      Case ROOK
        If pawn_rank(DK,(i And 7)+1)=7 Then
          If pawn_rank(LI,(i And 7)+1)=0 Then
            Inc score(DK),ROB
          Else
            Inc score(DK),RSOB
          EndIf
        EndIf
        If i\8=6 Then
          Inc score(DK),RSB
        EndIf
      Case KING
        If piece_mat(LI)<=1200 Then
          Inc score(DK),king_endgame_pcsq(flip(i))
        Else
          Inc score(DK),eval_dark_king(i)
        EndIf
    End Select
  EndIf
Next 'i

If side=LI Then
  eval_=score(LI)-score(DK)
Else
  eval_=score(DK)-score(LI)
EndIf
End Function

Function eval_light_pawn(sq)
Local r ' the value to return
Local f ' the pawn's file
Local x

r=0
f=(sq And 7)+1
Inc r,pawn_pcsq(sq)
x=sq\8

If pawn_rank(LI,f)>x Then Inc r,-DPP
If (pawn_rank(LI,f-1)=0)And(pawn_rank(LI,f+1)=0) Then
    Inc r,-IPP
ElseIf (pawn_rank(LI,f-1)<x)And(pawn_rank(LI,f+1)<x) Then
  Inc r,-BPP
EndIf

If (pawn_rank(DK,f-1)>=x)And(pawn_rank(DK,f)>=x) Then
  If pawn_rank(DK,f+1)>=x Then
   Inc r,(7-x)*PPB
  EndIf
EndIf

eval_light_pawn=r
End Function

Function eval_dark_pawn(sq)
Local r
Local f
Local x

f=(sq And 7)+1
r=pawn_pcsq(flip(sq))
x=sq\8

If pawn_rank(DK,f)<x Then Inc r,-DPP
If (pawn_rank(DK,f-1)=7)And(pawn_rank(DK,f+1)=7) Then
  Inc r,-IPP
ElseIf (pawn_rank(DK,f-1)>x)And(pawn_rank(DK,f+1)>x) Then
  Inc r,-BPP
EndIf

If (pawn_rank(LI,f-1)<=x)And(pawn_rank(LI,f)<=x) Then
  If pawn_rank(LI,f+1)<=x Then
    Inc r,x*PPB
  EndIf
EndIf
eval_dark_pawn=r
End Function

Function eval_light_king(sq)
Local r
Local i

r=king_pcsq(sq)
If (sq And 7)<3 Then
  Inc r,eval_lkp(1)
  Inc r,eval_lkp(2)
  Inc r,eval_lkp(3)\2
ElseIf (sq And 7)>4 Then
  Inc r,eval_lkp(8)
  Inc r,eval_lkp(7)
  Inc r,eval_lkp(6)\2
Else
  For i=(sq And 7) To (sq And 7)+2
    If (pawn_rank(LI,i)=0)And(pawn_rank(DK,i)=7) Then
      Inc r,-10
    EndIf
  Next 'i
EndIf

r=r*piece_mat(DK)
r=r\3100
eval_light_king=r
End Function

Function eval_lkp(f)
Local r=0

If pawn_rank(LI,f)=6 Then
ElseIf pawn_rank(LI,f)=5 Then
  Inc r,-10
ElseIf pawn_rank(LI,f) Then
  Inc r,-20
Else
  Inc r,-25
EndIf

If pawn_rank(DK,f)=7 Then
  Inc r,-15
ElseIf pawn_rank(DK,f)=5 Then
  Inc r,-10
ElseIf pawn_rank(DK,f)=4 Then
  Inc r,-5
EndIf

eval_lkp=r
End Function

Function eval_dark_king(sq)
Local r,i

r=king_pcsq(flip(sq))
If (sq And 7)<3 Then
  Inc r,eval_dkp(1)
  Inc r,eval_dkp(2)
  Inc r,eval_dkp(3)\2
ElseIf (sq And 7)>4 Then
  Inc r,eval_dkp(8)
  Inc r,eval_dkp(7)
  Inc r,eval_dkp(6)\2
Else
  For i=(sq And 7) To (sq And 7)+2
    If (pawn_rank(LI,i)=0)And(pawn_rank(DK,i)=7) Then
      Inc r,-10
    EndIf
  Next 'i
EndIf

'r=r*piece_mat(LI)
'r=r\3100
eval_dark_king=(r*piece_mat(LI))\3100
End Function

Function eval_dkp(f)
Local r=0

If pawn_rank(DK,f)=1 Then
ElseIf pawn_rank(DK,f)=2 Then
  Inc r,-10
ElseIf pawn_rank(DK,f)<>7 Then
  Inc r,-20
Else
  Inc r,-25
EndIf

If pawn_rank(LI,f)=0 Then
  Inc r,-15
ElseIf pawn_rank(LI,f)=2 Then
  Inc r,-10
ElseIf pawn_rank(LI,f)=3 Then
  Inc r,-5
EndIf

eval_dkp=r
End Function

' GRAPHICS
Const P_Y=464
Const R_H=7
Const B_W=136
Const B_T=10
Const E_W=320
Const B_T1=184
Const B_W1=224

'Sub load_sprites
'Local i,x,y
'For i=1 To 12
' x=(i-1)*20
' y=0
'Sprite Loadbmp i, "12piececol",x,y,20,20
'Next 'i
'End Sub

Sub oPrint s$
'text MM.INFO(HPOS),P_Y,s$
Print @(1,464);s$
End Sub

Sub oPrintR s$
'top x from, top y from , top x to. top y to, x length, y length
Blit 240,408,240,400,400,72
Blit 1,344,1,336,239,136
'Blit B_W,B_T1+R_H,B_W,B_T1,B_W1,P_Y-B_T1
'Blit 0,B_T+R_H,0,B_T,B_W,P_Y-B_T
Print @(1,464);s$
'Box 0,P_Y,E_W,R_H,0,0,0
If Not Instr(MM.Info(lcdpanel),"MODE")) Then FRAMEBUFFER copy f,n

'  Print
End Sub

Function oInput$(prompt$)
Local s$
draw_chessboard
'Box 0,P_Y,E_W,R_H,0,0,0
oPrint prompt$
If Not Instr(MM.Info(lcdpanel),"MODE")) Then FRAMEBUFFER copy f,n
'-------------------------------Volhout---------- autoplay itself -----
If autoplay Then
 s$="on" '<--------this is autoplay
Else
 Print @(40,464);:Input ;s$
EndIf
Blit 240,408,240,400,400,72
Blit 1,344,1,336,239,136
'Blit B_W,B_T1+R_H,B_W,B_T1,B_W1,P_Y-B_T1
'Blit 0,B_T+R_H,0,B_T,B_W,P_Y-B_T
'Box 0,P_Y,E_W,R_H,0,0,0
'  Print
oInput$=s$
If Not Instr(MM.Info(lcdpanel),"MODE")) Then FRAMEBUFFER copy f,n
End Function

Sub moves_template
' Clear the moves
  Box 0,1,227,310,,,RGB(black)
' draw the outline
  Box 0,1,227,310
  'tiles
  TILE 0,0,RGB(cyan),RGB(black),29,26
  TILE 3,1,RGB(white),RGB(black),5,24
  TILE 8,1,RGB(red),RGB(black),5,24
  TILE 17,1,RGB(white),RGB(black),5,24
  TILE 23,1,RGB(red),RGB(black),5,24
End Sub

Sub print_moves(num)
Local integer xposm(3)=(34,72,142,184)
Local integer xposc(1)=(4,110)
Local integer xmoves,ymoves,numm
If num=0 Then moves_template : Exit Sub
  'game list related
  numm=num-1
  xmoves=numm \ 2 +1
  ymoves=numm \ 4
'check if we need to scroll the moves list
  If numm>143 Then
   If (numm Mod 4)=0 Then
' move whole list up one line, blanking last line
    Blit 3,20,3,12,212,290
'check for move number going to 3 digits
    If numm<198 Then
      Print @(xposc(0),13+(35*MM.Info(FONTHEIGHT)));Str$(xmoves,2);"                ";Str$(xmoves+1,2);"                "
    Else
      Print @(xposc(0),13+(35*MM.Info(FONTHEIGHT)));Str$(xmoves,3);"               ";Str$(xmoves+1,3);"               "
    EndIf
   EndIf
   Print @(xposm(numm Mod 4),13+(35*MM.Info(Fontheight)));move_str$(hd(numm,0));
  Else
    If (numm Mod 4)=0 Then
      Print @(xposc(0),13+(ymoves*MM.Info(FONTHEIGHT)));Str$(xmoves,2);"                ";Str$(xmoves+1,2);"                "
    EndIf
'has undo command been used?
    If numm<tmpl Then
      Print @(xposc(0),13+((ymoves+1)*MM.Info(FONTHEIGHT)));"                                   "
    EndIf
    Print @(xposm(numm Mod 4),13+(ymoves*MM.Info(Fontheight)));move_str$(hd(numm,0));
  tmpl=numm
  EndIf

End Sub

Sub draw_chessboard
Local x,y,i,sq,s,d=0,from,too
Local tc,pc,px,py,tx,ty,t,q,dx
print_moves(hply)
Local fi$ length 7="Tiles\b", ex$ length 5="w.bin", tl$
Local pp$ length 15, pcs$(6) length 1=("p","n","b","r","q","k",".")

If hply>0 Then
'  from=Peek(var hd(hply-1,0),0):too=Peek(var hd(hply-1,0),1)
   from=hd(hply-1,0) And &HFF:too=hd(hply-1,0)>>8 And &HFF
Else
  from=-1:too=-1
EndIf
If flipped<>flippedl Then flippedl=flipped:Math set -1,Pl()
'scan all squares, only redraw ones which change.
For i=0 To 63
  sq=i:If flipped Then sq=63-i
  x=i Mod 8:y=i\8
  If Pl(sq)<>P(sq)+8*H(sq) Or froml=sq Or tool=sq Or from=sq Or too=sq Then 'piece and colour (a like capture)
'draw dark square
  If d Then
   tc=RGB(myrtle)
'draw light square
  Else
   tc=RGB(Green)
  EndIf
'calculate piece colour
  If H(sq)=1 Then
    pc=RGB(Red)
  Else
    pc=RGB(White)
  EndIf
'Calculate piece file to open
  pp$=fi$+pcs$(P(sq))+ex$
  Open pp$ For input As #2: tl$=Input$(200,#2):Close #2
' draw icon on enlarged tile
' make the tiles virtually bigger
  px=240+x*48:py=y*48+12
  Box px,py,48,48,1,RGB(white),RGB(white)
' print the 40x40 tile in centre
  GUI bitmap 4+px,4+py,tl$,40,40
' display the chess board squares
  tx=x:ty=y
  t=tx*6+30:q=4*ty+1
  For dx=t To t+5
   TILE dx, q,tc,pc:Inc q
   TILE dx, q,tc,pc:Inc q
   TILE dx, q,tc,pc:Inc q
   TILE dx, q,tc,pc:Inc q,-3
  Next
  Pl(sq)=P(sq)+8*H(sq)

  If sq=from Or sq=too Then Box x*48+240,y*48+12,48,48,2,RGB(Black)
EndIf
  If x<>7 Then d=Not d
Next 'i
froml=from:tool=too

' board legend increase font size temporarily
' only print once at startup
If hply=0 Then
 Font 1
 For i=1 To 8
  d=i:If flipped Then d=9-i
  Text 625,i*48-16,Str$(9-d)
  Text 210+i*48,0,Chr$(64+d)
 Next 'i
 Font 7
EndIf
'Print
End Sub

' MAIN
Sub main
Local computer_side,m,s$,selfplay
CLS
Text 0,440, "'help' lists commands. TSCP Chess By Tom Kerrigan. Converted MMBasic Ceptimus, Et al."

init_hash
init_board
'moves_template
open_book
gen
computer_side=MT
max_time=6e4' 1min
max_depth=12
If Not Instr(MM.Info(lcdpanel),"MODE")) Then FRAMEBUFFER copy f,n
ClockReset 'White's clock runs

Do
  ' auto: the computer takes whichever side is to move, until a key
  If selfplay Then
    computer_side=side
    If Inkey$<>"" Then selfplay=0:computer_side=MT
  EndIf
  If side=computer_side Then
    think(1)
    If pv(0,0)=0 Then
      oPrintR "(no legal moves)"
      oPrintR ""
      computer_side=MT
      selfplay=0
      print_result
    Else
      oPrintR "Computer's move: "+UCASE$(move_str$(pv(0,0)))
      oPrintR""
      m=makemove(pv(0,0))
      ply=0
      ClockMove 'the other side's clock runs
      gen
      print_result
      If selfplay Then
        draw_chessboard ' no tscp> prompt comes to draw it
        ' a draw print_result reported ends auto
        If (reps()=2)Or(fifty>=100) Then selfplay=0:computer_side=MT
      EndIf
    EndIf
  Else
    s$=oInput$("tscp> ")
    If s$="on" Then
      computer_side=side
    ElseIf s$="off" Then
      computer_side=MT
    ElseIf s$="auto" Then
      selfplay=1
    ElseIf (LEFT$(s$,2)="st")And(Len(s$)>2) Then
      max_time=1000*Val(Mid$(s$,3))
      max_depth=16'32
    ElseIf (LEFT$(s$,2)="sd")And(Len(s$)>2) Then
      max_depth=Val(Mid$(s$,3))
      If max_depth<1 Then max_depth=1
      If max_Depth>MAX_PLY Then max_depth=MAX_PLY+1
      max_time=72e5'2 hours '2^24
    ElseIf s$="undo" Then
      If hply Then
        computer_side=MT
        takeback
        ply=0
        ClockMove
        gen
      EndIf
    ElseIf s$="new" Then
      computer_side=MT
      init_board
      ClockReset
      gen
'      ElseIf s$="d" Then
'        print_board
    ElseIf s$="v" Then
      flipped=Not flipped
    ElseIf s$="bench" Then
      computer_side=MT
      bench
    ElseIf s$="bye" Then
      oPrintR "Share and enjoy!"
      Exit
    ElseIf s$="help" Then
      oPrintR "on - computer plays"
      oPrintR "     for the side to move"
      oPrintR "auto - computer plays both sides"
      oPrintR "       until a key is pressed"
      oPrintR "off - computer stops playing"
      oPrintR "st n - search for n secs per move"
      oPrintR "sd n - search n (1-16) ply per move"
      oPrintR "undo - takes back a move"
      oPrintR "new - starts a new game"
      oPrintR "v - view board from opposite side"
      oPrintR "bench - run the built-in benchmark"
      oPrintR "bye - exit the program"
      oPrintR "Enter moves in coordinate notation"
      oPrintR "               e.g., e2e4, e7e8Q"
    Else
      m=parse_move(s$)
      If m=-1 Then
        oPrintR "Illegal move."
        oPrintR ""
      ElseIf makemove(gd(m) And &HFFFFFFFF)=0 Then
        oPrintR "Illegal move."
        oPrintR ""
      Else
        ply=0
        ClockMove 'the other side's clock runs
        gen
        print_result
        draw_chessboard
      EndIf
    EndIf
  EndIf
  If Not Instr(MM.Info(lcdpanel),"MODE")) Then FRAMEBUFFER copy f,n
Loop
close_book
End Sub

Function parse_move(vs$)
Local from,too,i,a(4),s$

parse_move=-1 'default to 'illegal move'
s$=LCASE$(vs$)
If Len(s$)<4 Then Exit Function
For i=0 To 3:a(i)=Asc(Mid$(s$,i+1)):Next 'i
If (a(0)<Asc("a"))Or(a(0)>Asc("h"))Then Exit Function
If (a(1)<Asc("1"))Or(a(1)>Asc("8"))Then Exit Function
If (a(2)<Asc("a"))Or(a(2)>Asc("h"))Then Exit Function
If (a(3)<Asc("1"))Or(a(3)>Asc("8"))Then Exit Function
a(4)=Asc("q")
If Len(s$)>4 Then a(4)=Asc(Mid$(s$,5))

from=a(0)-Asc("a")
from=from+8*(8-(a(1)-Asc("0")))
too=a(2)-Asc("a")
too=too+8*(8-(a(3)-Asc("0")))

For i=0 To first_move(1)-1
'  If (Peek(var gd(i,0),0)=from)And(Peek(var gd(i,0),1)=too)Then
   If (gd(i)And &HFF)=from And (gd(i)>>8 And &HFF)=too Then
'    If (Peek(var gd(i,0),3)And 32) Then
    If gd(i)And &H20000000 Then
      Select Case a(4)
        Case Asc("n")
          parse_move=i:Exit For
        Case Asc("b")
          parse_move=i+1:Exit For
        Case Asc("r")
          parse_move=i+2:Exit For
        Case Else 'assume it's a queen
          parse_move=i+3:Exit For
      End Select
    EndIf
    parse_move=i:Exit For
  EndIf
Next i
End Function

Function move_str$(m)
'Local from=Peek(var m,0),too=Peek(var m,1)
Local from=m And &HFF, too=m>>8 And &HFF

move_str$=Chr$((from And 7)+Asc("a"))
move_str$=move_str$+Chr$(8-(from\8)+Asc("0"))
move_str$=move_str$+Chr$((too And 7)+Asc("a"))
move_str$=move_str$+Chr$(8-(too\8)+Asc("0"))

'If (Peek(var m,3)And 32) Then
If (m And &H20000000) Then
  Select Case Peek(var m,2)
    Case KT
      move_str$=move_str$+"N"
    Case BISHOP
      move_str$=move_str$+"B"
    Case ROOK
      move_str$=move_str$+"R"
    Case Else
      move_str$=move_str$+"Q"
  End Select
EndIf
End Function

'Sub print_board
'  Local i
'
'  oPrintR "" :oPrint "8 "
'  For i=0 To 63
'    Select Case H(i)
'      Case MT
'        oPrint " ."
'      Case LI
'        oPrint " "+Mid$(piece_char$,P(i)+1,1)
'      Case DK
'        oPrint " "+Mid$(piece_char$,P(i)+7,1)
'    End Select
'    If ((i+1)Mod 8=0)And(i<>63) Then oPrintR "" :oPrint Str$(7-(i\8))+" "
'  Next 'i
'  oPrintR "" :oPrintR "" :oPrintR "   a b c d e f g h":oPrintR ""
'End Sub

Sub print_result
Local i

For i=0 To first_move(1)-1
  If makemove(gd(i) And &HFFFFFFFF) Then takeback:Exit For
Next 'i
If i=first_move(1)) Then
  ClockStop
  If in_check(side) Then
    If side=LI Then
      oPrintR "0-1 {Black mates}":oPrintR ""
    Else
      oPrintR "1-0 {White mates}":oPrintR ""
    EndIf
  Else
    oPrintR "1/2-1/2 {Stalemate}":oPrintR ""
  EndIf
ElseIf reps()=2 Then
  ClockStop
  oPrintR "1/2-1/2 {Draw by repetition}":oPrintR ""
ElseIf fifty>=100 Then
  ClockStop
  oPrintR "1/2-1/2 {Draw by fifty move rule}":oPrintR ""
  If Not Instr(MM.Info(lcdpanel),"MODE")) Then FRAMEBUFFER copy f,n
EndIf

End Sub

data_bench_color:
Data 6, 1, 1, 6, 6, 1, 1, 6
Data 1, 6, 6, 6, 6, 1, 1, 1
Data 6, 1, 6, 1, 1, 6, 1, 6
Data 6, 6, 6, 1, 6, 6, 0, 6
Data 6, 6, 1, 0, 6, 6, 6, 6
Data 6, 6, 0, 6, 6, 6, 0, 6
Data 0, 0, 0, 6, 6, 0, 0, 0
Data 0, 6, 0, 6, 0, 6, 0, 6

data_bench_piece:
Data 6, 3, 2, 6, 6, 3, 5, 6
Data 0, 6, 6, 6, 6, 0, 0, 0
Data 6, 0, 6, 4, 0, 6, 1, 6
Data 6, 6, 6, 1, 6, 6, 1, 6
Data 6, 6, 0, 0, 6, 6, 6, 6
Data 6, 6, 0, 6, 6, 6, 0, 6
Data 0, 0, 4, 6, 6, 0, 2, 0
Data 3, 6, 2, 6, 3, 6, 5, 6

Sub bench
Local i,t
Local float nps

close_book

Restore data_bench_color:For i=0 To 63:Read H(i):Next 'i
Restore data_bench_piece:For i=0 To 63:Read P(i):Next 'i

side=LI
xside=DK
castle=0
ep=-1
fifty=0
ply=0
hply=0
set_hash
draw_chessboard
'  print_board
max_time=300e5 '60 min
max_depth=5
think(1)
t=Timer-start_time
oPrintR "Time: "+Str$(t)+" ms"
oPrintR "Nodes: "+Str$(nodes)

If t=0 Then
  oPrintR "(invalid)"
Else
  nps=nodes
  nps=nps*1000.0/t
  ' Score: 1.000 = my Athlon XP 2000+
  oPrint "Nodes per second: "+Str$(Int(nps+0.5))
  oPrintR " (Score: "+Str$(nps/243169.0,0,6)+")"
oPrintR ""
EndIf

init_board
max_time=6e4'1min
max_depth=12
open_book
ClockReset
gen
End Sub

' SEARCH
Sub think(output)
Local i,j,x
Local move$=""

pv(0,0)=book_move():If pv(0,0)<>-1 Then Exit Sub

stop_search=0
start_time=Timer
stop_time=start_time+max_time

ply=0
nodes=0
For i=0 To MAX_PLY
  For j=0 To MAX_PLY
    pv(i,j)=0
  Next 'j
 Next 'i
For i=0 To 63
  For j=0 To 63
    hi(i,j)=0
  Next 'j
Next 'i

If output=1 Then oPrint "ply  nodes  score   PV"
For i=1 To max_depth
  follow_pv=1
  x=search(-10000,10000,i)
  If stop_search Then Exit For
  If output=1 Then
   For j = 0 To pvl(0)-1
    move$=move$+" "+move_str$(pv(0, j))
   Next 'j
   move$=UCASE$(move$)
   oPrintR ""
   oPrint Str$(i, 3)+Str$(nodes, 7)+Str$(x, 7)+"  "+move$
'   oPrintR ""
   move$=""
   EndIf
  If x>9000 Or x<-9000 Then i=max_depth
Next i
If stop_search Then
  Do
    If ply = 0 Then Exit
    takeback()
  Loop
EndIf
End Sub

/*
' --- mmb2csub: search replaced by a CSUB; original follows
Function search(a,b,d)
' search() and quiesce() as one loop.  A recursive call per ply took
' about 24 BASIC call levels at MAX_PLY, more than the stack holds.  Here
' each ply keeps alpha, beta, depth and its move index in the ss_ arrays
' while its children are searched, and the call depth stays the same
' however deep the search goes.  depth=0 marks a quiesce node.
Local alpha=a,beta=b,depth=d,i,j,x,c,f,from,too,mv,done,v,ply0=ply

search=alpha
If stop_search Then Exit Function
Do
  ' enter the node at this ply
  Inc nodes,1
  If (nodes And 127)=0 Then
    If Timer>stop_time Then stop_search=1:Exit Function
    Thread Yield 'the program's other threads have their turns (compiled: CSubYield)
  EndIf
  pvl(ply)=ply
  done=0
  If depth Then
    If ply Then
      If reps() Then v=0:done=1
    EndIf
  EndIf
  If done=0 Then
    If (ply>=MAX_PLY-1)Or(hply>=HIST_STACK-1) Then
      v=eval_():done=1
    ElseIf depth Then
      c=in_check(side):If c Then Inc depth,1
      gen()
    Else
      x=eval_()
      If x>=beta Then
        v=beta:done=1
      Else
        If x>alpha Then alpha=x
        gen_caps()
      EndIf
    EndIf
    If done=0 Then
      If follow_pv Then sort_pv()
      f=0:i=first_move(ply)
    EndIf
  EndIf
  Do
    If done=0 Then
      ' the first legal move from i on is searched as a child
      mv=0
      Do While i<first_move(ply+1)
        tscpSort(i)
        If makemove(gd(i) And &HFFFFFFFF) Then mv=1:Exit Do
        Inc i,1
      Loop
      If mv Then
        ss_alpha(ply-1)=alpha:ss_beta(ply-1)=beta
        ss_depth(ply-1)=depth:ss_move(ply-1)=i
        x=alpha:alpha=-beta:beta=-x
        If depth Then Inc depth,-1
        Exit Do
      EndIf
      ' no moves left: the node's value
      If depth=0 Then
        v=alpha
      ElseIf f=0 Then
        v=0:If c Then v=ply-10000
      ElseIf fifty>=100 Then
        v=0
      Else
        v=alpha
      EndIf
    EndIf
    ' hand v back to the parent
    If ply=ply0 Then search=v:Exit Function
    takeback()
    x=-v
    alpha=ss_alpha(ply):beta=ss_beta(ply)
    depth=ss_depth(ply):i=ss_move(ply):f=1
    done=0
    If x>alpha Then
      If depth Then
        from=gd(i) And &HFF'Peek(var gd(i,0),0)
        too=gd(i)>>8 And &HFF'Peek(var gd(i,0),1)
        Inc hi(from,too),depth
      EndIf
      If x>=beta Then
        v=beta:done=1
      Else
        alpha=x
        pv(ply,ply)=gd(i) And &HFFFFFFFF
        For j=ply+1 To pvl(ply+1)-1
          pv(ply,j)=pv(ply+1,j)
        Next 'j
        pvl(ply)=pvl(ply+1)
      EndIf
    EndIf
    If done=0 Then Inc i,1
  Loop
Loop
End Function
*/

' CHESS CLOCK
' clk!(C_W) and clk!(C_B) are each side's time booked so far, in ms (C_W and C_B are LI and DK, defined at the top
' with the thread); clk_side's clock is running from clk_t0!. The main program books at each move (ClockMove), so
' the totals are exact whatever the display is doing; ClockThread shows them, the running side's live, five times a
' second, each refresh in one go (THREAD HOLD). It runs whenever the main program waits at its prompt, and while the
' computer thinks the library's search lets it run every 128 nodes (THREAD YIELD in search).
Sub ClockReset
Thread Hold
clk!(C_W)=0:clk!(C_B)=0:clk_last!(C_W)=0:clk_last!(C_B)=0
clk_side=C_W:clk_t0!=Timer:clk_run=1:clk_redraw=1
Thread Release
End Sub

Sub ClockMove 'the side to move is now side: book the time to the side that moved
Local float dt
Thread Hold
If clk_run Then
  dt=Timer-clk_t0!
  Inc clk!(clk_side),dt:clk_last!(clk_side)=dt
EndIf
clk_side=side:clk_t0!=Timer
Thread Release
End Sub

Sub ClockStop 'the game is over: both clocks stop
Thread Hold
If clk_run Then Inc clk!(clk_side),Timer-clk_t0!
clk_run=0
Thread Release
End Sub

Function HMS$(ms!)
Local s=Int(ms!/1000)
HMS$=Str$(s\3600)+":"+Str$((s\60) Mod 60,2,0,"0")+":"+Str$(s Mod 60,2,0,"0")
End Function

Sub ClockThread
Local float tw,tb,mw,mb,dt
Local s,going,y=312
Local w1$ Length 20,w2$ Length 20,b1$ Length 20,b2$ Length 20
Local o1$ Length 20,o2$ Length 20,o3$ Length 20,o4$ Length 20
Do
  'the whole refresh in one go: while the computer thinks, the search lets this thread run every 128 nodes, but for
  'one time slice (a millisecond) at a time, so without the hold a refresh took up to half a second
  Thread Hold
  tw=clk!(C_W):tb=clk!(C_B):mw=clk_last!(C_W):mb=clk_last!(C_B):s=clk_side:going=clk_run
  If clk_redraw Then 'after the program's CLS (ClockReset): the colours and every field again
    clk_redraw=0:o1$="":o2$="":o3$="":o4$=""
    If vga Then TILE 0,26,RGB(white),RGB(black),15,2:TILE 15,26,RGB(red),RGB(black),15,2
  EndIf
  dt=0:If going Then dt=Timer-clk_t0!
  If going Then 'the running side's time, live
    If s=C_W Then
      Inc tw,dt:mw=dt
    Else
      Inc tb,dt:mb=dt
    EndIf
  EndIf
  w1$="WHITE "+HMS$(tw):b1$="BLACK "+HMS$(tb):w2$=" move "+HMS$(mw):b2$=" move "+HMS$(mb)
  'only a field whose figures have changed is drawn: drawing the stopped clock again made it flash
  If w1$<>o1$ Then ClockText 0,y,w1$,RGB(white):o1$=w1$
  If b1$<>o2$ Then ClockText 120,y,b1$,RGB(red):o2$=b1$
  If w2$<>o3$ Then ClockText 0,y+12,w2$,RGB(white):o3$=w2$
  If b2$<>o4$ Then ClockText 120,y+12,b2$,RGB(red):o4$=b2$
  Thread Release
  Pause 200
Loop
End Sub

Sub ClockText(x,y,a$,fc) 'one field of the clock, in its side's colour
If vga Then 'MODE 1: the pixels only, the tiles give the colours
  Text x,y,a$+"  ","LT",1,1,RGB(white),RGB(black)
Else 'a panel: the colour itself, in both F (which the program copies to N) and N
  FRAMEBUFFER write f:Text x,y,a$+"  ","LT",1,1,fc,RGB(black)
  FRAMEBUFFER write n:Text x,y,a$+"  ","LT",1,1,fc,RGB(black)
EndIf
End Sub

Function reps()
Local i
reps=0
For i=hply-fifty To hply-1
  If hd(i,1)=hash Then Inc reps'1
Next 'i
End Function

Sub sort_pv
Local i
follow_pv=0
For i=first_move(ply) To first_move(ply+1)-1
  If (gd(i) And &HFFFFFFFF)=pv(0,ply) Then
    follow_pv=1
    Inc gd(i),10000000<<32
    i=first_move(ply+1)-1
  EndIf
Next 'i
End Sub

Sub tscpSort(from)
Local g
Local i,bs,bi
bs=-1
bi=from

For i=from To first_move(ply+1)-1
  If (gd(i)>>32)>bs Then bs=gd(i)>>32:bi=i
Next 'i
g=gd(from):gd(from)=gd(bi):gd(bi)=g
End Sub

' globals: Dim'd here at program level rather than inside init_arrays,
' so mmb2csub sees them as globals
Dim H(63) 'Hue
Dim P(63) 'Piece
Dim side
Dim xside
Dim castle
Dim ep
Dim fifty
Dim hash
Dim ply
Dim hply
Dim gd(GEN_STACK) 'Gen Dat: the move in the low 32 bits, its score in the high 32
Dim first_move(MAX_PLY)
Dim hi(63,63) 'History
Dim hd(HIST_STACK, 1) 'History Data: (n,0) the move, the captured piece<<32, castle<<36,
'  (ep+1)<<40 and fifty<<48; (n,1) the hash (48 bits)
Dim max_time,max_depth
Dim start_time,stop_time
Dim nodes
Dim pv(MAX_PLY, MAX_PLY)
Dim pvl(MAX_PLY) 'Pv Length
Dim ss_alpha(MAX_PLY),ss_beta(MAX_PLY),ss_depth(MAX_PLY),ss_move(MAX_PLY) 'search stack
Dim follow_pv
Dim HP(1,5,63) 'Hash Piece
Dim hash_side
Dim hash_ep(63)
Dim mb(119)
Dim mb64(63)
Dim slide(5)=(0,0,1,1,1,0)
Dim offsets(5)=(0,8,4,4,8,8)
Dim offset(5,7)
Dim castle_mask(63)
Dim piece_char$="PNBRQKpnbrqk"
Dim stop_search=0
Dim flipped=0
Dim flippedl=0
Dim froml
Dim tool
Dim move
Dim tmpl
Dim Pl(63)
Math Set -1,Pl()
init_arrays 'initialize those arrays that are too big for a one-line statement
'load_sprites
'Dim km1, km2, km3
main 'start tscp's main loop
