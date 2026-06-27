' bubble_s.bas - single-precision version of bubble.bas (uses the new sfloat
' CallTable routines: SAdd/SMul/SSin + DtoS/StoD/StoI). Identical density to
' bubble.bas so you can compare the per-frame Timer directly.
'
' REQUIRES firmware rebuilt with the single-precision CallTable entries (0x104+).
'
' Regenerate the CSUB block with:
'   python armcfgen.py bubble_s.c --compile -n bubblerow -e bubblerow -O s -I <firmware dir>

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
  bubblerow c(),d(),pf()                     ' single-precision recurrence (in C)
  Memory unpack Peek(varaddr m(0,g)),nn,66,32
  Pixel c(),d(),n()
  Inc g
 Next 'i
 FRAMEBUFFER copy f,n
Loop

CSUB bubblerow INTEGER, INTEGER, FLOAT
  00000000
  4C69B5F0 6823B08F 69DB900C 33FC910D 68516810 00176B1B 68234798 69DB9006
  68F968B8 6B1B33FC 68234798 69DB9007 69796938 6B1B33FC 68234798 69DB1C06
  69F969B8 6B1B33FC 68234798 69DB9002 6A796A38 6B1B33FC 68234798 69DB9003
  6AF96AB8 6B1B33FC 68234798 69DB9008 6B796B38 6B1B33FC 68234798 69DB9009
  6BF96BB8 6B1B33FC 68234798 69DB900A 6C796C38 6B1B33FC 23004798 9301900B
  1C316823 980669DB 689B33FC 68234798 69DB1C05 69DB33FC 68234798 69DB9903
  33FC9004 1C2869DE 4798689B 682347B0 69DB9005 33FC9802 479869DB 99036823
  1C0569DB 69DE33FC 689B9802 47B04798 1C066823 1C2969DB 689B33FC 47989804
  1C056823 1C3169DB 689B33FC 47989805 99076823 1C0669DB 689B33FC 47981C28
  99086823 900269DB 6B9A33FC 92041C28 691B689A 47989205 9B05990A 9B044798
  9A014798 189B9B0C 60596018 99096823 1C3069DB 689A33FC 691B6B9D 47989204
  990B9B04 47A84798 9B0D9A01 6018189B 00136059 33082284 00929301 D18F4293
  1C306823 33FC69DB 47986B5B 61386823 69DB6179 33FC9802 47986B5B 61F961B8
  21002000 BDF0B00F E000ED08
END CSUB
