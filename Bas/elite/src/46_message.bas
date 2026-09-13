' =====================================================================
'  In-flight messages
'
'  The original writes them at column 9 of row 22, near the foot of the
'  space view, and leaves them there for 22 iterations of its main loop.
'  Ours runs four or five times faster than a BBC did, so counting frames
'  would flash them past before they could be read; the delay is a time
'  instead, set to sit where the original's did in seconds rather than in
'  frames.
'
'  A new message replaces whatever is there, as the original's does.
' =====================================================================

SUB Message(t$)
  msgText$ = t$
  msgUntil = TIMER + MSGTIME
END SUB

SUB DrawMessage
  IF msgText$ = "" THEN EXIT SUB
  IF TIMER > msgUntil THEN
    msgText$ = ""
    EXIT SUB
  ENDIF
  TEXT MSGX, MSGY, msgText$, "LT", 7, 1, cWhite
END SUB

' The energy warning is the one message the original raises on a schedule
' rather than on an event: it looks on the tenth frame of every block of
' thirty-two, and only speaks when the banks are under fifty.
SUB EnergyWarning
  IF (mcnt AND 31) <> 10 THEN EXIT SUB
  IF pEnergy >= 50 THEN EXIT SUB
  Message "ENERGY LOW"
  Sfx SFX_BEEP
END SUB
