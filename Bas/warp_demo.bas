' warp_demo.bas - FRAMEBUFFER WARPH, WARPV, AFFINE and SPAN: the rows and columns of a picture moved, turned,
' scaled and cut to shape, each by its own entry in an integer array
' PicoMite V8, VGA or HDMI, RP2350 (it uses framebuffers F and 2 and both layers), MODE 2 at 320x240.
'  1. 240 stars, each row at its own speed; a logo waving in the layer; a sine scroller in the top layer
'  2. a lake from day to dusk: parallax hills, and a reflection that ripples more the nearer it is
'  3. a road in the style of the arcade racers: curves, hills, kerbs, and the mountains panning
'  4. a playing card turned through any angle by three shears (WARPH, WARPV, WARPH); it arrives and
'     leaves as a mosaic (BLIT RESIZE shrinks it, and a WARPH row map stretches it back)
'  5. a map turned and zoomed: one AFFINE, from four arrays of one element
'  6. the same map as the ground, seen from a car going round the track: an AFFINE element for each row
'     below the horizon; the sky is a WARPH, and the compass a picture turned through AFFINE's window
'  7. a torch in the dark, and an iris that closes and opens: SPAN, a left and a right column for each row
'  8. the credits, waved both ways (a WARPH into a WARPV)
' Each picture melts away column by column into the next. Any key ends it.
' No per-pixel BASIC in the frame loops: each layer is one command, and the tables are made with MATH
' (SCALE, ADD, C_ADD, CLAMP, SLICE, POWER) or animated with the phase argument.
OPTION EXPLICIT
OPTION DEFAULT INTEGER
MODE 2
IF MM.HRES <> 320 OR MM.VRES <> 240 THEN
  MODE 1
  PRINT "This demo needs MODE 2 at 320x240 (OPTION RESOLUTION 640x480)"
  END
ENDIF
CONST W = 320, H = 240, FP = 65536  ' FP: AFFINE's places and steps are in 1/65536 of a pixel
DIM cl(15), quit, i
FOR i = 0 TO 15 : cl(i) = MAP(i) : NEXT  ' the colour that draws in each of the 16 palette slots
DIM rbw(7) = (&HFF2020, &HFF9000, &HFFF000, &H30FF30, &H20E0FF, &H3070FF, &HA040FF, &HFF40C0)
' scenes 5 to 7: the map's palette, and its colours lit and in the dark (ground, its squares, road, kerb, water)
DIM mpal(15) = (0, &H1C7A30, &H2A9A40, &H707070, &HE8E8E8, &H2850D0, &H4070D8, &H78A8F0, &HE02020, &H506090, &HC8D8F0, &H901010, &HFFD820, &H303030, &H0A3012, &HFFFFFF)
DIM mlit(4) = (1, 2, 3, 4, 5), mdark(4) = (14, 0, 13, 13, 0)
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
  IF NOT quit THEN scene5
  IF NOT quit THEN melt
  IF NOT quit THEN scene6
  IF NOT quit THEN melt
  IF NOT quit THEN scene7
  IF NOT quit THEN melt
  IF NOT quit THEN scene8
  IF NOT quit THEN melt
LOOP UNTIL quit
FRAMEBUFFER CLOSE
MAP RESET
MODE 1
PRINT "FRAMEBUFFER WARPH, WARPV, AFFINE and SPAN demo ended"
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

