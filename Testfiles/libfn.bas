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
