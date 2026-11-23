' warp_demo.bas - FRAMEBUFFER WARPH and WARPV: every row or column of a picture moved by its own offset
' PicoMite V8, VGA or HDMI, RP2350 (it uses framebuffers F and 2 and both layers), MODE 2 at 320x240.
'  1. 240 stars, each row at its own speed; a logo waving in the layer; a sine scroller in the top layer
'  2. a lake from day to dusk: parallax hills, and a reflection that ripples more the nearer it is
'  3. a road in the style of the arcade racers: curves, hills, kerbs, and the mountains panning
'  4. the credits, waved both ways (a WARPH into a WARPV)
' Each picture melts away column by column into the next. Any key ends it.
' No per-pixel BASIC in the frame loops: each layer is one command, and the offset tables are made
' with MATH (SCALE, ADD, C_ADD, CLAMP) or animated with the phase argument.
OPTION EXPLICIT
OPTION DEFAULT INTEGER
MODE 2
IF MM.HRES <> 320 OR MM.VRES <> 240 THEN
  MODE 1
  PRINT "This demo needs MODE 2 at 320x240 (OPTION RESOLUTION 640x480)"
  END
ENDIF
CONST W = 320, H = 240
DIM cl(15), quit, i
FOR i = 0 TO 15 : cl(i) = MAP(i) : NEXT  ' the colour that draws in each of the 16 palette slots
DIM rbw(7) = (&HFF2020, &HFF9000, &HFFF000, &H30FF30, &H20E0FF, &H3070FF, &HA040FF, &HFF40C0)
FRAMEBUFFER CREATE
FRAMEBUFFER CREATE 2
FRAMEBUFFER LAYER
FRAMEBUFFER LAYER TOP
FRAMEBUFFER WRITE N
DO
  scene1 : IF NOT quit THEN melt
  IF NOT quit THEN scene2
  IF NOT quit THEN melt
  IF NOT quit THEN scene3
  IF NOT quit THEN melt
  IF NOT quit THEN scene4
  IF NOT quit THEN melt
LOOP UNTIL quit
FRAMEBUFFER CLOSE
MAP RESET
MODE 1
PRINT "FRAMEBUFFER WARPH and WARPV demo ended"
END

