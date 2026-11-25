' affine_test.bas - FRAMEBUFFER SPAN and AFFINE checked with PIXEL() against a pixel model
' (docs/FRAMEBUFFER_AFFINE_SPAN_Design.md sections 4, 8 and 10). F holds (x - y) MOD 8 as diagonals, 2 holds
' (x + y + 3) MOD 8; 2 is copied to N before each command from F, and samples of N are then compared.
OPTION EXPLICIT
OPTION DEFAULT INTEGER
OPTION CONSOLE SERIAL  ' text on the screen would change the pixels being checked
CONST NCASES = 80, NSAMP = 60, FP = 65536
DIM pal(7), rb(7), mds(3) = (2, 3, 5, 4)
pal(0) = RGB(0,0,0) : pal(1) = RGB(255,0,0) : pal(2) = RGB(0,255,0) : pal(3) = RGB(0,0,255)
pal(4) = RGB(255,255,0) : pal(5) = RGB(0,255,255) : pal(6) = RGB(255,0,255) : pal(7) = RGB(255,255,255)
DIM md, W, H, mi, k, c, cs, s, x, y, ex, exc, got, fails, shown, nsam, badpat, allfails, ok
DIM aff, stp, fst, n, edg, trn, ev, tv, lft, wid, outs, same, kind, naff, nspan
DIM au(0), av(0), adu(0), adv(0), sl(0), sr(0)
DIM res$(5) LENGTH 100, nres
RANDOMIZE 2026
allfails = 0
FOR mi = 0 TO 3
  md = mds(mi)
  ON ERROR SKIP 1
  MODE md
  ok = (MM.ERRNO = 0)
  IF NOT ok THEN PRINT "MODE"; md; ": not available ("; MM.ERRMSG$; ")"
  IF ok THEN
    W = MM.HRES : H = MM.VRES
    FRAMEBUFFER CREATE
    FRAMEBUFFER CREATE 2
    FOR k = 0 TO 7 : PIXEL 0, 0, pal(k) : rb(k) = PIXEL(0, 0) : NEXT
    FRAMEBUFFER WRITE F : CLS
    FOR c = -(H - 1) TO W - 1 : LINE c, 0, c + H - 1, H - 1, 1, pal(pm(c, 8)) : NEXT
    badpat = 0
    FOR s = 1 TO 200
      x = ri(0, W - 1) : y = ri(0, H - 1)
      IF PIXEL(x, y) <> rb(pm(x - y, 8)) THEN badpat = badpat + 1
    NEXT
    FRAMEBUFFER WRITE 2 : CLS
    FOR c = 0 TO W + H - 2 : LINE c, 0, c - H + 1, H - 1, 1, pal(pm(c + 3, 8)) : NEXT
    FOR s = 1 TO 200
      x = ri(0, W - 1) : y = ri(0, H - 1)
      IF PIXEL(x, y) <> rb(pm(x + y + 3, 8)) THEN badpat = badpat + 1
    NEXT
    FRAMEBUFFER WRITE N
    fails = 0 : shown = 0 : nsam = 0 : naff = 0 : nspan = 0
    FOR cs = 1 TO NCASES
      aff = (RND < 0.6)
      stp = 1 : IF RND < 0.6 THEN stp = ri(1, MIN(H, 40))
      n = ri(1, MIN(H \ stp + 2, 60))
      fst = 0 : IF RND < 0.6 THEN fst = ri(-(n * stp) \ 2, H - 1)
      edg = -2 : IF RND < 0.5 THEN edg = ri(-1, 7)
      trn = -1 : IF RND < 0.3 THEN trn = ri(0, 7)
      ev = -1 : IF edg >= 0 THEN ev = pal(edg)
      IF trn >= 0 THEN tv = tval(pal(trn))
      FRAMEBUFFER COPY 2, N
      IF aff THEN
        naff = naff + 1
        ERASE au, av, adu, adv : DIM au(n - 1), av(n - 1), adu(n - 1), adv(n - 1)
        kind = ri(0, 5)
        FOR k = 0 TO n - 1 : setval k : NEXT
        lft = 0 : IF RND < 0.5 THEN lft = ri(-W \ 2, W - 1)
        wid = W : IF RND < 0.5 THEN wid = ri(1, W + 20)
        doaffine
      ELSE
        nspan = nspan + 1
        ERASE sl, sr : DIM sl(n - 1), sr(n - 1)
        FOR k = 0 TO n - 1
          sl(k) = ri(-10, W + 10) : sr(k) = ri(-10, W + 10)
          IF RND < 0.1 THEN sl(k) = ri(-(2^40), 2^40)
          IF RND < 0.1 THEN sr(k) = ri(-(2^40), 2^40)
        NEXT
        outs = (RND < 0.5)
        same = (RND < 0.2)
        dospan
      ENDIF
      FOR s = 1 TO NSAMP
        x = ri(0, W - 1) : y = ri(0, H - 1)
        IF RND < 0.6 AND fst + n * stp > 0 THEN y = ri(MAX(fst, 0), MIN(fst + n * stp, H) - 1)
        IF aff THEN ex = amodel(x, y) ELSE ex = smodel(x, y)
        IF ex < 0 THEN exc = rb(pm(x + y + 3, 8)) ELSE exc = rb(ex)
        got = PIXEL(x, y)
        nsam = nsam + 1
        IF got <> exc THEN
          fails = fails + 1
          IF shown < 6 THEN
            shown = shown + 1
            IF aff THEN
              PRINT "  case"; cs; " AFFINE kind"; kind; " n"; n; " step"; stp; " first"; fst; " left"; lft; " width"; wid;
            ELSE
              PRINT "  case"; cs; " SPAN n"; n; " step"; stp; " first"; fst; " outside"; outs; " same"; same;
            ENDIF
            PRINT " edge"; edg; " trans"; trn; " at"; x; ","; y; " want"; ex; " got &H"; HEX$(got)
          ENDIF
        ENDIF
      NEXT
    NEXT
    res$(nres) = "MODE" + STR$(md) + " (" + STR$(W) + "x" + STR$(H) + "):" + STR$(naff) + " AFFINE and" + STR$(nspan) + " SPAN cases,"
    res$(nres) = res$(nres) + STR$(nsam) + " samples," + STR$(fails) + " wrong," + STR$(badpat) + " pattern errors"
    PRINT res$(nres) : nres = nres + 1
    allfails = allfails + fails + badpat
    FRAMEBUFFER CLOSE
  ENDIF
