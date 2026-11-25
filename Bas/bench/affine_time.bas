' affine_time.bas - how long FRAMEBUFFER AFFINE and SPAN take, in ms for one call (and BLIT RESIZE, for comparison)
OPTION EXPLICIT
OPTION DEFAULT INTEGER
OPTION CONSOLE SERIAL
CONST FP = 65536, REPS = 50
DIM mds(3) = (2, 5, 4, 3), mi, md, W, H, k, i, y, ok, n
DIM t!, a!, z!, ca!, sa!
DIM u1(0), v1(0), du1(0), dv1(0)
FOR mi = 0 TO 3
  md = mds(mi)
  ON ERROR SKIP 1
  MODE md
  ok = (MM.ERRNO = 0)
  IF ok THEN
    W = MM.HRES : H = MM.VRES
    FRAMEBUFFER CREATE
    FRAMEBUFFER CREATE 2
    FRAMEBUFFER WRITE F : CLS RGB(blue)
    FOR k = 0 TO 39 : BOX RND * (W - 40), RND * (H - 30), 40, 30, 1, RGB(white), RGB(RND * 255, RND * 255, RND * 255) : NEXT
    FRAMEBUFFER WRITE 2 : CLS
    BOX W \ 2 - 24, H \ 2 - 24, 48, 48, 2, RGB(yellow), RGB(red)
    FRAMEBUFFER WRITE N : CLS
    PRINT "MODE"; md; " ("; STR$(W); "x"; STR$(H); "), "; STR$(VAL(MM.INFO(CPUSPEED)) \ 1000000); " MHz"
    t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER COPY F, N : NEXT : show "FRAMEBUFFER COPY F, N"
    ' AFFINE, the whole screen from one element
    setone 0, 1, W \ 2, H \ 2
    t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER AFFINE F, N, u1(), v1(), du1(), dv1(), H : NEXT : show "AFFINE, the picture as it is"
    setone 30, 1.5, W \ 2, H \ 2
    t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER AFFINE F, N, u1(), v1(), du1(), dv1(), H : NEXT : show "AFFINE, turned 30 degrees, 1.5 times"
    setone 30, 0.5, W \ 2, H \ 2
    t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER AFFINE F, N, u1(), v1(), du1(), dv1(), H : NEXT : show "AFFINE, turned 30 degrees, half size (wrapping)"
    t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER AFFINE F, N, u1(), v1(), du1(), dv1(), H, 0, 0, W, RGB(black) : NEXT : show "  the same, an edge colour"
    t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER AFFINE F, N, u1(), v1(), du1(), dv1(), H, 0, 0, W, , 1 : NEXT : show "  the same, wrapping, a transparent colour"
    setone 90, 1, W \ 2, H \ 2
    t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER AFFINE F, N, u1(), v1(), du1(), dv1(), H : NEXT : show "AFFINE, a quarter turn"
    ' a floor: every row from the horizon down its own line
    n = H - H * 3 \ 8 - 1
    floor n, H * 3 \ 8
    ' a sprite through a 69 x 69 window
    setone 25, 1, 34, 34
    u1(0) = u1(0) + (W \ 2 - 34) * FP : v1(0) = v1(0) + (H \ 2 - 34) * FP
    t! = TIMER : FOR i = 1 TO REPS * 4 : FRAMEBUFFER AFFINE 2, N, u1(), v1(), du1(), dv1(), 69, 40, 60, 69, -1, 0 : NEXT
    PRINT "  AFFINE, a 69 x 69 window, edge -1, transparent:", (TIMER - t!) / (REPS * 4)
    spans
    IF md = 2 OR md = 3 THEN
      t! = TIMER : FOR i = 1 TO 10 : BLIT RESIZE F, N, 0, 0, W \ 8, H \ 8, 0, 0, W, H : NEXT
      PRINT "  BLIT RESIZE, an eighth of the screen to the whole:", (TIMER - t!) / 10
      t! = TIMER : FOR i = 1 TO 10 : BLIT RESIZE F, N, 0, 0, W, H, 0, 0, W, H : NEXT
      PRINT "  BLIT RESIZE, the whole screen at its own size:", (TIMER - t!) / 10
    ENDIF
    FRAMEBUFFER CLOSE
  ENDIF
NEXT
MODE 1
OPTION CONSOLE BOTH
PRINT "affine_time done"
END

SUB show m$
  PRINT "  "; m$; ":", (TIMER - t!) / REPS
END SUB

' one element: the screen's (cx, cy) shows the source's middle, turned d! degrees clockwise and zm! times its size
SUB setone d!, zm!, cx, cy
  du1(0) = FP * COS(RAD(d!)) / zm! : dv1(0) = -FP * SIN(RAD(d!)) / zm!
  u1(0) = FP * (W \ 2) - cx * du1(0) + cy * dv1(0)
  v1(0) = FP * (H \ 2) - cx * dv1(0) - cy * du1(0)
END SUB

SUB floor n, hz
  LOCAL zt!(n - 1), fu(n - 1), fv(n - 1), fdu(n - 1), fdv(n - 1), k, i, d! = W / 2
  FOR k = 0 TO n - 1 : zt!(k) = 24 / (k + 1) : NEXT
  ca! = COS(0.5) : sa! = SIN(0.5)
  t! = TIMER
  FOR i = 1 TO REPS
    MATH SCALE zt!(), -FP * sa!, fdu() : MATH SCALE zt!(), FP * ca!, fdv()
    MATH SCALE zt!(), FP * (d! * ca! + W / 2 * sa!), fu() : MATH ADD fu(), FP * 100, fu()
    MATH SCALE zt!(), FP * (d! * sa! - W / 2 * ca!), fv() : MATH ADD fv(), FP * 100, fv()
  NEXT
  PRINT "  a floor of"; n; " rows: its six MATH statements:", (TIMER - t!) / REPS
  t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER AFFINE F, N, fu(), fv(), fdu(), fdv(), 1, hz + 1 : NEXT
  PRINT "  a floor of"; n; " rows: AFFINE:", (TIMER - t!) / REPS
END SUB

SUB spans
  LOCAL lf(H - 1), rt(H - 1), y, hw, i, r = H \ 2 - 10
  FOR y = 0 TO H - 1 : lf(y) = 0 : rt(y) = W - 1 : NEXT
  t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER SPAN F, N, lf(), rt() : NEXT : show "SPAN, every row whole"
  FOR y = 0 TO H - 1
    hw = 0 : IF ABS(y - H \ 2) <= r THEN hw = SQR(r * r - (y - H \ 2) * (y - H \ 2))
    lf(y) = W \ 2 - hw + 1 : rt(y) = W \ 2 + hw - 1
  NEXT
  t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER SPAN F, N, lf(), rt() : NEXT : show "SPAN, a circle"
  t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER SPAN F, N, lf(), rt(), 1, 0, RGB(black) : NEXT : show "SPAN, a circle, the rest filled"
  t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER SPAN F, N, lf(), rt(), 1, 0, , , 1 : NEXT : show "SPAN, all but the circle"
  t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER SPAN F, N, lf(), rt(), 1, 0, , 1 : NEXT : show "SPAN, a circle, a transparent colour"
  t! = TIMER : FOR i = 1 TO REPS : FRAMEBUFFER SPAN N, N, lf(), rt(), 1, 0, RGB(black) : NEXT : show "SPAN N to N, only the fill"
END SUB
