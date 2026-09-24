' calls_pcs.bas - Phase 1 H3: sample where the time of a SUB call goes.
' One loop of calls to a SUB with 6 AS INTEGER parameters and 4 LOCAL
' scalars (the costliest ordinary shape in bench_micro2), then one of a SUB
' with 7 LOCAL arrays; the PC sampler runs over both. pcs_report.py splits
' the samples into call-site argument evaluation, the definition-side parse
' (makeargs over the SUB line, BYVAL/AS checks), local creation and zeroing,
' and the return.
Option default float
Const N% = 30000
Dim a%, b%, c%, d%, e%, f%, i%, t0
a% = 1 : b% = 2 : c% = 3 : d% = 4 : e% = 5 : f% = 6
Timer = 0
Option profiling on, sample, 8192
For i% = 1 To N%
  S6T a%, b%, c%, d%, e%, f%
Next
Print "ELAPSED "; Timer
End

Sub S6T p1 As integer, p2 As integer, p3 As integer, p4 As integer, p5 As integer, p6 As integer
  Local integer l1, l2, l3, l4
  l1 = p1
End Sub

Sub SeLoc
  Local rsun(3), rmoon(3)
  Local rtmoon(3)
  Local rm2s(3)
  Local uaxis(3)
  Local um2o(3)
  Local dvec(5)
  Local jdtdb, jdutc
  rsun(1) = 1 : dvec(1) = 1
End Sub
