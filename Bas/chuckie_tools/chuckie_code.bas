' =====================================================================
'  C H U C K I E   E G G        PicoMite MMBasic
'  after the 1983 BBC Micro game by A&F Software
'
'  Runs in MODE 2 (320 x 240, 16 colours) on VGA and HDMI builds, and on
'  any 320 x 240 LCD panel that supports FRAMEBUFFER.  MODE 2 is only
'  selected if the screen is not already 320 x 240.
'
'  Harry has to gather all twelve eggs on each floor before the clock runs
'  out while the hens patrol the girders.  An egg is worth 100 points on the
'  first four floors and another 100 every four after that.  A pile of grain
'  is worth 50 and stops the clock for a moment - it does not add time, and
'  a second pile restarts that pause rather than extending it.  The hens
'  stop to eat the grain too.  Riding a lift into the top of the shaft
'  costs a life, so get off before it wraps round.
'
'  The eight floors come round five times over, and each time the odds get
'  worse:
'
'    levels  1-8   the floor's own hens (3 or 4), duck still caged
'    levels  9-16  no hens at all, but the duck is loose and hunts Harry
'    levels 17-24  the hens are back, and the duck stays out with them
'    levels 25-32  five hens on every floor, plus the duck
'    levels 33-40  five hens and the duck, everything at full speed
'
'  Keys      Left / Right ... arrow keys, or Z and X
'            Up / Down ...... arrow keys (ladders)
'            Jump ........... SPACE
'            Quit ........... ESC
'
'  For testing: UP and DOWN on the title screen pick the level to start
'  on, and N during play skips straight to the next one.
'
'  How it matches the original
'  ---------------------------
'  The BBC screen is 256 rows: 32 for the status panel and 224 for the
'  floor.  Ours is 240, and the whole 16 row difference comes out of the
'  panel - the BBC set its two text rows in a 32 row band and our 6 x 8
'  font fits the same two rows in 16 - so the playfield keeps its full 224
'  rows.  The BBC is 160 pixels across against our 320, so everything
'  horizontal is doubled and the sprites, girders, ladders and jump arcs
'  come out the same size on screen as they did in 1983.
'
'  The eight floors were traced from screenshots of the real game; Harry,
'  the hens and the duck are lifted pixel for pixel off its title screen.
'  Sound is synthesised on the PicoMite's square, triangle and noise
'  generators in the style of the original.
' =====================================================================

OPTION EXPLICIT
OPTION DEFAULT NONE

' ------------------------------------------------------------ geometry
CONST SCRW = 320, SCRH = 240
CONST FONTN = 7, FW = 6, FH = 8   ' font 7 is the 6 x 8 one; font 1 is 8 x 12
CONST PLAYTOP = 16                ' first pixel row below the status panel
CONST CW = 16, CH = 8             ' one map cell
CONST GCOLS = 20, GROWS = 28      ' map size in cells
CONST FLTHK = 5                   ' a girder is five pixels deep
CONST HW = 16, HH = 18            ' Harry
CONST NW = 14, NH = 20            ' hen
CONST NHEAD = 6                   ' its raised head and neck are not lethal
CONST DW = 31, DH = 22            ' duck
CONST LW = 20, LH = 4             ' lift

' -------------------------------------------------------- map cell bits
CONST C_SOLID = 1, C_LADDER = 2, C_SHAFT = 4

' ------------------------------------------------------ sprite buffers
CONST S_STAND = 1, S_RUN1 = 2, S_RUN2 = 3, S_CLIMB1 = 4, S_CLIMB2 = 5
CONST S_DUCK1 = 6, S_DUCK2 = 7
CONST S_HEN1 = 8, S_HEN2 = 9, S_HEN3 = 10
CONST S_LIFT = 11, S_EGGA = 12, S_EGGB = 13, S_SEED = 14
CONST NSPR = 14

' --------------------------------------------------------------- limits
CONST MAXEGG = 12, MAXSEED = 16, MAXHEN = 5, MAXLIFT = 2
CONST S_HENA = 15                 ' MAXHEN copies each of walk A, walk B, peck
CONST S_LIFT2 = S_HENA + 3 * MAXHEN
CONST E_DUCK = MAXHEN + 1
CONST E_LIFT = MAXHEN + 2
CONST NENT = MAXHEN + 3           ' 0 Harry, 1..MAXHEN hens, duck, two lifts

' -------------------------------------------------------------- physics
CONST GRAV = 0.38
CONST JUMPV = -4.8
CONST WALKSPD = 2
CONST CLIMBSPD = 2
CONST MAXFALL = 7.0
CONST LIFTSPD = 0.6
CONST FRAMEMS = 33                ' about 30 frames a second
CONST ST_WALK = 0, ST_CLIMB = 1
CONST LADDERODDS = 0.03           ' chance a hen takes a ladder each frame
CONST PECKFRAMES = 45
' Eating grain sets the BBC's clock_stop_timer to &14, and .update_timers
' skips the whole clock while it runs down - so the clock pauses, it does
' not gain time, and a second pile resets the count rather than extending it.
CONST CLOCKFREEZE = 20
' Getting off a ladder: how far above a girder Harry may still step across.
CONST LEDGESNAP = 3

' ---------------------------------------------------------------- sound
CONST NSFX = 10
CONST SFX_EGG = 0, SFX_SEED = 1, SFX_JUMP = 2, SFX_DIE = 3, SFX_CLUCK = 4
CONST SFX_STEP = 5, SFX_TICK = 6, SFX_EXTRA = 7, SFX_QUACK = 8, SFX_LAND = 9
CONST MAXSTEP = 20
CONST NCHAN = 4

' ======================================================================
'  globals
' ======================================================================
DIM INTEGER grid(GROWS - 1, GCOLS - 1)
DIM lvl$(7, GROWS - 1) LENGTH 20
DIM INTEGER pal(15)
DIM INTEGER img(1023)
DIM hexd$ LENGTH 20

DIM INTEGER egX(MAXEGG - 1), egY(MAXEGG - 1), egOn(MAXEGG - 1)
DIM INTEGER nEgg, eggLeft
DIM INTEGER sdX(MAXSEED - 1), sdY(MAXSEED - 1), sdOn(MAXSEED - 1)
DIM INTEGER nSeed

DIM FLOAT henX(MAXHEN - 1), henY(MAXHEN - 1)
DIM INTEGER henDir(MAXHEN - 1), henSt(MAXHEN - 1), henVD(MAXHEN - 1)
DIM INTEGER henAnim(MAXHEN - 1), henPeck(MAXHEN - 1)
DIM INTEGER henX0(MAXHEN - 1), henY0(MAXHEN - 1)
DIM INTEGER nHen, nHen0, gIdx
DIM INTEGER henBase(7)

DIM FLOAT px, py, pvy
DIM INTEGER pvx, pState, pFace, pAnim, pGround, pLift, pKilled
DIM INTEGER startX, startY

DIM FLOAT dkX, dkY
DIM INTEGER dkOn, dkAnim, dkQuack, dkFace

DIM FLOAT lfY(MAXLIFT - 1)
DIM INTEGER lfN, lfX, lfTop, lfBot

DIM INTEGER entBuf(NENT), entWant(NENT), entX(NENT), entY(NENT)
DIM INTEGER entRot(NENT), entLay(NENT)

DIM INTEGER kLeft, kRight, kUp, kDown, kJump, kQuit, kSkip
DIM INTEGER gStart
DIM INTEGER gScore, gHi, gLives, gLevel, gLap, gTime, gBonus, gNextLife
DIM INTEGER panelCol, eggBuf, eggVal, clockStop
DIM FLOAT henSpd, dkSpd
DIM INTEGER sndOK

