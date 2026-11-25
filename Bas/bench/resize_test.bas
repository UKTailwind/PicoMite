' resize_test.bas - BLIT RESIZE checked with PIXEL() against its rule: pixel (k, m) of the destination rectangle
' shows pixel (k * sw \ dw, m * sh \ dh) of the source rectangle. F holds (x - y) MOD 8 as diagonals and 2 holds
' (x + y + 3) MOD 8. Each case copies 2 to N and resizes a rectangle of F into N; one case in five instead copies
' F to N and resizes within N, the rectangles overlapping or not.
OPTION EXPLICIT
OPTION DEFAULT INTEGER
OPTION CONSOLE SERIAL  ' text on the screen would change the pixels being checked
CONST NCASES = 150, NSAMP = 80
DIM pal(7), rb(7)
pal(0) = RGB(0,0,0) : pal(1) = RGB(255,0,0) : pal(2) = RGB(0,255,0) : pal(3) = RGB(0,0,255)
pal(4) = RGB(255,255,0) : pal(5) = RGB(0,255,255) : pal(6) = RGB(255,0,255) : pal(7) = RGB(255,255,255)
DIM W, H, k, m, c, cs, s, x, y, ex, exc, got, fails, shown, nsam, badpat, badcases, wasbad
DIM sx, sy, sw, sh, dx, dy, dw, dh, trn, tv, self, kind
RANDOMIZE 2026
MODE 2
W = MM.HRES : H = MM.VRES
FRAMEBUFFER CREATE
FRAMEBUFFER CREATE 2
FOR k = 0 TO 7 : PIXEL 0, 0, pal(k) : rb(k) = PIXEL(0, 0) : NEXT
FRAMEBUFFER WRITE F : CLS
FOR c = -(H - 1) TO W - 1 : LINE c, 0, c + H - 1, H - 1, 1, pal(pm(c, 8)) : NEXT
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
FOR cs = 1 TO NCASES
  kind = ri(0, 3)
  sw = ri(1, W) : sh = ri(1, H)
  IF kind = 0 THEN sw = ri(1, 40) : sh = ri(1, 30)               ' a small piece made large
  IF kind = 1 THEN sw = W \ ri(1, 12) : sh = H \ ri(1, 12)       ' the mosaic's sizes
  sx = ri(0, W - sw) : sy = ri(0, H - sh)
  dw = ri(1, W + 40) : dh = ri(1, H + 30)
  IF kind = 1 THEN dw = W : dh = H
  IF kind = 2 THEN dw = sw * ri(1, 7) : dh = sh * ri(1, 7)       ' whole multiples
  IF kind = 3 THEN dw = ri(1, 30) : dh = ri(1, 30)               ' made small
  dx = ri(-dw \ 2, W - 1) : dy = ri(-dh \ 2, H - 1)
  IF kind = 1 THEN dx = 0 : dy = 0
  trn = -1 : IF RND < 0.25 THEN trn = ri(0, 7)
  self = (RND < 0.2)
  IF self THEN FRAMEBUFFER COPY F, N ELSE FRAMEBUFFER COPY 2, N
  IF trn >= 0 THEN tv = ((pal(trn) >> 20) AND 8) OR ((pal(trn) >> 13) AND 6) OR ((pal(trn) >> 7) AND 1)
  IF self THEN
    IF trn < 0 THEN
      BLIT RESIZE N, N, sx, sy, sw, sh, dx, dy, dw, dh
    ELSE
      BLIT RESIZE N, N, sx, sy, sw, sh, dx, dy, dw, dh, tv
    ENDIF
  ELSEIF trn < 0 THEN
    BLIT RESIZE F, N, sx, sy, sw, sh, dx, dy, dw, dh
  ELSE
    BLIT RESIZE F, N, sx, sy, sw, sh, dx, dy, dw, dh, tv
  ENDIF
  wasbad = 0
  FOR s = 1 TO NSAMP
    x = ri(0, W - 1) : y = ri(0, H - 1)
    IF RND < 0.7 THEN
      x = ri(MAX(dx, 0), MIN(dx + dw, W) - 1) : y = ri(MAX(dy, 0), MIN(dy + dh, H) - 1)
    ENDIF
    ex = -1
    IF x >= dx AND x < dx + dw AND y >= dy AND y < dy + dh THEN
      k = sx + ((x - dx) * sw) \ dw : m = sy + ((y - dy) * sh) \ dh
      ex = pm(k - m, 8)
      IF ex = trn THEN ex = -1
    ENDIF
    IF ex >= 0 THEN
      exc = rb(ex)
    ELSEIF self THEN
      exc = rb(pm(x - y, 8))
    ELSE
      exc = rb(pm(x + y + 3, 8))
    ENDIF
    got = PIXEL(x, y)
    nsam = nsam + 1
    IF got <> exc THEN
      fails = fails + 1 : wasbad = 1
      IF shown < 8 THEN
        shown = shown + 1
        PRINT "  case"; cs; " from"; sx; ","; sy; " "; STR$(sw); "x"; STR$(sh); " to"; dx; ","; dy; " "; STR$(dw); "x"; STR$(dh);
        PRINT " trans"; trn; " self"; self; " at"; x; ","; y; " want"; ex; " got &H"; HEX$(got)
      ENDIF
    ENDIF
  NEXT
  badcases = badcases + wasbad
NEXT
FRAMEBUFFER CLOSE
MODE 1
OPTION CONSOLE BOTH
PRINT NCASES; " cases,"; nsam; " samples,"; fails; " wrong in"; badcases; " cases,"; badpat; " pattern errors"
IF fails + badpat = 0 THEN PRINT "RESIZE TEST PASSED" ELSE PRINT "RESIZE TEST FAILED:"; fails + badpat
END

FUNCTION pm(a, m)
  LOCAL r
  r = a MOD m
  IF r < 0 THEN r = r + m
  pm = r
END FUNCTION

FUNCTION ri(lo, hi)
  ri = lo + INT(RND * (hi - lo + 1))
END FUNCTION
