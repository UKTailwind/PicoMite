' dotest.bas - DO WHILE/UNTIL and LOOP WHILE/UNTIL conditions of the form the
' DO fast path takes ("var OP number" / "number OP var"). Every result must
' equal a build without the fast path. Each line prints the pass count.
Dim i%, x!, g%, k%, c%
c% = 0 : i% = 0 : Do While i% < 10 : Inc i% : Inc c% : Loop : Print "A"; c%
c% = 0 : i% = 0 : Do Until i% >= 10 : Inc i% : Inc c% : Loop : Print "B"; c%
c% = 0 : i% = 0 : Do : Inc i% : Inc c% : Loop While i% < 2.5 : Print "C"; c%; i%
c% = 0 : i% = 0 : Do : Inc i% : Inc c% : Loop Until i% > 2.5 : Print "D"; c%; i%
c% = 0 : x! = 0 : Do While x! < 3 : x! = x! + 0.5 : Inc c% : Loop : Print "E"; c%; x!
c% = 0 : i% = 0 : Do While 10 > i% : Inc i% : Inc c% : Loop : Print "F"; c%
c% = 0 : i% = 0 : Do While i% <> 7 : Inc i% : Inc c% : Loop : Print "G"; c%
c% = 0 : i% = 5 : Do While i% = 5 : Inc i% : Inc c% : Loop : Print "H"; c%
c% = 0 : i% = 0 : Do While i% <= &HF : Inc i% : Inc c% : Loop : Print "I"; c%
c% = 0 : x! = 0 : Do While x! < 1E1 : Inc x! : Inc c% : Loop : Print "J"; c%
c% = 0 : x! = 0 : Do While 2.5 >= x! : x! = x! + 0.5 : Inc c% : Loop : Print "K"; c%
c% = 0 : Do While nv < 3 : Inc nv : Inc c% : Loop : Print "L"; c%
c% = 0 : g% = 0 : Do While g% < 5 : Inc g% : Inc c% : If c% = 3 Then Erase g% : Dim g% = 1
Loop : Print "M"; c%
LocLoop
k% = 0 : ByRefLoop k% : Print "O"; k%
c% = 0 : i% = 0 : Do : Inc i% : Inc c% : If c% > 20 Then Exit Do
Loop While i% <> 4 : Print "P"; c%
End

Sub LocLoop
  Local j% = 0, t% = 0
  Do While j% < 4
    Inc j%
    Inc t%
  Loop
  Print "N"; t%
End Sub

Sub ByRefLoop v%
  Do While v% < 6
    Inc v%
  Loop
End Sub
