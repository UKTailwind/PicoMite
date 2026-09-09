' keytest.bas - Elite Phase 0 keyboard check for the PC3 USB keyboard
' Hold several keys at once (e.g. S + < + A) for a few seconds.  Reports the
' largest number of simultaneously held keys KEYDOWN(0) ever returned, the
' codes seen, and whether the arrow keys and function keys report.
' Elite needs at least three held keys (pitch + roll + fire).  ESC ends.
OPTION EXPLICIT
DIM INTEGER i, k, n, best, seen(255), t0
DIM k$
PRINT "Hold keys now (ESC to finish)."
PRINT "Try: S + < + A together, then the arrows, then F1..F4."
best = 0
t0 = TIMER
DO
  k$ = INKEY$
  n = KEYDOWN(0)
  IF n > best THEN best = n : PRINT "held "; n; " keys at once"
  FOR i = 1 TO 6
    k = KEYDOWN(i)
    IF k > 0 AND k < 256 THEN
      IF seen(k) = 0 THEN
        seen(k) = 1
        PRINT "key code "; k; " ("; CHR$(k * (k > 31 AND k < 127)); ")"
      ENDIF
    ENDIF
  NEXT i
  PAUSE 10
LOOP UNTIL k$ = CHR$(27) OR TIMER - t0 > 30000
PRINT "max simultaneous keys: "; best
PRINT "arrows seen: "; seen(128); seen(129); seen(130); seen(131); "  F1-F4 seen: "; seen(145); seen(146); seen(147); seen(148)
END
