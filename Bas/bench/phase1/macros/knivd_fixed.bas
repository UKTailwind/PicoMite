' knivd_fixed.bas - Phase 1 macro: KnivD's benchmark (Bas/knivd.bas) doing a
' FIXED amount of work, so it reaches END and can be timed and profiled.
' Bas/knivd.bas loops for ever, scoring what it gets through in 30 s.
' runit's body is the original's, statement for statement. Changes:
'  - runit stops after NOUTER% outer passes, not when Timer reaches 30000;
'    i rises by exactly 1 per pass, so "i < NOUTER%" is a pass count;
'  - RND is replaced by rt(t), a table of 100 fixed pseudo-random values in
'    [0,1): on the RP2350 RND reseeds from the hardware TRNG every 100 calls
'    and RANDOMIZE does nothing (core/Functions.c:1212, AllCommands.h:650),
'    so RND cannot be repeated there. On a host model (_knivd_sim.py) the
'    table keeps the statement mix: 75.2% of inner passes take the SIN/STR$
'    branch and 3.6% append to s; true RND gives 75.4% and 3.5%;
'  - the elapsed time, a checksum and BENCH lines are printed at the end;
'  - MODE 1 is set before timing, so the display load is the same whatever
'    ran before (MODE survives END, and julia and gfx leave MODE 2).
' First runs (pre-review file): PC3 HDMIUSB 378 MHz 28.2 s; RP2040 VGA
' 315 MHz 65.7 s.
Option explicit
Const PROFILE% = 0             ' 1 = run the PC sampler over the timed region
Const NOUTER% = 3000           ' outer passes; 100 inner passes each

Dim integer t, i=0,n=1,o=100
Dim float x(1000), f=0.0
Dim string s=""
Dim float rt(100), mhz, el, ck, us
Dim integer k, seed, sh, pcsn
mhz = Val(MM.Info(CPUSPEED)) / 1000000
Print "BENCHSTART "; MM.Device$; " "; MM.Ver; " "; mhz; " MHz"
' pin the display mode: MODE stays set after END (only a reset or OPTION
' DEFAULT MODE restores the default) and scan-out loads the bus differently
' in each mode, so without this the time would depend on what ran before.
' MODE 1 is the default mode, the one Phase 0 ran in unless OPTION DEFAULT
' MODE was set (OPTION LIST then shows a DEFAULT MODE line).
Mode 1
' the stand-in for RND: a 31-bit LCG, filled before the timed region
seed = 12345
For k = 1 To 100
  seed = (seed * 1103515245 + 12345) And &H7FFFFFFF
  rt(k) = seed / 2147483648
Next k
' the sampler's tables: the largest power of two up to 8192 entries (16
' bytes each) that leaves 20 KB of heap for OPTION PROFILING's counters.
' The PC3 needs 8192: the first board runs sampled 2,700-5,900 distinct PCs.
pcsn = 8192
Do While pcsn > 1024 And pcsn * 16 + 20480 > MM.Info(HEAP)
  pcsn = pcsn \ 2
Loop
s=""
f=0.0
i=0
Timer =0
If PROFILE% Then Option profiling on, sample, pcsn
runit
el = Timer
' checksum over everything runit leaves behind
ck = 0
For k = 1 To 1000
  ck = ck + x(k) * k
Next k
sh = 0
For k = 1 To Len(s)
  sh = sh + Asc(Mid$(s, k, 1)) * k
Next k
' runit is called once, so there is no harness loop to subtract
us = el * 1000 / NOUTER%
Print "BENCH knivd_fixed_us_per_outer_pass "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
Print "BENCH knivd_fixed_us_per_inner_pass "; Str$(us / 100, 0, 3); " "; Str$(us / 100 * mhz, 0, 0)
Print "CHECK knivd_fixed i="; Str$(i); " sumx="; Str$(ck, 0, 9); " f="; Str$(f, 0, 9); " len="; Str$(Len(s)); " shash="; Str$(sh)
Print "ELAPSED knivd_fixed "; Str$(el, 0, 3); " ms for "; Str$(NOUTER%); " outer passes ("; Str$(NOUTER% * 30000 / el * 1024 / 286, 0, 0); " grains-equivalent)"
Print "BENCHEND"
End
'
Sub runit
Do While i<NOUTER%
  i=i+2 : f=f+2.0002
  If (i Mod 2)=0 Then
    i=i*2 : i=i\2
    f=f*2.0002 : f=f/2.0002
  EndIf
  i=i-1
  For t=1 To 100
    f=f-1.0001
    If (f-Int(f))>=0.5 Then
      f=Sin(f*Log(i))
      s=Str$(f,6,6)
    EndIf
    f=(f-Tan(i))*(rt(t)/i)
    If Instr(s,LEFT$(Str$(i),2))>0 Then s=s+"0"
  Next
  x(1+(i Mod 1000))=f
Loop
End Sub