' ---------------------------------------------------------------- 4: the card
' A turn by the angle a is three shears:
'  - every row moved sideways by -TAN(a / 2) times its distance from the middle row;
'  - every column moved down by SIN(a) times its distance from the middle column;
'  - the rows again, as the first time.
' Each shear is one WARP, so the picture goes F -> 2 -> N -> T, and no pixel is lost or made on the way.
' N is only a workspace here: the layer L, the table, has no transparent pixel, so it hides N.
' A constant added to a shear's table moves the picture as well as turning it.
' The picture must stay on the screen through all three shears, because a row that wraps is sheared
' wrongly: for -90 to 90 degrees that is a picture within 113 pixels of the middle of the screen.
' A playing card is the same upside down, so -90 to 90 degrees is every angle it needs.
SUB scene4
  LOCAL pal(15) = (0, &H0C5830, &H126A3C, 0, 0, 0, 0, 0, &HE01020, 0, 0, &HC0E8D0, &HFFD040, &H203050, &H282828, &HFFFFFF)
  LOCAL ms(7) = (2, 4, 5, 8, 10, 16, 20, 40), mt(H - 1, 7), mr(H - 1), z(H - 1)
  LOCAL ry!(H - 1), rx!(W - 1), ha(H - 1), hb(H - 1), va(W - 1)
  LOCAL k, n, x, x2, y, fr, dx, dy, a!, t!, e!, c$
  setpal pal(), 0 : MAP SET
  FRAMEBUFFER WRITE T : CLS
  ' L: the table, every pixel of it drawn (slot 0 is the layer's transparent colour)
  FRAMEBUFFER WRITE L : CLS cl(1)
  FOR k = -220 TO 300 STEP 20  ' a lattice, each line cut to the screen
    x = MAX(k, 0) : x2 = MIN(k + 239, W - 1)
    LINE x, x - k, x2, x2 - k, 1, cl(2) : LINE x, 239 - (x - k), x2, 239 - (x2 - k), 1, cl(2)
  NEXT
  TEXT 160, 4, "A MOSAIC, AND A TURN BY THREE SHEARS", "CT", 1, 1, cl(12), cl(1)
  ' F: the card, on slot 0. Each part is drawn twice, the second time turned 180 degrees about (160, 120)
  FRAMEBUFFER WRITE F : CLS cl(0)
  RBOX 105, 43, 111, 155, 8, cl(14), cl(15)
  RBOX 109, 47, 103, 147, 6, cl(8), -1
  pip 160, 120, 15, 21
  pip 160, 68, 11, 15 : pip 160, 172, 11, 15
  pip 121, 78, 4, 6 : pip 199, 162, 4, 6
  TEXT 121, 54, "3", "CT", 1, 1, cl(8), -1 : TEXT 199, 186, "3", "CTI", 1, 1, cl(8), -1
  TEXT 160, 97, "PicoMite", "CB", 1, 1, cl(13), -1 : TEXT 160, 143, "PicoMite", "CBI", 1, 1, cl(13), -1
  FRAMEBUFFER WRITE N
  FOR y = 0 TO H - 1 : ry!(y) = y - 120 : NEXT
  FOR x = 0 TO W - 1 : rx!(x) = x - 160 : NEXT
  FOR k = 0 TO 7  ' the mosaic's row map for each size of block: the rows from 120 on, each shown ms(k) times
    FOR y = 0 TO H - 1 : mt(y, k) = 120 + y \ ms(k) : NEXT
  NEXT
  DO
    fr = fr + 1
    IF fr <= 25 THEN setpal pal(), fr / 25
    IF fr = 1249 THEN FRAMEBUFFER COPY T, F  ' the card as it stopped, to leave as it came
    IF fr = 1 OR fr = 49 OR fr = 1249 THEN   ' the table says which commands are at work
      c$ = CHOICE(fr = 49, "WARPH, then WARPV, then WARPH", "BLIT RESIZE twice, then WARPH")
      FRAMEBUFFER WRITE L : TEXT 160, 224, c$, "CT", 1, 1, cl(11), cl(1) : FRAMEBUFFER WRITE N
    ENDIF
    IF fr <= 48 OR fr >= 1249 THEN
      ' A mosaic of n x n blocks: the picture shrunk to 1/n in the corner of 2, then made full width
      ' again in the rows from 120 on, and then a WARPH whose row map shows each of those rows n times.
      IF fr <= 48 THEN k = 7 - (fr - 1) \ 6 ELSE k = (fr - 1249) \ 6
      n = ms(k)
      BLIT RESIZE F, 2, 0, 0, W, H, 0, 0, W \ n, H \ n
      BLIT RESIZE 2, 2, 0, 0, W \ n, H \ n, 0, 120, W, H \ n
      MATH SLICE mt(), , k, mr()
      IF fr <= 25 THEN MAP SET ELSE FRAMEBUFFER WAIT
      FRAMEBUFFER WARPH 2, T, z(), 1, 0, , , , mr()
    ELSE
      e! = MIN((fr - 48) / 150, 1)                 ' it starts gently
      a! = a! + e! * (1.7 + 1.2 * SIN(fr * 0.012)) ' degrees clockwise
      IF a! > 90 THEN a! = a! - 180                ' the same card, by its symmetry
      dx = e! * 50 * SIN(fr * 0.019) : dy = e! * 14 * SIN(fr * 0.031)
      t! = TAN(RAD(a!) / 2)
      MATH SCALE ry!(), -t!, ha() : MATH ADD ha(), dx + t! * dy, hb()    ' the rows: first and third shears
      MATH SCALE rx!(), -SIN(RAD(a!)), va() : MATH ADD va(), -dy, va()  ' the columns: the second
      FRAMEBUFFER WARPH F, 2, ha()
      FRAMEBUFFER WARPV 2, N, va()
      FRAMEBUFFER WAIT
      FRAMEBUFFER WARPH N, T, hb()
    ENDIF
    IF INKEY$ <> "" THEN quit = 1
  LOOP UNTIL quit OR fr >= 1296
