' warp_time.bas - what WARPH and WARPV cost, per call, in MODE 2 and MODE 5
OPTION EXPLICIT
OPTION DEFAULT INTEGER
DIM md, i, t, W, H, mi, mds(1) = (2, 5)
DIM a!
FOR mi = 0 TO 1
  md = mds(mi)
  MODE md
  W = MM.HRES : H = MM.VRES
  FRAMEBUFFER CREATE
  DIM oh(H - 1), ov(W - 1), oh2((H \ 2) - 1), ov8((W \ 8) - 1), os(H - 1), ov2((W \ 2) - 1), ov4((W \ 4) - 1)
  FOR i = 0 TO H - 1 : oh(i) = i * 3 + 1 : NEXT
  FOR i = 0 TO W - 1 : ov(i) = i : NEXT
  FOR i = 0 TO (H \ 2) - 1 : oh2(i) = i : NEXT
  t = TIMER : FOR i = 1 TO 100 : FRAMEBUFFER COPY F, N : NEXT : a! = (TIMER - t) / 100
  PRINT "MODE"; md; ": COPY F,N"; a!; " ms"
  t = TIMER : FOR i = 1 TO 100 : FRAMEBUFFER WARPH F, N, oh() : NEXT : a! = (TIMER - t) / 100
  PRINT "MODE"; md; ": WARPH odd offsets"; a!; " ms"
  t = TIMER : FOR i = 1 TO 100 : FRAMEBUFFER WARPH F, N, oh2(), 2 : NEXT : a! = (TIMER - t) / 100
  PRINT "MODE"; md; ": WARPH step 2"; a!; " ms"
  t = TIMER : FOR i = 1 TO 100 : FRAMEBUFFER WARPH F, N, oh(), , , , , 0 : NEXT : a! = (TIMER - t) / 100
  PRINT "MODE"; md; ": WARPH transparent"; a!; " ms"
  t = TIMER : FOR i = 1 TO 100 : FRAMEBUFFER WARPV F, N, ov() : NEXT : a! = (TIMER - t) / 100
  PRINT "MODE"; md; ": WARPV step 1"; a!; " ms"
  t = TIMER : FOR i = 1 TO 100 : FRAMEBUFFER WARPV F, N, ov2(), 2 : NEXT : a! = (TIMER - t) / 100
  PRINT "MODE"; md; ": WARPV step 2"; a!; " ms"
  t = TIMER : FOR i = 1 TO 100 : FRAMEBUFFER WARPV F, N, ov4(), 4 : NEXT : a! = (TIMER - t) / 100
  PRINT "MODE"; md; ": WARPV step 4"; a!; " ms"
  t = TIMER : FOR i = 1 TO 100 : FRAMEBUFFER WARPV F, N, ov8(), 8 : NEXT : a! = (TIMER - t) / 100
  PRINT "MODE"; md; ": WARPV step 8 (40 elements used)"; a!; " ms"
  t = TIMER : FOR i = 1 TO 100 : FRAMEBUFFER WAIT : NEXT : a! = (TIMER - t) / 100
  PRINT "MODE"; md; ": FRAMEBUFFER WAIT"; a!; " ms"
  t = TIMER : FOR i = 1 TO 100 : MATH SCALE oh(), 1.5, os() : NEXT : a! = (TIMER - t) / 100
  PRINT "MODE"; md; ": MATH SCALE of"; H; " elements"; a!; " ms"
  FRAMEBUFFER CLOSE
  ERASE oh, ov, oh2, ov8, os, ov2, ov4
NEXT
MODE 1
