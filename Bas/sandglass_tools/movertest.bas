' movertest.bas - Phase 5: the moving parts, checked against the host exactly.
'
' Plates, gates, spikes, loose floors, slicers and torches all animate through
' one mechanism in the original: a block's state byte, advanced once a frame
' for every object on a transition list, plus a list of floors in free fall.
' This is that mechanism in BASIC.  It runs the same scripted list of events as
' moverref.py, on the same level, and checksums the blueprint, the transition
' list and the falling floors after every frame.  The number must match.
'
' The routines here are the engine's: they are written to be lifted into
' player.bas unchanged, which is why the character is called c* throughout.
'
' Needs blocks.dat, levels.dat and tables.idx from the converter.

Option Console Serial
Option Explicit
Option Default None

Const LEVELBYTES = 2304
Const COLS = 10
Const ROWS = 3
Const OFF_SPEC = 720
Const OFF_LINKLOC = 1440
Const OFF_LINKMAP = 1696
Const OFF_MAP = 1952
Const MAXTR = 31
Const MAXMOB = 15

' Block types that move.
Const T_SPACE = 0 : Const T_FLOOR = 1 : Const T_SPIKES = 2 : Const T_GATE = 4
Const T_DPLATE = 5 : Const T_PLATE = 6 : Const T_FLASK = 10 : Const T_LOOSE = 11
Const T_RUBBLE = 14 : Const T_UPLATE = 15 : Const T_EXIT = 16 : Const T_SLICER = 18
Const T_TORCH = 19 : Const T_BLOCK = 20

' Expected results, from moverref.py.
Const EXP1_SUM = 10148516
Const EXP1_SOUNDS = 593
Const EXP1_CRUSH = 2
Const EXP2_SUM = 16136518
Const EXP2_SOUNDS = 14
' Print the transition list at a few frames of the second run.
Const TRACE = 0

Dim INTEGER packed(5000), level(LEVELBYTES), blocks(1024)
Dim INTEGER blocksLen, gBlockAy, gBlockBot, aGateInc, aGateVel
Dim INTEGER kPPTimer, kSpikeTimer, kSliceTimer, kGateTimer, kLooseTimer
Dim INTEGER kFFAccel, kFFTermVel, kCrumble, kDisappear, kCrushDist, kMaxGateVel
Dim INTEGER kWiggle, kSlicerSync, kGMax, kGMin, kTorchLast, kSpikeExt, kSpikeRet
Dim INTEGER kSlicerExt, kSlicerRet

' The transition list and the falling floors.
Dim INTEGER trLoc(MAXTR), trScrn(MAXTR), trDir(MAXTR), numTrans
Dim INTEGER mobX(MAXMOB), mobY(MAXMOB), mobScrn(MAXMOB), mobVel(MAXMOB)
Dim INTEGER mobType(MAXMOB), mobLevel(MAXMOB), numMob
' The object being worked on, and the mob being worked on.
Dim INTEGER oLoc, oScrn, oDir, oState, ppType, linkIndex
Dim INTEGER wX, wY, wScrn, wVel, wType, wLevel, wIdx
' The original's zero-page temporaries left by a block read.
Dim INTEGER tScrn, tBX, tBY
Dim INTEGER rndSeed, visScrn, curLevel, nSounds, alertGuard, exitOpen, jarAbove, nCrush
' The character, as the engine names him.
Dim INTEGER cScrn, cBlockX, cBlockY, cY, cLife

Dim INTEGER f, ck, i, run, frames, ok
Dim STRING home
Dim FLOAT t0

home = MM.Info(Path)
If home = "NONE" Then home = "A:/"

Print "--- moving parts test"
ReadLayout
LoadBytes home + "blocks.dat", blocksLen, blocks()

