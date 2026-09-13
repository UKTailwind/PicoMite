' =====================================================================
'  Sound: the original's ten effects
'
'  The 6502 source carries them as an SFX table, each entry being what the
'  BBC's SOUND statement was handed - a channel, an amplitude or envelope
'  number, a pitch and a duration:
'
'    0   &12,&01,&00,&10   lasers fired by us
'    8   &12,&02,&2C,&08   we are being hit by lasers
'    16  &11,&03,&F0,&18   we died / we made a kill, part 2
'    24  &10,&F1,&07,&1A   we died / we made a kill, part 1
'    32  &03,&F1,&BC,&01   short, high beep
'    40  &13,&F4,&0C,&08   long, low beep
'    48  &10,&F1,&06,&0C   missile launched, or we launched from the station
'    56  &10,&02,&60,&10   hyperspace drive engaged
'    64  &13,&04,&C2,&FF   E.C.M. on
'    72  &13,&00,&00,&00   E.C.M. off
'
'  Two conversions.  BBC pitch is quarter-semitones with 53 as middle C, so
'  a pitch p is 261.63 * 2^((p - 53) / 48) hertz; duration is in twentieths
'  of a second.  Channel 0 is the noise channel, where the pitch chooses the
'  kind of noise rather than a note - 0 to 3 periodic, 4 to 7 white - and
'  channels 1 to 3 are the square wave tones of the BBC's sound chip, which
'  is why every tone here is type Q.
'
'  What cannot be converted is the envelopes.  Five of the ten ask for
'  envelopes 1 to 4, and those were defined by the cassette loader rather
'  than by the game, so they are in nothing we have.  An envelope sweeps
'  pitch across the life of a note, so each of those five is approximated
'  by a sweep between two frequencies and marked as such below.  The other
'  five are exactly the original's numbers.
'
'  Nothing here blocks.  Sfx starts an effect; SoundService, called from the
'  frame loop and from any wait for a key, slides the pitch and switches the
'  channel off when its time is up.  A PLAY SOUND waveform is a keyword and
'  not a string, so it cannot come out of a variable - hence PlayCh.
' =====================================================================

SUB LoadSounds
  LOCAL INTEGER i
  RESTORE dat_sfx
  FOR i = 0 TO NSFX - 1
    READ sfxCh(i), sfxWv(i), sfxF0(i), sfxF1(i), sfxMs(i), sfxVol(i), sfxWb(i)
  NEXT i
  FOR i = 1 TO 4 : chT1(i) = 0 : chLast(i) = -1 : NEXT i
END SUB

SUB Sfx(n AS INTEGER)
  LOCAL INTEGER c
  IF SOUNDON = 0 THEN EXIT SUB
  c = sfxCh(n)
  chWv(c) = sfxWv(n)
  chF0(c) = sfxF0(n) : chF1(c) = sfxF1(n)
  chVol(c) = sfxVol(n) : chWb(c) = sfxWb(n)
  chT0(c) = TIMER
  chT1(c) = TIMER + sfxMs(n)
  chLast(c) = chF0(c)
  PlayCh c, chWv(c), chF0(c), chVol(c)
END SUB

' Stop one effect early.  The E.C.M. is the only one that needs it: the
' original gives it a duration of 255, which on the BBC means play until
' told otherwise.
SUB SfxStop(n AS INTEGER)
  LOCAL INTEGER c
  IF SOUNDON = 0 THEN EXIT SUB
  c = sfxCh(n)
  IF chT1(c) = 0 THEN EXIT SUB
  chT1(c) = 0
  PLAY SOUND c, B, O
END SUB

' Called often rather than on a schedule, so it works off the clock rather
' than counting frames, and only touches a channel when the pitch it wants
' has actually moved.
SUB SoundService
  LOCAL INTEGER c, f
  LOCAL FLOAT t, k
  IF SOUNDON = 0 THEN EXIT SUB
  t = TIMER
  FOR c = 1 TO 4
    IF chT1(c) > 0 THEN
      IF t >= chT1(c) THEN
        chT1(c) = 0
        PLAY SOUND c, B, O
      ELSE
        k = (t - chT0(c)) / (chT1(c) - chT0(c))
        f = chF0(c) + (chF1(c) - chF0(c)) * k
        ' The E.C.M.'s envelope is a warble rather than a slide.
        IF chWb(c) THEN
          IF (INT(t / 45) AND 1) = 1 THEN f = f * 3 \ 4
        ENDIF
        IF f < 1 THEN f = 1
        IF f <> chLast(c) THEN
          PlayCh c, chWv(c), f, chVol(c)
          chLast(c) = f
        ENDIF
      ENDIF
    ENDIF
  NEXT c
END SUB

SUB PlayCh(c AS INTEGER, w AS INTEGER, f AS INTEGER, v AS INTEGER)
  SELECT CASE w
    CASE 0 : PLAY SOUND c, B, Q, f, v      ' square, as the BBC's tone channels
    CASE 1 : PLAY SOUND c, B, N, f, v      ' white noise
    CASE ELSE : PLAY SOUND c, B, P, f, v   ' periodic noise
  END SELECT
END SUB

SUB SoundOff
  LOCAL INTEGER c
  FOR c = 1 TO 4
    chT1(c) = 0
    PLAY SOUND c, B, O
  NEXT c
  PLAY STOP
END SUB

dat_sfx:
' channel, waveform (0 square, 1 white noise, 2 periodic noise),
' start frequency, end frequency, milliseconds, volume, warble
'
' SFX 0: laser.  Envelope 1, so the sweep is a guess; the original's 0.8
' seconds is longer than a pulse laser's repeat, and each shot restarts the
' sound as the BBC's flush control did, so what is heard is the top of the
' sweep over and over - which is the zap.
DATA 1, 0, 900, 122, 800, 12, 0
' SFX 8: hit by lasers.  Pitch 44 is 230 Hz; envelope 2 approximated.
DATA 1, 0, 230, 150, 400, 15, 0
' SFX 24: the noise half of an explosion, white noise, 1.3 seconds.
DATA 2, 1, 3000, 200, 1300, 18, 0
' SFX 16: the tone half.  Pitch 240 is 3891 Hz; envelope 3 approximated.
DATA 3, 0, 3891, 400, 1200, 10, 0
' SFX 32: short high beep.  Pitch 188, one twentieth of a second.  Exact.
DATA 3, 0, 1839, 1839, 50, 15, 0
' SFX 40: long low beep.  Pitch 12, four twentieths.  Exact.
DATA 3, 0, 145, 145, 400, 18, 0
' SFX 48: missile away, or our own launch.  Low white noise, 0.6 seconds.
DATA 2, 1, 800, 200, 600, 15, 0
' SFX 56: hyperspace.  The noise channel with envelope 2; pitch 96 lands on
' periodic noise.  Rising, because it is a drive spinning up.
DATA 2, 2, 200, 2400, 800, 15, 0
' SFX 64: E.C.M.  Pitch 194 is 1997 Hz, envelope 4 approximated as a warble,
' and it runs for as long as the burst does rather than for a fixed time.
DATA 4, 0, 1997, 1997, 1200, 12, 1
