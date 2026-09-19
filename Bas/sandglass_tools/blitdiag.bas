' blitdiag.bas - work out how a sheet written by sheets.py actually lands in an
' image slot.  Reads back a grid of pixels from a blitted patch and prints the
' raw values, so the mapping can be deduced on the host rather than guessed.
'
' Expected colour of sheet pixel (x,y) as written: ((x>>3) + (y>>3)) AND 15

Option Console Serial
Option Explicit
Option Default None

Const SLOT = 4

Dim INTEGER ref(15), c, i, j, x, y
Dim STRING home$, row$

home$ = MM.Info(Path)
If home$ = "NONE" Then home$ = "A:/"

Mode 2
CLS

Flash Load Image SLOT, home$ + "testshet.bmp", O

' How Pixel() reports each of the sixteen indices.
For c = 0 To 15
  Pixel 8 + c, 2, c
Next c
row$ = "ref"
For c = 0 To 15
  row$ = row$ + " " + Str$(Pixel(8 + c, 2))
Next c
Print row$

' Blit a 128 x 128 patch from the sheet origin and read a grid back.
Blit Flash SLOT, N, 0, 0, 0, 100, 128, 128

Print "grid sx sy value  (patch drawn at display 0,100)"
For j = 0 To 120 Step 24
  row$ = "row " + Str$(j) + ":"
  For i = 0 To 120 Step 24
    row$ = row$ + " " + Str$(Pixel(i, 100 + j))
  Next i
  Print row$
Next j

Print "sheet dims "; Str$(MM.Info(Flash Address SLOT))
Option Console Both
End