ok = 1
For run = 1 To 2
  If run = 1 Then curLevel = 1 Else curLevel = 3
  LoadLevel curLevel
  numTrans = 0 : numMob = 0 : nSounds = 0 : nCrush = 0 : exitOpen = 0 : alertGuard = 0
  rndSeed = 0 : visScrn = 1 : jarAbove = 0
  cScrn = 0 : cBlockX = 0 : cBlockY = 0 : cY = 0 : cLife = &HFF
  ck = 0
  If run = 1 Then frames = 300 Else frames = 120
  t0 = Timer
  For f = 1 To frames
    If run = 1 Then Script1 f Else Script2 f
    MoverFrame
    Checksum
    If TRACE And run = 2 And (f < 4 Or f = 59 Or f = 60 Or f = 89 Or f = 90 Or f = 120) Then TraceFrame
  Next f
  Print "level "; Str$(curLevel); ", "; Str$(frames); " frames in "; Str$(Timer - t0, 0, 1); " ms"
  Print "  in transition  "; Str$(numTrans)
  Print "  falling floors "; Str$(numMob)
  Print "  sounds         "; Str$(nSounds)
  Print "  crushes        "; Str$(nCrush)
  Print "  exit open      "; Str$(exitOpen)
  Print "  checksum       "; Str$(ck)
  If run = 1 Then
    If ck <> EXP1_SUM Or nSounds <> EXP1_SOUNDS Or nCrush <> EXP1_CRUSH Then ok = 0
  Else
    If ck <> EXP2_SUM Or nSounds <> EXP2_SOUNDS Then ok = 0
  End If
Next run

Print
If ok Then
  Print "PASS - the moving parts agree with the host reference exactly"
Else
  Print "FAIL - the engine and the reference disagree"
End If
Option Console Both
End

'-----------------------------------------------------------------------------
' The scripted events.  Same list as moverref.py, frame for frame.
Sub Script1(fr As INTEGER)
  Select Case fr
    Case 1   : EnterScreen 5, 0
    Case 2   : PushPP 5, 4
    Case 3   : TrigSpikes 6, 23
    Case 5   : BreakLoose 7, 5, 1
    Case 6   : PushPP 5, 2
    Case 8   : ShakeM1 1, 2
    Case 20  : JamPP 6, 2
    Case 40  : PushPP 12, 3
    Case 60  : TrigSpikes 6, 23
    Case 61  : JamSpikes 6, 24
    Case 100 : cScrn = 8 : cBlockX = 3 : cBlockY = 2 : cY = 181
    Case 101 : BreakLoose 8, 13, 1
    Case 150 : PushPP 5, 4
    Case 200 : TrigSpikes 10, 21
    Case 230 : EnterScreen 9, 1
    Case 231 : PushPP 9, 0
  End Select
End Sub

Sub Script2(fr As INTEGER)
  Select Case fr
    Case 1  : EnterScreen 16, 2 : cScrn = 16 : cBlockX = 4 : cBlockY = 2 : cY = 181
    Case 60 : cScrn = 16 : cBlockX = 4 : cBlockY = 1 : cY = 118
    Case 90 : AddSlicers 16, 2
    Case 91 : cScrn = 16 : cBlockX = 4 : cBlockY = 2 : cY = 181
  End Select
End Sub

Sub TraceFrame
  Local INTEGER k
  Print "  f"; Str$(f); " trans";
  For k = 0 To numTrans - 1
    Print " "; Str$(trLoc(k)); ":"; Str$(trScrn(k)); ":"; Str$(trDir(k)); "="; Str$(BSpec(trScrn(k), trLoc(k)));
  Next k
  Print "  specs "; Str$(BSpec(16,23)); " "; Str$(BSpec(16,24)); " "; Str$(BSpec(16,25));
  Print "  snd "; Str$(nSounds); " sum "; Str$(ck)
End Sub

'-----------------------------------------------------------------------------
' The checksum covers everything the movers can change.
Sub Checksum
  Local INTEGER k
  For k = 0 To 1439 : ck = (ck * 31 + level(k)) And &HFFFFFF : Next k
  For k = 0 To numTrans - 1
    ck = (ck * 31 + trLoc(k)) And &HFFFFFF
    ck = (ck * 31 + trScrn(k)) And &HFFFFFF
    ck = (ck * 31 + (trDir(k) And &HFF)) And &HFFFFFF
  Next k
  For k = 0 To numMob - 1
    ck = (ck * 31 + mobX(k)) And &HFFFFFF
    ck = (ck * 31 + mobY(k)) And &HFFFFFF
    ck = (ck * 31 + mobScrn(k)) And &HFFFFFF
    ck = (ck * 31 + mobVel(k)) And &HFFFFFF
    ck = (ck * 31 + mobLevel(k)) And &HFFFFFF
  Next k
  ck = (ck * 31 + nSounds) And &HFFFFFF