NEXT

' a quarter turn must be exact: the whole of N against F, pixel by pixel on a part of the screen (MODE 2)
MODE 2
W = MM.HRES : H = MM.VRES
FRAMEBUFFER CREATE
FOR k = 0 TO 7 : PIXEL 0, 0, pal(k) : rb(k) = PIXEL(0, 0) : NEXT
FRAMEBUFFER WRITE F : CLS
FOR c = -(H - 1) TO W - 1 : LINE c, 0, c + H - 1, H - 1, 1, pal(pm(c, 8)) : NEXT
FRAMEBUFFER WRITE N : CLS
DIM qu(0), qv(0), qdu(0), qdv(0)
' N's (x, y) shows F's (y, H - 1 - x): a quarter turn of the left H x H of F
qdu(0) = 0 : qdv(0) = -FP : qu(0) = 0 : qv(0) = (H - 1) * FP
FRAMEBUFFER AFFINE F, N, qu(), qv(), qdu(), qdv(), H, 0, 0, H
fails = 0
FOR y = 0 TO H - 1 STEP 3
  FOR x = 0 TO H - 1 STEP 3
    IF PIXEL(x, y) <> rb(pm(y - (H - 1 - x), 8)) THEN fails = fails + 1
  NEXT
NEXT
res$(nres) = "quarter turn:" + STR$(fails) + " wrong" : PRINT res$(nres) : nres = nres + 1
allfails = allfails + fails

' the errors (section 10)
DIM e1(9), e9(8), f1!(9)
errtest 1, "Same framebuffer"
errtest 2, "Layer not created"
errtest 3, "Array 2 has 9 elements, array 1 has 10"
errtest 4, "Array 4 has 9 elements, array 1 has 10"
errtest 5, "*"
errtest 6, "*"
errtest 7, "*"
errtest 8, "Right array has 9 elements, left array has 10"
errtest 9, "*"
errtest 10, "*"
errtest 11, "*"
errtest 12, "*"
errtest 13, "Layer Top not created"
FRAMEBUFFER CLOSE
MODE 1
FRAMEBUFFER CREATE
errtest 14, "Not available in mode 1"
errtest 15, "Not available in mode 1"
FRAMEBUFFER CLOSE
OPTION CONSOLE BOTH
FOR k = 0 TO nres - 1 : PRINT res$(k) : NEXT
IF allfails = 0 THEN PRINT "AFFINE AND SPAN TEST PASSED" ELSE PRINT "AFFINE AND SPAN TEST FAILED:"; allfails
END