DIM INTEGER sfxF(NSFX - 1, MAXSTEP - 1), sfxD(NSFX - 1, MAXSTEP - 1)
DIM INTEGER sfxN(NSFX - 1), sfxV(NSFX - 1), sfxW(NSFX - 1), sfxCh(NSFX - 1)
DIM INTEGER chSfx(NCHAN - 1), chStep(NCHAN - 1), chLeft(NCHAN - 1)
DIM INTEGER chPri(NCHAN - 1)

' ======================================================================
'  main
' ======================================================================
Setup
DO
  TitleScreen
  PlayGame
LOOP
END

' ======================================================================
'  one-time set-up
' ======================================================================
SUB Setup
  LOCAL INTEGER i, j

  IF MM.HRES <> 320 OR MM.VRES <> 240 THEN
    ON ERROR SKIP 1
    MODE 2
  ENDIF
  IF MM.HRES <> 320 OR MM.VRES <> 240 THEN
    ON ERROR CLEAR
    PRINT "Chuckie Egg needs a 320 x 240 screen (MODE 2)."
    PRINT "This display is"; MM.HRES; " x"; MM.VRES
    END
  ENDIF
  ON ERROR CLEAR

  pal(0) = RGB(BLACK)    : pal(1) = RGB(BLUE)     : pal(2) = RGB(MYRTLE)
  pal(3) = RGB(COBALT)   : pal(4) = RGB(MIDGREEN) : pal(5) = RGB(CERULEAN)
  pal(6) = RGB(GREEN)    : pal(7) = RGB(CYAN)     : pal(8) = RGB(RED)
  pal(9) = RGB(MAGENTA)  : pal(10) = RGB(RUST)    : pal(11) = RGB(FUCHSIA)
  pal(12) = RGB(BROWN)   : pal(13) = RGB(LILAC)   : pal(14) = RGB(YELLOW)
  pal(15) = RGB(WHITE)
  hexd$ = "0123456789ABCDEF"

  SPRITE CLOSE ALL
  ON ERROR SKIP 1
  FRAMEBUFFER CREATE
  ON ERROR CLEAR
  FRAMEBUFFER WRITE F
  CLS RGB(BLACK)

  LoadSprites
  ' Working copies, so the four hens can animate independently while
  ' sharing one set of images, plus a second lift for the shafts that
  ' carry two of them.
  SPRITE COPY S_HEN1, S_HENA, MAXHEN
  SPRITE COPY S_HEN2, S_HENA + MAXHEN, MAXHEN
  SPRITE COPY S_HEN3, S_HENA + 2 * MAXHEN, MAXHEN
  SPRITE COPY S_LIFT, S_LIFT2, 1

  RESTORE hendata
  FOR i = 0 TO 7
    READ henBase(i)
  NEXT i

  RESTORE lvldata
  FOR i = 0 TO 7
    FOR j = 0 TO GROWS - 1
      READ lvl$(i, j)
    NEXT j
  NEXT i

  LoadSfx
  sndOK = 0
  ON ERROR SKIP 2
  PLAY SOUND 1, B, Q, 1000, 0
  PLAY SOUND 1, B, O
  IF MM.ERRNO = 0 THEN sndOK = 1
  ON ERROR CLEAR

  gHi = 1000
  gStart = 1
  FOR i = 0 TO NENT
    entBuf(i) = 0 : entWant(i) = 0
  NEXT i
END SUB

' ----------------------------------------------------------------------
'  Build every sprite from the packed bitmaps at the end of the listing.
'  Each DATA line is width, height, colour index and a hex string holding
'  <height> rows of ceil(width/4) digits, most significant bit on the left.
' ----------------------------------------------------------------------
SUB LoadSprites
  LOCAL INTEGER n, w, h, ci, col, j, nd, nyb, d, b, i, k
  LOCAL bits$ LENGTH 200
  RESTORE sprdata
  FOR n = 1 TO NSPR
    READ w, h, ci, bits$
    col = pal(ci)
    nyb = (w + 3) \ 4
    k = 0
    FOR j = 0 TO h - 1
      FOR nd = 0 TO nyb - 1
        d = INSTR(hexd$, MID$(bits$, j * nyb + nd + 1, 1)) - 1
        FOR b = 3 TO 0 STEP -1
          i = nd * 4 + 3 - b
          IF i < w THEN
            IF (d AND (1 << b)) <> 0 THEN img(k) = col ELSE img(k) = 0
            k = k + 1
          ENDIF
        NEXT b
      NEXT nd
    NEXT j
    SPRITE LOADARRAY n, w, h, img()
  NEXT n
END SUB

' ======================================================================
'  sound
' ======================================================================
SUB LoadSfx
  LOCAL INTEGER s, n, f, d
  RESTORE sfxdata
  FOR s = 0 TO NSFX - 1
    READ sfxCh(s), sfxW(s), sfxV(s)
    n = 0
    DO
      READ f, d
      IF f < 0 THEN EXIT DO
      sfxF(s, n) = f : sfxD(s, n) = d
      n = n + 1
    LOOP UNTIL n >= MAXSTEP
    sfxN(s) = n
  NEXT s
  SfxSilence
END SUB

SUB SfxSilence
  LOCAL INTEGER c
  FOR c = 0 TO NCHAN - 1
    chSfx(c) = -1 : chStep(c) = 0 : chLeft(c) = 0 : chPri(c) = 0
    IF sndOK <> 0 THEN PLAY SOUND c + 1, B, O
  NEXT c
END SUB

SUB SfxPlay(s AS INTEGER, pri AS INTEGER)
  LOCAL INTEGER c
  IF sndOK = 0 THEN EXIT SUB
  c = sfxCh(s)
  IF chSfx(c) >= 0 AND chPri(c) > pri THEN EXIT SUB
  chSfx(c) = s : chStep(c) = 0 : chLeft(c) = 0 : chPri(c) = pri
END SUB

' Advance every channel by one frame, so an effect never blocks the game.
SUB SfxService
  LOCAL INTEGER c, s
  IF sndOK = 0 THEN EXIT SUB
  FOR c = 0 TO NCHAN - 1
    IF chSfx(c) >= 0 THEN
      IF chLeft(c) > 0 THEN chLeft(c) = chLeft(c) - 1
      IF chLeft(c) <= 0 THEN
        s = chSfx(c)
        IF chStep(c) >= sfxN(s) THEN
          chSfx(c) = -1 : chPri(c) = 0
          SndOut c + 1, 0, 0, 0
        ELSE
          SndOut c + 1, sfxW(s), sfxF(s, chStep(c)), sfxV(s)
          chLeft(c) = sfxD(s, chStep(c))
          chStep(c) = chStep(c) + 1
        ENDIF
      ENDIF
    ENDIF
  NEXT c
END SUB

' PLAY SOUND wants the waveform as a literal, so pick it here.
SUB SndOut(ch AS INTEGER, wf AS INTEGER, f AS INTEGER, v AS INTEGER)
  IF sndOK = 0 THEN EXIT SUB
  IF f <= 0 OR v <= 0 THEN
    PLAY SOUND ch, B, O
  ELSE
    SELECT CASE wf
      CASE 0 : PLAY SOUND ch, B, Q, f, v
      CASE 1 : PLAY SOUND ch, B, T, f, v
      CASE 2 : PLAY SOUND ch, B, S, f, v
      CASE ELSE : PLAY SOUND ch, B, N, f, v
    END SELECT
  ENDIF
END SUB

' A short blocking tune, used between lives and floors.
SUB Tune(which AS INTEGER)
  LOCAL INTEGER f, d
  IF sndOK = 0 THEN EXIT SUB
  SfxSilence
  SELECT CASE which
    CASE 0 : RESTORE tune0            ' new floor
    CASE 1 : RESTORE tune1            ' floor cleared
    CASE ELSE : RESTORE tune2         ' game over
  END SELECT
  DO
    READ f, d
    IF f < 0 THEN EXIT DO
    IF f = 0 THEN
      PLAY SOUND 1, B, O
    ELSE
      PLAY SOUND 1, B, Q, f, 18
    ENDIF
    PAUSE d
  LOOP
  PLAY SOUND 1, B, O
