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

' --- the original's control constants, all in units per frame
CONST JCENTRE = 128                ' the centre of the 1..255 range
CONST JROLLSTEP = 7                ' a held roll key moves JSTX this far
CONST JPITCHSTEP = 14              ' a held pitch key moves JSTY this far
CONST JDAMPROLL = 2                ' the spring pulls roll back this fast
CONST JDAMPPITCH = 1               ' and pitch this fast
CONST MAXSPEED = 40                ' DELTA's ceiling
CONST ANGSCALE = 256               ' ALP1 / 256 is the angle in radians

SUB ReadKeys
  LOCAL INTEGER i, k
  LOCAL kb$ LENGTH 2
  kRollL = 0 : kRollR = 0 : kUp = 0 : kDn = 0
  kFaster = 0 : kSlower = 0 : kFire = 0 : kQuit = 0
  kView = -1 : kPause = 0
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
    END SELECT
  NEXT i
  IF kView >= 0 THEN vw = kView
END SUB

' One frame of control input.  Order matters: the keys move the value,
' then the spring pulls it back, then the angle is derived from where it
' ended up.
SUB UpdatePlayer
  LOCAL INTEGER d
  ' --- roll.  Pushing the opposite way from beyond centre snaps to
  '     centre first, so a reversal is immediate rather than sluggish.
  IF kRollL THEN
    IF pRoll > JCENTRE THEN pRoll = JCENTRE
    pRoll = pRoll - JROLLSTEP
    IF pRoll < 1 THEN pRoll = 1
  ELSEIF kRollR THEN
    IF pRoll < JCENTRE THEN pRoll = JCENTRE
    pRoll = pRoll + JROLLSTEP
    IF pRoll > 255 THEN pRoll = 255
  ELSE
    pRoll = Spring(pRoll, JDAMPROLL)
  ENDIF

  ' --- pitch
  IF kUp THEN
    IF pPitch > JCENTRE THEN pPitch = JCENTRE
    pPitch = pPitch - JPITCHSTEP
    IF pPitch < 1 THEN pPitch = 1
  ELSEIF kDn THEN
    IF pPitch < JCENTRE THEN pPitch = JCENTRE
    pPitch = pPitch + JPITCHSTEP
    IF pPitch > 255 THEN pPitch = 255
  ELSE
    pPitch = Spring(pPitch, JDAMPPITCH)
  ENDIF

  ' --- speed
  IF kFaster THEN dSpeed = dSpeed + 1
  IF kSlower THEN dSpeed = dSpeed - 1
  IF dSpeed > MAXSPEED THEN dSpeed = MAXSPEED
  IF dSpeed < 0 THEN dSpeed = 0

  ' --- the rotation angles.  Both curves are deliberately non-linear:
  '     small deflections are halved again, which gives fine control
  '     near centre and a hard bank at the extremes.
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

' The spring: move n one step per call towards the centre, without
' overshooting it.
FUNCTION Spring(n AS INTEGER, steps AS INTEGER) AS INTEGER
  LOCAL INTEGER v, i
  v = n
  FOR i = 1 TO steps
    IF v > JCENTRE THEN
      v = v - 1
    ELSEIF v < JCENTRE THEN
      v = v + 1
    ENDIF
  NEXT i
  Spring = v
END FUNCTION