END SUB

' a diamond, which is the same upside down
SUB pip x, y, a, b
  TRIANGLE x - a, y, x + a, y, x, y - b, cl(8), cl(8)
  TRIANGLE x - a, y, x + a, y, x, y + b, cl(8), cl(8)
END SUB

' ---------------------------------------------------------------- 5: the map turned and zoomed
' AFFINE reads each row of the screen from the source along a straight line: its first pixel shows the
' source's (u, v) / 65536, and each pixel to the right is (du, dv) / 65536 further on. With one element and
' step = the screen's height, each next row starts one step on at right angles, (-dv, du): so these four
' numbers turn and zoom the whole picture. The screen's centre shows the map's (sx!, sy!).
SUB scene5
  LOCAL u(0), v(0), du(0), dv(0), fr, a!, z!, sx!, sy!
  setpal mpal(), 0 : MAP SET
  FRAMEBUFFER WRITE T : CLS
  FRAMEBUFFER WRITE L : CLS
  caption "A MAP TURNED AND ZOOMED", "one AFFINE, four arrays of one element"
  FRAMEBUFFER WRITE F : drawmap mlit()
  FRAMEBUFFER WRITE N
  DO
    fr = fr + 1
    IF fr <= 25 THEN setpal mpal(), fr / 25
    a! = fr * 0.017 : z! = 1.5 + 1.1 * SIN(fr * 0.021)  ' the angle, and the size
    sx! = 160 + 70 * COS(fr * 0.011) : sy! = 120 + 50 * SIN(fr * 0.014)
    du(0) = FP * COS(a!) / z! : dv(0) = -FP * SIN(a!) / z!
    u(0) = FP * sx! - 160 * du(0) + 120 * dv(0)
    v(0) = FP * sy! - 160 * dv(0) - 120 * du(0)
    IF fr <= 25 THEN MAP SET ELSE FRAMEBUFFER WAIT
    FRAMEBUFFER AFFINE F, N, u(), v(), du(), dv(), H
    IF INKEY$ <> "" THEN quit = 1
  LOOP UNTIL quit OR fr >= 900
END SUB