SUB errtest t, want$
  SELECT CASE t
    CASE 1
      ON ERROR SKIP 1 : FRAMEBUFFER AFFINE F, F, e1(), e1(), e1(), e1()
    CASE 2
      ON ERROR SKIP 1 : FRAMEBUFFER AFFINE L, N, e1(), e1(), e1(), e1()
    CASE 3
      ON ERROR SKIP 1 : FRAMEBUFFER AFFINE F, N, e1(), e9(), e1(), e1()
    CASE 4
      ON ERROR SKIP 1 : FRAMEBUFFER AFFINE F, N, e1(), e1(), e1(), e9()
    CASE 5
      ON ERROR SKIP 1 : FRAMEBUFFER AFFINE F, N, e1(), e1(), f1!(), e1()
    CASE 6
      ON ERROR SKIP 1 : FRAMEBUFFER AFFINE F, N, e1(), e1(), e1(), e1(), 0
    CASE 7
      ON ERROR SKIP 1 : FRAMEBUFFER AFFINE F, N, e1(), e1(), e1(), e1(), 1, 0, 0, 0
    CASE 8
      ON ERROR SKIP 1 : FRAMEBUFFER SPAN F, N, e1(), e9()
    CASE 9
      ON ERROR SKIP 1 : FRAMEBUFFER SPAN F, N, e1(), e1(), 1, 0, -1, , 2
    CASE 10
      ON ERROR SKIP 1 : FRAMEBUFFER SPAN F, N, e1(), e1(), 0
    CASE 11
      ON ERROR SKIP 1 : FRAMEBUFFER AFFINE F, N, e1(), e1(), e1()
    CASE 12
      ON ERROR SKIP 1 : FRAMEBUFFER SPAN F, N, e1(), e1(), 1, 0, -1, 16
    CASE 13
      ON ERROR SKIP 1 : FRAMEBUFFER SPAN T, N, e1(), e1()
    CASE 14
      ON ERROR SKIP 1 : FRAMEBUFFER AFFINE F, N, e1(), e1(), e1(), e1()
    CASE 15
      ON ERROR SKIP 1 : FRAMEBUFFER SPAN F, N, e1(), e1()
  END SELECT
  IF MM.ERRNO = 0 THEN
    PRINT "error test"; t; ": no error, wanted "; want$ : allfails = allfails + 1
  ELSEIF want$ = "*" OR MM.ERRMSG$ = want$ THEN
    PRINT "error test"; t; ": "; MM.ERRMSG$
  ELSE
    PRINT "error test"; t; ": got "; MM.ERRMSG$; " wanted "; want$ : allfails = allfails + 1
  ENDIF
END SUB

' element k of the four AFFINE arrays, by the case's kind
SUB setval k
  SELECT CASE kind
    CASE 0  ' the picture as it is, moved
      adu(k) = FP : adv(k) = 0 : au(k) = ri(-W, W) * FP : av(k) = ri(-H, H) * FP
    CASE 1  ' any small step
      adu(k) = ri(-3 * FP, 3 * FP) : adv(k) = ri(-3 * FP, 3 * FP)
      au(k) = ri(-4 * W * FP, 4 * W * FP) : av(k) = ri(-4 * H * FP, 4 * H * FP)
    CASE 2  ' a quarter turn
      adu(k) = 0 : adv(k) = FP * (2 * ri(0, 1) - 1) : au(k) = ri(-W, W) * FP : av(k) = ri(-H, H) * FP
    CASE 3  ' far away, with long steps
      adu(k) = ri(-(2^30), 2^30) : adv(k) = ri(-(2^30), 2^30) : au(k) = ri(-(2^44), 2^44) : av(k) = ri(-(2^44), 2^44)
    CASE 4  ' steps of a few thousandths of a pixel
      adu(k) = ri(-300, 300) : adv(k) = ri(-300, 300) : au(k) = ri(0, W * FP) : av(k) = ri(0, H * FP)
    CASE 5  ' a mirrored row
      adu(k) = -FP : adv(k) = 0 : au(k) = ri(-W, 2 * W) * FP + FP \ 2 : av(k) = ri(-H, 2 * H) * FP
  END SELECT
