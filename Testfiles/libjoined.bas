' libfn.bas - part one of a three-file library: ordinary BASIC only.
' Loaded with libcsub.bas and libfont.bas by one LIBRARY LOAD.
Const LIB_MAGIC = 4242
Dim integer LibCalls
Function LibAdd(a As integer, b As integer) As integer
  LibAdd = a + b
  LibCalls = LibCalls + 1
End Function
Function LibName$()
  LibName$ = "three-file library"
End Function
Sub LibHello(what$)
  Print "  LibHello says: "; what$
End Sub
' libcsub.bas - part two: nothing but a CSUB.  The hex text is dropped as the
' file streams past; only the binary reaches the library.
CSUB CHECKSUM
	00000000
	'checksum_csub
	6804B5F0 68086845 B0896849 95059404 414D1824 41491800 60556014 25002400 
	91079006 2100469C 95039402 97019600 0EC24814 4313014B 01439301 9A009300 
	1A129B01 9C04418B 19129D05 9C06416B 19129D07 9D02416B 24009E03 41731952 
	18ED2301 00524166 95022100 08509603 D1DE2D10 2B009B03 4663D1DB 60596018 
	BDF0B009 00001234 
End CSUB
' libfont.bas - part three: nothing but a font.  A font's address word is its
' number, so it needs no fixing up afterwards - unlike a CSUB.
DefineFont #8
	08200808 00000000 00000000 18181818 00180018 00246666 00000000 247E2424 
	0024247E 3C583E18 00187C1A 10086462 00864620 76386C38 0076CCDC 00301818 
	00000000 
End DefineFont