' ---------------------------------------------------------------- 1: stars, logo, scroller
SUB scene1
  LOCAL lay(119), spd!(H - 1), so(H - 1), sr(H - 1), sco(0), ssr(0), sw(159), lw(159)
  LOCAL pal(15) = (0, &H404060, &H9090B0, &HFFFFFF, 0, 0, 0, 0, 0, 0, 0, 0, &HFFE860, 0, &H181830, &HFFFFFF)
  LOCAL x, y, k, c, fr, spos, nextc, cx, cyc
  LOCAL m$ = "    FRAMEBUFFER WARPH + WARPV ...  EACH ROW OR COLUMN MOVED BY ITS OWN OFFSET FROM AN INTEGER ARRAY ...  "
  m$ = m$ + "240 STARS: ONE WARPH ...  THE LOGO: ONE WARPV ...  THIS TEXT: A WARPH TO SCROLL IT, A WARPV TO WAVE IT ...  "
  m$ = m$ + "ONE COMMAND A LAYER ...   "
  FOR k = 4 TO 11 : pal(k) = rbw(k - 4) : NEXT
  setpal pal(), 0 : MAP SET
  FRAMEBUFFER WRITE L : CLS : FRAMEBUFFER WRITE T : CLS
  FRAMEBUFFER WRITE 2 : CLS cl(0)
  ' F: stars in rows 0-119, the scroller's strip in 120-151, the logo in 152-226, nothing below
  FRAMEBUFFER WRITE F : CLS cl(0)
  FOR y = 0 TO 119
    lay(y) = 1 + INT(RND * 3)
    x = INT(RND * (W - 1))
    PIXEL x, y, cl(lay(y))
    IF lay(y) = 3 THEN PIXEL x + 1, y, cl(3)
  NEXT
  FOR y = 0 TO H - 1
    sr(y) = y MOD 120
    spd!(y) = lay(sr(y)) ^ 1.7 * 0.45 + RND * 0.2
  NEXT
  TEXT 160, 162, "FRAMEBUFFER  WARPH  +  WARPV", "CT", 1, 1, cl(15), -1
  TEXT 163, 179, "WARP", "CT", 1, 4, cl(14), -1
  TEXT 160, 176, "WARP", "CT", 1, 4, cl(15), -1
  PIXEL 0, 239, cl(15) : k = PIXEL(0, 239) : PIXEL 0, 239, cl(0)  ' what PIXEL() reads for slot 15
  FOR y = 176 TO 223  ' the logo in eight bands of colour, which MAP then cycles
    c = 4 + ((y - 176) \ 6) MOD 8
    FOR x = 96 TO 226
      IF PIXEL(x, y) = k THEN PIXEL x, y, cl(c)
    NEXT
  NEXT
  FRAMEBUFFER WRITE N
  FOR k = 0 TO 159
    sw(k) = -50 + 20 * SIN(2 * PI * k / 80)  ' the scroller's wave: 2 periods, so the phase can turn it
    lw(k) = 156 + 4 * SIN(2 * PI * k / 80)   ' the logo's
  NEXT
  ssr(0) = 120
  DO
    fr = fr + 1
    MATH SCALE spd!(), -fr, so()
    spos = spos + 2 : sco(0) = -spos
    DO WHILE nextc * 16 < spos + W + 16  ' the next letter into the strip, one cell ahead of the screen
      cx = (nextc * 16) MOD W
      FRAMEBUFFER WRITE F
      BOX cx, 120, 16, 32, 0, cl(0), cl(0)
      TEXT cx, 124, MID$(m$, (nextc MOD LEN(m$)) + 1, 1), "LT", 1, 2, cl(12), -1
      FRAMEBUFFER WRITE N
      nextc = nextc + 1
    LOOP
    IF fr <= 25 THEN
      setpal pal(), fr / 25
    ELSEIF fr MOD 3 = 0 THEN
      cyc = cyc + 1
      FOR c = 0 TO 7 : MAP(4 + c) = rbw((c + cyc) MOD 8) : NEXT
    ENDIF
    IF fr <= 25 OR fr MOD 3 = 0 THEN MAP SET ELSE FRAMEBUFFER WAIT  ' MAP SET waits for the frame too
    FRAMEBUFFER WARPH F, N, so(), 1, 0, , , , sr()        ' the stars
    FRAMEBUFFER WARPH F, 2, sco(), 32, 100, , , , ssr()   ' the strip, scrolled, to rows 100-131 of 2
    FRAMEBUFFER WRITE 2
    BOX 0, 100, 16, 32, 0, cl(0), cl(0) : BOX W - 16, 100, 16, 32, 0, cl(0), cl(0)
    FRAMEBUFFER WRITE N
    FRAMEBUFFER WARPV 2, T, sw(), 2, 0, fr * 2            ' waved into the top layer
    FRAMEBUFFER WARPV F, L, lw(), 2, 0, fr, -1            ' the logo waved into the layer
    IF INKEY$ <> "" THEN quit = 1
  LOOP UNTIL quit OR fr > 1500
END SUB