END SUB

SUB doaffine
  IF edg = -2 AND trn < 0 THEN
    IF lft = 0 AND wid = W THEN
      FRAMEBUFFER AFFINE F, N, au(), av(), adu(), adv(), stp, fst
    ELSE
      FRAMEBUFFER AFFINE F, N, au(), av(), adu(), adv(), stp, fst, lft, wid
    ENDIF
  ELSEIF edg = -2 THEN
    FRAMEBUFFER AFFINE F, N, au(), av(), adu(), adv(), stp, fst, lft, wid, , tv
  ELSEIF trn < 0 THEN
    FRAMEBUFFER AFFINE F, N, au(), av(), adu(), adv(), stp, fst, lft, wid, ev
  ELSE
    FRAMEBUFFER AFFINE F, N, au(), av(), adu(), adv(), stp, fst, lft, wid, ev, tv
  ENDIF
END SUB

' SPAN's fill is the case's edge: none for -2 (the argument left out) and for -1
SUB dospan
  IF same THEN
    IF edg = -2 THEN
      FRAMEBUFFER SPAN N, N, sl(), sr(), stp, fst, , , outs
    ELSE
      FRAMEBUFFER SPAN N, N, sl(), sr(), stp, fst, ev, , outs
    ENDIF
  ELSEIF trn < 0 THEN
    IF edg = -2 AND outs = 0 THEN
      FRAMEBUFFER SPAN F, N, sl(), sr(), stp, fst
    ELSE
      FRAMEBUFFER SPAN F, N, sl(), sr(), stp, fst, ev, , outs
    ENDIF
  ELSE
    FRAMEBUFFER SPAN F, N, sl(), sr(), stp, fst, ev, tv, outs
  ENDIF
END SUB

' what PIXEL should read at (px, py) of N after AFFINE: a palette index, or -1 for N's own (x + y + 3) MOD 8
FUNCTION amodel(px, py)
  LOCAL b, j, i, uu, vv, sx, sy
  amodel = -1
  IF py < fst OR py >= fst + n * stp THEN EXIT FUNCTION
  i = px - lft
  IF i < 0 OR i >= wid THEN EXIT FUNCTION
  b = (py - fst) \ stp : j = py - fst - b * stp
  uu = au(b) - j * adv(b) + i * adu(b) : vv = av(b) + j * adu(b) + i * adv(b)
  sx = fl(uu) : sy = fl(vv)
  IF edg = -2 THEN
    sx = pm(sx, W) : sy = pm(sy, H)
  ELSEIF sx < 0 OR sx >= W OR sy < 0 OR sy >= H THEN
    IF edg >= 0 THEN amodel = edg
    EXIT FUNCTION
  ENDIF
  amodel = pm(sx - sy, 8)
  IF amodel = trn THEN amodel = -1
END FUNCTION

' and after SPAN
FUNCTION smodel(px, py)
  LOCAL b, inside
  smodel = -1
  IF py < fst OR py >= fst + n * stp THEN EXIT FUNCTION
  b = (py - fst) \ stp
  inside = (sl(b) <= px AND px <= sr(b))
  IF inside <> outs THEN
    IF same = 0 THEN
      smodel = pm(px - py, 8)
      IF smodel = trn THEN smodel = -1
    ENDIF
  ELSEIF edg >= 0 THEN
    smodel = edg
  ENDIF
END FUNCTION

' a / 65536 rounded down
FUNCTION fl(a)
  IF a >= 0 THEN fl = a \ FP ELSE fl = -((FP - 1 - a) \ FP)
END FUNCTION

FUNCTION pm(a, m)
  LOCAL r
  r = a MOD m
  IF r < 0 THEN r = r + m
  pm = r
END FUNCTION

FUNCTION ri(lo, hi)
  ri = lo + INT(RND * (hi - lo + 1))
END FUNCTION

' the transparent value as FRAMEBUFFER MERGE takes it in this mode
FUNCTION tval(cv)
  IF md = 2 OR md = 3 THEN
    tval = ((cv >> 20) AND 8) OR ((cv >> 13) AND 6) OR ((cv >> 7) AND 1)
  ELSEIF md = 5 THEN
    tval = ((cv >> 16) AND &HE0) OR ((cv >> 11) AND &H1C) OR ((cv >> 6) AND 3)
  ELSE
    tval = cv
  ENDIF
END FUNCTION