End Sub

'=============================================================================
' THE MOVERS.  Everything from here to the loaders is engine code.
'=============================================================================

' One frame: floors in flight first, then everything in transition.
Sub MoverFrame
  AnimMobs
  AnimTrans
End Sub

'---- the blueprint -----------------------------------------------------------
' Screens are numbered from one and the planes are laid out from screen one at
' offset zero.  The type byte keeps its high bits (the "required" flag).
Function BType(scrn As INTEGER, loc As INTEGER) As INTEGER
  BType = level((scrn - 1) * 30 + loc) And &H1F
End Function

Function BSpec(scrn As INTEGER, loc As INTEGER) As INTEGER
  BSpec = level(OFF_SPEC + (scrn - 1) * 30 + loc)
End Function

Sub SetSpec(scrn As INTEGER, loc As INTEGER, v As INTEGER)
  level(OFF_SPEC + (scrn - 1) * 30 + loc) = v And &HFF
End Sub

Sub SetType(scrn As INTEGER, loc As INTEGER, v As INTEGER)
  Local INTEGER k
  k = (scrn - 1) * 30 + loc
  level(k) = (level(k) And &HE0) Or v
End Sub

Function Neighbour(scrn As INTEGER, which As INTEGER) As INTEGER
  If scrn = 0 Then Neighbour = 0 : Exit Function
  Neighbour = level(OFF_MAP + (scrn - 1) * 4 + which)
End Function

' RDBLOCK: resolve a reference that runs off the screen, then read the type.
' Leaves tScrn/tBX/tBY at the resolved position, which the callers use.  A
' null screen reads as solid block, which is what makes the world's edge a
' wall.  The parameters are copied first: a bare variable is passed by
' reference, and this routine reassigns them.
Function RdBlock(scrn As INTEGER, bx As INTEGER, by As INTEGER) As INTEGER
  Local INTEGER s, x, y
  s = scrn : x = bx : y = by
  Do
    If x < 0 Then
      x = x + COLS : s = Neighbour(s, 0)
    ElseIf x >= COLS Then
      x = x - COLS : s = Neighbour(s, 1)
    ElseIf y < 0 Then
      y = y + ROWS : s = Neighbour(s, 2)
    ElseIf y >= ROWS Then
      y = y - ROWS : s = Neighbour(s, 3)
    Else
      Exit Do
    End If
  Loop
  tScrn = s : tBX = x : tBY = y
  If s = 0 Then RdBlock = T_BLOCK : Exit Function
  RdBlock = BType(s, y * COLS + x)
End Function

Function RdBlock1() As INTEGER
  RdBlock1 = RdBlock(tScrn, tBX, tBY)
End Function

'---- the random generator, as GRAFIX.S has it --------------------------------
Function Rnd8() As INTEGER
  rndSeed = (rndSeed * 5 + 23) And &HFF
  Rnd8 = rndSeed
End Function

'---- the link table: plates to what they trigger ----------------------------
Function GetTimer(x As INTEGER) As INTEGER
  GetTimer = level(OFF_LINKMAP + x) And &H1F
End Function

Sub ChgTimer(x As INTEGER, v As INTEGER)
  level(OFF_LINKMAP + x) = (level(OFF_LINKMAP + x) And &HE0) Or (v And &H1F)
End Sub

Function GetLoc(x As INTEGER) As INTEGER
  GetLoc = level(OFF_LINKLOC + x) And &H1F
End Function

Function GetLastFlag(x As INTEGER) As INTEGER
  GetLastFlag = level(OFF_LINKLOC + x) And &H80
End Function

Function GetScrnOf(x As INTEGER) As INTEGER
  Local INTEGER lo, hi
  lo = (level(OFF_LINKLOC + x) And &H60) >> 2
  hi = level(OFF_LINKMAP + x) And &HE0
  GetScrnOf = ((lo + hi) And &HFF) >> 3
End Function

'---- the transition list -------------------------------------------------------
Sub StopObj
  oDir = -1
End Sub