' ---------------------------------------------------------------- 6: the map as the ground
' Row hz + 1 + k of the screen is zt!(k) map pixels wide for each of its own, which is made once. Each frame,
' six MATH statements turn that table into the four arrays for where the camera is and which way it looks:
' an element for each row, from a line through the map at the camera's distance for that row.
SUB scene6
  LOCAL hz = 90, n = 149                       ' the horizon's row, and the rows below it
  LOCAL zt!(n - 1), fu(n - 1), fv(n - 1), fdu(n - 1), fdv(n - 1), so(0), u(0), v(0), du(0), dv(0)
  LOCAL k, x, r, fr, t!, a!, ca!, sa!, camx!, camy!, d! = 160
  setpal mpal(), 0 : MAP SET
  FRAMEBUFFER WRITE T : CLS
  FRAMEBUFFER WRITE L : CLS
  caption "THE MAP AS THE GROUND", "an AFFINE element for each row"
  FRAMEBUFFER WRITE F : drawmap mlit()
  ' 2: the sky and the mountains in rows 0-89, which repeat every 320 pixels; the compass alone lower down
  FRAMEBUFFER WRITE 2 : CLS cl(0)
  BOX 0, 0, W, 50, 0, cl(6), cl(6) : BOX 0, 50, W, 40, 0, cl(7), cl(7)
  FOR x = 0 TO W - 1
    r = 62 + 12 * SIN(2 * PI * x / W * 3 + 0.4) + 7 * SIN(2 * PI * x / W * 7 + 2)
    LINE x, r, x, 89, 1, cl(9)
    IF r < 58 THEN LINE x, r, x, r + 2, 1, cl(10)
  NEXT
  CIRCLE 124, 168, 22, 2, 1, cl(15), cl(13)
  TRIANGLE 124, 148, 114, 170, 134, 170, cl(8), cl(8)
  TRIANGLE 124, 188, 114, 170, 134, 170, cl(4), cl(4)
  FRAMEBUFFER WRITE N : CLS cl(10)               ' row 90, between the sky and the ground, stays as haze
  FOR k = 0 TO n - 1 : zt!(k) = 14 / (k + 1) : NEXT
  DO
    fr = fr + 1
    IF fr <= 25 THEN setpal mpal(), fr / 25
    t! = fr * 0.011                              ' round the track
    camx! = 160 + 118 * COS(t!) : camy! = 120 + 74 * SIN(t!)
    a! = ATAN2(74 * COS(t!), -118 * SIN(t!)) + 0.25 * SIN(fr * 0.05)
    ca! = COS(a!) : sa! = SIN(a!)
    MATH SCALE zt!(), -FP * sa!, fdu() : MATH SCALE zt!(), FP * ca!, fdv()
    MATH SCALE zt!(), FP * (d! * ca! + 160 * sa!), fu() : MATH ADD fu(), FP * camx!, fu()
    MATH SCALE zt!(), FP * (d! * sa! - 160 * ca!), fv() : MATH ADD fv(), FP * camy!, fv()
    so(0) = -a! * 4 * W / (2 * PI)               ' the screen shows a quarter of the way round
    ' the compass, turned as the camera is: a 69x69 window round the 48x48 picture at (124, 168) of 2
    du(0) = FP * COS(a! + PI / 2) : dv(0) = -FP * SIN(a! + PI / 2)
    u(0) = FP * 124 - 34 * (du(0) - dv(0)) : v(0) = FP * 168 - 34 * (dv(0) + du(0))
    IF fr <= 25 THEN MAP SET ELSE FRAMEBUFFER WAIT
    FRAMEBUFFER WARPH 2, N, so(), hz
    FRAMEBUFFER AFFINE F, N, fu(), fv(), fdu(), fdv(), 1, hz + 1
    FRAMEBUFFER AFFINE 2, N, u(), v(), du(), dv(), 69, 4, 247, 69, -1, 0
    IF INKEY$ <> "" THEN quit = 1
  LOOP UNTIL quit OR fr >= 1200
END SUB