END SUB

' ======================================================================
'  keyboard
'
'  KEYDOWN(0) gives the number of keys held and KEYDOWN(1..6) the
'  character of each, so the whole set is read once a frame and latched
'  into flags.  Every KEYDOWN call also empties the console input buffer,
'  so INKEY$ has to be read first.
' ======================================================================
SUB ReadKeys
  LOCAL INTEGER i, k
  LOCAL ky$ LENGTH 2
  kLeft = 0 : kRight = 0 : kUp = 0 : kDown = 0 : kJump = 0 : kQuit = 0
  kSkip = 0
  ky$ = INKEY$
  IF ky$ = " " THEN kJump = 1
  IF ky$ = CHR$(27) THEN kQuit = 1
  IF ky$ = "n" OR ky$ = "N" THEN kSkip = 1
  FOR i = 1 TO 6
    k = KEYDOWN(i)
    SELECT CASE k
      CASE 130, 122, 90  : kLeft = 1
      CASE 131, 120, 88  : kRight = 1
      CASE 128, 59, 39   : kUp = 1
      CASE 129, 47, 46   : kDown = 1
      CASE 32            : kJump = 1
      CASE 27            : kQuit = 1
      CASE 110, 78       : kSkip = 1        ' N, skip a level while testing
    END SELECT
  NEXT i
END SUB

' ======================================================================
'  map helpers
' ======================================================================
FUNCTION CellFlag(cx AS INTEGER, cy AS INTEGER) AS INTEGER
  LOCAL INTEGER r, c
  CellFlag = 0
  IF cy < PLAYTOP OR cx < 0 OR cx >= SCRW THEN EXIT FUNCTION
  r = (cy - PLAYTOP) \ CH
  IF r < 0 OR r >= GROWS THEN EXIT FUNCTION
  c = cx \ CW
  CellFlag = grid(r, c)
END FUNCTION

' Is any cell solid across the given pixel span on map row rr?
FUNCTION RowSolid(rr AS INTEGER, x0 AS INTEGER, wd AS INTEGER) AS INTEGER
  LOCAL INTEGER c0, c1, c
  RowSolid = 0
  IF rr < 0 OR rr >= GROWS THEN EXIT FUNCTION
  c0 = (x0 + 2) \ CW
  c1 = (x0 + wd - 3) \ CW
  IF c0 < 0 THEN c0 = 0
  IF c1 > GCOLS - 1 THEN c1 = GCOLS - 1
  FOR c = c0 TO c1
    IF (grid(rr, c) AND C_SOLID) <> 0 THEN RowSolid = 1 : EXIT FUNCTION
  NEXT c
END FUNCTION

' ======================================================================
'  level loading and drawing
' ======================================================================
SUB LoadLevel
  LOCAL INTEGER r, c, idx
  LOCAL cel$ LENGTH 1
  LOCAL row$ LENGTH 20

  idx = (gLevel - 1) MOD 8
  gIdx = idx
  gLap = (gLevel - 1) \ 8
  nEgg = 0 : nSeed = 0 : nHen0 = 0
  lfN = 0 : lfX = 0 : lfTop = 0 : lfBot = 0
  startX = 16 : startY = PLAYTOP

  ' An egg is worth 100 on the first four floors and another 100 every
  ' four after that, to a ceiling of 1000: the BBC does
  '     LDA level_number : LSR A : LSR A : ADC #&01, capped at &0A
  ' and adds that at the hundreds digit.
  eggVal = ((gLevel - 1) \ 4 + 1) * 100
  IF eggVal > 1000 THEN eggVal = 1000

  ' the panel and the eggs change colour every six floors, as they do on
  ' the BBC
  IF ((gLevel - 1) \ 6) MOD 2 = 0 THEN
    panelCol = RGB(MAGENTA) : eggBuf = S_EGGA
  ELSE
    panelCol = RGB(RED) : eggBuf = S_EGGB
  ENDIF

  FOR r = 0 TO GROWS - 1
    row$ = lvl$(idx, r)
    FOR c = 0 TO GCOLS - 1
      grid(r, c) = 0
      cel$ = MID$(row$, c + 1, 1)
      SELECT CASE cel$
        CASE "#"
          grid(r, c) = C_SOLID
        CASE "H"
          grid(r, c) = C_LADDER
        CASE "+"
          ' a ladder passing through a girder: walk over it, or climb down
          grid(r, c) = C_LADDER OR C_SOLID
        CASE "L", "K"
          grid(r, c) = C_SHAFT
          IF lfN = 0 THEN
            ' the shaft is two columns wide, so the 20 pixel lift sits
            ' just inside its left hand edge
            lfX = c * CW + 2
            lfTop = r : lfBot = r
            IF cel$ = "K" THEN lfN = 2 ELSE lfN = 1
          ELSE
            IF r < lfTop THEN lfTop = r
            IF r > lfBot THEN lfBot = r
          ENDIF
        CASE "E"
          IF nEgg < MAXEGG THEN
            egX(nEgg) = c * CW + (CW - 12) \ 2
            egY(nEgg) = PLAYTOP + (r + 1) * CH - 6
            egOn(nEgg) = 1 : nEgg = nEgg + 1
          ENDIF
        CASE "S"
          IF nSeed < MAXSEED THEN
            sdX(nSeed) = c * CW + (CW - 14) \ 2
            sdY(nSeed) = PLAYTOP + (r + 1) * CH - 4
            sdOn(nSeed) = 1 : nSeed = nSeed + 1
          ENDIF
        CASE "P"
          startX = c * CW
          startY = PLAYTOP + (r + 1) * CH - HH
        CASE "1", "2", "3", "4", "5"
          IF nHen0 < MAXHEN THEN
            henX0(nHen0) = c * CW + (CW - NW) \ 2
            henY0(nHen0) = PLAYTOP + (r + 1) * CH - NH
            nHen0 = nHen0 + 1
          ENDIF
      END SELECT
    NEXT c
  NEXT r
  eggLeft = nEgg
END SUB

' One girder cell.  The BBC draws three courses of green a pixel deep with
' a pixel of mortar between them, each course broken by one vertical joint
' and the joints staggered from course to course.
SUB DrawGirder(x AS INTEGER, y AS INTEGER)
  BOX x, y, CW, 1, 0, RGB(GREEN), RGB(GREEN)
  BOX x, y + 2, CW, 1, 0, RGB(GREEN), RGB(GREEN)
  BOX x, y + 4, CW, 1, 0, RGB(GREEN), RGB(GREEN)
  BOX x + 10, y, 2, 1, 0, RGB(BLACK), RGB(BLACK)
  BOX x + 2, y + 2, 2, 1, 0, RGB(BLACK), RGB(BLACK)
  BOX x + 6, y + 4, 2, 1, 0, RGB(BLACK), RGB(BLACK)
END SUB

' One ladder cell: two magenta rails with a rung across them.
SUB DrawRungs(x AS INTEGER, y AS INTEGER)
  BOX x + 2, y, 2, CH, 0, RGB(MAGENTA), RGB(MAGENTA)
  BOX x + 12, y, 2, CH, 0, RGB(MAGENTA), RGB(MAGENTA)
  BOX x + 2, y + 3, 12, 1, 0, RGB(MAGENTA), RGB(MAGENTA)
END SUB