' ---------------------------------------------------------------- 2: the lake
SUB scene2
  LOCAL day(15) = (0, &H3070D0, &H5090E0, &H80B0F0, &HB0D0F0, &HD8E8F8, &HFFFFE0, &H6080A8, &H405878, &H183820, &H90B0D0, &HB8D0E8, &HFFFFFF, &H385068, &H102818, &H2850A0)
  LOCAL dusk(15) = (0, &H201848, &H503070, &HA04870, &HF07050, &HFFB040, &HFFE8A0, &H402850, &H281838, &H081008, &HA05050, &HC08848, &HFFE0A0, &H301830, &H040804, &H101838)
  LOCAL night(15) = (0, &H000010, &H080828, &H181838, &H302048, &H603050, &HC08060, &H101020, &H080810, &H000000, &H201828, &H402838, &H806050, &H080810, &H000000, &H000008)
  LOCAL tcn(9) = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9)
  LOCAL tcr(9) = (0, 15, 15, 10, 10, 11, 12, 13, 13, 14)
  LOCAL bgo(0), hlo(0), hlr(0), rbr(89), rhr(49), rwo(89), hwo(49)
  LOCAL wsn!(89), wcs!(89), wa!(89), wb!(89), wc!(89), hsn!(49), hcs!(49), ha!(49), hb!(49), hc!(49)
  LOCAL k, fr, ph!, amp!, f!, br!, gl!
  lerppal day(), dusk(), 0, 0
  MAP SET
  FRAMEBUFFER WRITE L : CLS : FRAMEBUFFER WRITE T : CLS
  FRAMEBUFFER WRITE 2 : CLS cl(0) : drawland tcn()     ' the land in 2 (hills in rows 150-199)
  FRAMEBUFFER WRITE F : CLS cl(0) : drawland tcr()     ' and in the water's colours in F
  FRAMEBUFFER WRITE N
  hlr(0) = 150
  FOR k = 0 TO 89
    rbr(k) = 149 - k                ' the reflection is the land upside down
    amp! = 0.3 + k * 0.075          ' ripples grow towards the viewer...
    ph! = 9 * SQR(k + 1)            ' ...and spread out
    wsn!(k) = amp! * SIN(ph!) : wcs!(k) = amp! * COS(ph!)
    IF k < 50 THEN rhr(k) = 199 - k : hsn!(k) = wsn!(k) : hcs!(k) = wcs!(k)
  NEXT
  DO
    fr = fr + 1
    bgo(0) = -fr \ 4 : hlo(0) = -(fr * 7) \ 10
    ' amp*sin(ph + wt) = amp*sin(ph)*cos(wt) + amp*cos(ph)*sin(wt): two MATH SCALEs and a C_ADD
    MATH SCALE wsn!(), COS(fr * 0.12), wa!() : MATH SCALE wcs!(), SIN(fr * 0.12), wb!()
    MATH C_ADD wa!(), wb!(), wc!() : MATH ADD wc!(), bgo(0), wa!() : MATH SCALE wa!(), 1, rwo()
    MATH SCALE hsn!(), COS(fr * 0.12), ha!() : MATH SCALE hcs!(), SIN(fr * 0.12), hb!()
    MATH C_ADD ha!(), hb!(), hc!() : MATH ADD hc!(), hlo(0), ha!() : MATH SCALE ha!(), 1, hwo()
    IF fr MOD 4 = 1 THEN  ' the sky from day to dusk to night
      br! = MIN(fr / 25, 1)
      f! = fr / 700
      IF f! < 1 THEN lerppal day(), dusk(), f!, br! ELSE lerppal dusk(), night(), MIN(f! - 1, 1), br!
    ENDIF
    gl! = 150 + RND * 105 : MAP(12) = RGB(gl!, gl! * 0.85, gl! * 0.45)  ' the sun glints on the water
    MAP SET                                                 ' (and the frame sync)
    FRAMEBUFFER WARPH 2, N, bgo(), 150                      ' sky and mountains, slowly
    FRAMEBUFFER WARPH 2, N, hlo(), 50, 100, , , 0, hlr()    ' hills and trees, faster, over them
    FRAMEBUFFER WARPH F, N, rwo(), 1, 150, , , , rbr()      ' their reflection, rippling
    FRAMEBUFFER WARPH F, N, hwo(), 1, 150, , , 0, rhr()
    IF INKEY$ <> "" THEN quit = 1
  LOOP UNTIL quit OR fr > 1500
END SUB

