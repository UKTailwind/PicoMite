' pshint.bas - the PSRAM heap's search hint (Memory.c GetPSMemory).
' Fills the SRAM heap with arrays under half its size (so they stay in SRAM),
' then makes, erases and remakes arrays big enough to go to PSRAM, printing
' where each lands: the addresses must be the same with and without the hint.
' Last, times temporary strings, which come from PSRAM once SRAM is full.
Option Default Integer
Dim i, t, n
Dim p1(20000), p2(15000), p3(12000)
Dim a1(Min(8000, MM.Info(HEAP) \ 8 - 64))
Dim a2(Min(8000, MM.Info(HEAP) \ 8 - 64))
Dim a3(Min(8000, MM.Info(HEAP) \ 8 - 64))
Dim a4(Min(8000, MM.Info(HEAP) \ 8 - 64))
Print "SRAM free"; MM.Info(HEAP)
Print "P "; Hex$(Peek(VarAddr p1())); " "; Hex$(Peek(VarAddr p2())); " "; Hex$(Peek(VarAddr p3()))
Erase p2
Dim p4(5000), p5(9000)
Print "P "; Hex$(Peek(VarAddr p4())); " "; Hex$(Peek(VarAddr p5()))
Erase p1
Dim p6(30000)
Dim p7(19000)
Print "P "; Hex$(Peek(VarAddr p6())); " "; Hex$(Peek(VarAddr p7()))
Dim s$
t = Timer
For i = 1 To 20000
  s$ = Str$(i) + "x" + Str$(i * 3)
  n = n + Len(s$)
Next
Print "N"; n
Print "TIME "; Timer - t
Erase p3, p4, p5, p6, p7
Dim p8(40000)
Print "P "; Hex$(Peek(VarAddr p8()))
Print "PSDONE"
