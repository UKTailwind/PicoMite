' tmapset_p.bas - threads: MAP SET's wait for the frame's end (the PicoMite's version of tmapset.bas: MODE 2, the mode
' with a colour map that every VGA and HDMI build has, the RP2040's included, and MODE 1 back at the end; MODE 3 is
' tmapset3_p.bas). Each MAP SET takes one frame, with no thread and with two threads setting colours at once while a
' third is busy, and the busy thread runs meanwhile.
Option Explicit
Dim Integer tests, pass, busy, spins, i, n1
Dim Float t0, t1, one

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Spin
  Do While busy
    Inc spins
  Loop
End Sub

Sub Setter
  Local Integer k
  For k = 1 To 30
    Map(1) = RGB(k * 8, 0, 0)
    Map Set
    Inc n1
  Next
End Sub

Sub Try(m$)
  t0 = Timer
  For i = 1 To 30 : FrameBuffer Wait : Next
  one = (Timer - t0) / 30               ' a frame's time in this mode
  t0 = Timer
  For i = 1 To 30
    Map(2) = RGB(0, i * 8, 0)
    Map Set
  Next
  t1 = Timer - t0
  Check m$ + ": thirty MAP SETs in thirty frames, no thread", t1 > one * 29 And t1 < one * 31.5, 1
  busy = 1 : spins = 0 : n1 = 0
  Thread Start 2, Spin
  Thread Start 1, Setter
  t0 = Timer
  For i = 1 To 30
    Map(2) = RGB(0, 0, i * 8)
    Map Set
  Next
  t1 = Timer - t0
  Thread Wait 1
  busy = 0
  Thread Wait 2
  Check m$ + ": the main program's thirty in thirty frames while a thread sets colours too", t1 > one * 29 And t1 < one * 31.5, 1
  Check m$ + ": the thread's thirty", n1, 30
  Check m$ + ": the busy thread ran meanwhile", spins > 5000, 1
  Map Reset
End Sub

Mode 2
Try "MODE 2"
Mode 1
Print "tmapset:"; pass; " of"; tests; " passed"