' Add the current object, or if it is already listed just change its direction.
' A full list means the trigger fails, as it does in the original.
Sub AddTrob
  Local INTEGER k
  For k = 0 To numTrans - 1
    If trLoc(k) = oLoc And trScrn(k) = oScrn Then trDir(k) = oDir : Exit Sub
  Next k
  If numTrans >= MAXTR Then Exit Sub
  trLoc(numTrans) = oLoc : trScrn(numTrans) = oScrn : trDir(numTrans) = oDir
  numTrans = numTrans + 1
End Sub

Sub AddSound(n As INTEGER)
  nSounds = nSounds + 1
End Sub

'---- triggers from the character's side ----------------------------------------
' PUSHPP: a plate is stepped on.
Sub PushPP(scrn As INTEGER, loc As INTEGER)
  ppType = BType(scrn, loc)
  PushPP1 scrn, loc
End Sub

' JAMPP: dead weight lands on a plate and crushes it.
Sub JamPP(scrn As INTEGER, loc As INTEGER)
  ppType = BType(scrn, loc)
  If ppType = T_PLATE Then
    SetType scrn, loc, T_DPLATE
  Else
    SetType scrn, loc, T_FLOOR
    SetSpec scrn, loc, 0
    ppType = T_RUBBLE
  End If
  PushPP1 scrn, loc
End Sub

Sub PushPP1(scrn As INTEGER, loc As INTEGER)
  Local INTEGER x, t
  x = BSpec(scrn, loc)
  linkIndex = x
  t = GetTimer(x)
  If t = 31 Then Exit Sub                     ' permanently down
  If t >= 2 Then
    ChgTimer x, kPPTimer                      ' already down: restart the count
    Trigger
    Exit Sub
  End If
  ChgTimer x, kPPTimer
  oLoc = loc : oScrn = scrn : oDir = 1
  AddTrob
  alertGuard = 1
  AddSound 0
  Trigger
End Sub

' Walk the plate's chain of linked objects, triggering each.
Sub Trigger
  Local INTEGER x, t
  Do
    x = linkIndex
    If level(OFF_LINKLOC + x) = &HFF Then Exit Sub
    oLoc = GetLoc(x)
    oScrn = GetScrnOf(x)
    If oScrn = 0 Then t = T_BLOCK Else t = BType(oScrn, oLoc)
    TrigObj t
    If oDir >= 0 Then AddTrob
    linkIndex = (x + 1) And &HFF
    If GetLastFlag(x) Then Exit Sub
  Loop
End Sub

Sub TrigObj(t As INTEGER)
  If t = T_GATE Then
    TrigGate
  ElseIf t = T_EXIT Then
    If BSpec(oScrn, oLoc) <> 0 Then oDir = -1 Else oDir = 1
  End If
End Sub

' What a plate does to a gate depends on the plate: an up-plate raises it, a
' plate lowers it, rubble opens it and jams it there.
Sub TrigGate
  Local INTEGER st
  st = BSpec(oScrn, oLoc)
  If ppType = T_UPLATE Then
    oDir = 1
    If st = &HFF Then StopObj : Exit Sub      ' jammed
    If st < kGMax Then Exit Sub
    SetSpec oScrn, oLoc, kGateTimer           ' already open: restart its timer
    StopObj
  ElseIf ppType = T_RUBBLE Then
    oDir = 2
    If st < kGMax Then Exit Sub
    SetSpec oScrn, oLoc, &HFF
    StopObj
  Else
    If st = kGMin Then StopObj Else oDir = 3
  End If
End Sub

Sub TrigSpikes(scrn As INTEGER, loc As INTEGER)
  Local INTEGER st
  st = BSpec(scrn, loc)
  If st = 0 Then
    oLoc = loc : oScrn = scrn : oDir = 1
    AddTrob
    AddSound 2
  ElseIf st And &H80 Then
    If st <> &HFF Then SetSpec scrn, loc, kSpikeTimer
  End If
End Sub

Sub JamSpikes(scrn As INTEGER, loc As INTEGER)
  SetSpec scrn, loc, &HFF
  oLoc = loc : oScrn = scrn : oDir = -1
  AddTrob
  AddSound 2
End Sub

