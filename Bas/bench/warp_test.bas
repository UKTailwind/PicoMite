' warp_test.bas - FRAMEBUFFER WARPH and WARPV checked with PIXEL() against a pixel model
' (docs/FRAMEBUFFER_WARP_Design.md sections 4-6). F holds (x - y) MOD 8 as diagonals, 2 holds
' (x + y + 3) MOD 8, each copied to N before a warp from F; samples of N are then compared.
OPTION EXPLICIT
OPTION DEFAULT INTEGER
CONST NCASES = 60, NSAMP = 60
DIM pal(7), rb(7), mds(3) = (2, 3, 5, 4)
pal(0) = RGB(0,0,0) : pal(1) = RGB(255,0,0) : pal(2) = RGB(0,255,0) : pal(3) = RGB(0,0,255)
pal(4) = RGB(255,255,0) : pal(5) = RGB(0,255,255) : pal(6) = RGB(255,0,255) : pal(7) = RGB(255,255,255)
DIM md, W, H, mi, k, c, cs, s, x, y, ex, exc, got, fails, shown, nsam, badpat, allfails, ok, d$
DIM vert, bands, sz, stp, fst, maxn, n, ph, edg, trn, userows, ev, tv
DIM ofs(0), rws(0)
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
    fails = 0 : shown = 0 : nsam = 0
    FOR cs = 1 TO NCASES
      vert = (RND < 0.5)
      bands = H : sz = W
      IF vert THEN
        bands = W : sz = H
      ENDIF
      stp = 1 : IF RND < 0.6 THEN stp = ri(1, MIN(bands, 40))
      fst = 0 : IF RND < 0.6 THEN fst = ri(0, bands - 1)
      maxn = (bands - fst + stp - 1) \ stp
      n = ri(1, maxn)
      ERASE ofs : DIM ofs(n - 1)
      FOR k = 0 TO n - 1 : ofs(k) = roff(sz) : NEXT
      ph = 0 : IF RND < 0.5 THEN ph = ri(-50, 50)
      IF RND < 0.15 THEN ph = ri(-(2^40), 2^40)
      edg = -2 : IF RND < 0.5 THEN edg = ri(-1, 7)
      trn = -1 : IF RND < 0.3 THEN trn = ri(0, 7)
      userows = 0
      IF vert = 0 AND RND < 0.5 THEN
        userows = 1 : ERASE rws : DIM rws(n - 1)
        FOR k = 0 TO n - 1 : rws(k) = ri(-1, H - 1) : NEXT
      ENDIF
      ev = -1 : IF edg >= 0 THEN ev = pal(edg)
      IF trn >= 0 THEN tv = tval(pal(trn))
      FRAMEBUFFER COPY 2, N
      dowarp
      FOR s = 1 TO NSAMP
        x = ri(0, W - 1) : y = ri(0, H - 1)
        IF RND < 0.6 THEN
          IF vert THEN x = ri(fst, MIN(fst + n * stp, W) - 1) ELSE y = ri(fst, MIN(fst + n * stp, H) - 1)
        ENDIF
        ex = model(x, y)
        IF ex < 0 THEN exc = rb(pm(x + y + 3, 8)) ELSE exc = rb(ex)
        got = PIXEL(x, y)
        nsam = nsam + 1
        IF got <> exc THEN
          fails = fails + 1
          IF shown < 6 THEN
            shown = shown + 1
            d$ = "H" : IF vert THEN d$ = "V"
            PRINT "  case"; cs; " "; d$; " n"; n; " step"; stp; " first"; fst; " phase"; ph;
            PRINT " edge"; edg; " trans"; trn; " rows"; userows; " at"; x; ","; y; " want"; ex; " got &H"; HEX$(got)
          ENDIF
        ENDIF
      NEXT
    NEXT
    PRINT "MODE"; md; " ("; STR$(W); "x"; STR$(H); "):"; NCASES; " cases,"; nsam; " samples,"; fails; " wrong,"; badpat; " pattern errors"
    allfails = allfails + fails + badpat
    FRAMEBUFFER CLOSE
  ENDIF
NEXT

' the errors (section 6), in MODE 2
MODE 2
W = MM.HRES : H = MM.VRES
FRAMEBUFFER CREATE
DIM e1(9), f1!(9), big(H), bigv(W), r1(8)
DIM sfit = (H - 10 + 3) \ 4
DIM e4(sfit)
errtest 1, "Same framebuffer"
errtest 2, "Layer not created"
errtest 3, "Layer Top not created"
errtest 4, "Array has " + STR$(H + 1) + " elements, step 1 from row 0 fits " + STR$(H)
errtest 5, "Array has " + STR$(W + 1) + " elements, step 1 from column 0 fits " + STR$(W)
errtest 6, "Array has " + STR$(sfit + 1) + " elements, step 4 from row 10 fits " + STR$(sfit)
errtest 7, "Row array has 9 elements, offset array has 10"
errtest 8, "Row array element 5 is " + STR$(H) + ": rows are -1 to " + STR$(H - 1)
errtest 9, "*"
errtest 10, "*"
errtest 11, "*"
errtest 12, "*"
FRAMEBUFFER CLOSE
MODE 1
FRAMEBUFFER CREATE
errtest 13, "Not available in mode 1"
FRAMEBUFFER CLOSE
IF allfails = 0 THEN PRINT "WARP TEST PASSED" ELSE PRINT "WARP TEST FAILED:"; allfails
END

