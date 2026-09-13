' =====================================================================
'  Reading the keyboard, and turning it into the player's rotation
'
'  The original keeps two "joystick" values, one for roll and one for
'  pitch, that a held key pushes away from centre and a spring pulls
'  back, and then derives the frame's rotation angles from how far from
'  centre they have got.  That spring is why Elite's controls feel the
'  way they do, so it is reproduced exactly rather than replaced with a
'  direct key-to-angle mapping.
' =====================================================================

SUB ReadKeys
  IF demoMode THEN DemoFly : EXIT SUB
  LOCAL INTEGER i, k, hnow, hnew
  LOCAL kb$ LENGTH 2
  kRollL = 0 : kRollR = 0 : kUp = 0 : kDn = 0
  kFaster = 0 : kSlower = 0 : kFire = 0 : kQuit = 0
  kView = -1 : kPause = 0
  hnow = 0
  ' INKEY$ first: every KEYDOWN call empties the console input buffer.
  kb$ = INKEY$
  IF kb$ = CHR$(27) THEN kQuit = 1
  IF kb$ = "p" OR kb$ = "P" THEN kPause = 1
  FOR i = 1 TO 6
    k = KEYDOWN(i)
    SELECT CASE k
      CASE 130, 44, 60   : kRollL = 1        ' left arrow, comma, less than
      CASE 131, 46, 62   : kRollR = 1        ' right arrow, full stop, greater than
      CASE 128, 88, 120  : kUp = 1           ' up arrow, X
      CASE 129, 83, 115  : kDn = 1           ' down arrow, S
      CASE 32            : kFaster = 1       ' space
      CASE 47, 63        : kSlower = 1       ' slash, question mark
      CASE 65, 97        : kFire = 1         ' A
      CASE 27            : kQuit = 1
      CASE 145 TO 148    : kView = k - 145   ' F1..F4 select the four views
      ' The original's one-shot keys, and the six information screens.
      CASE 84, 116       : hnow = hnow OR KB_TARGET      ' T
      CASE 77, 109       : hnow = hnow OR KB_MISSILE     ' M
      CASE 69, 101       : hnow = hnow OR KB_ECM         ' E
      CASE 67, 99        : hnow = hnow OR KB_DOCK        ' C
      CASE 72, 104       : hnow = hnow OR KB_JUMP        ' H
      CASE 9             : hnow = hnow OR KB_BOMB        ' TAB, as the BBC
      CASE 74, 106       : hnow = hnow OR KB_HOP         ' J
      CASE 71, 103       : hnow = hnow OR KB_GAL         ' G
      CASE 149 TO 154    : hnow = hnow OR (KB_SCREEN << (k - 149))
    END SELECT
  NEXT i
  IF kView >= 0 THEN vw = kView

  ' Everything above this line is a rate that a held key should keep
  ' feeding.  Everything below it happens once per press: holding C would
  ' otherwise toggle the docking computer on and off every frame, and
  ' holding M would empty the missile racks in a fifth of a second.
  hnew = hnow AND (hnow XOR kHeld)
  kHeld = hnow
  kTarget = (hnew AND KB_TARGET) <> 0
  kMissile = (hnew AND KB_MISSILE) <> 0
  kECM = (hnew AND KB_ECM) <> 0
  kDock = (hnew AND KB_DOCK) <> 0
  kJump = (hnew AND KB_JUMP) <> 0
  kBomb = (hnew AND KB_BOMB) <> 0
  kHop = (hnew AND KB_HOP) <> 0
  kGal = (hnew AND KB_GAL) <> 0
  kChart = 0
  IF (hnew AND KB_SCREENS) <> 0 THEN
    FOR i = 0 TO 5
      IF (hnew AND (KB_SCREEN << i)) <> 0 THEN kChart = i + 1 : EXIT FOR
    NEXT i
  ENDIF
END SUB

' One frame of control input.  Order matters: the keys move the value,
' then the spring pulls it back, then the angle is derived from where it
' ended up.
SUB UpdatePlayer
  LOCAL INTEGER d
  ' --- roll and pitch.  A held key pushes the rate away from centre, but
  '     it cannot cross the centre in one press: if the step would take it
  '     to the far side, it stops dead at centre instead.  That is the
  '     original's auto-recentre, and it is what makes a reversal feel
  '     crisp rather than sluggish.
  IF kRollL THEN
    pRoll = Recentre(pRoll, -JROLLSTEP)
  ELSEIF kRollR THEN
    pRoll = Recentre(pRoll, JROLLSTEP)
  ELSE
    pRoll = Spring(pRoll, JDAMPROLL)
  ENDIF
  IF kUp THEN
    pPitch = Recentre(pPitch, -JPITCHSTEP)
  ELSEIF kDn THEN
    pPitch = Recentre(pPitch, JPITCHSTEP)
  ELSE
    pPitch = Spring(pPitch, JDAMPPITCH)
  ENDIF

  ' --- speed.  The ceiling is tested before the increase but not before
  '     the decrease, so holding both keys at full speed settles at one
  '     below it rather than doing nothing.
  IF kFaster THEN
    IF dSpeed < MAXSPEED THEN dSpeed = dSpeed + 1
  ENDIF
  IF kSlower THEN dSpeed = dSpeed - 1
  IF dSpeed < 1 THEN dSpeed = 1

  ' --- the rotation angles.  Both curves are deliberately non-linear:
  '     small deflections are halved again, which gives fine control near
  '     centre and a hard bank at the extremes.  Roll reaches 31/256 of a
  '     radian a frame, pitch only 8/256, so a ship rolls nearly four
  '     times as fast as it pitches - which is why Elite is flown by
  '     rolling onto a target and then pulling.
  d = ABS(pRoll - JCENTRE)
  alp1 = d \ 4
  IF alp1 < 8 THEN alp1 = alp1 \ 2
  alp2 = SGN(pRoll - JCENTRE)

  d = ABS(pPitch - JCENTRE) + 4
  bet1 = d \ 16
  IF bet1 < 3 THEN bet1 = bet1 \ 2
  bet2 = SGN(pPitch - JCENTRE)

  alpha = alp2 * alp1 / ANGSCALE
  beta = bet2 * bet1 / ANGSCALE
END SUB

' Move a rate by one key step, stopping at the centre rather than through
' it.  Returns the new value, clamped to the 1..255 range it lives in.
FUNCTION Recentre(n AS INTEGER, dv AS INTEGER) AS INTEGER
  LOCAL INTEGER v
  v = n + dv
  IF SGN(n - JCENTRE) <> 0 THEN
    IF SGN(v - JCENTRE) <> SGN(n - JCENTRE) THEN v = JCENTRE
  ENDIF
  IF v < 1 THEN v = 1
  IF v > 255 THEN v = 255
  Recentre = v
END FUNCTION

' The spring: move n one step per call towards the centre, without
' overshooting it.
FUNCTION Spring(n AS INTEGER, nstep AS INTEGER) AS INTEGER
  LOCAL INTEGER v, i
  v = n
  FOR i = 1 TO nstep
    IF v > JCENTRE THEN
      v = v - 1
    ELSEIF v < JCENTRE THEN
      v = v + 1
    ENDIF
  NEXT i
  Spring = v
END FUNCTION