' 0 safe, 1 lethal, 2 springing.
Function GetSpikes(scrn As INTEGER, loc As INTEGER) As INTEGER
  Local INTEGER st
  st = BSpec(scrn, loc)
  GetSpikes = 0
  If st And &H80 Then
    If st <> &HFF Then GetSpikes = 1
  ElseIf st <> 0 Then
    If st < kSpikeExt Then GetSpikes = 2
  End If
End Function

Sub BreakLoose(scrn As INTEGER, loc As INTEGER, initial As INTEGER)
  Local INTEGER st
  If level((scrn - 1) * 30 + loc) And &H20 Then Exit Sub    ' a required floor
  st = BSpec(scrn, loc)
  If (st And &H80) = 0 And st <> 0 Then Exit Sub            ' already going
  SetSpec scrn, loc, initial
  oLoc = loc : oScrn = scrn : oDir = 0
  AddTrob
End Sub

Sub ShakeIt(scrn As INTEGER, loc As INTEGER)
  If BSpec(scrn, loc) <> 0 Then Exit Sub
  SetSpec scrn, loc, &H80
  oLoc = loc : oScrn = scrn : oDir = 1
  AddTrob
End Sub

' Shake every loose floor on one row of one screen.
Sub ShakeM1(scrn As INTEGER, row As INTEGER)
  Local INTEGER bx, s, r
  s = scrn : r = row
  For bx = COLS - 1 To 0 Step -1
    If RdBlock(s, bx, r) = T_LOOSE Then ShakeIt tScrn, tBY * COLS + tBX
  Next bx
End Sub

Sub ShakeM(row As INTEGER)
  If curLevel = 13 Then Exit Sub
  ShakeM1 visScrn, row
End Sub

' A landing jars the floorboards: which row depends on the sequence's flag.
Sub ShakeLoose
  If jarAbove < 0 Then
    jarAbove = 0 : ShakeM cBlockY
  ElseIf jarAbove > 0 Then
    jarAbove = 0 : ShakeM cBlockY - 1
  End If
End Sub

Sub TrigTorch(scrn As INTEGER, loc As INTEGER)
  oLoc = loc : oScrn = scrn : oDir = 1
  SetSpec scrn, loc, Rnd8() And &HF
  AddTrob
End Sub

Sub TrigSlicer(scrn As INTEGER, loc As INTEGER, newstate As INTEGER)
  Local INTEGER st
  st = BSpec(scrn, loc)
  If st <> 0 And st < kSlicerRet Then Exit Sub             ' mid-slice
  oLoc = loc
  SetSpec scrn, loc, newstate
  oScrn = scrn : oDir = 1
  AddTrob
End Sub

' Start the slicers on the character's row, phased apart.  Runs on arriving at
' a screen and whenever he changes row.
Sub AddSlicers(scrn As INTEGER, row As INTEGER)
  Local INTEGER loc, st, phase, s
  s = scrn
  If row < 0 Or row >= ROWS Then Exit Sub
  phase = kSliceTimer
  For loc = row * COLS To row * COLS + COLS - 1
    If BType(s, loc) = T_SLICER Then
      st = BSpec(s, loc)
      If (st And &H7F) = 0 Or (st And &H7F) >= kSlicerRet Then
        TrigSlicer s, loc, (st And &H80) Or phase
        phase = (phase - kSlicerSync) And &HFF
        If phase < kSlicerRet Then phase = phase + kSliceTimer + 1 - kSlicerRet
      End If
    End If
  Next loc
End Sub

' Arriving at a screen: light its torches, start its slicers.
Sub EnterScreen(scrn As INTEGER, row As INTEGER)
  Local INTEGER loc, s
  s = scrn
  visScrn = s
  For loc = 0 To 29
    If BType(s, loc) = T_TORCH Then TrigTorch s, loc
  Next loc
  AddSlicers s, row
End Sub

'---- ANIMTRANS: one step for everything in transition -------------------------
Sub AnimTrans
  Local INTEGER k, n, clean
  If numTrans = 0 Then Exit Sub
  clean = 0
  For k = numTrans - 1 To 0 Step -1
    AnimObj k
    If oDir < 0 Then clean = 1
    trDir(k) = oDir
  Next k
  If clean = 0 Then Exit Sub
  n = 0
  For k = 0 To numTrans - 1
    If trDir(k) <> -1 Then
      trLoc(n) = trLoc(k) : trScrn(n) = trScrn(k) : trDir(n) = trDir(k)
      n = n + 1
    End If
  Next k
  numTrans = n