' the land, its colours through tc(): sky in rows 0-149, hills and trees (seen at 100-149) in 150-199
SUB drawland tc()
  LOCAL x, y, b, j, r, tx, tb, th
  FOR y = 0 TO 149
    b = y \ 30
    IF (y MOD 30) >= 22 AND (y MOD 2) = 0 AND b < 4 THEN b = b + 1
    LINE 0, y, W - 1, y, 1, cl(tc(1 + b))
  NEXT
  CIRCLE 210, 112, 15, 1, 1, cl(tc(6)), cl(tc(6))
  FOR x = 0 TO W - 1  ' ridges whose sines repeat every 320 pixels, as the rows wrap
    r = 98 + 14 * SIN(2 * PI * x / W * 2 + 0.5) + 7 * SIN(2 * PI * x / W * 5 + 1.3) + 3 * SIN(2 * PI * x / W * 11)
    LINE x, r, x, 149, 1, cl(tc(7))
    r = 124 + 8 * SIN(2 * PI * x / W * 3 + 2) + 4 * SIN(2 * PI * x / W * 7 + 0.2)
    LINE x, r, x, 149, 1, cl(tc(8))
    r = 139 + 3 * SIN(2 * PI * x / W * 4 + 1)
    LINE x, r + 50, x, 199, 1, cl(tc(9))
  NEXT
  RANDOMIZE 77  ' the same trees in both copies, each wholly on the screen (a shape drawn partly off it
  FOR j = 1 TO 30 ' leaves a line in column 0, which the warp then shows)
    th = 8 + INT(RND * 16) : tx = th + INT(RND * (W - 2 * th)) : tb = 140 + INT(RND * 8)
    TRIANGLE tx, tb - th + 50, tx - th \ 3, tb + 50, tx + th \ 3, tb + 50, cl(tc(9)), cl(tc(9))
  NEXT
  RANDOMIZE TIMER
END SUB