' ---------------------------------------------------------------- 7: a torch, and an iris
' SPAN copies, on each row, only the columns from its left to its right entry. The torch is a circle of one
' size, so its half-widths are made once for its 2 * r + 1 rows: three MATH statements move it across, and
' SPAN's first row moves it down (and off the screen). The iris is a circle whose size changes, so its
' half-widths are made each frame, with MATH POWER for the square root; SPAN fills the rest with black.
SUB scene7
  LOCAL r = 46
  LOCAL hw!(2 * r), q!(2 * r), lf(2 * r), rt(2 * r)
  LOCAL d2!(H - 1), wk!(H - 1), ih!(H - 1), iq!(H - 1), il(H - 1), ir(H - 1)
  LOCAL k, fr, cx, cy, ri
  setpal mpal(), 0 : MAP SET
  FRAMEBUFFER WRITE T : CLS
  FRAMEBUFFER WRITE L : CLS
  caption "A TORCH IN THE DARK", "SPAN: two columns for each row"
  FRAMEBUFFER WRITE F : drawmap mlit()
  FRAMEBUFFER WRITE 2 : drawmap mdark()
  FRAMEBUFFER WRITE N
  FOR k = 0 TO 2 * r : hw!(k) = SQR(r * r - (k - r) * (k - r)) : NEXT
  FOR k = 0 TO H - 1 : d2!(k) = (k - 120) * (k - 120) : NEXT
  DO
    fr = fr + 1
    IF fr <= 25 THEN setpal mpal(), fr / 25
    IF fr = 701 THEN
      FRAMEBUFFER WRITE L : CLS
      caption "AN IRIS", "SPAN, and a fill colour for the rest"
      FRAMEBUFFER WRITE N
    ENDIF
    IF fr <= 700 THEN
      cx = 160 + 150 * SIN(fr * 0.021) : cy = 120 + 130 * SIN(fr * 0.033)
      MATH SCALE hw!(), -1, q!() : MATH ADD q!(), cx, lf() : MATH ADD hw!(), cx - 1, rt()
      IF fr <= 25 THEN MAP SET ELSE FRAMEBUFFER WAIT
      FRAMEBUFFER COPY 2, N
      FRAMEBUFFER SPAN F, N, lf(), rt(), 1, cy - r
    ELSE
      ' a circle of radius ri about (160, 120): its half-width on row y is SQR(ri^2 - (y - 120)^2), or 0
      ri = ABS(fr - 850) * 1.4
      MATH SCALE d2!(), -1, wk!() : MATH ADD wk!(), ri * ri, wk!() : MATH CLAMP wk!(), 0, 1000000, wk!()
      MATH POWER wk!(), 0.5, ih!()
      MATH SCALE ih!(), -1, iq!() : MATH ADD iq!(), 160, il() : MATH ADD ih!(), 159, ir()
      FRAMEBUFFER WAIT
      FRAMEBUFFER SPAN F, N, il(), ir(), 1, 0, cl(0)
    ENDIF
    IF INKEY$ <> "" THEN quit = 1
  LOOP UNTIL quit OR fr >= 1000
END SUB

' the map, in the colours tc() (ground, its squares, road, kerb, water): a track round a pond
SUB drawmap tc()
  LOCAL x, y, k
  CLS cl(tc(0))
  FOR y = 0 TO 220 STEP 20
    FOR x = ((y \ 20) AND 1) * 20 TO 300 STEP 40 : BOX x, y, 20, 20, 0, cl(tc(1)), cl(tc(1)) : NEXT
  NEXT
  CIRCLE 160, 120, 92, 36, 1.55, cl(tc(3))
  CIRCLE 160, 120, 88, 28, 1.55, cl(tc(2))
  FOR k = 0 TO 6 : BOX 156 + (k AND 1) * 4, 32 + k * 4, 4, 4, 0, cl(tc(3)), cl(tc(3)) : NEXT
  BOX 140, 104, 40, 32, 0, cl(tc(4)), cl(tc(4))
END SUB

' a title at the top of the layer, and a line under the picture (the map's palette: 12 yellow, 13 grey)
SUB caption t$, s$
  TEXT 160, 3, t$, "CT", 1, 1, cl(12), cl(13)
  TEXT 160, 226, s$, "CT", 1, 1, cl(15), cl(13)
END SUB

' ---------------------------------------------------------------- 8: the credits
SUB scene8
  LOCAL pal(15) = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &H404040, &HFFFFFF)
  LOCAL hx(239), vy(159), k, fr, cyc, c
  FOR k = 4 TO 11 : pal(k) = rbw(k - 4) : NEXT
  setpal pal(), 0 : MAP SET
  FRAMEBUFFER WRITE L : CLS : FRAMEBUFFER WRITE T : CLS
  FRAMEBUFFER WRITE F : CLS cl(0)
  TEXT 160, 22, "FRAMEBUFFER", "CT", 1, 2, cl(4), -1
  TEXT 160, 50, "WARPH  WARPV", "CT", 1, 2, cl(6), -1
  TEXT 160, 76, "AFFINE  SPAN", "CT", 1, 2, cl(10), -1
  TEXT 160, 112, "every row or column of a picture", "CT", 1, 1, cl(15), -1
  TEXT 160, 128, "moved, turned, scaled or cut", "CT", 1, 1, cl(15), -1
  TEXT 160, 144, "by tables in integer arrays", "CT", 1, 1, cl(15), -1
  TEXT 160, 174, "PicoMite V8", "CT", 1, 2, cl(8), -1
  TEXT 160, 212, "press any key", "CT", 1, 1, cl(14), -1
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