End Sub

Sub AnimObj(k As INTEGER)
  Local INTEGER t
  oLoc = trLoc(k) : oScrn = trScrn(k) : oDir = trDir(k)
  If oScrn = 0 Then
    oState = 0 : t = T_BLOCK
  Else
    oState = BSpec(oScrn, oLoc) : t = BType(oScrn, oLoc)
  End If
  Select Case t
    Case T_TORCH  : AnimTorch
    Case T_UPLATE, T_PLATE : AnimPlate
    Case T_SPIKES : AnimSpikes
    Case T_LOOSE  : AnimFloor
    Case T_SPACE  : StopObj                   ' a loose floor that has gone
    Case T_SLICER : AnimSlicer
    Case T_GATE   : AnimGate
    Case T_EXIT   : AnimExit
    Case Else     : StopObj
  End Select
  If oScrn <> 0 Then SetSpec oScrn, oLoc, oState
End Sub

Sub AnimTorch
  If oDir < 0 Then Exit Sub
  If oScrn <> visScrn Then StopObj : Exit Sub
  oState = GetFlameFrame(oState)
End Sub

Function GetFlameFrame(st As INTEGER) As INTEGER
  Local INTEGER r, s
  s = st
  r = Rnd8()
  If r <> s And r < kTorchLast + 1 Then GetFlameFrame = r : Exit Function
  s = s + 1
  If s >= kTorchLast + 1 Then s = 0
  GetFlameFrame = s
End Function

' A plate's state byte is its link index; the timer lives in the link table.
Sub AnimPlate
  Local INTEGER x, t
  If oDir < 0 Then Exit Sub
  x = oState
  t = (GetTimer(x) - 1) And &HFF
  ChgTimer x, t
  If t >= 2 Then Exit Sub
  AddSound 1
  StopObj
End Sub

' Out over five frames, held for a count, back in over four.  The high bit
' marks the hold, with the count in the low seven bits.
Sub AnimSpikes
  Local INTEGER old
  If oDir < 0 Then Exit Sub
  old = oState
  If old And &H80 Then
    oState = (oState - 1) And &HFF
    If oState And &H7F Then Exit Sub
    oState = kSpikeExt + 1
    Exit Sub
  End If
  oState = (oState + 1) And &HFF
  If old = kSpikeExt Then
    oState = kSpikeTimer
  ElseIf old = kSpikeRet Then
    oState = 0
    StopObj
  End If
End Sub

' A loose floor wobbles when jarred (high bit), or counts down to letting go,
' at which point the block becomes empty space and a falling floor is born.
Sub AnimFloor
  If oDir < 0 Then Exit Sub
  oState = (oState + 1) And &HFF
  If oState And &H80 Then
    If curLevel = 13 Then Exit Sub
    If oState < kWiggle + &H80 Then Exit Sub
    oState = 0
    StopObj
    Exit Sub
  End If
  If oState < kLooseTimer Then Exit Sub
  SetType oScrn, oLoc, T_SPACE
  oState = 0
  StopObj
  wLevel = oLoc \ COLS
  wX = (oLoc Mod COLS) * 4
  wY = blocks(gBlockBot + wLevel + 1)
  wScrn = oScrn : wVel = 0 : wType = 0
  AddAMob
End Sub

Sub AnimSlicer
  Local INTEGER hi, n, onscreen
  If oDir < 0 Then Exit Sub
  hi = oState And &H80
  n = (oState And &H7F) + 1
  If n >= kSliceTimer + 1 Then n = 1
  oState = hi Or n
  If n = kSlicerExt Then AddSound 19
  onscreen = 0
  If oScrn = visScrn Then
    If oLoc \ COLS = cBlockY Then onscreen = 1
  End If
  If onscreen Then
    If cLife And &H80 Then Exit Sub
    If hi Then Exit Sub                       ' bloodied: keeps going
  End If
  If n >= kSlicerRet Then StopObj
End Sub

