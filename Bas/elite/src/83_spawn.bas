' =====================================================================
'  The traffic: who turns up, and why
'
'  Once every 256 frames the original decides whether to put something new
'  in the bubble.  It is three decisions taken in order, and each one can
'  end the pass:
'
'    1  something harmless - a trader, an asteroid, a cargo canister
'    2  the police, if our record or our cargo hold warrants it
'    3  a bounty hunter, a Thargoid, or a pack of pirates
'
'  The second is what makes Elite a game rather than a shooting gallery.
'  It weighs the hold - slaves and narcotics count double, firearms once -
'  and doubles that, and if a Viper is already nearby it takes the worse of
'  the result and our legal record, because by then they have scanned us.
'  A random byte is drawn against the total, so a clean ship carrying
'  nothing is almost never troubled, while a full hold of slaves brings a
'  Viper on nearly every pass.  Nothing here asks whether we have done
'  anything: it asks what we are carrying and what is on our record, and
'  those two are the whole of the player's relationship with the law.
'
'  The third asks the system's government.  An anarchy always rolls the
'  dice; everywhere else has to pass two tests that the safer governments
'  fail more often, which is why the trouble on the chart is always in the
'  Feudal and Anarchic systems.
'
'  Witchspace spawns nothing - what is out there arrived with us - and
'  neither does the inside of a station's safe zone, except for traders.
' =====================================================================

SUB SpawnTraffic
  IF mcnt <> 0 THEN EXIT SUB              ' once every 256 frames, as MCNT
  IF docked OR dead OR inWitch THEN EXIT SUB
  ' --- 1: something harmless, on one pass in eight
  IF INT(RND * 256) < 35 THEN
    IF CountType(T_ASTEROID) < 3 THEN
      IF SpawnBenign() THEN EXIT SUB
    ENDIF
  ENDIF
  ' Nothing hostile is ever created inside the station's zone.
  IF inSafe THEN EXIT SUB
  IF SpawnPolice() THEN EXIT SUB
  SpawnHostiles
END SUB

' A trader, a rock or a canister, a long way off and off to one side.
' Returns 1 when this is the whole of the pass: a trader is, and so is
' being inside the safe zone, where nothing else may be created.
FUNCTION SpawnBenign() AS INTEGER
  LOCAL INTEGER n, t
  LOCAL FLOAT x, y, z
  SpawnBenign = 0
  z = 38 * 256                            ' far enough to be a dot at first
  x = INT(RND * 256)
  IF RND < 0.5 THEN x = x + 512           ' and sometimes half a unit further
  IF RND < 0.5 THEN x = -x
  y = INT(RND * 256)
  IF RND < 0.5 THEN y = -y

  IF RND < 0.5 THEN
    ' A Cobra Mk III on its way somewhere, with no AI at all: it will not
    ' evade, it will not shoot, and it will not thank you for either.
    n = NewFacing(T_COBRA3, x, y, z, 180)
    IF n >= 0 THEN
      sAI(n) = 0
      sSpd(n) = 16 + INT(RND * 16)
      sRol(n) = INT(RND * 128)            ' clockwise, and rarely undamped
    ENDIF
    SpawnBenign = 1
    EXIT FUNCTION
  ENDIF

  ' The station keeps its own space clear of rocks.
  IF inSafe THEN SpawnBenign = 1 : EXIT FUNCTION
  t = T_ASTEROID
  IF INT(RND * 256) < 5 THEN t = T_CANISTER      ' 1.5 per cent of the time
  n = NewFacing(t, x, y, z, 180)
  IF n >= 0 THEN
    ' Tumbling: half of them roll and travel, half pitch on the spot, and
    ' half of each turn for ever rather than winding down.
    IF RND < 0.5 THEN
      sSpd(n) = 16 + INT(RND * 16)
      sRol(n) = INT(RND * 256) OR 111
    ELSE
      sPit(n) = INT(RND * 256) OR 127
    ENDIF
  ENDIF
END FUNCTION