SUB errtest t, want$
  LOCAL r(9)
  SELECT CASE t
    CASE 1
      ON ERROR SKIP 1 : FRAMEBUFFER WARPH F, F, e1()
    CASE 2
      ON ERROR SKIP 1 : FRAMEBUFFER WARPH L, N, e1()
    CASE 3
      ON ERROR SKIP 1 : FRAMEBUFFER WARPV T, N, e1()
    CASE 4
      ON ERROR SKIP 1 : FRAMEBUFFER WARPH F, N, big()
    CASE 5
      ON ERROR SKIP 1 : FRAMEBUFFER WARPV F, N, bigv()
    CASE 6
      ON ERROR SKIP 1 : FRAMEBUFFER WARPH F, N, e4(), 4, 10
    CASE 7
      ON ERROR SKIP 1 : FRAMEBUFFER WARPH F, N, e1(), , , , , , r1()
    CASE 8
      r(5) = H : ON ERROR SKIP 1 : FRAMEBUFFER WARPH F, N, e1(), , , , , , r()
    CASE 9
      ON ERROR SKIP 1 : FRAMEBUFFER WARPV F, N, e1(), , , , , , r()
    CASE 10
      ON ERROR SKIP 1 : FRAMEBUFFER WARPH F, N, f1!()
    CASE 11
      ON ERROR SKIP 1 : FRAMEBUFFER WARPH F, N, e1(), 0
    CASE 12
      ON ERROR SKIP 1 : FRAMEBUFFER WARPH F, N, e1(), 1, 1, 0, -1, 16
    CASE 13
      ON ERROR SKIP 1 : FRAMEBUFFER WARPH F, N, e1()
  END SELECT
  IF MM.ERRNO = 0 THEN
    PRINT "error test"; t; ": no error, wanted "; want$ : allfails = allfails + 1
  ELSEIF want$ = "*" OR MM.ERRMSG$ = want$ THEN
    PRINT "error test"; t; ": "; MM.ERRMSG$
  ELSE
    PRINT "error test"; t; ": got "; MM.ERRMSG$; " wanted "; want$ : allfails = allfails + 1
  ENDIF
END SUB

SUB dowarp
  IF vert THEN
    IF edg = -2 AND trn < 0 THEN
      IF stp = 1 AND fst = 0 AND ph = 0 THEN
        FRAMEBUFFER WARPV F, N, ofs()
      ELSE
        FRAMEBUFFER WARPV F, N, ofs(), stp, fst, ph
      ENDIF
    ELSEIF edg = -2 THEN
      FRAMEBUFFER WARPV F, N, ofs(), stp, fst, ph, , tv
    ELSEIF trn < 0 THEN
      FRAMEBUFFER WARPV F, N, ofs(), stp, fst, ph, ev
    ELSE
      FRAMEBUFFER WARPV F, N, ofs(), stp, fst, ph, ev, tv
    ENDIF
  ELSEIF userows THEN
    IF edg = -2 AND trn < 0 THEN
      FRAMEBUFFER WARPH F, N, ofs(), stp, fst, ph, , , rws()
    ELSEIF edg = -2 THEN
      FRAMEBUFFER WARPH F, N, ofs(), stp, fst, ph, , tv, rws()
    ELSEIF trn < 0 THEN
      FRAMEBUFFER WARPH F, N, ofs(), stp, fst, ph, ev, , rws()
    ELSE
      FRAMEBUFFER WARPH F, N, ofs(), stp, fst, ph, ev, tv, rws()
    ENDIF
  ELSE
    IF edg = -2 AND trn < 0 THEN
      IF stp = 1 AND fst = 0 AND ph = 0 THEN
        FRAMEBUFFER WARPH F, N, ofs()
      ELSE
        FRAMEBUFFER WARPH F, N, ofs(), stp, fst, ph
      ENDIF
    ELSEIF edg = -2 THEN
      FRAMEBUFFER WARPH F, N, ofs(), stp, fst, ph, , tv
    ELSEIF trn < 0 THEN
      FRAMEBUFFER WARPH F, N, ofs(), stp, fst, ph, ev
    ELSE
      FRAMEBUFFER WARPH F, N, ofs(), stp, fst, ph, ev, tv
    ENDIF
  ENDIF
END SUB

' what PIXEL should read at (px, py) of N: a palette index, or -1 for N's own (x + y + 3) MOD 8
FUNCTION model(px, py)
  LOCAL kb, b, e, o, sx, sy
  IF vert THEN kb = px ELSE kb = py
  IF kb < fst OR kb >= fst + n * stp THEN model = -1 : EXIT FUNCTION
  b = (kb - fst) \ stp
  e = pm(b + pm(ph, n), n)
  o = ofs(e)
  IF vert THEN
    sx = px : sy = py + o
  ELSE
    sy = py
    IF userows THEN
      IF rws(e) = -1 THEN model = -1 : EXIT FUNCTION
      sy = pm(rws(e) + py - (fst + b * stp), H)
    ENDIF
    sx = px - o
  ENDIF
  IF edg = -2 THEN
    sx = pm(sx, W) : sy = pm(sy, H)
  ELSEIF sx < 0 OR sx >= W OR sy < 0 OR sy >= H THEN
    IF edg = -1 THEN model = -1 ELSE model = edg
    EXIT FUNCTION
  ENDIF
  model = pm(sx - sy, 8)
  IF model = trn THEN model = -1
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

' a random offset: none, small, the size exactly, past the size, or huge
FUNCTION roff(size)
  LOCAL r
  r = ri(0, 5)
  IF r = 0 THEN roff = 0
  IF r = 1 THEN roff = ri(-3 * size, 3 * size)
  IF r = 2 THEN roff = ri(-size - 1, size + 1)
  IF r = 3 THEN roff = size * (2 * ri(0, 1) - 1)
  IF r = 4 THEN roff = ri(-(2^40), 2^40)
  IF r = 5 THEN roff = ri(-2, 2)
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
