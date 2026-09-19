' MultiFileLibraryTest.bas - LIBRARY LOAD with several source files.
'
' One library assembled from three files, each exercising a different part of
' the loader: libfn.bas is ordinary BASIC, libcsub.bas is nothing but a CSUB
' (whose address has to be fixed up after the text is written) and
' libfont.bas is nothing but a font (whose address word is its number, so it
' is NOT fixed up).  Getting all three right at once is the point: the text
' and the binary records have to stay in the same order across the files or
' the CSUB is matched to the wrong record.
'
' Put libfn.bas, libcsub.bas, libfont.bas and libjoined.bas on A: first.
' RAM keeps this off the flash library; drop it to test the flash path.
LIBRARY LOAD "A:/libfn.bas", "A:/libcsub.bas", "A:/libfont.bas", RAM
Option EXPLICIT
Option DEFAULT NONE
Option CONSOLE SERIAL

Dim integer fails, tests, a, b, s, m, s2, m2

Print "multi-file LIBRARY LOAD on "; MM.DEVICE$; " "; Str$(MM.VER, 1, 6)

' ---- file 1: ordinary BASIC reached the library
Check "CONST from file 1", LIB_MAGIC, 4242
Check "FUNCTION from file 1", LibAdd(20, 22), 42
Check "the call counter it keeps", LibCalls, 1
If LibName$() = "three-file library" Then
  Print "ok:   string FUNCTION from file 1"
  tests = tests + 1
Else
  Print "FAIL: string FUNCTION from file 1 gave '" + LibName$() + "'"
  tests = tests + 1 : fails = fails + 1
EndIf
LibHello "the program"

' ---- file 2: the CSUB runs, and its address was fixed up correctly.
' A CSUB matched to the wrong binary record does not fail cleanly - it runs
' whatever code is there - so check two different inputs and that repeating
' one gives the same answer.
a = 1234 : b = 5678
CHECKSUM a, b, s, m
Print "  CHECKSUM 1234,5678 ->"; s; m
Check "CSUB result is not zero", (s <> 0) Or (m <> 0), 1
a = 1234 : b = 5678
CHECKSUM a, b, s2, m2
Check "CSUB repeats itself, s", s2, s
Check "CSUB repeats itself, m", m2, m
a = -99 : b = 7
CHECKSUM a, b, s2, m2
Print "  CHECKSUM -99,7    ->"; s2; m2
Check "different input, different answer", (s2 <> s) Or (m2 <> m), 1

' ---- file 3: the font is installed and has the right metrics
FONT 8
Check "font 8 width from file 3", MM.INFO(FONTWIDTH), 8
Check "font 8 height from file 3", MM.INFO(FONTHEIGHT), 8
FONT 1
Check "back to font 1 width", MM.INFO(FONTWIDTH), 8

Print "MULTI-FILE LIBRARY:" + Str$(tests - fails) + " of" + Str$(tests) + " checks passed"
If fails = 0 Then Print "PASS" Else Print "FAIL"
Option CONSOLE BOTH
End

Sub Check(what$, got As integer, want As integer)
  tests = tests + 1
  If got = want Then
    Print "ok:   " + what$ + " =" + Str$(got)
  Else
    Print "FAIL: " + what$ + " got" + Str$(got) + " want" + Str$(want)
    fails = fails + 1
  EndIf
End Sub