' The police.  Returns 1 if a Viper is about, whether it has just arrived
' or was already here - either way the law is this pass's business.
FUNCTION SpawnPolice() AS INTEGER
  LOCAL INTEGER bad, n
  bad = Contraband() * 2
  ' A Viper already in the bubble has had a good look at us, so our record
  ' counts as well as the hold - whichever of the two is worse.
  IF CountType(T_VIPER) > 0 THEN bad = bad OR legal
  IF INT(RND * 256) < bad THEN n = Aggressor(T_VIPER, 0)
  SpawnPolice = 0
  IF CountType(T_VIPER) > 0 THEN SpawnPolice = 1
END FUNCTION

' A lone bounty hunter, something much worse, or a pack of pirates.
SUB SpawnHostiles
  LOCAL INTEGER r, i, t, ai, cnt, n
  ' Not every pass: the last spawning sets how many to sit out.
  spawnEV = spawnEV - 1
  IF spawnEV >= 0 THEN EXIT SUB
  spawnEV = 0

  r = INT(RND * 256)
  IF sysGov <> 0 THEN
    ' Anywhere but an anarchy has to pass both of these, and the higher the
    ' government number the safer the system, so the more often it does not.
    IF r >= 90 THEN EXIT SUB
    IF (r AND 7) < sysGov THEN EXIT SUB
  ENDIF

  r = INT(RND * 256)
  IF r >= 200 THEN
    ' One to four pirates, Sidewinders and Mambas, and a rest afterwards.
    cnt = INT(RND * 4)
    spawnEV = cnt
    FOR i = 0 TO cnt
      t = INT(RND * 4) OR 1               ' 1 or 3: Sidewinder or Mamba
      n = Aggressor(t, 0)
    NEXT i
    EXIT SUB
  ENDIF

  ' A lone hunter in something serious, and a pass off afterwards.
  spawnEV = spawnEV + 1
  t = (r AND 3) + 3                       ' Mamba, Python, Cobra Mk III, Thargoid
  ai = 192
  IF INT(RND * 256) >= 200 THEN ai = ai OR 1     ' 22 per cent carry an E.C.M.
  IF t = T_THARGOID THEN
    ' And only sometimes, because a Thargoid is not a bounty hunter.
    IF INT(RND * 256) >= 200 THEN
      IF Aggressor(T_THARGOID, ai) >= 0 THEN n = Aggressor(T_THARGON, 129)
    ENDIF
    EXIT SUB
  ENDIF
  n = Aggressor(t, ai)
END SUB

' The original's Ze: a ship a fair way off in one of the four corners,
' already hostile.  Bit 7 of the AI flag means it has AI at all and bits
' 1 to 6 are how aggressive it is; 192 is the least it is ever given.
' It arrives stationary and its own tactics wind it up to speed.
FUNCTION Aggressor(t AS INTEGER, ai AS INTEGER) AS INTEGER
  LOCAL INTEGER n, a
  LOCAL FLOAT x, y
  x = 8192 : IF RND < 0.5 THEN x = -x
  y = 8192 : IF RND < 0.5 THEN y = -y
  a = ai
  IF a = 0 THEN
    a = 192
    IF INT(RND * 256) >= 245 THEN a = a OR 1     ' 4 per cent carry an E.C.M.
  ENDIF
  n = NewFacing(t, x, y, 8192, 180)
  IF n >= 0 THEN sAI(n) = a
  Aggressor = n
END FUNCTION

' What is in the hold that we would rather the police did not see.  Slaves
' and narcotics are twice as illegal as firearms, which is the original's
' own arithmetic and the reason a hold of slaves is such a bad idea.
FUNCTION Contraband() AS INTEGER
  Contraband = (cargo(3) + cargo(6)) * 2 + cargo(10)
END FUNCTION

FUNCTION CountType(t AS INTEGER) AS INTEGER
  LOCAL INTEGER n, c
  c = 0
  FOR n = 2 TO nUsed - 1
    IF sTyp(n) = t THEN
      IF sExp(n) = 0 THEN c = c + 1
    ENDIF
  NEXT n
  CountType = c
END FUNCTION
