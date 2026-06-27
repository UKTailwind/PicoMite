' bubble.bas - "bubble universe" demo driven by the bubblerow CSUB (bubble.c).
' The CSUB does the sin/cos recurrence + scale + offset for one row (66 points),
' returning integer screen coordinates in c() and d().
'
' Regenerate the CSUB block below with:
'   python armcfgen.py bubble.c --compile -n bubblerow -e bubblerow -O s -I <firmware dir>

Font 7
FRAMEBUFFER create
FRAMEBUFFER write f

Dim Float t,pf(15)              ' pf: 0=i 1=b 2=v 3=x 4=pi/2 5=xs 6=ys 7=xc 8=yc
Dim Integer c(65),d(65),n(65),m(32,195),nn   ' m: 196 palette rows (one per i step)
Dim Integer a,g,i,j,xc,yc,xs,ys
Const r=(2*Pi)/235,k=255,s=50
CLS RGB(black)
t=Rnd*10
nn=Peek(varaddr n())

' centre and scale factor
xc=MM.HRES\2:yc=MM.VRES\2
xs=MM.HRES/4.2:ys=MM.VRES/4.0  ' Oval
pf(2)=0:pf(3)=0:pf(4)=Pi/2     ' v=0, x=0, half-pi
pf(5)=xs:pf(6)=ys:pf(7)=xc:pf(8)=yc

' build the colour palette, packed 32-bit into m()
For a=0 To 195
 For g=0 To 65
  If a<18 And g<18 Then
   n(g)=RGB(0,255,0)
  Else
   n(g)=RGB(a*1.3,g*3.93,128*(a+g<65))
  EndIf
 Next 'g
 Memory pack nn,Peek(varaddr m(0,a)),66,32
Next 'a

Do
 CLS
 Inc t,0.035:g=0:Print Timer:Timer=0
 For i=60 To 255 Step 1                       ' Step 1 = densest; raise to 2/3 for fewer points
  pf(0)=i:pf(1)=r*i+t
  bubblerow c(),d(),pf()                     ' recurrence + scale + offset (in C)
  Memory unpack Peek(varaddr m(0,g)),nn,66,32
  Pixel c(),d(),n()
  Inc g
 Next 'i
 FRAMEBUFFER copy f,n
Loop

CSUB bubblerow INTEGER, INTEGER, FLOAT
  00000000
  0014B5F0 68126853 920CB09B 68A2930D 920E68E3 6922930F 92026963 69A29303
  920469E3 6A229305 92066A63 6AA29307 92106AE3 6B229311 92126B63 6BA29313
  92146BE3 6C229315 92166C63 23009317 91199018 4D519301 990D980C 69DB682B
  681E33A4 9B039A02 682B47B0 69DB0006 3390000F 4798681B 9008682B 69DB9109
  001A0030 681B33A4 469C3290 46666812 92020039 9B079A06 9B0247B0 682B4798
  91039002 99059804 339069DB 4798681B 900A682B 9804910B 69DB9905 33A4001A
  68163290 9A06681F 47B89B07 682B47B0 91059004 99099808 33A469DB 9A0A681E
  47B09B0B 9008682B 98029109 69DB9903 681E33A4 9B059A04 682B47B0 91039002
  99099808 33A469DB 9A0E681E 47B09B0F 9004682B 98089105 69DB9909 3288001A
  920A6812 33A0001A 681632A4 9A10681F 47B89B11 9B159A14 9B0A47B0 9A014798
  189B9B18 60596018 99039802 69DB682B 33A0001A 68153288 681F69D6 9B139A12
  9A1647B8 47B09B17 9A0147A8 189B9B19 60596018 22840013 93013308 42930092
  E768D000 9A022000 21009B03 61636122 9B059A04 61E361A2 BDF0B01B E000ED08
END CSUB