' The duck's cage hangs in the top left corner of the floor, as it does on
' the BBC.  From level nine the duck is out and the cage stands empty.
' It is scenery only - nothing collides with it.
SUB DrawCage(occupied AS INTEGER)
  LOCAL INTEGER i
  BOX 24, 20, 2, 5, 0, RGB(YELLOW), RGB(YELLOW)
  CIRCLE 25, 20, 3, 1, 1, RGB(YELLOW)
  RBOX 4, 24, 43, 46, 12, RGB(YELLOW)
  IF occupied <> 0 THEN
    CIRCLE 22, 54, 9, 0, 0.75, RGB(YELLOW), RGB(YELLOW)
    CIRCLE 33, 44, 4, 0, 1, RGB(YELLOW), RGB(YELLOW)
    BOX 37, 44, 6, 2, 0, RGB(YELLOW), RGB(YELLOW)
  ENDIF
  FOR i = 10 TO 41 STEP 6
    BOX i, 27, 1, 41, 0, RGB(YELLOW), RGB(YELLOW)
  NEXT i
  BOX 4, 68, 43, 2, 0, RGB(YELLOW), RGB(YELLOW)
END SUB

SUB DrawWorld
  LOCAL INTEGER r, c, y, fl
  CLS RGB(BLACK)
  ' Where a ladder passes through a girder the BBC draws the ladder alone:
  ' that square is a single tile that you can both climb and stand on, and
  ' no green shows through it.  So the girder is skipped there.
  FOR r = 0 TO GROWS - 1
    y = PLAYTOP + r * CH
    FOR c = 0 TO GCOLS - 1
      fl = grid(r, c)
      IF (fl AND C_LADDER) = 0 AND (fl AND C_SOLID) <> 0 THEN DrawGirder c * CW, y
    NEXT c
  NEXT r
  FOR r = 0 TO GROWS - 1
    y = PLAYTOP + r * CH
    FOR c = 0 TO GCOLS - 1
      IF (grid(r, c) AND C_LADDER) <> 0 THEN DrawRungs c * CW, y
    NEXT c
  NEXT r
  IF gLevel <= 8 THEN DrawCage 1 ELSE DrawCage 0
  FOR r = 0 TO nEgg - 1
    IF egOn(r) <> 0 THEN SPRITE WRITE eggBuf, egX(r), egY(r), 0
  NEXT r
  FOR r = 0 TO nSeed - 1
    IF sdOn(r) <> 0 THEN SPRITE WRITE S_SEED, sdX(r), sdY(r), 0
  NEXT r
END SUB

' Rub something out of the background.  All the sprites have to be lifted
' first, or they would put the old picture back when they next move.
SUB EraseBg(x AS INTEGER, y AS INTEGER, w AS INTEGER, h AS INTEGER)
  SPRITE HIDE ALL
  BOX x, y, w, h, 0, RGB(BLACK), RGB(BLACK)
  SPRITE RESTORE
END SUB

' ======================================================================
'  entity plumbing
' ======================================================================
SUB HideAllEnt
  LOCAL INTEGER e
  FOR e = 0 TO NENT
    IF entBuf(e) <> 0 THEN SPRITE HIDE entBuf(e)
    entBuf(e) = 0 : entWant(e) = 0
  NEXT e
END SUB

SUB SetEnt(e AS INTEGER, buf AS INTEGER, x AS INTEGER, y AS INTEGER, lay AS INTEGER, rot AS INTEGER)
  entWant(e) = buf : entX(e) = x : entY(e) = y
  entLay(e) = lay : entRot(e) = rot
END SUB

SUB ShowEnts
  LOCAL INTEGER e, sw, sh
  FOR e = 0 TO NENT
    IF entWant(e) = 0 THEN
      IF entBuf(e) <> 0 THEN
        SPRITE HIDE SAFE entBuf(e)
        entBuf(e) = 0
      ENDIF
    ELSE
      IF entBuf(e) <> entWant(e) THEN
        IF entBuf(e) <> 0 THEN SPRITE HIDE SAFE entBuf(e)
        entBuf(e) = entWant(e)
      ENDIF
      sw = SPRITE(W, entBuf(e)) : sh = SPRITE(H, entBuf(e))
      IF entX(e) < 1 - sw THEN entX(e) = 1 - sw
      IF entX(e) > SCRW - 1 THEN entX(e) = SCRW - 1
      IF entY(e) < 1 - sh THEN entY(e) = 1 - sh
      IF entY(e) > SCRH - 1 THEN entY(e) = SCRH - 1
      SPRITE SHOW SAFE entBuf(e), entX(e), entY(e), entLay(e), entRot(e)
    ENDIF
  NEXT e
END SUB