' Direction 0 lowers a step a frame, 1 raises, 2 raises and jams, 3 and up
' drops it with gathering speed.  Above fully open the state is a timer.
Sub AnimGate
  Local INTEGER x, old
  x = oDir
  If x < 0 Then Exit Sub
  If x >= 3 Then
    If x < kMaxGateVel Then x = x + 1 : oDir = x
    old = oState
    oState = (old - blocks(aGateVel + x)) And &HFF
    If old < blocks(aGateVel + x) Then      ' it borrowed: shut
      StopObj
      oState = 0
      AddSound 20
    End If
    Exit Sub
  End If
  If oState = &HFF Then StopObj : AddSound 2 : Exit Sub
  oState = (oState + Sgn8(blocks(aGateInc + x))) And &HFF
  If x = 0 Then
    If oState <= kGMin Then StopObj : AddSound 2 : Exit Sub
    If oState >= kGMax Then Exit Sub          ' the open timer running down
    AddSound 21
    Exit Sub
  End If
  If oState >= kGMax Then
    If x >= 2 Then
      oState = &HFF : StopObj : AddSound 2
      Exit Sub
    End If
    oState = kGateTimer
    oDir = 0
    Exit Sub
  End If
  AddSound 11
End Sub

Sub AnimExit
  If oDir < 0 Then Exit Sub
  If oDir >= 3 Then Exit Sub
  AddSound 11
  oState = (oState + 4) And &HFF
  If oState >= 43 * 4 Then
    StopObj
    AddSound 2
    exitOpen = 1
  End If
End Sub

'---- ANIMMOBS: floors in free fall ------------------------------------------------
Sub AddAMob
  If numMob >= MAXMOB Then Exit Sub
  mobX(numMob) = wX : mobY(numMob) = wY : mobScrn(numMob) = wScrn
  mobVel(numMob) = wVel : mobType(numMob) = wType : mobLevel(numMob) = wLevel
  numMob = numMob + 1
End Sub

Sub SaveMob(k As INTEGER)
  mobX(k) = wX : mobY(k) = wY : mobScrn(k) = wScrn
  mobVel(k) = wVel : mobType(k) = wType : mobLevel(k) = wLevel
End Sub

Sub AnimMobs
  Local INTEGER k, n
  If numMob = 0 Then Exit Sub
  For k = numMob - 1 To 0 Step -1
    wX = mobX(k) : wY = mobY(k) : wScrn = mobScrn(k)
    wVel = mobVel(k) : wType = mobType(k) : wLevel = mobLevel(k)
    wIdx = k
    If wType = 0 Then MobFloor
    If wVel And &H80 Then wVel = (wVel + 1) And &HFF
    CheckCrush
    SaveMob k
  Next k
  n = 0
  For k = 0 To numMob - 1
    If mobVel(k) <> &HFF Then
      mobX(n) = mobX(k) : mobY(n) = mobY(k) : mobScrn(n) = mobScrn(k)
      mobVel(n) = mobVel(k) : mobType(n) = mobType(k) : mobLevel(n) = mobLevel(k)
      n = n + 1
    End If
  Next k
  numMob = n
End Sub

' Falls, passes through empty rows, knocks other loose floors down with it,
' and shatters into rubble on the first solid floor.
Sub MobFloor
  Local INTEGER t
  If wVel And &H80 Then Exit Sub
  If wVel < kFFTermVel Then wVel = wVel + kFFAccel
  wY = (wY + wVel) And &HFF
  If wScrn = 0 Then
    If wY >= 192 + 17 Then wVel = (-kDisappear) And &HFF
    Exit Sub
  End If
  If wY >= 256 - 30 Then Exit Sub
  If wY < blocks(gBlockAy + wLevel + 1) Then Exit Sub
  tScrn = wScrn : tBX = wX >> 2 : tBY = wLevel
  t = RdBlock1()
  If t = T_SPACE Then
    PassThru
  ElseIf t = T_LOOSE Then
    KnockLoose
    PassThru
  Else
    AddSound 7
    ShakeM1 wScrn, wLevel
    wY = blocks(gBlockAy + wLevel + 1)
    wVel = (-kCrumble) And &HFF
    MakeRubble
  End If
End Sub

Sub PassThru
  wLevel = wLevel + 1
  If wLevel < ROWS Then Exit Sub
  wY = (wY - 192) And &HFF
  wLevel = 0
  wScrn = Neighbour(wScrn, 3)
