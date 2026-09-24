' bench_heap.bas - Phase 1 H9: does heap occupancy slow allocation?
' Temporaries (string results, function arguments) come from GetSystemMemory,
' which searches the SRAM heap BOTTOM-UP; variables, LOCAL strings and LOCAL
' arrays come from GetMemory, which searches it TOP-DOWN (core/Memory.c).
' So the same statements are timed with the SRAM heap:
'   A  as the program leaves it (nearly empty)
'   B  top 80% occupied (big arrays DIMmed first, the usual real-world case)
'   C  bottom 80% occupied (only possible after the heap has been full)
' On the PC3 the fill is kept in SRAM: chunks stay under half the heap
' (bigger requests go straight to PSRAM) and SRAM free is taken as
' MM.INFO(HEAP) less the PSRAM heap, which nothing here uses.
Option default float
Const N% = 10000
Const CH% = 6000
Dim a%, b%, i%, cnt%, fx, fy, t0, mhz, sf, ps, rest
Dim t$, u$, st$
mhz = Val(MM.Info(CPUSPEED)) / 1000000
t$ = "Hello world" : u$ = "abc" : fx = 1.25 : b% = 7
ps = 0
If Instr(MM.Device$, "RP2350") Then ps = MM.Info(PSRAM SIZE)
Print "BENCHSTART "; MM.Device$; " "; MM.Ver; " "; mhz; " MHz"
sf = MM.Info(HEAP) - ps
Print "BENCHINFO sram_free_at_start "; sf; " psram_heap "; ps

RunSet "A_empty"

' ---- B: top 80% of SRAM, in chunks of at most 48 KB
rest = sf * 0.8
cnt% = Min(rest \ 8, CH%) : Dim B1%(cnt% - 1) : rest = rest - cnt% * 8
cnt% = Max(Min(rest \ 8, CH%), 1) : Dim B2%(cnt% - 1) : rest = rest - cnt% * 8
cnt% = Max(Min(rest \ 8, CH%), 1) : Dim B3%(cnt% - 1) : rest = rest - cnt% * 8
cnt% = Max(Min(rest \ 8, CH%), 1) : Dim B4%(cnt% - 1) : rest = rest - cnt% * 8
Print "BENCHINFO B sram_free "; MM.Info(HEAP) - ps; " of "; sf
RunSet "B_top80"
Erase B1%, B2%, B3%, B4%

' ---- C: bottom 80%. The top 20% is taken first, the rest filled down to
' the last page, then the top 20% is released. Nothing between the exact
' fill and the ERASE may allocate (no PRINT, no string work).
cnt% = (sf * 0.2) \ 8 : Dim X%(cnt% - 1)
rest = MM.Info(HEAP) - ps - 2048
cnt% = Max(Min(rest \ 8, CH%), 1) : Dim C1%(cnt% - 1) : rest = rest - cnt% * 8
cnt% = Max(Min(rest \ 8, CH%), 1) : Dim C2%(cnt% - 1) : rest = rest - cnt% * 8
cnt% = Max(Min(rest \ 8, CH%), 1) : Dim C3%(cnt% - 1) : rest = rest - cnt% * 8
cnt% = (MM.Info(HEAP) - ps) \ 8
Dim C4%(cnt% - 1)
Erase X%
Print "BENCHINFO C sram_free "; MM.Info(HEAP) - ps; " of "; sf
RunSet "C_bottom80"
Erase C1%, C2%, C3%, C4%
Print "BENCHEND"
End

Sub RunSet lbl$
  Local integer li
  Local lbase, lt
  lt = Timer
  For li = 1 To N%
  Next
  lbase = (Timer - lt) * 1000
  lt = Timer
  For li = 1 To N%
    a% = b%
  Next
  Emit lbl$ + " a%=b% (no allocation)", Timer - lt, lbase
  lt = Timer
  For li = 1 To N%
    st$ = t$ + u$
  Next
  Emit lbl$ + " st$=t$+u$ (temp, bottom-up)", Timer - lt, lbase
  lt = Timer
  For li = 1 To N%
    st$ = Mid$(t$, 3, 4)
  Next
  Emit lbl$ + " st$=MID$(t$,3,4) (temp, bottom-up)", Timer - lt, lbase
  lt = Timer
  For li = 1 To N%
    st$ = Str$(fx, 0, 3)
  Next
  Emit lbl$ + " st$=STR$(fx,0,3) (temp, bottom-up)", Timer - lt, lbase
  lt = Timer
  For li = 1 To N%
    fy = Sin(fx)
  Next
  Emit lbl$ + " fy=SIN(fx) (argument temp)", Timer - lt, lbase
  lt = Timer
  For li = 1 To N%
    HLocStr
  Next
  Emit lbl$ + " call SUB with LOCAL s$ (top-down)", Timer - lt, lbase
  lt = Timer
  For li = 1 To N%
    HLocArr
  Next
  Emit lbl$ + " call SUB with LOCAL la(3) (top-down)", Timer - lt, lbase
  lt = Timer
  For li = 1 To N%
    HLocNone
  Next
  Emit lbl$ + " call SUB with no LOCALs (control)", Timer - lt, lbase
End Sub

Sub Emit lbl$, tms, bb
  Local us
  us = (tms * 1000 - bb) / N%
  Print "BENCH "; lbl$; " "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
End Sub

Sub HLocStr
  Local s$
End Sub

Sub HLocArr
  Local la(3)
End Sub

Sub HLocNone
End Sub
