' jsonbig.bas - build a LARGE JSON document in RAM and stress-parse it.
'
' No network needed. This is the controlled way to push the JSON$ arena: it
' synthesises an array of N objects (size set by N below), then parses it many
' times. Each JSON$ call parses the whole document from scratch and bulk-frees
' the tree via json_arena_reset(), so a sweep over all N elements is N full
' parses - a hard soak on the allocate/free cycle with zero network dependence.
'
' It also exercises the "items[i].field" path form (index an array, then read a
' named field of the element) that real REST APIs use.

OPTION EXPLICIT

CONST N = 500                       ' number of objects; raise to go bigger
CONST Q = CHR$(34)                  ' a double-quote character

DIM js%(96000 \ 8)                  ' JSON document buffer (96 KB)
DIM obj$, path$, got$
DIM i%, t1%, bad%, blen%

' --- build {"items":[ {"id":0,"name":"item0","value":0.0}, ... ]} -----------
PRINT "Building a JSON array of " N " objects ..."
LongString CLEAR js%()
LongString APPEND js%(), "{" + Q + "items" + Q + ":["
FOR i% = 0 TO N - 1
  obj$ =        "{" + Q + "id" + Q + ":" + STR$(i%)
  obj$ = obj$ + "," + Q + "name" + Q + ":" + Q + "item" + STR$(i%) + Q
  obj$ = obj$ + "," + Q + "value" + Q + ":" + STR$(i% / 10) + "}"
  IF i% < N - 1 THEN obj$ = obj$ + ","
  LongString APPEND js%(), obj$
NEXT
LongString APPEND js%(), "]}"
blen% = LLen(js%())
PRINT "Document   : " blen% " bytes (" N " objects)"
PRINT

' --- spot-check a few elements ---------------------------------------------
PRINT "--- spot checks ---"
PRINT "items[0].name      : " JSON$(js%(), "items[0].name")
PRINT "items[0].value     : " JSON$(js%(), "items[0].value")
PRINT "items[" STR$(N\2) "].name    : " JSON$(js%(), "items[" + STR$(N \ 2) + "].name")
PRINT "items[" STR$(N-1) "].name    : " JSON$(js%(), "items[" + STR$(N - 1) + "].name")
PRINT

' --- soak: read every element back and verify it parsed correctly ----------
' N full parses of the whole document. id must come back equal to its index;
' any drift or corruption from a leaked/over-reused arena would show here.
PRINT "Soak: parsing all " N " elements (" N " full parses) ..."
bad% = 0
t1% = TIMER
FOR i% = 0 TO N - 1
  path$ = "items[" + STR$(i%) + "].id"
  got$ = JSON$(js%(), path$)
  IF VAL(got$) <> i% THEN bad% = bad% + 1
  IF i% MOD 50 = 0 THEN PRINT ".";
NEXT
PRINT
IF bad% > 0 THEN
  PRINT bad% " MISMATCHES - arena is leaking or corrupting!"
  END
END IF
PRINT N " parses OK in " (TIMER - t1%) " ms (" STR$((TIMER - t1%) / N, 0, 2) " ms each)"
PRINT
PRINT "Done - " N " parses of a " blen% "-byte document, no reboot, no drift."
PRINT "Raise N for a bigger document (96 KB buffer holds ~2300 objects)."
END