' ---------------------------------------------------------------- 3: the road
SUB scene3
  LOCAL pal(15) = (0, &H1850B8, &H3C80D8, &H88C0F0, &H5868A0, &HF0F0FF, &H48B848, &H389038, &H9C9C9C, &H888888, &HE02020, &HF0F0F0, &HFFFFFF, &HD01818, &H202028, &HFFD040)
  LOCAL zz(119), cp!(69), sp!(69), t1!(69), t2!(69), t3!(69), ro(69), rm(69), sko(0), skr(0)
  LOCAL x, y, r, s, k, j, cx, cy, fr, hy, rr, dist, yy
  LOCAL hw!, kw!, lnw!, curve!, camx!, pan!
  setpal pal(), 0 : MAP SET
  FRAMEBUFFER WRITE T : CLS
  ' F: the road, rows 0-119 with light segments (red kerbs, lane line), 120-239 dark ones (white kerbs)
  FRAMEBUFFER WRITE F
  FOR r = 0 TO 119
    hw! = 2 + r * 0.95 : kw! = 1 + r * 0.13 : lnw! = r * 0.025
    zz(r) = 2400 \ (r + 6)          ' how far away road row r is
    FOR s = 0 TO 1
      yy = r + 120 * s
      LINE 0, yy, W - 1, yy, 1, cl(6 + s)
      LINE 160 - hw! - kw!, yy, 160 + hw! + kw!, yy, 1, cl(10 + s)
      LINE 160 - hw!, yy, 160 + hw!, yy, 1, cl(8 + s)
      IF s = 0 THEN LINE 160 - lnw!, yy, 160 + lnw!, yy, 1, cl(12)
    NEXT
  NEXT
  ' 2: sky, clouds and mountains, the horizon at row 139; grass below it to row 179
  FRAMEBUFFER WRITE 2 : CLS cl(1)
  FOR y = 20 TO 139
    k = (y - 20) \ 40
    IF ((y - 20) MOD 40) >= 30 AND (y MOD 2) = 0 AND k < 2 THEN k = k + 1
    LINE 0, y, W - 1, y, 1, cl(1 + k)
  NEXT
  FOR j = 1 TO 7  ' clouds, each wholly on the screen (as the trees in drawland)
    cx = 24 + INT(RND * (W - 80)) : cy = 35 + INT(RND * 40)
    CIRCLE cx, cy, 6, 1, 2.2, cl(5), cl(5) : CIRCLE cx + 14, cy - 4, 8, 1, 2, cl(5), cl(5)
    CIRCLE cx + 30, cy, 6, 1, 2.2, cl(5), cl(5)
  NEXT
  FOR x = 0 TO W - 1
    r = 114 + 11 * SIN(2 * PI * x / W * 3 + 0.4) + 8 * SIN(2 * PI * x / W * 7 + 2)
    LINE x, r, x, 139, 1, cl(4)
    IF r < 108 THEN LINE x, r, x, r + 2, 1, cl(5)
  NEXT
  BOX 0, 140, W, 40, 0, cl(6), cl(6)
  ' L: the car, and the speed
  FRAMEBUFFER WRITE L : CLS
  BOX 110, 228, 100, 8, 0, cl(14), cl(14)
  BOX 112, 212, 18, 20, 0, cl(14), cl(14) : BOX 190, 212, 18, 20, 0, cl(14), cl(14)
  BOX 116, 196, 88, 26, 0, cl(13), cl(13)
  BOX 132, 183, 56, 14, 0, cl(13), cl(13) : BOX 136, 186, 48, 9, 0, cl(14), cl(14)
  BOX 110, 191, 100, 4, 0, cl(14), cl(14)
  BOX 120, 201, 16, 6, 0, cl(15), cl(15) : BOX 184, 201, 16, 6, 0, cl(15), cl(15)
  BOX 150, 210, 20, 6, 0, cl(11), cl(11)
  BOX 2, 3, 124, 16, 0, cl(14), cl(14)  ' a dark panel, so the clouds behind cannot hide it
  TEXT 6, 6, "SPEED", "LT", 1, 1, cl(12), cl(14)
  FRAMEBUFFER WRITE N
  FOR k = 0 TO 69
    cp!(k) = (1 - k / 69) ^ 2 * 110     ' a curve moves the far rows most
    sp!(k) = (k + 1) / 70               ' the car's place across the road, the near rows most
  NEXT
  DO
    fr = fr + 1
    curve! = 1.3 * SIN(fr * 0.009) + 0.7 * SIN(fr * 0.023 + 1)
    camx! = curve! * 12 + 6 * SIN(fr * 0.041)
    hy = 120 + 16 * SIN(fr * 0.013)     ' the horizon: hills
    dist = dist + 5
    pan! = pan! - curve! * 1.4
    sko(0) = pan! : skr(0) = 140 - hy    ' the sky as one band, moved with the horizon
    MATH SCALE cp!(), curve!, t1!() : MATH SCALE sp!(), -camx!, t2!()
    MATH C_ADD t1!(), t2!(), t3!() : MATH SCALE t3!(), 1, ro()
    FOR k = 0 TO 69  ' each 2-row band: its road row (squeezed by the hill) in the light or dark copy
      y = 100 + 2 * k
      IF y < hy THEN
        rm(k) = -1
      ELSE
        rr = MIN((y - hy) * 120 \ (240 - hy), 118)
        rm(k) = rr + 120 * (((zz(rr) + dist) >> 4) AND 1)
      ENDIF
    NEXT
    IF fr <= 25 THEN setpal pal(), fr / 25
    IF fr MOD 10 = 0 THEN
      FRAMEBUFFER WRITE L
      TEXT 54, 6, STR$(240 + INT(20 * SIN(fr * 0.02)), 3) + " km/h", "LT", 1, 1, cl(15), cl(14)
      FRAMEBUFFER WRITE N
    ENDIF
    IF fr <= 25 THEN MAP SET ELSE FRAMEBUFFER WAIT
    FRAMEBUFFER WARPH 2, N, sko(), 140, 0, , , , skr()     ' sky and mountains
    FRAMEBUFFER WARPH F, N, ro(), 2, 100, , , , rm()       ' the road
    IF INKEY$ <> "" THEN quit = 1
  LOOP UNTIL quit OR fr > 2000
END SUB

