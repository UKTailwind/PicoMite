' Play each of Elite's ten sounds in turn, named, so they can be judged by
' ear.  Standalone: it carries its own copy of the table from 45_sound.bas.
'
' The five marked "approximated" ask for one of the BBC's sound envelopes,
' which the cassette loader defined and the game's source does not carry, so
' those are a sweep between two frequencies rather than the real thing.  Tell
' me which ones are wrong and what they should sound like.
'
'   RUN, and press a key between each.  ESC to stop.

OPTION EXPLICIT
OPTION DEFAULT NONE

CONST NSFX = 9
DIM INTEGER sfxCh(NSFX-1), sfxWv(NSFX-1), sfxF0(NSFX-1), sfxF1(NSFX-1)
DIM INTEGER sfxMs(NSFX-1), sfxVol(NSFX-1), sfxWb(NSFX-1)
DIM nm$(NSFX-1) LENGTH 40
DIM INTEGER chWv(4), chF0(4), chF1(4), chVol(4), chWb(4), chLast(4)
DIM FLOAT chT0(4), chT1(4)
DIM INTEGER i
DIM ky$ LENGTH 2

RESTORE dat_sfx
FOR i = 0 TO NSFX - 1
  READ nm$(i), sfxCh(i), sfxWv(i), sfxF0(i), sfxF1(i), sfxMs(i), sfxVol(i), sfxWb(i)
NEXT i
FOR i = 1 TO 4 : chT1(i) = 0 : chLast(i) = -1 : NEXT i

CLS
PRINT "Elite sounds - a key plays the next one, ESC stops"
PRINT
FOR i = 0 TO NSFX - 1
  PRINT i; "  "; nm$(i)
  Sfx i
  ' Let it run its course, servicing the sweep as the game does.
  DO
    SoundService
  LOOP UNTIL chT1(sfxCh(i)) = 0
  ' The E.C.M. runs until stopped, so stop it.
  SfxStop i
  PAUSE 400
  DO
    ky$ = INKEY$
  LOOP UNTIL ky$ <> ""
  IF ky$ = CHR$(27) THEN EXIT FOR
NEXT i
SoundOff
PRINT
PRINT "done"
END

SUB Sfx(n AS INTEGER)
  LOCAL INTEGER c
  c = sfxCh(n)
  chWv(c) = sfxWv(n)
  chF0(c) = sfxF0(n) : chF1(c) = sfxF1(n)
  chVol(c) = sfxVol(n) : chWb(c) = sfxWb(n)
  chT0(c) = TIMER
  chT1(c) = TIMER + sfxMs(n)
  chLast(c) = chF0(c)
  PlayCh c, chWv(c), chF0(c), chVol(c)
END SUB

SUB SfxStop(n AS INTEGER)
  LOCAL INTEGER c
  c = sfxCh(n)
  IF chT1(c) = 0 THEN EXIT SUB
  chT1(c) = 0
  PLAY SOUND c, B, O
END SUB

SUB SoundService
  LOCAL INTEGER c, f
  LOCAL FLOAT t, kk
  t = TIMER
  FOR c = 1 TO 4
    IF chT1(c) > 0 THEN
      IF t >= chT1(c) THEN
        chT1(c) = 0
        PLAY SOUND c, B, O
      ELSE
        kk = (t - chT0(c)) / (chT1(c) - chT0(c))
        f = chF0(c) + (chF1(c) - chF0(c)) * kk
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
    CASE 0 : PLAY SOUND c, B, Q, f, v
    CASE 1 : PLAY SOUND c, B, N, f, v
    CASE ELSE : PLAY SOUND c, B, P, f, v
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
DATA "laser, ours (approximated)",      1, 0, 900, 122, 800, 12, 0
DATA "hit by lasers (approximated)",    1, 0, 230, 150, 400, 15, 0
DATA "explosion, noise half",           2, 1, 3000, 200, 1300, 18, 0
DATA "explosion, tone half (approx)",   3, 0, 3891, 400, 1200, 10, 0
DATA "short high beep (exact)",         3, 0, 1839, 1839, 50, 15, 0
DATA "long low beep (exact)",           3, 0, 145, 145, 400, 18, 0
DATA "missile away / launch (exact)",   2, 1, 800, 200, 600, 15, 0
DATA "hyperspace (approximated)",       2, 2, 200, 2400, 800, 15, 0
DATA "E.C.M. (approximated)",           4, 0, 1997, 1997, 1200, 12, 1