' ======================================================================
'  Harry
' ======================================================================
SUB UpdatePlayer
  LOCAL INTEGER cx, fy0, fy1, r, rEnd, ytop, ybot, landed, oldAnim, i
  LOCAL FLOAT ny

  cx = INT(px) + HW \ 2
  oldAnim = pAnim \ 6

  ' ---------------------------------------------------------- on a ladder
  IF pState = ST_CLIMB THEN
    IF kJump <> 0 THEN
      pState = ST_WALK : pvy = JUMPV : pGround = 0
      SfxPlay SFX_JUMP, 2
    ELSE
      IF kUp <> 0 THEN
        IF (CellFlag(cx, INT(py) + HH - 1) AND C_LADDER) <> 0 THEN
          py = py - CLIMBSPD
          pAnim = pAnim + 1
        ENDIF
      ELSEIF kDown <> 0 THEN
        IF (CellFlag(cx, INT(py) + HH + 1) AND C_LADDER) <> 0 THEN
          py = py + CLIMBSPD
          pAnim = pAnim + 1
        ENDIF
      ENDIF
      IF kLeft <> 0 OR kRight <> 0 THEN
        IF kLeft <> 0 THEN pFace = 1 ELSE pFace = 0
        r = (INT(py) + HH - PLAYTOP) \ CH
        IF RowSolid(r, INT(px), HW) <> 0 THEN
          pState = ST_WALK : pvy = 0 : pGround = 1
        ELSE
          ' A ladder stands proud of the girder it serves and is climbed in
          ' two pixel steps, so Harry frequently ends up a pixel or two
          ' above the floor.  The original was not this fussy: let him step
          ' across from just above it, dropping him the last pixel or two.
          ytop = PLAYTOP + (r + 1) * CH
          IF ytop - (INT(py) + HH) <= LEDGESNAP THEN
            IF RowSolid(r + 1, INT(px), HW) <> 0 THEN
              py = ytop - HH
              pState = ST_WALK : pvy = 0 : pGround = 1
            ENDIF
          ENDIF
        ENDIF
      ENDIF
      IF (CellFlag(cx, INT(py) + HH - 1) AND C_LADDER) = 0 THEN
        IF (CellFlag(cx, INT(py) + HH) AND C_LADDER) = 0 THEN
          pState = ST_WALK : pvy = 0
        ENDIF
      ENDIF
    ENDIF
    IF py < PLAYTOP THEN py = PLAYTOP
    IF (pAnim \ 6) <> oldAnim THEN SfxPlay SFX_STEP, 0
    EXIT SUB
  ENDIF

  ' ---------------------------------------------------------- grab a rung
  IF kUp <> 0 THEN
    IF (CellFlag(cx, INT(py) + HH - 1) AND C_LADDER) <> 0 THEN
      pState = ST_CLIMB : pvy = 0 : pLift = 0
      px = (cx \ CW) * CW
      EXIT SUB
    ENDIF
  ELSEIF kDown <> 0 AND pGround <> 0 THEN
    IF (CellFlag(cx, INT(py) + HH + 1) AND C_LADDER) <> 0 THEN
      pState = ST_CLIMB : pvy = 0 : pLift = 0
      px = (cx \ CW) * CW
      EXIT SUB
    ENDIF
  ENDIF

  ' ------------------------------------------------------------- running
  pvx = 0
  IF kLeft <> 0 THEN pvx = -WALKSPD : pFace = 1
  IF kRight <> 0 THEN pvx = WALKSPD : pFace = 0
  IF pvx <> 0 THEN
    px = px + pvx
    IF px < 0 THEN px = 0
    IF px > SCRW - HW THEN px = SCRW - HW
    pAnim = pAnim + 1
    IF pGround <> 0 AND (pAnim \ 6) <> oldAnim THEN SfxPlay SFX_STEP, 0
  ENDIF

  IF kJump <> 0 AND pGround <> 0 THEN
    pvy = JUMPV : pGround = 0 : pLift = 0
    SfxPlay SFX_JUMP, 2
  ENDIF

  ' ----------------------------------------------------- riding the lift
  IF pLift <> 0 THEN
    i = pLift - 1
    IF (INT(px) + HW - 2 <= lfX) OR (INT(px) + 2 >= lfX + LW) THEN
      pLift = 0
    ELSE
      py = lfY(i) - HH : pvy = 0 : pGround = 1
      IF py < PLAYTOP THEN py = PLAYTOP
      EXIT SUB
    ENDIF
  ENDIF

  ' ------------------------------------------------------------- gravity
  pvy = pvy + GRAV
  IF pvy > MAXFALL THEN pvy = MAXFALL
  ny = py + pvy
  landed = 0

  IF pvy >= 0 THEN
    fy0 = INT(py) + HH
    fy1 = INT(ny) + HH
    IF lfN > 0 AND (INT(px) + HW - 2 > lfX) AND (INT(px) + 2 < lfX + LW) THEN
      FOR i = 0 TO lfN - 1
        ytop = INT(lfY(i))
        IF landed = 0 AND ytop >= fy0 - 1 AND ytop <= fy1 THEN
          py = ytop - HH : pvy = 0 : landed = 1 : pGround = 1 : pLift = i + 1
        ENDIF
      NEXT i
    ENDIF
    IF landed = 0 THEN
      r = (fy0 - PLAYTOP) \ CH
      IF r < 0 THEN r = 0
      rEnd = (fy1 - PLAYTOP) \ CH
      IF rEnd > GROWS - 1 THEN rEnd = GROWS - 1
      DO WHILE r <= rEnd
        ytop = PLAYTOP + r * CH
        IF ytop >= fy0 AND ytop <= fy1 THEN
          IF RowSolid(r, INT(px), HW) <> 0 THEN
            py = ytop - HH : pvy = 0 : landed = 1
            IF pGround = 0 THEN SfxPlay SFX_LAND, 1
            pGround = 1 : pLift = 0
            EXIT DO
          ENDIF
        ENDIF
        r = r + 1
      LOOP
    ENDIF
    IF landed = 0 THEN
      py = ny : pGround = 0 : pLift = 0
    ENDIF
  ELSE
    fy0 = INT(py)
    fy1 = INT(ny)
    r = (fy0 - PLAYTOP) \ CH
    IF r > GROWS - 1 THEN r = GROWS - 1
    rEnd = (fy1 - PLAYTOP) \ CH
    IF rEnd < 0 THEN rEnd = 0
    DO WHILE r >= rEnd
      ybot = PLAYTOP + r * CH + FLTHK - 1
      IF ybot <= fy0 AND ybot >= fy1 THEN
        IF RowSolid(r, INT(px), HW) <> 0 THEN
          py = ybot + 1 : pvy = 0 : landed = 1
          EXIT DO
        ENDIF
      ENDIF
      r = r - 1
    LOOP
    IF landed = 0 THEN py = ny
    pGround = 0 : pLift = 0
  ENDIF

  IF py < PLAYTOP THEN
    py = PLAYTOP
    IF pvy < 0 THEN pvy = 0
  ENDIF
  ' Through a gap in the ground girder there is nowhere to land, so going
  ' off the bottom of the screen costs a life rather than wedging him in
  ' the few pixels below it.  pKilled also covers being carried into the
  ' top of the screen by a lift.
  IF py > SCRH - HH THEN
    py = SCRH - HH
    pKilled = 1
  ENDIF
END SUB

' ======================================================================
'  hens
' ======================================================================

' A ladder stands proud of the floor it serves, so the rungs above a hen's
' head are very often just that overhang and lead nowhere.  Only climb when
' there is a girder at the far end: up, that means a run of ladder cells
' ending in one that is also solid.
FUNCTION LadderUp(cx AS INTEGER, fy AS INTEGER) AS INTEGER
  LOCAL INTEGER r, c
  LadderUp = 0
  c = cx \ CW
  IF c < 0 OR c > GCOLS - 1 THEN EXIT FUNCTION
  r = (fy - PLAYTOP) \ CH - 1
  DO WHILE r >= 0
    IF (grid(r, c) AND C_LADDER) = 0 THEN EXIT FUNCTION
    IF (grid(r, c) AND C_SOLID) <> 0 THEN LadderUp = 1 : EXIT FUNCTION
    r = r - 1
  LOOP
END FUNCTION

' Down, the hen has to be standing on a crossing square, and the run has to
' reach something solid to land on.
FUNCTION LadderDown(cx AS INTEGER, fy AS INTEGER) AS INTEGER
  LOCAL INTEGER r, c
  LadderDown = 0
  c = cx \ CW
  IF c < 0 OR c > GCOLS - 1 THEN EXIT FUNCTION
  r = (fy - PLAYTOP) \ CH
  IF r < 0 OR r > GROWS - 1 THEN EXIT FUNCTION
  IF (grid(r, c) AND C_LADDER) = 0 THEN EXIT FUNCTION
  r = r + 1
  DO WHILE r <= GROWS - 1
    IF (grid(r, c) AND C_SOLID) <> 0 THEN LadderDown = 1 : EXIT FUNCTION
    IF (grid(r, c) AND C_LADDER) = 0 THEN EXIT FUNCTION
    r = r + 1
  LOOP
