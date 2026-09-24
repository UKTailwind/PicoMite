"""gen_micro2.py - write the Phase 1 micro programs.

bench_micro2.bas   H2 depth, H3 calls, H4 nesting/args, H5 tail, H7 DATA/REM,
                   H8 FOR/DO body scan, SELECT 30 arms, GOSUB near/far label
bench_subs0.bas    lookups and calls with no extra SUBs defined
bench_subs200.bas  the same with 200 dummy SUBs defined (RP2040 subfun search)
"""
import os
OUT = os.path.dirname(os.path.abspath(__file__))

HEAD = r"""Option default float
Const N% = 20000
Const K1 = 1.5
Dim a%, b%, c%, d%, e%, f%, i%, j%, y%, arr%(1024), dmax%
Dim fx, fy, fz
Dim t$, st$
Dim g1(3), g2(3), g3(3), g4(3), g5(3), g6(3), g7(5)
Dim mhz, base_us, t0
GoTo Start
NearLbl:
Return
Start:
mhz = Val(MM.Info(CPUSPEED)) / 1000000
t$ = "Hello world" : fx = 1.25 : fz = 2.5 : b% = 7 : c% = 3 : d% = 4 : e% = 5 : f% = 6
Print "BENCHSTART "; MM.Device$; " "; MM.Ver; " "; mhz; " MHz"
t0 = Timer
For i% = 1 To N%
Next
base_us = (Timer - t0) * 1000
Print "BENCH empty_for_loop "; Str$(base_us / N%, 0, 3); " "; Str$(base_us / N% * mhz, 0, 0)
"""

REP = r"""
Sub Rep lbl$, secs
  Local us
  us = (secs * 1000 - base_us) / N%
  Print "BENCH "; lbl$; " "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
End Sub

Sub Rep10 lbl$, secs
  Local us
  us = (secs * 1000 - base_us / 10) / (N% \ 10)
  Print "BENCH "; lbl$; " "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
End Sub

Sub RepS lbl$, secs, bb
  Local us
  us = (secs * 1000 - bb) / N%
  Print "BENCH "; lbl$; " "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
End Sub
"""


def loop(label, body, tenth=False):
    n = "N% \\ 10" if tenth else "N%"
    rep = "Rep10" if tenth else "Rep"
    lines = ["t0 = Timer", "For i% = 1 To " + n]
    lines += ["  " + l for l in body]
    lines += ["Next", '%s "%s", Timer - t0' % (rep, label), ""]
    return lines