End Sub

Sub KnockLoose
  Local INTEGER s, loc
  s = tScrn : loc = tBY * COLS + tBX
  SetType s, loc, T_SPACE
  SetSpec s, loc, 0
  wVel = wVel >> 1
  SaveMob wIdx
  wY = (wY + 6) And &HFF
  PassThru
  AddAMob
End Sub

Sub MakeRubble
  Local INTEGER t, s, loc
  tScrn = wScrn : tBX = wX >> 2 : tBY = wLevel
  t = RdBlock1()
  s = tScrn : loc = tBY * COLS + tBX
  If s = 0 Then Exit Sub
  If t = T_PLATE Then
    PushPP s, loc
    t = RdBlock1()
    SetType s, loc, T_RUBBLE
  ElseIf t = T_UPLATE Then
    SetType s, loc, T_RUBBLE
    PushPP s, loc
    t = RdBlock1()
    SetType s, loc, T_RUBBLE
  ElseIf t = T_FLOOR Or t = T_SPIKES Or t = T_FLASK Or t = T_TORCH Then
    SetType s, loc, T_RUBBLE
  End If
End Sub

' Is the falling floor about to land on him?
Sub CheckCrush
  If wScrn <> cScrn Then Exit Sub
  If (wX >> 2) <> cBlockX Then Exit Sub
  If wY >= cY Then Exit Sub
  If ((cY - kCrushDist) And &HFF) >= wY Then Exit Sub
  nCrush = nCrush + 1
End Sub

'=============================================================================
Function Sgn8(v As INTEGER) As INTEGER
  If v > 127 Then Sgn8 = v - 256 Else Sgn8 = v
End Function

Sub LoadLevel(n As INTEGER)
  Open home + "levels.dat" For Input As #1
  Seek #1, n * LEVELBYTES + 1
  Memory Input #1, LEVELBYTES, packed()
  Close #1
  Memory Unpack packed(), level(), LEVELBYTES, 8
End Sub

Sub LoadBytes(path As STRING, n As INTEGER, dst() As INTEGER)
  Open path For Input As #1
  Memory Input #1, n, packed()
  Close #1
  Memory Unpack packed(), dst(), n, 8
End Sub

Sub ReadLayout
  Local STRING l, k
  Local INTEGER p, v
  Open home + "tables.idx" For Input As #2
  Do While Not Eof(#2)
    Line Input #2, l
    If l <> "" And Left$(l, 1) <> "#" Then
      p = Instr(l, " ")
      If p > 1 Then
        k = Left$(l, p - 1)
        v = Val(Mid$(l, p + 1))
        Select Case k
          Case "blocks_len"          : blocksLen = v
          Case "geom_BlockAy"        : gBlockAy = v
          Case "geom_BlockBot"       : gBlockBot = v
          Case "anim_gateinc"        : aGateInc = v
          Case "anim_gatevel"        : aGateVel = v
          Case "const_pptimer"       : kPPTimer = v
          Case "const_spiketimer"    : kSpikeTimer = v
          Case "const_slicetimer"    : kSliceTimer = v
          Case "const_gatetimer"     : kGateTimer = v
          Case "const_loosetimer"    : kLooseTimer = v
          Case "const_FFaccel"       : kFFAccel = v
          Case "const_FFtermvel"     : kFFTermVel = v
          Case "const_crumbletime"   : kCrumble = v
          Case "const_disappeartime" : kDisappear = v
          Case "const_CrushDist"     : kCrushDist = v
          Case "const_maxgatevel"    : kMaxGateVel = v
          Case "const_wiggletime"    : kWiggle = v
          Case "const_slicersync"    : kSlicerSync = v
          Case "const_gmaxval"       : kGMax = v
          Case "const_gminval"       : kGMin = v
          Case "const_torchLast"     : kTorchLast = v
          Case "const_spikeExt"      : kSpikeExt = v
          Case "const_spikeRet"      : kSpikeRet = v
          Case "const_slicerExt"     : kSlicerExt = v
          Case "const_slicerRet"     : kSlicerRet = v
        End Select
      End If
    End If
  Loop
  Close #2
End Sub