' ---------------------------------------------------------------- 4: the credits
SUB scene4
  LOCAL pal(15) = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &H404040, &HFFFFFF)
  LOCAL hx(239), vy(159), k, fr, cyc, c
  FOR k = 4 TO 11 : pal(k) = rbw(k - 4) : NEXT
  setpal pal(), 0 : MAP SET
  FRAMEBUFFER WRITE L : CLS : FRAMEBUFFER WRITE T : CLS
  FRAMEBUFFER WRITE F : CLS cl(0)
  TEXT 160, 30, "FRAMEBUFFER", "CT", 1, 2, cl(4), -1
  TEXT 160, 58, "WARPH  &  WARPV", "CT", 1, 2, cl(6), -1
  TEXT 160, 100, "every row or column of a picture", "CT", 1, 1, cl(15), -1
  TEXT 160, 116, "moved by its own offset,", "CT", 1, 1, cl(15), -1
  TEXT 160, 132, "read from an integer array", "CT", 1, 1, cl(15), -1
  TEXT 160, 164, "PicoMite V8", "CT", 1, 2, cl(8), -1
  TEXT 160, 206, "press any key", "CT", 1, 1, cl(14), -1
  FRAMEBUFFER WRITE N
  FOR k = 0 TO 239 : hx(k) = 7 * SIN(2 * PI * k / 80) : NEXT
  FOR k = 0 TO 159 : vy(k) = 6 * SIN(2 * PI * k / 80) : NEXT
  DO
    fr = fr + 1
    IF fr <= 25 THEN
      setpal pal(), fr / 25
    ELSEIF fr MOD 4 = 0 THEN
      cyc = cyc + 1
      FOR c = 0 TO 7 : MAP(4 + c) = rbw((c + cyc) MOD 8) : NEXT
    ENDIF
    IF fr <= 25 OR fr MOD 4 = 0 THEN MAP SET ELSE FRAMEBUFFER WAIT
    FRAMEBUFFER WARPH F, 2, hx(), 1, 0, fr * 2      ' each row sideways...
    FRAMEBUFFER WARPV 2, N, vy(), 2, 0, fr * 3      ' ...then each column up and down
    IF INKEY$ <> "" THEN quit = 1
  LOOP UNTIL quit OR fr > 900
END SUB

' ---------------------------------------------------------------- the melt between scenes
SUB melt
  LOCAL st!(159), tm!(159), dr(159), z(0), k, fr
  FRAMEBUFFER COPY N, F
  FRAMEBUFFER WARPH L, F, z(), H, , , , 0  ' the layers into the picture, so that they melt with it
  FRAMEBUFFER WARPH T, F, z(), H, , , , 0
  FRAMEBUFFER WRITE L : CLS : FRAMEBUFFER WRITE T : CLS : FRAMEBUFFER WRITE N
  FOR k = 0 TO 159 : st!(k) = RND * 40 : NEXT
  FOR fr = 1 TO 80
    MATH ADD st!(), -0.35 * fr * fr, tm!()
    MATH CLAMP tm!(), -H, 0, dr()      ' each column waits a little, then falls faster and faster
    FRAMEBUFFER WAIT
    CLS cl(0)
    FRAMEBUFFER WARPV F, N, dr(), 2, , , -1
    IF INKEY$ <> "" THEN quit = 1
    IF quit OR MATH(MAX dr()) <= -H THEN EXIT FOR
  NEXT
  CLS cl(0)
END SUB

' the 16 colours of p() at brightness f!, staged: MAP SET (which waits for the frame) commits them
SUB setpal p(), f!
  LOCAL n
  FOR n = 0 TO 15
    MAP(n) = RGB(((p(n) >> 16) AND 255) * f!, ((p(n) >> 8) AND 255) * f!, (p(n) AND 255) * f!)
  NEXT
END SUB

' the 16 colours a fraction f! of the way from a() to b(), at brightness br! (MAP SET by the caller)
SUB lerppal a(), b(), f!, br!
  LOCAL n, r, g, bl
  FOR n = 0 TO 15
    r = (((a(n) >> 16) AND 255) * (1 - f!) + ((b(n) >> 16) AND 255) * f!) * br!
    g = (((a(n) >> 8) AND 255) * (1 - f!) + ((b(n) >> 8) AND 255) * f!) * br!
    bl = ((a(n) AND 255) * (1 - f!) + (b(n) AND 255) * f!) * br!
    MAP(n) = RGB(r, g, bl)
  NEXT
END SUB