END FUNCTION
SUB UpdateHen(i AS INTEGER)
  LOCAL INTEGER fy, cx, edge, nx, r, ytop, nfy, wayUp, wayDn
  LOCAL FLOAT ny

  IF henPeck(i) > 0 THEN
    henPeck(i) = henPeck(i) - 1
    IF henPeck(i) = 0 THEN henAnim(i) = 0
    EXIT SUB
  ENDIF

  fy = INT(henY(i)) + NH
  cx = INT(henX(i)) + NW \ 2

  IF henSt(i) = ST_CLIMB THEN
    IF henVD(i) < 0 THEN
      IF (CellFlag(cx, fy - 1) AND C_LADDER) <> 0 THEN
        ' A ladder stands proud of the girder it serves, so stop on
        ' reaching a floor rather than carrying on to the top of the
        ' overhang, where there would be nothing to stand on.
        ny = henY(i) - henSpd
        nfy = INT(ny) + NH
        r = (fy - 1 - PLAYTOP) \ CH
        ytop = PLAYTOP + r * CH
        IF ytop < fy AND ytop >= nfy AND RowSolid(r, INT(henX(i)), NW) <> 0 THEN
          henY(i) = ytop - NH
          henSt(i) = ST_WALK
        ELSE
          henY(i) = ny
        ENDIF
      ELSE
        henSt(i) = ST_WALK : SnapHen i
      ENDIF
    ELSE
      IF (CellFlag(cx, fy + 1) AND C_LADDER) <> 0 THEN
        henY(i) = henY(i) + henSpd
      ELSE
        henSt(i) = ST_WALK : SnapHen i
      ENDIF
    ENDIF
    henAnim(i) = henAnim(i) + 1
    EXIT SUB
  ENDIF

  ' Nothing underfoot means it has stepped off the end of something, so
  ' let it drop rather than flip direction on the spot every frame.
  IF RowSolid((fy - PLAYTOP) \ CH, INT(henX(i)), NW) = 0 THEN
    henY(i) = henY(i) + 2
    nfy = INT(henY(i)) + NH
    r = (nfy - PLAYTOP) \ CH
    IF r <= GROWS - 1 THEN
      IF RowSolid(r, INT(henX(i)), NW) <> 0 THEN henY(i) = PLAYTOP + r * CH - NH
    ELSE
      henX(i) = henX0(i) : henY(i) = henY0(i)
    ENDIF
    EXIT SUB
  ENDIF

  ' Now and then take a ladder - but only one that actually goes somewhere,
  ' and choose freely between up and down when both do, so a hen is not
  ' forever drawn to whichever was tested first.
  IF RND < LADDERODDS THEN
    wayUp = LadderUp(cx, fy)
    wayDn = LadderDown(cx, fy)
    IF wayUp <> 0 AND wayDn <> 0 THEN
      IF RND < 0.5 THEN wayDn = 0 ELSE wayUp = 0
    ENDIF
    IF wayUp <> 0 THEN
      henSt(i) = ST_CLIMB : henVD(i) = -1
      henX(i) = (cx \ CW) * CW + (CW - NW) \ 2
      EXIT SUB
    ELSEIF wayDn <> 0 THEN
      henSt(i) = ST_CLIMB : henVD(i) = 1
      henX(i) = (cx \ CW) * CW + (CW - NW) \ 2
      EXIT SUB
    ENDIF
  ENDIF

  nx = INT(henX(i) + henDir(i) * henSpd)
  IF henDir(i) > 0 THEN edge = nx + NW - 3 ELSE edge = nx + 2
  IF nx < 0 OR nx > SCRW - NW THEN
    henDir(i) = -henDir(i)
  ELSEIF (CellFlag(edge, fy) AND C_SOLID) = 0 THEN
    henDir(i) = -henDir(i)
  ELSE
    henX(i) = henX(i) + henDir(i) * henSpd
    henAnim(i) = henAnim(i) + 1
  ENDIF
END SUB

SUB SnapHen(i AS INTEGER)
  LOCAL INTEGER r
  r = (INT(henY(i)) + NH - PLAYTOP + CH \ 2) \ CH
  IF r < 0 THEN r = 0
  IF r > GROWS - 1 THEN r = GROWS - 1
  henY(i) = PLAYTOP + r * CH - NH
END SUB