def micro2():
    L = HEAD.splitlines()
    L.append("' ---- H3 call machinery")
    L += loop("call S0 (no params)", ["S0"])
    L += loop("call S3F fx,fy,fz (untyped float params)", ["S3F fx, fy, fz"])
    L += loop("call S3I a%,b%,c% (suffixed int params)", ["S3I a%, b%, c%"])
    L += loop("call S3I 1,2,3 (literal args)", ["S3I 1, 2, 3"])
    L += loop("call S3T a%,b%,c% (AS INTEGER params)", ["S3T a%, b%, c%"])
    L += loop("call S3V a%,b%,c% (BYVAL int params)", ["S3V a%, b%, c%"])
    L += loop("call S6F fx,fy,fz,fx,fy,fz (untyped)", ["S6F fx, fy, fz, fx, fy, fz"])
    L += loop("call S6I a%,b%,c%,d%,e%,f% (suffixed)", ["S6I a%, b%, c%, d%, e%, f%"])
    L += loop("call S6T a%..f% (AS INTEGER)", ["S6T a%, b%, c%, d%, e%, f%"])
    L += loop("call S0L4 (0 params, 4 LOCAL scalars)", ["S0L4"])
    L += loop("call S0L12 (0 params, 12 LOCAL scalars)", ["S0L12"])
    L += loop("y%=F0%() (FUNCTION, 0 args)", ["y% = F0%()"])
    L += loop("fy=F2(fx,fz) (FUNCTION, 2 float args)", ["fy = F2(fx, fz)"])
    L += loop("call SeLoc (sefunc shape: 7 LOCAL arrays + 10 scalars)", ["SeLoc"], tenth=True)
    L += loop("call SeGlob (same body on global arrays)", ["SeGlob"], tenth=True)

    L.append("' ---- H4 built-in arguments")
    L += loop("fy=fx", ["fy = fx"])
    for k in range(1, 6):
        e = "fx"
        for _ in range(k):
            e = "Sin(%s)" % e
        L += loop("fy=SIN nested %d deep" % k, ["fy = " + e])
    L += loop("fy=MAX(fx,fz) (variables)", ["fy = Max(fx, fz)"])
    L += loop("fy=MAX(1.25,2.5) (literals)", ["fy = Max(1.25, 2.5)"])
    L += loop("st$=MID$(t$,c%,d%) (variables)", ["st$ = Mid$(t$, c%, d%)"])
    L += loop("st$=MID$(t$,3,4) (literals)", ["st$ = Mid$(t$, 3, 4)"])
    L += loop("st$=STR$(fx,0,3) (literals)", ["st$ = Str$(fx, 0, 3)"])

    L.append("' ---- H5 per-statement tail")
    L += loop("a%=a%+1 (nothing armed)", ["a% = a% + 1"])
    L.append("SetTick 60000, TickH, 1")
    L += loop("a%=a%+1 (SETTICK armed, 60 s)", ["a% = a% + 1"])
    L.append("SetTick 0, 0, 1")
    L.append("SetTick 1, TickH, 1")
    L += loop("a%=a%+1 (SETTICK 1 ms, empty handler)", ["a% = a% + 1"])
    L.append("SetTick 0, 0, 1")
    L.append("Option NoCheck On")
    L += loop("a%=a%+1 (OPTION NOCHECK ON)", ["a% = a% + 1"])
    L.append("Option NoCheck Off")

    L.append("' ---- H7 pre-scan")
    L += loop("DATA line with a 60-byte list in the body",
              ["Data 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25"])
    L += loop("REM line (40 chars) in the body", ["Rem a forty character comment................"])
    L += loop("' comment line (40 chars) in the body", ["' a forty character comment................."])

    L.append("' ---- H8 block scans (the IF 0 body is jumped by the IF table)")
    for nlines in (3, 30):
        pad = ["    a% = a% + 12345"] * nlines
        L += loop("FOR 1-pass inner loop, body %d lines" % nlines,
                  ["For j% = 1 To 1", "  If 0 Then"] + pad + ["  EndIf", "Next"])
    for nlines in (3, 30):
        pad = ["    a% = a% + 12345"] * nlines
        L += loop("DO 1-pass loop, body %d lines" % nlines,
                  ["Do", "  If 0 Then"] + pad + ["  EndIf", "Loop Until 1"])
    # inner-count comparison: same total inner passes, printed per inner pass
    L += ["t0 = Timer", "For i% = 1 To N% \\ 50", "  For j% = 1 To 100", "    a% = 1", "  Next", "Next",
          "Print \"BENCH inner FOR count 100 (per inner pass, raw) \"; Str$((Timer - t0) * 1000 / (N% * 2), 0, 3); \" \"; Str$((Timer - t0) * 1000 / (N% * 2) * mhz, 0, 0)", ""]
    L += ["t0 = Timer", "For i% = 1 To N%", "  For j% = 1 To 2", "    a% = 1", "  Next", "Next",
          "Print \"BENCH inner FOR count 2 (per inner pass, raw) \"; Str$((Timer - t0) * 1000 / (N% * 2), 0, 3); \" \"; Str$((Timer - t0) * 1000 / (N% * 2) * mhz, 0, 0)", ""]
    for which in (1, 30):
        body = ["Select Case %d" % which]
        for k in range(1, 31):
            body += ["  Case %d" % k, "    a% = " + str(k)]
        body += ["End Select"]
        L += loop("SELECT 30 arms, arm %d matches" % which, body)
    L += loop("GOSUB label near the top", ["GoSub NearLbl"])
    L += loop("GOSUB label after 1000 lines", ["GoSub FarLbl"])

    L.append("' ---- H2 lookups at call depth")
    L += ["dmax% = 1", "Deep 1", "dmax% = 10", "Deep 1", ""]
    L += ["Print \"BENCHEND\"", "End", ""]

    L += REP.splitlines()
    L += ["Sub TickH", "End Sub", "", "Sub S0", "End Sub", "",
          "Sub S3F p1, p2, p3", "End Sub", "",
          "Sub S3I p1%, p2%, p3%", "End Sub", "",
          "Sub S3T p1 As integer, p2 As integer, p3 As integer", "End Sub", "",
          "Sub S3V ByVal p1%, ByVal p2%, ByVal p3%", "End Sub", "",
          "Sub S6F p1, p2, p3, p4, p5, p6", "End Sub", "",
          "Sub S6I p1%, p2%, p3%, p4%, p5%, p6%", "End Sub", "",
          "Sub S6T p1 As integer, p2 As integer, p3 As integer, p4 As integer, p5 As integer, p6 As integer", "End Sub", "",
          "Sub S0L4", "  Local integer l1, l2, l3, l4", "End Sub", "",
          "Sub S0L12", "  Local integer l1, l2, l3, l4, l5, l6, l7, l8, l9, l10, l11, l12", "End Sub", "",
          "Function F0%()", "  F0% = 1", "End Function", "",
          "Function F2(p1, p2)", "  F2 = p1 + p2", "End Function", "",
          # sefunc's shape: its seven LOCAL array statements and scalar LOCALs, then one use of each array
          "Sub SeLoc",
          "  Local rsun(3), rmoon(3)", "  Local rtmoon(3)", "  Local rm2s(3)", "  Local uaxis(3)",
          "  Local um2o(3)", "  Local dvec(5)", "  Local jdtdb, jdutc", "  Local cpsi, pangle",
          "  Local rm2smag, gast", "  Local rtmmoon, psi", "  Local q1, q2",
          "  rsun(1) = 1 : rmoon(1) = 1 : rtmoon(1) = 1 : rm2s(1) = 1",
          "  uaxis(1) = 1 : um2o(1) = 1 : dvec(1) = 1",
          "End Sub", "",
          "Sub SeGlob",
          "  g1(1) = 1 : g2(1) = 1 : g3(1) = 1 : g4(1) = 1",
          "  g5(1) = 1 : g6(1) = 1 : g7(1) = 1",
          "End Sub", "",
          "Sub Deep dd%",
          "  Local integer li, la, lb",
          "  Local lx, ly, lt, lbase",
          "  If dd% < dmax% Then",
          "    Deep dd% + 1",
          "  Else",
          "    lb = 7 : lx = 1.25",
          "    lt = Timer",
          "    For li = 1 To N%",
          "    Next",
          "    lbase = (Timer - lt) * 1000",
          "    lt = Timer",
          "    For li = 1 To N%",
          "      la = lb",
          "    Next",
          "    RepS \"[depth \" + Str$(dd%) + \"] la=lb (LOCALs)\", Timer - lt, lbase",
          "    lt = Timer",
          "    For li = 1 To N%",
          "      ly = K1",
          "    Next",
          "    RepS \"[depth \" + Str$(dd%) + \"] ly=K1 (CONST)\", Timer - lt, lbase",
          "    lt = Timer",
          "    For li = 1 To N%",
          "      la = b%",
          "    Next",
          "    RepS \"[depth \" + Str$(dd%) + \"] la=b% (global)\", Timer - lt, lbase",
          "    lt = Timer",
          "    For li = 1 To N%",
          "      ly = lx * 1.000001 + 0.5",
          "    Next",
          "    RepS \"[depth \" + Str$(dd%) + \"] ly=lx*1.000001+0.5\", Timer - lt, lbase",
          "  EndIf",
          "End Sub", ""]
    L += ["Sub PadSub"] + ["  a% = a% + 12345"] * 1000 + ["End Sub", "", "FarLbl:", "Return", ""]
    return L


def subs(ndummy):
    L = HEAD.splitlines()
    L += loop("a%=b%", ["a% = b%"])
    L += loop("y%=arr%(i% AND 1023)", ["y% = arr%(i% And 1023)"])
    L += loop("fy=SIN(fx)", ["fy = Sin(fx)"])
    L += loop("call S0", ["S0"])
    L += loop("y%=F0%()", ["y% = F0%()"])
    L += ["Print \"BENCHEND\"", "End", ""]
    L += REP.splitlines()
    L += ["Sub S0", "End Sub", "", "Function F0%()", "  F0% = 1", "End Function", ""]
    for k in range(ndummy):
        L += ["Sub Dummy%03d" % k, "End Sub"]
    return L


def write(name, lines):
    txt = "' %s - generated by gen_micro2.py (Phase 1)\n" % name + "\n".join(lines) + "\n"
    open(os.path.join(OUT, name), "w", newline="\n").write(txt)
    print(name, len(txt), "bytes", len(lines), "lines")


write("bench_micro2.bas", micro2())
write("bench_subs0.bas", subs(0))
write("bench_subs200.bas", subs(200))
