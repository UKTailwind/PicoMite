' =====================================================================
'  The galaxy
'
'  Elite does not store its universe; it generates it.  Three 16-bit
'  seeds are stirred by one short routine, and every system's name,
'  position, government, economy, technology, population, productivity
'  and radius falls out of particular bits of those seeds.  Eight
'  galaxies of 256 systems each come out of six bytes.
'
'  All of this has to be bit for bit identical to the original or the
'  universe is a different one, so nothing here is tidied up: the twist
'  is a 16-bit add with the carry thrown away, and every field is the
'  slice of bits the original took.  elite_tools/galaxy_ref.py is the
'  oracle, and tests/galaxy_test.bas checks the board against it.
' =====================================================================

' One turn of the generator: the third seed becomes the sum of all three,
' and the other two shuffle down.  Anything above 16 bits is discarded.
SUB GalTwist
  LOCAL INTEGER t
  t = (gs0 + gs1) AND &HFFFF
  gs0 = gs1
  gs1 = gs2
  gs2 = (t + gs1) AND &HFFFF
END SUB

' Move to galaxy g, counting from 1.  A galaxy is reached by rotating
' each of the six seed bytes left by one bit, independently - which is
' why the eight galaxies are related but not similar.
SUB SetGalaxy(g AS INTEGER)
  LOCAL INTEGER i, k, b(5)
  gs0 = &H5A4A : gs1 = &H0248 : gs2 = &HB753
  FOR i = 2 TO g
    b(0) = gs0 AND 255 : b(1) = (gs0 >> 8) AND 255
    b(2) = gs1 AND 255 : b(3) = (gs1 >> 8) AND 255
    b(4) = gs2 AND 255 : b(5) = (gs2 >> 8) AND 255
    FOR k = 0 TO 5
      b(k) = ((b(k) << 1) OR (b(k) >> 7)) AND 255
    NEXT k
    gs0 = b(0) OR (b(1) << 8)
    gs1 = b(2) OR (b(3) << 8)
    gs2 = b(4) OR (b(5) << 8)
  NEXT i
  gSys = 0
END SUB

' Step to the next system: four turns of the generator.
SUB NextSystem
  GalTwist : GalTwist : GalTwist : GalTwist
  gSys = (gSys + 1) AND 255
END SUB

' Wind the seeds to system n of the current galaxy, from wherever they
' are now.  Going backwards means starting again, because the generator
' only runs one way.
SUB GotoSystem(g AS INTEGER, n AS INTEGER)
  LOCAL INTEGER i
  SetGalaxy g
  FOR i = 1 TO n
    NextSystem
  NEXT i
END SUB

' Everything the game knows about the system the seeds are sitting on.
' Technology is displayed one higher than it is stored, and population is
' in tenths of a billion.
SUB SysData
  LOCAL INTEGER h0, h1, h2, l1
  h0 = (gs0 >> 8) AND 255
  h1 = (gs1 >> 8) AND 255
  h2 = (gs2 >> 8) AND 255
  l1 = gs1 AND 255
  sysX = h1
  sysY = h0 >> 1
  sysGov = (l1 >> 3) AND 7
  sysEco = h0 AND 7
  ' An anarchy or a feudal state can never be rich.
  IF sysGov <= 1 THEN sysEco = sysEco OR 2
  sysTech = (sysEco XOR 7) + (h1 AND 3) + ((sysGov + 1) >> 1)
  sysPop = sysTech * 4 + sysEco + sysGov + 1
  sysProd = ((sysEco XOR 7) + 3) * (sysGov + 4) * sysPop * 8
  sysRad = ((h2 AND 15) + 11) * 256 + h1
END SUB

' The system's name, built from three or four two-letter fragments picked
' out of the seeds.  It runs on a copy, so the caller's seeds are left
' where they were.  Fragment 0 is never emitted, and one fragment
' contributes a single letter rather than two.
FUNCTION SysName$()
  LOCAL INTEGER i, n, k, t0, t1, t2
  LOCAL nm$ LENGTH 10
  t0 = gs0 : t1 = gs1 : t2 = gs2
  n = 3
  IF (gs0 AND 64) <> 0 THEN n = 4
  nm$ = ""
  FOR k = 1 TO n
    i = ((gs2 >> 8) AND 31)
    IF i <> 0 THEN nm$ = nm$ + MID$(DIGRAPHS, i * 2 + 1, 2)
    GalTwist
  NEXT k
  gs0 = t0 : gs1 = t1 : gs2 = t2
  ' One fragment contributes a single letter, and carries a marker for the
  ' second one it does not have.  A name can pick that fragment more than
  ' once - eleven of the 2048 do - so strip every marker, not just the
  ' first.
  DO
    i = INSTR(nm$, "?")
    IF i = 0 THEN EXIT DO
    nm$ = LEFT$(nm$, i - 1) + MID$(nm$, i + 1)
  LOOP
  SysName$ = nm$
END FUNCTION

FUNCTION GovName$(g AS INTEGER)
  GovName$ = FIELD$("Anarchy,Feudal,Multi-gov,Dictatorship,Communist,Confederacy,Democracy,Corporate State", g + 1)
END FUNCTION

FUNCTION EcoName$(e AS INTEGER)
  EcoName$ = FIELD$("Rich Ind,Average Ind,Poor Ind,Mainly Ind,Mainly Agri,Rich Agri,Average Agri,Poor Agri", e + 1)
END FUNCTION