' ======================================================================
'  one floor
'  returns 0 caught, 1 cleared, 2 quit
' ======================================================================
FUNCTION RunLevel() AS INTEGER
  LOCAL INTEGER i, e, rot, hit, cx
  LOCAL FLOAT nextFrame, dly
  LOCAL INTEGER tickAcc, running

  LoadLevel
  HideAllEnt
  DrawWorld

  px = startX : py = startY
  pvx = 0 : pvy = 0 : pState = ST_WALK : pFace = 0 : pAnim = 0
  pGround = 1 : pLift = 0 : pKilled = 0 : clockStop = 0

  ' Each time round the eight floors the odds get worse.  The second
  ' cycle empties the coop and lets the duck out; the third puts the hens
  ' back alongside it; the fourth fields the full five on every floor; the
  ' fifth does that with everything running flat out.
  SELECT CASE gLap
    CASE 0    : nHen = henBase(gIdx) : dkOn = 0
    CASE 1    : nHen = 0             : dkOn = 1
    CASE 2    : nHen = henBase(gIdx) : dkOn = 1
    CASE ELSE : nHen = MAXHEN        : dkOn = 1
  END SELECT
  IF nHen > nHen0 THEN nHen = nHen0
  FOR i = 0 TO nHen - 1
    henX(i) = henX0(i) : henY(i) = henY0(i)
    henDir(i) = 1 : IF RND < 0.5 THEN henDir(i) = -1
    henSt(i) = ST_WALK : henVD(i) = 0
    henAnim(i) = 0 : henPeck(i) = 0
  NEXT i

  henSpd = 1.0 + 0.05 * gIdx + 0.28 * gLap
  IF henSpd > 1.9 THEN henSpd = 1.9        ' Harry must still outrun them
  dkSpd = 0.55 + 0.18 * gLap
  IF dkSpd > 1.3 THEN dkSpd = 1.3
  dkAnim = 0 : dkQuack = 0 : dkFace = 0
  IF dkOn <> 0 THEN dkX = 20 : dkY = PLAYTOP + 26     ' out of the cage

  FOR i = 0 TO lfN - 1
    lfY(i) = PLAYTOP + lfBot * CH - i * (((lfBot - lfTop) * CH) \ 2)
  NEXT i

  gTime = 900
  gBonus = gLevel * 1000
  tickAcc = 0
  running = 1
  RunLevel = 0

  ' Put everything on screen and hold it while the tune plays, so the
  ' floor can be studied before anything starts moving.
  SetEnt 0, S_STAND, INT(px), INT(py), 3, 0
  FOR i = 0 TO nHen - 1
    SetEnt i + 1, S_HENA + i, INT(henX(i)), INT(henY(i)), 2, 0
  NEXT i
  FOR i = nHen TO MAXHEN - 1
    SetEnt i + 1, 0, 0, 0, 2, 0
  NEXT i
  IF dkOn <> 0 THEN
    SetEnt E_DUCK, S_DUCK1, INT(dkX), INT(dkY), 4, 0
  ELSE
    SetEnt E_DUCK, 0, 0, 0, 4, 0
  ENDIF
  FOR i = 0 TO MAXLIFT - 1
    IF i < lfN THEN
      SetEnt E_LIFT + i, LiftBuf(i), lfX, INT(lfY(i)), 1, 0
    ELSE
      SetEnt E_LIFT + i, 0, 0, 0, 1, 0
    ENDIF
  NEXT i
  ShowEnts
  DrawHud
  FRAMEBUFFER COPY F, N
  Tune 0
  nextFrame = TIMER + FRAMEMS

  DO
    ' ------------------------------------------------------------ input
    ReadKeys
    IF kQuit <> 0 THEN RunLevel = 2 : EXIT DO
    IF kSkip <> 0 THEN
      gBonus = 0
      RunLevel = 1
      EXIT DO
    ENDIF

    UpdatePlayer
    FOR i = 0 TO nHen - 1
      UpdateHen i
    NEXT i

    ' ------------------------------------------------------------ lifts
    FOR i = 0 TO lfN - 1
      lfY(i) = lfY(i) - LIFTSPD
      IF lfY(i) < PLAYTOP + lfTop * CH THEN
        lfY(i) = PLAYTOP + lfBot * CH
        ' Still aboard when it reaches the top of the shaft?  On the BBC
        ' that costs a life - get off before the lift wraps round.
        IF pLift = i + 1 THEN pLift = 0 : pKilled = 1
      ENDIF
    NEXT i

    ' ------------------------------------------------------------- duck
    IF dkOn <> 0 THEN
      ' Home in on Harry, but leave a dead band on each axis.  Without one
      ' the duck overshoots by a fraction of a pixel every frame once it
      ' has drawn level with him and shivers on the spot.
      IF dkX < px - 3 THEN
        dkX = dkX + dkSpd
      ELSEIF dkX > px + 3 THEN
        dkX = dkX - dkSpd
      ENDIF
      IF dkY < py - 3 THEN
        dkY = dkY + dkSpd * 0.75
      ELSEIF dkY > py + 3 THEN
        dkY = dkY - dkSpd * 0.75
      ENDIF
      IF dkX < 0 THEN dkX = 0
      IF dkX > SCRW - DW THEN dkX = SCRW - DW
      IF dkY < PLAYTOP THEN dkY = PLAYTOP
      IF dkY > SCRH - DH THEN dkY = SCRH - DH
      ' Only turn round once Harry is clearly to one side, so the sprite
      ' does not mirror back and forth while the duck is right above him.
      IF dkX > px + 8 THEN dkFace = 1
      IF dkX < px - 8 THEN dkFace = 0
      dkAnim = dkAnim + 1
      dkQuack = dkQuack + 1
      IF dkQuack > 60 THEN dkQuack = 0 : SfxPlay SFX_QUACK, 1
    ENDIF

    ' ------------------------------------------------------- collecting
    cx = INT(px)
    FOR i = 0 TO nEgg - 1
      IF egOn(i) <> 0 THEN
        IF cx + HW - 2 > egX(i) AND cx + 2 < egX(i) + 12 THEN
          IF INT(py) + HH > egY(i) - 2 AND INT(py) + 4 < egY(i) + 6 THEN
            egOn(i) = 0 : eggLeft = eggLeft - 1
            AddScore eggVal
            EraseBg egX(i), egY(i), 12, 6
            SfxPlay SFX_EGG, 3
          ENDIF
        ENDIF
      ENDIF
    NEXT i
    FOR i = 0 TO nSeed - 1
      IF sdOn(i) <> 0 THEN
        IF cx + HW - 2 > sdX(i) AND cx + 2 < sdX(i) + 14 THEN
          IF INT(py) + HH >= sdY(i) - 2 AND INT(py) + HH <= sdY(i) + 8 THEN
            sdOn(i) = 0
            AddScore 50
            clockStop = CLOCKFREEZE    ' set, never added to
            EraseBg sdX(i), sdY(i), 14, 4
            SfxPlay SFX_SEED, 2
          ENDIF
        ENDIF
      ENDIF
    NEXT i

    ' the hens stop to eat the grain too
    FOR e = 0 TO nHen - 1
      IF henPeck(e) = 0 AND henSt(e) = ST_WALK THEN
        FOR i = 0 TO nSeed - 1
          IF sdOn(i) <> 0 THEN
            IF INT(henX(e)) + NW \ 2 > sdX(i) AND INT(henX(e)) + NW \ 2 < sdX(i) + 14 THEN
              IF ABS(INT(henY(e)) + NH - (sdY(i) + 4)) < 5 THEN
                henPeck(e) = PECKFRAMES
                sdOn(i) = 0
                EraseBg sdX(i), sdY(i), 14, 4
                SfxPlay SFX_CLUCK, 1
              ENDIF
            ENDIF
          ENDIF
        NEXT i
      ENDIF
    NEXT e

    ' ------------------------------------------------------- collisions
    hit = pKilled
    FOR i = 0 TO nHen - 1
      IF cx + HW - 3 > INT(henX(i)) + 2 AND cx + 3 < INT(henX(i)) + NW - 2 THEN
        ' the top of a hen is its raised head and neck, which Harry clears
        IF INT(py) + HH - 2 > INT(henY(i)) + NHEAD AND INT(py) + 3 < INT(henY(i)) + NH THEN
          hit = 1
        ENDIF
      ENDIF
    NEXT i
    IF dkOn <> 0 AND hit = 0 THEN
      IF cx + HW - 3 > INT(dkX) + 3 AND cx + 3 < INT(dkX) + DW - 3 THEN
        IF INT(py) + HH - 2 > INT(dkY) + 8 AND INT(py) + 3 < INT(dkY) + DH THEN
          hit = 1
        ENDIF
      ENDIF
    ENDIF

    ' ----------------------------------------------------------- render
    SetEnt 0, PlayerFrame(), INT(px), INT(py), 3, 0
    FOR i = 0 TO nHen - 1
      IF henDir(i) < 0 THEN rot = 1 ELSE rot = 0
      SetEnt i + 1, HenFrame(i), INT(henX(i)), INT(henY(i)), 2, rot
    NEXT i
    IF dkOn <> 0 THEN
      rot = dkFace
      IF (dkAnim \ 8) MOD 2 = 0 THEN e = S_DUCK1 ELSE e = S_DUCK2
      SetEnt E_DUCK, e, INT(dkX), INT(dkY), 4, rot
    ENDIF
    FOR i = 0 TO lfN - 1
      SetEnt E_LIFT + i, LiftBuf(i), lfX, INT(lfY(i)), 1, 0
    NEXT i
    ShowEnts
    DrawHud
    FRAMEBUFFER COPY F, N
    SfxService

    ' ------------------------------------------------------------ clock
    tickAcc = tickAcc + 1
    IF tickAcc >= 3 THEN
      tickAcc = 0
      IF clockStop > 0 THEN
        ' grain eaten recently: the clock and the bonus both stand still
        clockStop = clockStop - 1
      ELSE
        gTime = gTime - 1
        gBonus = ((gLevel * 1000 * gTime) \ 9000) * 10
        IF gTime <= 0 THEN gTime = 0 : gBonus = 0 : hit = 1
      ENDIF
    ENDIF

    IF hit <> 0 THEN
      Died
      RunLevel = 0
      running = 0
    ELSEIF eggLeft <= 0 THEN
      RunLevel = 1
      running = 0
    ENDIF

    ' ----------------------------------------------------------- timing
    IF running <> 0 THEN
      dly = nextFrame - TIMER
      IF dly > 0 THEN PAUSE dly
      nextFrame = nextFrame + FRAMEMS
      IF nextFrame < TIMER THEN nextFrame = TIMER + FRAMEMS
    ENDIF
  LOOP UNTIL running = 0
END FUNCTION

FUNCTION LiftBuf(i AS INTEGER) AS INTEGER
  IF i = 0 THEN LiftBuf = S_LIFT ELSE LiftBuf = S_LIFT2
END FUNCTION

FUNCTION PlayerFrame() AS INTEGER
  IF pState = ST_CLIMB THEN
    IF (pAnim \ 4) MOD 2 = 0 THEN PlayerFrame = S_CLIMB1 ELSE PlayerFrame = S_CLIMB2
    EXIT FUNCTION
  ENDIF
  IF pGround = 0 THEN
    IF pFace = 0 THEN PlayerFrame = S_RUN1 ELSE PlayerFrame = S_RUN2
    EXIT FUNCTION
  ENDIF
  IF pvx = 0 THEN
    PlayerFrame = S_STAND
  ELSE
    IF (pAnim \ 6) MOD 2 = 0 THEN
      PlayerFrame = S_STAND
    ELSE
      IF pFace = 0 THEN PlayerFrame = S_RUN1 ELSE PlayerFrame = S_RUN2
    ENDIF
  ENDIF
END FUNCTION

FUNCTION HenFrame(i AS INTEGER) AS INTEGER
  IF henPeck(i) > 0 THEN
    IF (henPeck(i) \ 5) MOD 2 = 0 THEN
      HenFrame = S_HENA + 2 * MAXHEN + i
    ELSE
      HenFrame = S_HENA + i
    ENDIF
  ELSE
    IF (henAnim(i) \ 5) MOD 2 = 0 THEN
      HenFrame = S_HENA + i
    ELSE
      HenFrame = S_HENA + MAXHEN + i
    ENDIF
  ENDIF
END FUNCTION

' ======================================================================
'  status panel
'  The BBC sets it as black lettering on coloured panels, in two rows,
'  with a row of chicks for the lives in hand.
' ======================================================================
SUB Panel(x AS INTEGER, y AS INTEGER, t$)
  BOX x, y, LEN(t$) * FW + 4, FH, 0, panelCol, panelCol
  TEXT x + 2, y, t$, "LT", FONTN, 1, RGB(BLACK), panelCol
END SUB

SUB DrawHud
  LOCAL INTEGER i
  BOX 0, 0, SCRW, PLAYTOP, 0, RGB(BLACK), RGB(BLACK)
  Panel 2, 0, "SCORE"
  Panel 44, 0, RIGHT$("000000" + STR$(gScore), 6)
  FOR i = 1 TO gLives - 1
    IF i <= 8 THEN
      BOX 93 + i * 8, 1, 4, 2, 0, RGB(YELLOW), RGB(YELLOW)
      BOX 92 + i * 8, 3, 6, 4, 0, RGB(YELLOW), RGB(YELLOW)
    ENDIF
  NEXT i
  Panel 2, FH, "PLAYER 1"
  Panel 62, FH, "LEVEL " + RIGHT$("0" + STR$(gLevel), 2)
  Panel 124, FH, "BONUS " + RIGHT$("0000" + STR$(gBonus), 4)
  Panel 196, FH, "TIME " + RIGHT$("0000" + STR$(gTime), 4)
END SUB

SUB AddScore(n AS INTEGER)
  gScore = gScore + n
  IF gScore > gHi THEN gHi = gScore
  IF gScore >= gNextLife THEN
    gNextLife = gNextLife + 10000
    gLives = gLives + 1
    SfxPlay SFX_EXTRA, 4
  ENDIF
END SUB

' ======================================================================
'  losing a life
' ======================================================================
SUB Died
  LOCAL INTEGER i, e, y
  SfxSilence
  SfxPlay SFX_DIE, 5
  FOR i = 0 TO 24
    IF (i AND 1) = 0 THEN e = S_STAND ELSE e = S_CLIMB1
    y = INT(py) - i \ 3
    IF y < PLAYTOP THEN y = PLAYTOP
    SetEnt 0, e, INT(px), y, 3, 0
    ShowEnts
    FRAMEBUFFER COPY F, N
    SfxService
    PAUSE 40
  NEXT i
  SfxSilence
  gLives = gLives - 1
END SUB

' ======================================================================
'  a whole game
' ======================================================================
SUB PlayGame
  LOCAL INTEGER r, b
  gScore = 0 : gLives = 5 : gLevel = gStart : gNextLife = 10000
  DO
    ' Losing a life restarts the floor from scratch, as the original does:
    ' RunLevel calls LoadLevel every time, so all twelve eggs and the grain
    ' come back and the clock returns to 900.  Only the score, the lives and
    ' the level number carry over.  That is deliberate - it is not an
    ' oversight to be "fixed" into keeping the eggs already collected.
    r = RunLevel()
    IF r = 2 THEN EXIT DO
    IF r = 1 THEN
      Tune 1
      DO WHILE gBonus > 0
        b = 10 : IF gBonus < 10 THEN b = gBonus
        gBonus = gBonus - b
        AddScore b
        DrawHud
        FRAMEBUFFER COPY F, N
        SfxPlay SFX_TICK, 1
        SfxService
        PAUSE 12
      LOOP
      SfxSilence
      gLevel = gLevel + 1
      IF gLevel > 40 THEN gLevel = 33
    ENDIF
  LOOP UNTIL gLives <= 0
  IF gLives <= 0 THEN GameOver
  HideAllEnt
END SUB

SUB GameOver
  LOCAL INTEGER i
  Tune 2
  BOX 60, 96, 200, 48, 2, RGB(WHITE), RGB(BLACK)
  TEXT 160, 106, "GAME OVER", "CT", FONTN, 2, RGB(YELLOW), RGB(BLACK)
  TEXT 160, 128, "SCORE " + STR$(gScore), "CT", FONTN, 1, RGB(WHITE), RGB(BLACK)
  FRAMEBUFFER COPY F, N
  PAUSE 2500
  FOR i = 1 TO 200
    IF INKEY$ <> "" THEN EXIT FOR
    PAUSE 10
  NEXT i
END SUB

' ======================================================================
'  title screen
' ======================================================================
SUB TitleScreen
  LOCAL INTEGER t, x, hb, nb, ud

  HideAllEnt
  panelCol = RGB(MAGENTA)
  CLS RGB(BLACK)
  TEXT 160, 10, "CHUCKIE EGG", "CT", FONTN, 3, RGB(YELLOW), RGB(BLACK)
  BOX 60, 40, 240, 2, 0, RGB(MAGENTA), RGB(MAGENTA)
  TEXT 160, 48, "after the 1983 A&F Software original", "CT", FONTN, 1, RGB(CYAN), RGB(BLACK)
  DrawCage 1

  TEXT 160, 72, "COLLECT ALL 12 EGGS ON EACH FLOOR", "CT", FONTN, 1, RGB(WHITE), RGB(BLACK)
  TEXT 160, 84, "BEFORE THE CLOCK RUNS DOWN", "CT", FONTN, 1, RGB(WHITE), RGB(BLACK)

  TEXT 76, 106, "ARROWS / Z X", "LT", FONTN, 1, RGB(GREEN), RGB(BLACK)
  TEXT 186, 106, "RUN, CLIMB", "LT", FONTN, 1, RGB(WHITE), RGB(BLACK)
  TEXT 76, 118, "SPACE", "LT", FONTN, 1, RGB(GREEN), RGB(BLACK)
  TEXT 186, 118, "JUMP", "LT", FONTN, 1, RGB(WHITE), RGB(BLACK)
  TEXT 76, 130, "ESC", "LT", FONTN, 1, RGB(GREEN), RGB(BLACK)
  TEXT 186, 130, "QUIT", "LT", FONTN, 1, RGB(WHITE), RGB(BLACK)

  SPRITE WRITE S_EGGA, 106, 152, 0
  TEXT 122, 152, "100", "LT", FONTN, 1, RGB(WHITE), RGB(BLACK)
  SPRITE WRITE S_SEED, 184, 154, 0
  TEXT 202, 152, "50", "LT", FONTN, 1, RGB(WHITE), RGB(BLACK)

  TEXT 160, 170, "HIGH SCORE " + STR$(gHi), "CT", FONTN, 1, RGB(MAGENTA), RGB(BLACK)

  FOR x = 0 TO SCRW - 1 STEP CW
    DrawGirder x, 231
  NEXT x

  t = 0
  ud = 0
  ' start from a clean slate so a key still held from the last game does
  ' not nudge the level select or start play straight away
  kUp = 0 : kDown = 0 : kJump = 0
  DO
    ' level select, for testing: UP and DOWN pick where to start
    IF kUp <> 0 AND ud = 0 THEN
      gStart = gStart + 1
      IF gStart > 40 THEN gStart = 1
    ELSEIF kDown <> 0 AND ud = 0 THEN
      gStart = gStart - 1
      IF gStart < 1 THEN gStart = 40
    ENDIF
    ud = kUp OR kDown
    TEXT 160, 184, "START ON LEVEL " + RIGHT$("0" + STR$(gStart), 2) + "  (UP/DOWN)", "CT", FONTN, 1, RGB(CYAN), RGB(BLACK)

    x = 20 + ((t * 2) MOD 280)
    IF (t \ 6) MOD 2 = 0 THEN hb = S_STAND ELSE hb = S_RUN1
    IF (t \ 5) MOD 2 = 0 THEN nb = S_HENA ELSE nb = S_HENA + MAXHEN
    SetEnt 0, hb, x, 231 - HH, 3, 0
    SetEnt 1, nb, 290 - ((t * 1) MOD 300), 231 - NH, 2, 1
    ShowEnts
    IF (t \ 10) MOD 2 = 0 THEN
      TEXT 160, 200, "PRESS SPACE TO PLAY", "CT", FONTN, 1, RGB(YELLOW), RGB(BLACK)
    ELSE
      BOX 40, 200, 240, 10, 0, RGB(BLACK), RGB(BLACK)
    ENDIF
    FRAMEBUFFER COPY F, N
    PAUSE 45
    t = t + 1
    ReadKeys
  LOOP UNTIL kJump <> 0
  HideAllEnt
END SUB
