' player.bas - Phase 4 put together.
'
' The pieces proved separately - the frame advance, the input layer, the floor
' test, the landing and the ledge-grab gates - driving one character on a real
' screen, at the game's own twelve frames a second.
'
' It runs a scripted set of inputs rather than waiting for a keyboard, so it can
' be checked over a serial console with nobody at the board.  The trajectory it
' prints is the thing to look at: standing still should not drift, running
' should move a block at a time, and walking off an edge should fall and land.
'
' Needs everything the converter writes.

Option Console Serial
Option Explicit
Option Default None
' 736 variable slots are shared between globals and locals.  This engine has
' hundreds of globals and only ever a couple of dozen locals live at once, so
' the balance is moved.  It must come before the first variable is defined,
' and it lasts until the board is reset.
Option Local Variables 128

Const LEVELBYTES = 2304
Const ROWS = 3
Const COLS = 10
Const BLOCKW = 28
Const ORIGINX = 20
Const ORIGINY = 24
' The slot the blitter skips.  The sheets' unset pixels carry it; black inside
' a figure is slot 7, which is drawn.  Both show as black.
Const TRANSP = 11
' A colour the artwork never contains, so a blit named with it copies every
' pixel.  That is the original's STA opacity, used for the front pieces
' drawfrnt writes solid: the posts, and the arch tops from 27 up.
Const OPAQUE = 15
' The interludes.  The original cuts away to the princess's room on the way
' into certain levels and at both endings.  How long one is held, in
' milliseconds; any key cuts it short.
Const CUTMS = 3500
' The room picture is 280 by 192, which is the Apple's whole screen and
' sits inside the display with the same margins the game uses.
Const ROOMW = 280
Const ROOMH = 192
Dim INTEGER cutRoom, cutSlot, nCuts
' The clock.  A game minute is 725 frames, which at the game's twelve frames
' a second is about a minute of real time, and the whole game allows sixty.
Const FRAMESPERMIN = 725
Const GAMEMINUTES = 60
Const LEVELTIMER = 24
Const TIMEMSGTIMER = 20
Const MSG_LEVEL = 1
Const MSG_TIME = 3
Dim INTEGER frameCount, minLeft, secLeft, msgTimer, message, msgLevel
Dim INTEGER nextTimeMsg, timeRequest, outOfTime, nTimeMsgs
' How the game ends.  0 while it is still being played, 1 lost, 2 won.
Dim INTEGER gameOver, overTimer
' The minutes remaining at which the game speaks up, from the original's
' table: every five down to fifteen, then ten, five, and each of the last
' five.  The last entry is time up.
Dim INTEGER timeMsg(17)
Dim STRING msgText
Const SLOT = 4
Const TPW = 12
Const GUARD = 400

Const OP_LOW = &HF1
Const OP_CHX = &HFB
Const OP_CHY = &HFA
Const OP_ABOUTFACE = &HFE
Const OP_GOTO = &HFF
Const OP_UP = &HFD
Const OP_DOWN = &HFC
Const OP_ACT = &HF9
Const OP_SETFALL = &HF8
Const OP_IFWTLESS = &HF7
Const OP_DIE = &HF6
Const OP_JARU = &HF5
Const OP_JARD = &HF4
Const OP_EFFECT = &HF3
Const OP_TAP = &HF2
Const OP_NEXTLEVEL = &HF1

Const ACT_FALLING = 4
Const OOF_VELOCITY = 22
Const DEATH_VELOCITY = 33
' SUBS.S GRAVITY.  A fall gathers speed by three a frame and goes no faster
' than thirty-three; while the float potion is in him it is one a frame and
' four.  The terminal speed is what the landing severity is read off, so
' without the cap a long fall killed where the original only hurt, and
' without the weightless pair the potion changed nothing at all.
Const FALL_ACCEL = 3
Const FALL_TERMVEL = 33
Const WTLESS_ACCEL = 1
Const WTLESS_TERMVEL = 4
Const GRAB_SPEED = 32
Const GRAB_LEAD = 25

Const SEQ_STARTRUN = 1
Const SEQ_STAND = 2
Const SEQ_STANDJUMP = 3
Const SEQ_RUNJUMP = 4
Const SEQ_TURN = 5
Const SEQ_RUNTURN = 6
' SEQDATA.S turnrun = 43, which is not runturn: it is the two frames that
' turn a turn already under way into a run.
Const SEQ_TURNRUN = 43
Const SEQ_DIVEROLL = 26
Const SEQ_FREEFALL = 12
' The four falls a step off an edge can turn into, besides a plain one.  Which
' one is chosen by the pose he was in, and getting this wrong is why a running
' jump dropped straight down instead of carrying across.
Const SEQ_STEPFALL = 7 : Const SEQ_JUMPFALL = 18
Const SEQ_STEPFALL2 = 19 : Const SEQ_RJUMPFALL = 21
Const SEQ_RUNSTOP = 13
Const SEQ_JUMPUP = 14
Const SEQ_SOFTLAND = 17
Const SEQ_MEDLAND = 20
Const SEQ_HARDLAND = 22
Const SEQ_FALLHANG = 15
Const SEQ_CLIMBUP = 10
' The climb down is the climb up run backwards - poses 148 down to 141,
' a row down, then 140, 138, 136 and into the hang.  It has no name in
' the port's list because nothing ever asked for it.
Const SEQ_CLIMBDOWN = 68
Const SEQ_HANGDROP = 11
Const SEQ_HANGFALL = 23
Const SEQ_HANGSTRAIGHT = 25
Const SEQ_CLIMBFAIL = 73
Const GCLIMBTHRES = 6
' CTRL.S fallon: "lda #grabreach / jsr addcharx / sta CharX", and
' grabreach is MINUS eight.  addcharx adds in the direction he faces, so a
' negative reach moves him BACKWARDS - which is right, because the ledge
' you grab on the way down is the one you have just run off, and it is
' behind you.  The port kept the sign in the code rather than the constant
' and then stretched him forwards, putting the whole grab window sixteen
' pixels from where it belongs.
Const GRAB_REACH = -8     ' backwards, the way the ledge is
Const STUN_TIME = 12
Const SEQ_BUMPFALL = 45
Const SEQ_HARDBUMP = 46
Const SEQ_BUMP = 47
Const SEQ_STANDUP = 49
Const SEQ_STOOP = 50
Const SEQ_CRAWL = 79
Const SEQ_RUNNING = 84

' Sized from the converter's own figures, not rounded up: an integer array
' costs eight bytes an element, so the slack was tens of kilobytes and the
' program ran out of global variable memory when the clock was added.
' art.bin 10812, seq.dat 2546, frames.dat 2001, blocks.dat 685, and packed
' only ever holds the largest of those eight bytes to an element.
Dim INTEGER packed(1400), level(LEVELBYTES), blocks(900), snd(84)
Dim INTEGER seqb(2600), frmb(2050), art(11000), tp(4 * TPW)
Dim INTEGER artFirst(20), artCount(20), artFacings(20), artBase, artLen
Dim INTEGER seqTab(200), floory(5)
Dim INTEGER pPieceA,pPieceAY,pPieceB,pPieceBY,pPieceC,pPieceD,pFrontI,pFrontY,pFrontX
Dim INTEGER pStripe
Dim INTEGER gBlockBot, gFloorY, blockTypes, seqLen, seqCount, frmLen, frmCount, frmEntry
Dim INTEGER blocksLen
Dim INTEGER T_BG1, T_BG2, T_CH(8), noFloor(31), vertDist
Dim INTEGER sLeft, sRight, sUp, sDown, cFalling, composedScrn, stunned, jarAbove
Dim INTEGER nStandup, nCrawl, nRunJump, nRoll, nRunTurn, nDead
Dim INTEGER nClimb, nClimbFail, nDrop, nHangFall, nHangStr, nClimbDown
Dim INTEGER nGates, nGrabs, nBumps, nSoft, nMed, nHard, nStepOff, nCross, nStoop
Dim INTEGER poseSeen(256), scenFrames, loadedLevel
Dim INTEGER palette(15)
Const OFF_MAP = 1952
Const SCRNW = 140                ' ten blocks of fourteen units
' CTRLSUBS.S GETBASEBLOCK is "getbasex" then "getblockxp", and GETBLOCKXP is
'     sec / sbc #angle / tay / lda BlockTable,y
' so a character's block is BlockTable(base - angle) with angle = 7.
' BlockTable itself is "screen x -> block number" over 14-wide blocks starting
' at 2, which is the port's (x - 2) \ 14 - 4; what the port left out is the
' angle.  It compensated by putting the kid down at BlockEdge + 7 instead of
' the original's BlockEdge + angle + 7, so his BLOCK came out right while his
' x was seven pixels light - and InitGuards, BonesRise, TryStairs, DoImpale
' and SmashMirror all use the original's absolute constants, so the distance
' between the kid and anyone else was wrong by those seven pixels.
Const ANGLE = 7
Const BLOCKLO = 2 + ANGLE        ' a block's front edge, less 14 * (b + 4)
Const SECMAX = 5 * 32
' The middle section depends on the neighbour's TYPE *and* its STATE byte, so
' unlike the others it cannot be precomputed per type alone.  States are small,
' so it is precomputed per (type, state) pair - still a flat table the draw loop
' only indexes.  Resolving this inside the loop cost 2,298 us a section when it
' was tried, which is the whole lesson of the renderer.
Const BSTATES = 8
Const BMAX = 30 * BSTATES
Dim INTEGER bOn(BMAX), bSheet(BMAX), bSX(BMAX), bSY(BMAX)
Dim INTEGER bW(BMAX), bH(BMAX), bDX(BMAX), bDY(BMAX)
Dim INTEGER ts(4 * TPW)
Dim INTEGER vPanelB, vSpaceB, vSpaceBY, vFloorB, vFloorBY, vBlockB
Dim INTEGER nPans, nBlox, nBpans, panelB0, bgPalace
Dim INTEGER secOn(SECMAX), secSheet(SECMAX), secSX(SECMAX), secSY(SECMAX)
Dim INTEGER secW(SECMAX), secH(SECMAX), secDX(SECMAX), secDY(SECMAX)

' The character
Dim INTEGER cPosn, cX, cY, cFace, cBlockX, cBlockY, cAction, cXVel, cYVel
Dim INTEGER cSeq, cScrn, cLife
' Input
Dim INTEGER jstkX, jstkY, btn, clrF, clrB, clrU, clrD, clrBtn
Dim INTEGER pF, pB, pU, pD, pBtn
Dim INTEGER weightless, i, f, frame, nSheets, realArt
Dim STRING home, note, lastWhat
Dim FLOAT t0, tFrame, tWork, tStart


' ---- the moving parts (see movertest.bas, which proves this code exact) ----
Const OFF_SPEC = 720
Const OFF_LINKLOC = 1440
Const OFF_LINKMAP = 1696
Const OFF_INFO = 2048
Const MAXTR = 31
Const MAXMOB = 15
Const T_SPACE = 0 : Const T_FLOOR = 1 : Const T_SPIKES = 2 : Const T_GATE = 4
Const T_DPLATE = 5 : Const T_PLATE = 6 : Const T_FLASK = 10 : Const T_LOOSE = 11
Const T_RUBBLE = 14 : Const T_UPLATE = 15 : Const T_EXIT = 16 : Const T_SLICER = 18
Const T_TORCH = 19 : Const T_BLOCK = 20
Const SEQ_IMPALE = 51
' SEQDATA.S crush = 52, turndraw = 89.
Const SEQ_CRUSH = 52 : Const SEQ_TURNDRAW = 89
Dim INTEGER gBlockAy, gBlockEdge, aGateInc, aGateVel
Dim INTEGER kPPTimer, kSpikeTimer, kSliceTimer, kGateTimer, kLooseTimer
Dim INTEGER kFFAccel, kFFTermVel, kCrumble, kDisappear, kCrushDist, kMaxGateVel
Dim INTEGER kWiggle, kSlicerSync, kGMax, kGMin, kTorchLast, kSpikeExt, kSpikeRet
Dim INTEGER kSlicerExt, kSlicerRet
Dim INTEGER trLoc(MAXTR), trScrn(MAXTR), trDir(MAXTR), numTrans
Dim INTEGER mobX(MAXMOB), mobY(MAXMOB), mobScrn(MAXMOB), mobVel(MAXMOB)
Dim INTEGER mobType(MAXMOB), mobLevel(MAXMOB), numMob
Dim INTEGER oLoc, oScrn, oDir, oState, ppType, linkIndex
Dim INTEGER wX, wY, wScrn, wVel, wType, wLevel, wIdx
Dim INTEGER tScrn, tBX, tBY
Dim INTEGER rndSeed, visScrn, curLevel, nSounds, alertGuard, exitOpen, nCrush
Dim INTEGER nPlates, nLooseTrig, nSpikesTrig, nImpaled
' Where the animation tables sit in blocks.dat, and the image record found.
Dim INTEGER aSpikeA, aSpikeB, aLooseA, aLooseBY, aLooseD, aGate8C, aGate8B
Dim INTEGER aSlicerSeq, aSlicerTop, aSlicerBot, aSlicerBot2, aSlicerGap, aSlicerFrnt
Dim INTEGER aTorchFlame, kFfalling, kLooseB, kGateBotORA, kGateB1
Dim INTEGER imSheet, imSX, imSY, imW, imH, kGateMargin, kStairThres
Dim INTEGER levelDone, startScrn, nStairs
' AUTO.S milestone3: set once he has been past the first gate on level 3.
Dim INTEGER milestone
' The strength meter, and what a potion did last.
Dim INTEGER kidStr, maxKidStr, chgKidStr, origStrength, lastPotion, gotSword
Dim INTEGER deadTimer, nPotions, kInitMaxStr, kMaxMaxStr, kWtlessTimer, kDeadEnough
Const T_SWORD = 22
' How far open the exit's stair animation runs: 43 frames of four.
Const EXITOPENVAL = 43 * 4
Const SEQ_DRINK = 78
Const SEQ_PICKUPSWORD = 91
Const SEQ_CLIMBSTAIRS = 70
Const SEQ_JUMPHANGMED = 8
Const SEQ_JUMPHANGLONG = 24
Const SEQ_JUMPBACKHANG = 16
Const SEQ_HIGHJUMP = 28
Const JUMPBACK_THRES = 6
' Down at the front edge of the block steps off it; down well back in the
' block, which is to say close to the edge behind him, climbs down it.
Const STEPDOWN_THRES = 3 : Const CLIMBDOWN_THRES = 8
' The low five bits of a frame's check byte: how far in from the left edge
' of the image his base X sits.  Ffootmark in the original.
Const FFOOTMARK = &H1F
Dim INTEGER nJumpHang, pstep, pTimer, playDone
' Two characters.  The engine works on the c* set; a record holds the other.
' Record: posn x y face bx by action xvel yvel seq scrn id sword life falling stunned
Dim INTEGER kRec(16), gRec(16), cID, cSword
Dim INTEGER oPosn, oX, oY, oFace, oBlockX, oBlockY, oAction, oSword, oLife, opScrn
Dim INTEGER gdPresent, gdTimer, refract, justBlocked, guardProg, guardColor
Dim INTEGER oppStr, maxOppStr, chgOppStr, enemyAlert, droppedOut, offGuard, heroic
Dim INTEGER kJstkX, kJstkY, kBtn, kClrF, kClrB, kClrU, kClrD, kClrBtn
' The guards of the current level, one slot a screen, updated as he leaves.
Dim INTEGER gdBlock(24), gdX(24), gdFace(24), gdProg(24), gdSeq(24)
Dim INTEGER aStrikeProb, aRestrikeProb, aBlockProb, aImpBlockProb, aAdvProb
Dim INTEGER aRefracTimer, aSpecialColor, aExtraStrength, aBasicStrength, aBasicColor
Dim INTEGER aBgSet, aChSet
' The shadow: where he stands on each of his levels, and the thief's
' recorded moves.  He is a third kind of character, neither the player nor
' a guard, and what he does depends entirely on which level he is on.
Dim INTEGER aShad6a, aShad5, aShad12, aShadProg5
Dim INTEGER kFlaskScrn, kFlaskX, kFlaskY, kSwordScrn, kSwordX, kSwordY, kShadStr
' The pickups.  The original draws these three from code rather than from the
' block tables, which is why a sword lying on the floor was invisible here: the
' table lookup for its block type is empty and the port drew plain floor.
Dim INTEGER kSpecialFlask, kSwordGleam0, kSwordGleam1, nSwordsDrawn
Dim INTEGER playCount, preRecPtr, shadowAction, nShadows
Dim INTEGER kMirScrn, kMirX, kMirY, nMirrors, nMerges
' TOPCTRL.S mergetimer.  Zero before the two become one on the twelfth
' level and negative afterwards, which is how the rest of the level knows
' the meeting is over: the shadow is not put back in the room, and the
' bridge to the last screen is there to be walked on.
Dim INTEGER mergeTimer, nBridge, shadHold, mouseTimer, nMice, mouseTurned, nGuardsGone
Const T_MIRROR = 13
' The enemy image set each level loads (MISC.S chset), and the tables.
Dim INTEGER tSet(6), chSet(15), bgSet(15)
' Both background sets, and which is in use.  The release holds two sets of
' tiles, the dungeon and the palace.  The level table names three: dungeon,
' palace, and a second palace variant the drawing code tells apart only by
' its striped back wall, so that one reuses the palace tiles without it.
Dim INTEGER bg1Dun, bg2Dun, bg1Pal, bg2Pal, usingBg
' The twenty effects, each a frequency and a length, from the data the
' converter writes.  Sound switches itself off if the board has no audio
' configured, so the game still runs on one that has none.
Dim INTEGER sndF(19), sndD(19), soundOn
Dim INTEGER frmSword, nStrikes, nBlocks, nStabs, nGuardsDead, nEngarde, cutDir, nTransfers
Const SEQ_ENGARDE = 55 : Const SEQ_ADVANCE = 56 : Const SEQ_RETREAT = 57
' The mouse.  Sequence 105 is the run - two poses and twelve pixels a
' cycle - and 107 stops him, sits him up, turns him round and drops back
' into the run.  His poses are 186 to 188, images 68 to 70 of CHTAB2,
' twenty-one by four and fourteen by ten: he is the only character in the
' game who is wider than he is tall.
Const SEQ_MOUSERUN = 105 : Const SEQ_MOUSETURN = 107
Const MOUSE_ID = 24
' MISC.S MOUSERESCUE: in at x 200, round again at 166 - which is the
' up-plate's own block - and gone when he is back where he came from.
Const MOUSE_START = 200 : Const MOUSE_TURN = 166
' TOPCTRL.S misctimers counts this out before sending him.
Const MOUSE_WAIT = 150
Const SEQ_STRIKE = 58 : Const SEQ_TURNENGARDE = 60 : Const SEQ_STRIKEBLOCK = 61
Const SEQ_READYBLOCK = 62 : Const SEQ_LANDENGARDE = 63 : Const SEQ_BLOCKTOSTRIKE = 66
Const SEQ_BLOCKEDSTRIKE = 69 : Const SEQ_DROPDEAD = 71 : Const SEQ_STABBED = 74
Const SEQ_FASTSTRIKE = 75 : Const SEQ_ALERTSTAND = 77 : Const SEQ_ALERTTURN = 80
Const SEQ_FIGHTFALL = 81 : Const SEQ_STABKILL = 85 : Const SEQ_FASTADVANCE = 86
Const SEQ_GOALERTSTAND = 87 : Const SEQ_GUARDENGARDE = 90 : Const SEQ_RESHEATHE = 92
Const SEQ_FASTSHEATHE = 93
' The careful step.  step1 to step13 move that many units, fullstep a whole
' block, testfoot pokes a foot out without leaving the edge.
Const SEQ_STEP1 = 29 : Const SEQ_FULLSTEP = 42 : Const SEQ_TESTFOOT = 44
' The running jump's aiming constants, from CTRL.S.  These live up here with
' the rest: a Const only exists once the line has run, and everything below the
' main body never does.
Dim STRING traceLine, lastTrace
Const RJCHANGE = 4        ' how far he moves in the frame being projected
Const RJLOOKAHEAD = 1     ' blocks to look ahead for an edge
Const RJLEADDIST = 14     ' the run-up the jump itself needs
Const RJMAXFUJBAK = 8     ' pixels it will shift him back to make it work
Const RJMAXFUJFWD = 2     ' and forward
Dim INTEGER cRepeat, nSteps, invert, nInverts, nGateKnocks
' What GetFwdDist found in the way: 0 the edge of his own block, 1 a
' barrier, 2 clear - and the block type it looked at.
Dim INTEGER fwdKind, fwdType
' COLL.S CHECKGATE wants the overlap to have held for two frames running
' before it shoves.  Only the player is ever asked, so one flag does.
Dim INTEGER gateLast
' The sixteen tunes the original asks for by name.  The player supplies the
' files; a cue whose file is missing simply does not play, which is the
' normal case and the one that is tested.  The audio output does one thing
' at a time, so an effect is dropped while a tune is running rather than
' cutting the tune short.
Dim STRING musName(16) LENGTH 12
Dim INTEGER nCues
Const SEQ_BUMPENGFWD = 64 : Const SEQ_BUMPENGBACK = 65
Const SWORDTHRES = 90 : Const SWORDTHRESN = 246 : Const BLOCKTHRES = 32
Const GRACEPERIOD = 9 : Const GDPATIENCE = 15 : Const BLOCKTIME = 4
Const STRIKERANGE1 = 12 : Const STRIKERANGE2 = 29 : Const BLOCKRANGE2 = 29
Const BLOCKTHRES1 = 10 : Const TOOCLOSE = 12 : Const TOOFAR = 35
Const OFFGUARDTHRES = 8 : Const JUMPTHRES = 50 : Const RUNTHRES = 40 : Const ESTWIDTH = 13
Const NOGUARD = 86
Const SEQ_ARISE = 88
Const SKELPROG = 2
Dim INTEGER nBones

' chset from MISC.S: 0 guard, 1 skeleton, 2 palace guard, 3 fat guard, 4 (level 12), 5 vizier

For i = 0 To 9 : timeMsg(i) = 60 - i * 5 : Next i
timeMsg(10) = 10 : timeMsg(11) = 5 : timeMsg(12) = 4 : timeMsg(13) = 3
timeMsg(14) = 2 : timeMsg(15) = 1 : timeMsg(16) = 0 : timeMsg(17) = -1

musName(1) = "accid"    : musName(2) = "heroic"   : musName(3) = "danger"
musName(4) = "sword"    : musName(5) = "rejoin"   : musName(6) = "shadow"
musName(7) = "victory"  : musName(8) = "stairs"   : musName(9) = "upstairs"
musName(10) = "jaffar"  : musName(11) = "potion"  : musName(12) = "shortpot"
musName(13) = "timer"   : musName(14) = "tragic"  : musName(15) = "embrace"
musName(16) = "heartbeat"

home = MM.Info(Path)
If home = "NONE" Then home = "A:/"

Print "--- Prince of Pico"
ReadLayout
LoadBytes home + "blocks.dat", blocksLen, blocks()
LoadBytes home + "seq.dat", seqLen, seqb()
LoadBytes home + "frames.dat", frmLen, frmb()
LoadArt
LoadSounds
For i = 0 To 14 : bgSet(i) = blocks(aBgSet + i) : chSet(i) = blocks(aChSet + i) : Next i
LoadLevel 1
For i = 0 To seqCount - 1 : seqTab(i+1) = seqb(i*2) Or (seqb(i*2+1) << 8) : Next i
For i = 0 To 4 : floory(i) = blocks(gFloorY + i) : Next i
For i = 0 To 31 : noFloor(i) = 0 : Next i
noFloor(0)=1 : noFloor(9)=1 : noFloor(12)=1 : noFloor(20)=1
noFloor(26)=1 : noFloor(27)=1 : noFloor(28)=1 : noFloor(29)=1

Mode 2
' MODE 2's sixteen entries are programmable, so the walls, the player and his
' opponents can be told apart even though the source artwork is one bit a pixel
' and knows nothing about colour.  MAP is only valid in MODE 2 and 3, and only
' after the mode is set.
ApplyPalette

' The static background lives in F, everything that moves is drawn fresh into
' framebuffer 2 over a transparent colour, and one MERGE composites the two
' into the display on the blanking.  Nothing is ever drawn on a live buffer,
' which is what showed as artefacts with the layer.
FrameBuffer Create
FrameBuffer Create 2
LoadSheets
BuildSections

' Start where the level says the player starts: the INFO block's screen and
' block number, and which way he faces.  On level 1 that is block 0 of screen 1
' with nothing under it - he drops in from the ceiling, as the game opens.
cScrn = level(OFF_INFO + 64)
cBlockX = level(OFF_INFO + 65) Mod COLS : cBlockY = level(OFF_INFO + 65) \ COLS
cX = 58 + cBlockX * 14 + 7 + ANGLE : cY = floory(cBlockY + 1)
' SUBS.S STARTKID: "lda KidStartFace / eor #$ff / sta CharFace".  The
' blueprint's facing byte is INVERTED before it is used, so level 1 starts
' him facing right, not left.
cFace = level(OFF_INFO + 66) Xor &HFF : cAction = 0 : cXVel = 0 : cYVel = 0 : cLife = &HFF : cFalling = 0
' For now the demo starts on screen 5 instead, the first room with plates and
' gates: a torch, an up-plate that raises the gate beside it and the one at the
' far end, a second up-plate, and a hole onto rubble.
' Then the exit room: he starts on the up-plate that opens the door, drops to
' the row below, runs to the door and climbs the stairs into level 2.
' (The walkthrough starts where the level does.)
composedScrn = -1
cPosn = 15
cSeq = seqTab(SEQ_STAND)
GetScreens cScrn
BuildTypeGrid cScrn
cID = 0 : cSword = 0
SaveChar kRec()
EnterScreen cScrn, cBlockY
ComposeBackground

Print "start  screen "; Str$(cScrn); " block "; Str$(cBlockX); ","; Str$(cBlockY);
Print "  x "; Str$(cX); " y "; Str$(cY)
Print
Print "frame  input     posn   x    y  blk  act yvel   what"

' Paced at the game's own twelve frames a second so it can be watched.  Only
' the work is timed; the wait that fills out the frame is not.
Const FRAMEMS = 83
Const PACED = 1
' The coverage pass runs with the display closed; palette changes wait.
Dim INTEGER HEADLESS
' 0 none, 1 the level one walkthrough, 2 a sword fight, 3 the skeleton,
' 4 play it yourself from the keyboard.
Const DEMO = 4
Dim INTEGER maxFrames
' DEMO 1 is the level walkthrough; DEMO 2 a sword fight on level 1 screen 3.
Dim STRING fightScript
fightScript = "..<<<<....>>>>>>>>>>>>>>........B...B...^..B...B..^..B...>..B...B...^..B...B...>..B...B..B...^..B...B...B...B...B...B...B...B...B...B.."
Const TRACE_PLAY = 0
' Frame to save a still on, or 0 for a clean run.
Const SNAPSHOT = 0
' Begin play somewhere other than the start of level one, so a later level can
' be looked at without playing up to it.  Level 0 for a real game.
Const BEGINLEVEL = 0
Const BEGINSCRN = 1
Const BEGINBX = 0
Const BEGINBY = 0
' A scenario file beside the program turns the engine into a test rig and
' nothing is played: see RunScenarios at the foot of this file.  Everything
' the scenarios need is built by now, and with no such file this costs one
' directory lookup.
If Dir$(home + "scen.txt", FILE) <> "" Then RunScenarios : End

tWork = 0
If DEMO <> 0 Then ShowTitle
pstep = 0 : pTimer = 0 : playDone = 0
If DEMO = 0 Then playDone = 1
If DEMO = 2 Then gotSword = 1 : ResetCharAt 1, 3, 2, 1 : ComposeBackground
' DEMO 3: level 3's skeleton.
If DEMO = 3 Then gotSword = 1 : ResetCharAt 3, 1, 1, 1 : exitOpen = 1 : ComposeBackground
If DEMO = 3 Then fightScript = "..<<<<....>>>>>>>>........B...B...^..B...B..^..B...B...B...B..B...B...B...B...B..."
' The walkthrough meets the guard on screen 3, so it carries the sword.
If DEMO = 1 Then gotSword = 1
If BEGINLEVEL <> 0 Then
  ResetCharAt BEGINLEVEL, BEGINSCRN, BEGINBX, BEGINBY
  EnterScreen cScrn, cBlockY
  ComposeBackground
End If
If DEMO = 4 Then maxFrames = 60000 Else maxFrames = 900
For frame = 1 To maxFrames
  tStart = Timer
  ' A scripted set of inputs: stand, then run forward, then keep going off the
  ' end of whatever he is standing on.
  ' Stand, then run, then hold the button as well from the point he reaches the
  ' edge - which is how you ask to catch a ledge on the way down.
  ' Stand, turn, then hold right: onto the up-plate, which sets two gates
  ' rising; into the near gate until it is high enough to pass under; over
  ' the second plate; and off the end into the hole.
  ' Stand, then hold right along the top row: onto the up-plate, through the
  ' gate once it has risen, over the second plate and into the hole.  On the
  ' bottom row, back left to the flask and drink it.
  ' An ended game holds its message, then stops.
  If gameOver <> 0 Then
    overTimer = overTimer + 1
    SetInput 0,0,0,0,0
    If overTimer > 36 Then playDone = 1
  ElseIf DEMO = 4 Then
    ReadKeys
  ElseIf DEMO >= 2 Then
    If frame > Len(fightScript) Then Exit For
    ApplyCode Mid$(fightScript, frame, 1)
  Else
    PlayCtrl
  End If
  If playDone Then Exit For

  GameFrame
  If levelDone Then NextLevel
  ' Dead long enough: the level starts again, as it does after the message.
  If cLife = 0 Then
    deadTimer = deadTimer + 1
    If deadTimer > 24 Then StartLevel curLevel : Print "  restart"
  End If

  ' One line only when something changes.  At twelve frames a second an
  ' unfiltered trace is unreadable and the interesting frame scrolls away.
  If TRACE_PLAY Then
    traceLine = "pose" + Pad$(Str$(cPosn),4) + " x" + Pad$(Str$(cX),4)
    traceLine = traceLine + " y" + Pad$(Str$(cY),4)
    traceLine = traceLine + " blk" + Pad$(Str$(cBlockX),3) + "," + Str$(cBlockY)
    traceLine = traceLine + Choice((cFace And &H80) <> 0, " <", " >")
    traceLine = traceLine + " up" + Str$(jstkY) + " fresh" + Pad$(Str$(clrU),3)
    traceLine = traceLine + " act" + Str$(cAction)
    traceLine = traceLine + "  " + lastWhat
    If traceLine <> lastTrace Then
      Print Pad$(Str$(frame),4) + " " + traceLine
      lastTrace = traceLine
    End If
  End If

  ' Clear first.  The background does not cover every pixel - fifty-seven of
  ' the hundred and fifty possible sections are empty - so without this the
  ' previous frame's character survives wherever nothing paints over him, and
  ' he smears across the screen.
  CLS
  ' Everything that moves goes in the layer, which the hardware composites over
  ' the display line by line.  That is what a layer is for.  I had it the other
  ' way round - background in the layer - so the background was drawn ON TOP of
  ' the character every frame, which is what made him flash.
  ' Restore the background, add what moves on top, then present the finished
  ' frame in one copy on the blanking.  The restore copy is also the clear.
  ComposeBackground
  FrameBuffer Write 2
  CLS Map(TRANSP)
  DrawMovers
  DrawReflection
  DrawChar
  If gdPresent Then
    SaveChar kRec() : LoadShadWOp : DrawChar : LoadKidWOp
  End If
  DrawFront cScrn
  DrawMeter
  DrawMessage
  DrawScrnNo cScrn
  DrawFrameNo frame
  If invert Then FlipBuffer
  tWork = tWork + (Timer - tStart)
  ' Three stills for the record: the gate part-way up with him against it,
  ' him through it, and the end.
  If SNAPSHOT <> 0 And frame = SNAPSHOT Then
    FrameBuffer Merge TRANSP, B
    FrameBuffer Write N
    Save Image home + "shot" + Str$(frame) + ".bmp"
    FrameBuffer Write 2
  Else
    FrameBuffer Merge TRANSP, B
  End If
  If PACED Then Do While Timer - tStart < FRAMEMS : Loop
Next frame
tFrame = tWork / frame

' Both endings happen in the princess's room, so they are shown there.
If gameOver = 2 Then ShowRoom "YOU WIN"
If gameOver = 1 Then ShowRoom "TIME UP"

' Hold the last frame so there is something to look at.
Pause 4000

Print
Print "ledge reach: gates passed "; Str$(nGates); ", caught "; Str$(nGrabs)
Print "per frame "; Str$(tFrame,0,2); " ms of the 83 ms the game allows"
If tFrame < 83 Then Print "within budget" Else Print "OVER BUDGET"


' ---- coverage: does anything actually reach the code we have written? -------
'
' The visual demo drives one input, so most of the engine never runs.  These
' scenarios are played out with no drawing and no pacing, purely to find out
' which paths execute at all.  A path that never executes is not working code,
' it is untested code that happens to compile.
'
' One character per frame: . nothing  > forward  < back  ^ up  v down
'                          F forward+button  B button  U up+button
Dim STRING scen(21), scenName(21)
Dim INTEGER si, sf, np, totalPoses
Dim INTEGER scenL(21), scenS(21), scenX(21), scenY(21)
For si = 1 To 21 : scenL(si) = 1 : scenS(si) = 1 : scenX(si) = 7 : scenY(si) = 0 : Next si
' Positions are (level, screen, block x, block y) in the ORIGINAL numbering,
' chosen by searching the blueprints for the geometry each scenario needs.  He
' starts facing left, so "forward" is towards lower block numbers.
'
' Level 1 screen 1, top row: floor from block 3 to 7, a gap at 2 with floor one
' row down, then torches and the wall of the screen to the left.
scen(1)    = "....>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
scenName(1)= "run off the edge"
scenX(1) = 5 : scenY(1) = 0
scen(2)    = "....vvvvvvvv........vvvvDDDD...."
scenName(2)= "crouch and stand"
' Level 1 screen 8, middle row: two floor blocks, a gap, then a loose floor
' beyond it whose edge can be caught; below the gap it is open for four rows.
scen(3)    = "....>>>>>>>>>>>>FFFFFFFFFFFFFFFFFFFF"
scenName(3)= "run then reach for a ledge"
scenS(3) = 8 : scenX(3) = 7 : scenY(3) = 1
scen(4)    = "....^^^^^^^^........^^^^...."
scenName(4)= "jump"
scen(5)    = "....<<<<....>>>>>>>>>>>>>>>>...."
scenName(5)= "turn, then run the other way"
scen(6)    = "....BBBBUUUU....BBBB...."
scenName(6)= "button, then up with button"
scen(7)    = "..>>>FFFFFFFFFFFFFFFFFFUUUUUUUUUUUU.........................."
scenName(7)= "reach for a real ledge"
scenS(7) = 8 : scenX(7) = 7 : scenY(7) = 1
' The same gap without the button: four rows straight down into screen 11.
scen(8)    = "..>>>>>>>>>>>>>>........................"
scenName(8)= "long drop through a screen"
scenS(8) = 8 : scenX(8) = 7 : scenY(8) = 1
' Level 1 screen 1, middle row: two torches to the left, then the solid wall
' of the neighbouring screen.
scen(9)    = "..>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
scenName(9)= "run into a wall"
scenX(9) = 2 : scenY(9) = 1
' Level 1 screen 7, top row: floor, an up-plate, then a one-row drop.
scen(10)    = "..>>>>>>>>>>>>>>>>>>>>>>"
scenName(10)= "short drop, soft landing"
scenS(10) = 7 : scenX(10) = 4 : scenY(10) = 0
' Level 1 screen 2, middle row: floor the whole width for a long run.
scen(11)    = "..>>>>>>>>JJ>>>>>>>>>><<<<>>>>>>>>>RR>>>>>>>>"
scenName(11)= "running jump, roll, turn"
scenS(11) = 2 : scenX(11) = 9 : scenY(11) = 1
scen(12)    = "..>>>FFFFFFFFFFFFFFFFFF........"
scenName(12)= "catch a ledge and let go"
scenS(12) = 8 : scenX(12) = 7 : scenY(12) = 1
' Level 1 screen 5, top row: turn, press the up-plate, wait at the gate until
' it has risen enough, pass it, press the second plate, and fall into the hole.
scen(13)    = "..<<<<....>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
scenName(13)= "plate, gate, plate, hole"
scenS(13) = 5 : scenX(13) = 3 : scenY(13) = 0
' Level 1 screen 3: a guard on the middle row.  Unarmed, he runs at him.
scen(14)    = "..<<<<....>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
scenName(14)= "unarmed, into the guard"
scenS(14) = 3 : scenX(14) = 2 : scenY(14) = 1
' The same, with the sword: he draws when the guard is near, then presses
' the attack, blocks, and advances.
scen(15)    = "..<<<<....>>>>>>>>>>>>>>........B...B...^..B...B..^..B...>..B...B...^..B...B...>..B...B..B...^..B...B...B...B...B...B...B...B...B...B.."
scenName(15)= "armed, the fight"
scenS(15) = 3 : scenX(15) = 2 : scenY(15) = 1
' Level 3 screen 1: the bones by the torches rise when he comes near.
scen(16)    = "..<<<<....>>>>>>>>........B...B...^..B...B..^..B...B...B...B.."
scenName(16)= "the skeleton rises"
scenL(16) = 3 : scenS(16) = 1 : scenX(16) = 1 : scenY(16) = 1
' Level 1 screen 8, middle row: floor at block 5 with a gap at 4.  Forward
' and the button, released between presses so each one counts as fresh,
' walks him to the edge, tests it with a foot, then steps off.
scen(17)    = "..F.F.F.F.F.F.F.F.F.F.F.F.F.F.F.F."
scenName(17)= "careful steps to the edge"
scenS(17) = 8 : scenX(17) = 5 : scenY(17) = 1
' Level 6 screen 1: the shadow is standing there when the room is entered,
' and takes the leap at the same moment the player does.
scen(18)    = "..<<<<....>>>>>>>>>>>>JJ>>>>>>>>>>>>>>>>"
scenName(18)= "the shadow on the sixth level"
scenL(18) = 6 : scenS(18) = 1 : scenX(18) = 8 : scenY(18) = 1
' Level 4 screen 4: with the exit open the mirror stands on the top row.
' It takes a running jump to go through, not a run, so the script leaps
' repeatedly on the way in.
scen(19)    = "..>>>>>>J>>>J>>>J>>>J>>>J>>>J>>>J>>>>"
scenName(19)= "through the mirror"
scenL(19) = 4 : scenS(19) = 4 : scenX(19) = 8 : scenY(19) = 0
' Level 12, the screen the sword was on: with no sword drawn, walking into
' the shadow ends it.  Nothing is pressed but forward.
scen(20)    = "..>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
scenName(20)= "the meeting on the twelfth level"
scenL(20) = 12 : scenS(20) = 15 : scenX(20) = 6 : scenY(20) = 0
' Level 13: the vizier. Armed, and the fight decides whether the way opens.
scen(21)    = "..B...B...^..B...B..^..B...B...B...B...B...B...B...B...B...B...B...B..."
scenName(21)= "the vizier"
scenL(21) = 13 : scenS(21) = 1 : scenX(21) = 3 : scenY(21) = 0

Print
Print "--- coverage pass, no drawing"
HEADLESS = 1
Const TRACE_SCEN = 0
Dim INTEGER g0, r0, f0, l0
For si = 1 To 21
  gotSword = 0 : If si >= 15 Then gotSword = 1
  ResetCharAt scenL(si), scenS(si), scenX(si), scenY(si)
  If si = 16 Then exitOpen = 1
  If si = 19 Then exitOpen = 1 : MirAppear : composedScrn = -1
  If si = 20 Then
    ' He is only there once the sword has gone from its block.
    SetType kSwordScrn, kSwordY * COLS + kSwordX, T_FLOOR
    SaveChar kRec()
    AddGuard kSwordScrn
    LoadKidWOp
  End If
  If si = 21 Then gotSword = 1
  g0 = nGrabs : r0 = nGates : f0 = nStepOff : l0 = nSoft + nMed + nHard
  For sf = 1 To Len(scen(si))
    ApplyCode Mid$(scen(si), sf, 1)
    GameFrame
    ' Frame by frame for one scenario.  Sampling only at the interesting moment
    ' hid a contradiction - not moving, yet falling - for six attempts.
    If si = TRACE_SCEN Then
      Print "      f"; Str$(sf); " in="; Mid$(scen(si), sf, 1);
      Print " posn="; Str$(cPosn); " x="; Str$(cX); " y="; Str$(cY);
      Print " blk="; Str$(cBlockX); ","; Str$(cBlockY); " scr="; Str$(cScrn);
      Print " act="; Str$(cAction); " fall="; Str$(cFalling);
      Print " yv="; Str$(cYVel); " seq="; Str$(cSeq); " "; lastWhat;
      Print " | gd "; Str$(gdPresent); " p"; Str$(gRec(0)); " x"; Str$(gRec(1)); " sw"; Str$(gRec(12)); " al"; Str$(enemyAlert); " str"; Str$(oppStr)
    End If
  Next sf
  Print "  "; scenName(si);
  Print "  [fell "; Str$(nStepOff - f0); ", reached "; Str$(nGates - r0);
  Print ", caught "; Str$(nGrabs - g0); ", landed "; Str$(nSoft + nMed + nHard - l0);
  Print ", ended at "; Str$(cBlockX); ","; Str$(cBlockY); " scr "; Str$(cScrn); "]"
Next si
LoadLevel 1 : loadedLevel = 1

totalPoses = 0
For si = 0 To 255
  If poseSeen(si) Then totalPoses = totalPoses + 1
Next si

Print
Print "reached:"
Print "  distinct poses   "; Str$(totalPoses); " of "; Str$(frmCount)
Print "  bumps            "; Str$(nBumps)
Print "  step-offs        "; Str$(nStepOff)
Print "  landings         soft "; Str$(nSoft); "  medium "; Str$(nMed); "  hard "; Str$(nHard)
Print "  screen changes   "; Str$(nCross)
Print "  deaths           "; Str$(nDead)
Print "  hanging          climbs "; Str$(nClimb); "  fails "; Str$(nClimbFail); "  drops "; Str$(nDrop); "  falls "; Str$(nHangFall); "  straight "; Str$(nHangStr)
Print "  running          jumps "; Str$(nRunJump); "  rolls "; Str$(nRoll); "  turns "; Str$(nRunTurn)
Print "  stoops           "; Str$(nStoop); "  stand-ups "; Str$(nStandup); "  crawls "; Str$(nCrawl)
Print "  ledge reaches    "; Str$(nGates); ", caught "; Str$(nGrabs)
Print "  careful steps    "; Str$(nSteps); "  gate shoves "; Str$(nGateKnocks); "  inversions "; Str$(nInverts); "  music cues "; Str$(nCues)
' Neither ending is reachable by walking, so the clock one is checked
' directly: wind the clock to its limit and see that the game ends.
LoadLevel 13 : curLevel = 13 : exitOpen = 0
DeadEnemy
Print "  vizier's death   "; Choice(exitOpen = 1, "opens the way out", "FAILS TO OPEN IT")
Print "  pickups          swords drawn "; Str$(nSwordsDrawn)
Print "  scenes           "; Str$(nCuts); " shown"
Print "  shadows          "; Str$(nShadows); "  mirrors placed "; Str$(nMirrors); "  merges "; Str$(nMerges)
Print "  ending           "; Choice(gameOver = 2, "won", Choice(gameOver = 1, "out of time", "still playing"))
gameOver = 0 : curLevel = 1 : frameCount = GAMEMINUTES * FRAMESPERMIN
GetMinLeft
Print "  clock runs out   "; Choice(gameOver = 1, "ends the game", "FAILS TO END THE GAME"); " - "; msgText
Print "  stairs climbed   "; Str$(nStairs); "  potions "; Str$(nPotions); "  strength "; Str$(kidStr); "/"; Str$(maxKidStr)
Print "  clock            "; Str$(minLeft); " game minutes left of "; Str$(GAMEMINUTES); ", "; Str$(nTimeMsgs); " announcements"
Print "  fighting         bones "; Str$(nBones); "  followed "; Str$(nTransfers); "  en garde "; Str$(nEngarde); "  strikes "; Str$(nStrikes); "  blocks "; Str$(nBlocks); "  stabs "; Str$(nStabs); "  guards killed "; Str$(nGuardsDead)
Print "  plates pressed   "; Str$(nPlates); "  loose floors "; Str$(nLooseTrig); "  spikes sprung "; Str$(nSpikesTrig); "  impaled "; Str$(nImpaled)
If nGrabs = 0 Then Print "  NOTE: the grab has still never succeeded"
If nMed = 0 And nHard = 0 Then Print "  NOTE: only soft landings have happened"

FrameBuffer Close

' Put the colours back, or the console is left unreadable.
Map Reset
Map Set
Option Console Both
End

'-----------------------------------------------------------------------------
' One tick: decide, advance, then settle where he ended up.
' Begin a level at its own start position, as the game does.
' Finishing a level: the fanfare, whatever strength he earned, the interlude
' if this is one of the levels that has one, then the level itself.
Sub NextLevel
  CueSong 9
  origStrength = maxKidStr
  CutScene curLevel + 1
  StartLevel curLevel + 1
  Print "  -> level "; Str$(curLevel)
End Sub

Sub StartLevel(n As INTEGER)
  Local INTEGER st, bl, fc
  LoadLevel n : loadedLevel = n
  If n <> 3 Then milestone = 0
  ' TOPCTRL.S RESTART: "lda level / cmp #1 / bne :gotswd / lda #0 / sta
  ' gotsword ;Start Level 1 w/o sword".  The port kept it, so after picking
  ' the sword up on the first level and dying he restarted armed - with the
  ' sword still lying on the floor where he had found it.
  If n = 1 Then gotSword = 0
  cID = 0 : cSword = 0 : gdPresent = 0
  message = MSG_LEVEL : msgLevel = n : msgTimer = LEVELTIMER
  invert = 0
  If n = 1 Then CueSong 3                 ' danger, as the first level opens
  st = level(OFF_INFO + 64)
  bl = level(OFF_INFO + 65)
  fc = level(OFF_INFO + 66)
  ' SUBS.S STARTKID :special3 - the third level's checkpoint:
  '     lda milestone / beq :nomile
  '     lda #-1 / sta KidStartFace
  '     lda #2  / sta KidStartScrn
  '     lda #6  / sta KidStartBlock ;put him just inside 1st gate...
  '     lda #7 / ldx #4 / ldy #0 / jsr rdblock
  '     lda #space / sta (BlueType),y ;remove loose floor...
  ' Once he has been past the first gate, a death puts him back just inside it
  ' rather than at the start of the level, and the loose floor he has already
  ' brought down on screen 7 stays down.  The port had none of it.
  If n = 3 And milestone Then
    st = 2 : bl = 6 : fc = &HFF
    SetType 7, 4, T_SPACE
  End If
  cScrn = st
  cBlockX = bl Mod COLS : cBlockY = bl \ COLS
  cX = 58 + cBlockX * 14 + 7 + ANGLE : cY = floory(cBlockY + 1)
  cFace = fc Xor &HFF : cAction = 0 : cXVel = 0 : cYVel = 0 : cLife = &HFF
  cFalling = 0 : stunned = 0 : cPosn = 15
  ' STARTKID starts him in a different sequence on each of the levels that
  ' need one: the first drops him in from the ceiling, the thirteenth has him
  ' already running, and every other level turns him round on the spot.  The
  ' port stood him still on all of them.
  If n = 1 Then
    cSeq = seqTab(SEQ_STEPFALL)
  ElseIf n = 13 Then
    cSeq = seqTab(SEQ_RUNNING)
  Else
    cSeq = seqTab(SEQ_TURN)
  End If
  GetScreens cScrn
  numTrans = 0 : numMob = 0
  SaveChar kRec()
  EnterScreen cScrn, cBlockY
  Entrance
  composedScrn = -1
End Sub

' The walkthrough of level 1: a step machine that watches where he is and
' presses what a player would.  Screen-relative input throughout.
' The keyboard.  KEYDOWN reports every key held at once, which a game of
' this kind needs: running and jumping is two keys, and catching a ledge on
' the way down is three.  Either shift is the action button, as the original
' uses, and the space bar does the same for keyboards that swallow shift.
Sub ReadKeys
  Local INTEGER n, k, i, lf, rt, up, dn, bt
  lf = 0 : rt = 0 : up = 0 : dn = 0 : bt = 0
  n = KeyDown(0)
  For i = 1 To n
    k = KeyDown(i)
    Select Case k
      Case &H80, &HA4 : up = 1
      Case &H81, &HA1 : dn = 1
      Case &H82, &HA2 : lf = 1
      Case &H83, &HA3 : rt = 1
      Case 32         : bt = 1
      Case 27         : playDone = 1
    End Select
  Next i
  ' lshift is 8 and rshift is 128.  Compared, not ANDed into a condition:
  ' AND here is bitwise and 8 AND 1 is zero.
  If (KeyDown(7) And &H88) <> 0 Then bt = 1
  SetInputLR lf, rt, up, dn, bt
End Sub

Sub PlayCtrl
  Local INTEGER nxt, pl, pr, pu, pb
  nxt = pstep : pl = 0 : pr = 0 : pu = 0 : pb = 0
  note = "none    "
  ' A guard in play: fight him first.  Block when he is starting a
  ' strike, otherwise strike from the ready poses; the walkthrough waits.
  If gdPresent And cSword = 2 And gRec(13) <> 0 Then
    If gRec(0) = 151 Or gRec(0) = 152 Then
      pu = 1 : note = "block   "
    ElseIf cPosn = 158 Or cPosn = 170 Or cPosn = 171 Or cPosn = 165 Then
      If (pTimer And 1) Then pb = 1 : note = "strike  "
    End If
    pTimer = pTimer + 1
    SetInputLR 0, 0, pu, 0, pb
    Exit Sub
  End If
  If cSword = 2 Then SetInputLR 0, 0, 0, 0, 0 : Exit Sub
  If gdPresent And gRec(13) <> 0 And enemyAlert >= 2 And gotSword Then
    SetInputLR 0, 0, 0, 0, 0 : note = "guard!  "
    Exit Sub
  End If
  Select Case pstep
    Case 0                                    ' drop in, land
      If cBlockY = 1 And cPosn = 15 Then nxt = 1
    Case 1                                    ' right, off the ledge onto the rubble
      pr = 1 : note = "right   "
      If cBlockY = 2 Then nxt = 2
    Case 2                                    ' on to the loose floor
      pr = 1 : note = "right   "
      If cBlockX >= 6 Then nxt = 3
    Case 3                                    ' running carried him past it: stop
      If cPosn = 15 Then nxt = 30
    Case 30                                   ' turn back
      If (cFace And &H80) = 0 Then pl = 1 : note = "left    "
      If (cFace And &H80) <> 0 And cPosn = 15 Then nxt = 31
    Case 31                                   ' back onto the loose floor, or the hole it left
      pl = 1 : note = "left    "
      If cBlockX <= 6 Then nxt = 32
    Case 32                                   ' it gives, and he drops into screen 2
      If cScrn = 2 Then nxt = 4
      If pTimer > 60 Then nxt = 1
    Case 4
      If cPosn = 15 Then nxt = 5
    Case 5, 6                                 ' right through screens 2 and 3
      pr = 1 : note = "right   "
      If cScrn = 9 Then nxt = 7
    Case 7                                    ' into the exit room, a little way
      pr = 1 : note = "right   "
      If cBlockX >= 3 Then nxt = 8
    Case 8
      If cPosn = 15 Then nxt = 9
    Case 9                                    ' turn to face the plate's wall
      If (cFace And &H80) = 0 Then pl = 1 : note = "left    "
      If (cFace And &H80) <> 0 And cPosn = 15 Then nxt = 10
    Case 10                                   ' run back to the wall
      pl = 1 : note = "left    "
      ' Let go so the stop leaves him in block 1, under the clear block
      ' beside the plate: a run stops about sixteen units on.
      If cX <= 98 Then nxt = 11
    Case 11
      If cPosn = 15 Then nxt = 12
    Case 12                                   ' jump up and catch the ledge
      If pTimer = 1 Then pu = 1 : note = "up      "
      If cPosn >= 87 And cPosn < 100 Then nxt = 13
      If pTimer > 40 Then nxt = 11
    Case 13                                   ' climb onto the plate
      If stunned = 0 Then pu = 1 : note = "up      "
      If cBlockY = 0 And cPosn = 15 Then nxt = 14
    Case 14                                   ' the door rises
      If pTimer > 50 Then nxt = 15
    Case 15                                   ' off the plate and down
      pr = 1 : note = "right   "
      If cBlockY = 1 And cPosn = 15 Then nxt = 16
    Case 16                                   ' to the door
      pr = 1 : note = "right   "
      If cBlockX >= 3 Then nxt = 17
    Case 17
      If cPosn = 15 Then nxt = 18
    Case 18                                   ' up the stairs
      If (pTimer And 7) = 1 Then pu = 1 : note = "up      "
      If curLevel = 2 Then nxt = 19
    Case 19                                   ' level 2: look around, then stop
      If pTimer > 60 Then playDone = 1
  End Select
  SetInputLR pl, pr, pu, 0, pb
  If nxt <> pstep Then
    Print "  step "; Str$(nxt); " at frame "; Str$(frame); "  scr "; Str$(cScrn); " blk "; Str$(cBlockX); ","; Str$(cBlockY)
    pstep = nxt : pTimer = 0
  Else
    pTimer = pTimer + 1
  End If
End Sub

Sub ResetChar
  ResetCharAt 1, 1, 4, 1
End Sub

'-----------------------------------------------------------------------------
' Scenarios need to start somewhere specific: catchable ledges are rare - seven
' in the whole game - and a fall long enough to hurt has to cross a screen
' boundary, so neither happens where a walk from the default start can reach.
Sub ResetCharAt(lvl As INTEGER, scrn As INTEGER, bx As INTEGER, by As INTEGER)
  cID = 0 : cSword = 0 : gdPresent = 0
  ' Always a fresh level: the moving parts rewrite the blueprint, and a floor
  ' one scenario dropped must be back for the next.
  LoadLevel lvl : loadedLevel = lvl
  cScrn = scrn : cBlockX = bx : cBlockY = by
  cX = 58 + cBlockX * 14 + 7 + ANGLE : cY = floory(cBlockY + 1)
  cFace = &HFF : cAction = 0 : cXVel = 0 : cYVel = 0 : cLife = &HFF
  cFalling = 0 : stunned = 0 : cPosn = 15
  ' The flags a fight leaves behind are not part of the level, so LoadLevel
  ' does not clear them and they carried from one scenario into the next.
  ' alertGuard especially: it is the "something was heard" flag, and it is
  ' only ever consumed by a guard, so on a screen with none it stayed set.
  alertGuard = 0 : enemyAlert = 0 : refract = 0 : justBlocked = 0
  droppedOut = 0 : chgOppStr = 0 : offGuard = 0
  cSeq = seqTab(SEQ_STAND)
  GetScreens cScrn
  BuildTypeGrid cScrn
  composedScrn = -1
  ClearInput
  numTrans = 0 : numMob = 0
  SaveChar kRec()
  EnterScreen cScrn, cBlockY
  ' Say what he is actually standing in.  Placing a character at coordinates
  ' without checking the geometry there is how every one of these scenarios has
  ' gone wrong so far: he falls immediately, or there is nothing to hit.
  Print "      start blk "; Str$(bx); ","; Str$(by); " lvl "; Str$(lvl); " scr "; Str$(scrn);
  Print " under "; Str$(BlockAt(scrn,bx,by));
  Print " left "; Str$(BlockAt(scrn,bx-1,by));
  Print " right "; Str$(BlockAt(scrn,bx+1,by));
  If noFloor(BlockAt(scrn,bx,by)) Then Print " - NO FLOOR, he will fall at once" Else Print " - standing"
End Sub

'-----------------------------------------------------------------------------
Sub ClearInput
  jstkX = 0 : jstkY = 0 : btn = 1
  clrF = 1 : clrB = 1 : clrU = 1 : clrD = 1 : clrBtn = 1
  pF = 0 : pB = 0 : pU = 0 : pD = 0 : pBtn = 0
End Sub

'-----------------------------------------------------------------------------
' One character of a scenario string is one frame of input.
Sub ApplyCode(c As STRING)
  Select Case c
    Case ">" : SetInput 1,0,0,0,0
    Case "<" : SetInput 0,1,0,0,0
    Case "^" : SetInput 0,0,1,0,0
    Case "v" : SetInput 0,0,0,1,0
    Case "F" : SetInput 1,0,0,0,1
    Case "B" : SetInput 0,0,0,0,1
    Case "U" : SetInput 0,0,1,0,1
    Case "D" : SetInput 0,0,0,1,1
    Case "J" : SetInput 1,0,1,0,0
    Case "R" : SetInput 1,0,0,1,0
    Case Else : SetInput 0,0,0,0,0
  End Select
End Sub

'-----------------------------------------------------------------------------

' One frame of the game, in the original's order: the moving parts, who can
' see whom, the player, the guard, then the blows they exchanged.
Sub GameFrame
  KeepTime
  MiscTimers
  AnimMobs
  AnimTrans
  BonesRise
  CheckAlert
  LoadKidWOp
  StepCharacter
  SaveChar kRec()
  If gdPresent Then
    SaveInput
    LoadShadWOp
    If cScrn = visScrn Then StepCharacter
    SaveChar gRec()
    RestoreInput
  End If
  CheckStrike
  CheckStab
  LoadKidWOp
End Sub

Sub StepCharacter
  Local INTEGER want
  ' A grab leaves him briefly stunned, and the timer has to run down or he
  ' never accepts input again.  While it is running the control machine is
  ' skipped but the animation still advances, which is what lets the catch play
  ' out before he can do anything else.
  If stunned > 0 Then stunned = stunned - 1
  want = 0
  ' A hard landing costs all his strength, which is death, and a corpse takes
  ' no input and does not step off anything.  Without this a fatal fall onto a
  ' missing floor repeated for ever: land, step off, fall, land.
  If cLife = 0 Then
    If cPosn = 15 Or cPosn = 166 Or cPosn = 158 Or cPosn = 171 Then cSeq = seqTab(SEQ_DROPDEAD)
    If Advance() = 0 Then lastWhat = "STALLED" : Exit Sub
    ' A corpse still obeys the physics.  The original runs animchar, gravity,
    ' addfall and checkfloor for a dead character too; only the control
    ' machine below is skipped, because a corpse takes no input.  Leaving the
    ' physics out as well left a guard knocked off a ledge hanging in mid-air
    ' in his falling pose for the rest of the game.
    RereadBlocks
    Settle
    CutFallenGuard
    Exit Sub
  End If
  ' Anyone who is not the player thinks for himself.  Testing for two or
  ' more left the shadow, who is one, with no mind at all.
  If cID >= 1 Then
    If cID = 4 Then cSword = 2               ' a skeleton never sheathes
    AutoCtrl
    ' Held at the ceiling: no advance, no fall, nothing.
    If shadHold And cID = 1 Then Exit Sub
  End If
  If cFalling = 0 And cPosn >= 87 And cPosn < 100 Then
    want = HangCtrl()
  ElseIf cFalling = 0 And cSword = 2 And cAction < 2 Then
    want = FightCtrl()
  ElseIf cFalling = 0 And cID >= 2 And cID <> MOUSE_ID Then
    want = GuardCtrl()
  ElseIf cFalling = 0 And stunned = 0 Then
    ' CTRL.S GENCTRL: "cmp #5 ;is char in mid-bump? / beq :clr / cmp #4 ;or
    ' falling? / beq :clr".  Neither takes any input.  The port let StandCtrl
    ' run right through the bump recovery, poses 50 to 52.
    If cAction = 5 Or cAction = 4 Then
      want = 0
    ElseIf cPosn = 109 Then
      want = CrouchCtrl()
    ElseIf cPosn = 15 Then
      want = StandCtrl()
    ElseIf cPosn >= 1 And cPosn <= 3 Then
      want = StartingCtrl()
    ElseIf cPosn = 48 Then
      want = TurningCtrl()
    ElseIf cPosn >= 67 And cPosn <= 69 Then
      want = StJumpUpCtrl()
    ElseIf cPosn >= 50 And cPosn < 53 Then
      want = StandCtrl()
    ElseIf cPosn < 15 Then
      want = RunCtrl()
    End If
  End If
  If want > 0 Then
    cSeq = seqTab(want)
    lastWhat = "seq " + Str$(want)
  Else
    lastWhat = ""
  End If

  If Advance() = 0 Then lastWhat = "STALLED" : Exit Sub
  poseSeen(cPosn) = 1
  RereadBlocks
  HitBarrier
  If cID = 0 Then
    FirstGuard
    If CrossScreen() Then lastWhat = "-> screen " + Str$(cScrn) : nCross = nCross + 1
  Else
    ' The clamp keeps a guard on the screen he belongs to.  It must not be put
    ' on the shadow: on the fourth level he runs off to the right until his x
    ' wraps below eighty and he is gone, and held at 215 he never goes.
    If cID >= 2 Then
      If cX < 40 Then cX = 40
      If cX > 215 Then cX = 215
    End If
    RereadBlocks
  End If
  If cID = 0 Then CheckGate
  EnemyColl
  Settle
  CutFallenGuard
  CheckPress
  CheckSpikes
  CheckImpale
  ShakeLoose
  ChgMeters
  If weightless > 0 Then weightless = weightless - 1
End Sub

'-----------------------------------------------------------------------------
' Which block is he in now?  The original keeps a 256-entry lookup from screen x
' to block number; the same thing computed, since a block is fourteen units wide
' and the screen's left edge sits at 58.
'
' Without this the character walks the whole width of the screen while the floor
' test keeps reading the cell he started in, so he can never step off anything.
' THE BASE X.  The original does not measure a character from his coordinate.
' Every pose records how many pixels to count in from the left edge of its
' image, in the low five bits of the frame's check byte, and the frame's own
' displacement counts too.  That point is his base X, and both the block he is
' standing in and every distance are taken from it.
'
' It is not a small correction: the foot offset runs from nought to seventeen
' pixels depending on the pose and a block is fourteen wide, so measuring from
' the raw coordinate can put him in the wrong block outright.  That is what
' made the last frame of a running jump's take-off look as though it had left
' the floor when the original still has him on it.
Function BaseX() As INTEGER
  Local INTEGER f, d
  BaseX = cX
  If cPosn < 1 Then Exit Function
  f = (cPosn - 1) * frmEntry
  If f + 4 >= frmLen Then Exit Function
  d = Sgn8(frmb(f + 2)) - (frmb(f + 4) And FFOOTMARK)
  If (cFace And &H80) Then BaseX = (cX - d) And &HFF Else BaseX = (cX + d) And &HFF
End Function

' GETBASEBLOCK: which block he is in, from his base X and not his coordinate.
Sub RereadBlocks
  Local INTEGER bx
  bx = BaseX()
  If bx < BLOCKLO Then cBlockX = -4 : Exit Sub
  cBlockX = (bx - BLOCKLO) \ 14 - 4
End Sub

'-----------------------------------------------------------------------------
' Walking into a barrier has to STOP him, not just move him: pushing him out
' while the run sequence carries on means he walks straight back in, once a
' frame, for ever.  The bump sequence is what ends the run.
'
' This runs BEFORE any screen crossing.  Moving first and correcting afterwards
' let him cross into the next screen and only then be pushed back out, so he
' ping-ponged over the join and the background was rebuilt every frame.
Sub TryGrab(idx As INTEGER)
  Local INTEGER saved, ahead, above
  saved = cX
  ' The reach goes BACKWARDS, not forwards.  When you run off a ledge the ledge
  ' you are grabbing for is behind you, so a left-facing character reaches to
  ' the right.  Reaching the way he faces moves him away from it.
  cX = AddCharX(GRAB_REACH)
  RereadBlocks
  If (cFace And &H80) Then ahead = cBlockX - 1 Else ahead = cBlockX + 1
  above = BlockAt(cScrn, cBlockX, cBlockY - 1)
  If CanGrabAt(above, cScrn, ahead, cBlockY - 1) = 0 Then
    cX = saved : RereadBlocks : Exit Sub
  End If
  ' CTRL.S fallon :ok - "jsr getdist / jsr addcharx / sta CharX".  It is his
  ' BASE that is put on the block edge, not his coordinate, and that is what
  ' makes CharBlockX the ledge column for the whole of the hang.  Setting cX
  ' to the edge instead left the base a frame's offset away, so the hang then
  ' had to go looking for the ledge one block over.
  MoveFwd GetDist()
  cY = floory(idx)
  cYVel = 0
  cFalling = 0
  cAction = 2
  stunned = STUN_TIME
  cSeq = seqTab(SEQ_FALLHANG)
  nGrabs = nGrabs + 1
  lastWhat = "grabs ledge"
End Sub

'-----------------------------------------------------------------------------
' Clear above him, and a solid ledge above and in front.  Two block types can
' only be caught from one side, which is why facing is tested here.
' CHECKLEDGE has one test that needs the block's STATE and not just its type:
'     cmp #loose / bne :notloose
'     bit tempstate / bne :no   ;floor is already loose
' A loose floor that has begun to go is not something to hang from.  The port
' let him catch one that was on its way down, which is the one moment it is
' certainly not there.  CanGrab itself stays a function of the two types, so
' that the host reference in animref.py --mode grabrule and grabtest.bas keep
' pinning it; this is the layer above, where the block can be looked up.
Function CanGrabAt(above As INTEGER, scrn As INTEGER, bx As INTEGER, by As INTEGER) As INTEGER
  Local INTEGER front
  front = RdBlock(scrn, bx, by)
  CanGrabAt = CanGrab(above, front)
  If CanGrabAt = 0 Then Exit Function
  If front = T_LOOSE Then
    If BSpec(tScrn, tBY * COLS + tBX) <> 0 Then CanGrabAt = 0
  End If
End Function

Function CanGrab(above As INTEGER, aboveinf As INTEGER) As INTEGER
  CanGrab = 0
  If above = 20 Then Exit Function
  If above = 12 And (cFace And &H80) = 0 Then Exit Function
  If noFloor(above) = 0 Then Exit Function
  If noFloor(aboveinf) Then Exit Function
  If aboveinf = 7 And (cFace And &H80) <> 0 Then Exit Function
  CanGrab = 1
End Function

'-----------------------------------------------------------------------------

' Severity is chosen by the pose he hit it in: a jump or a fall is a hard bump,
' anything else a soft one.
Sub WallBump
  ' COLL.S ":normal" - pose 24 is the other half of the stand jump and belongs
  ' with 25.  And every bump, soft or hard, goes through BumpSound: "lda #1 /
  ' sta alertguard / lda #SmackWall / jmp addsound".  The port made no noise
  ' walking into a wall and told nobody, so a guard in the next room heard
  ' nothing of it.
  If cPosn = 24 Or cPosn = 25 Or (cPosn >= 40 And cPosn <= 42) Or (cPosn >= 102 And cPosn <= 106) Then
    cSeq = seqTab(SEQ_HARDBUMP) : lastWhat = "hard bump" : nBumps = nBumps + 1
  Else
    cSeq = seqTab(SEQ_BUMP) : lastWhat = "bump" : nBumps = nBumps + 1
  End If
  cAction = 5
  BumpSound
End Sub

' COLL.S BumpSound: the smack of hitting a wall, and it carries.
Sub BumpSound
  AddSound 13
  alertGuard = 1
End Sub

'-----------------------------------------------------------------------------

Sub HitBarrier
  Local INTEGER ahead, t, lo, hi, floorline
  ' NOT FIXED, and deliberately: COLL.S COLLISIONS lets a character through a
  ' barrier only while he is hanging (action 2 or 6), climbing (poses 135-148)
  ' or turning (action 7), so the original bumps in the air as well.  It can
  ' afford to, because CHECKBARR works from the character's own image edges -
  ' the CD/SN nybble buffers, a collision being a nybble that goes from 0 to 1
  ' between frames - and stops him where his edge meets the bar, from the
  ' BarL/BarR tables.  This port has none of that; what stands in for it is
  ' "the block ahead is a barrier and he has reached its edge", which on the
  ' ground is the same answer and in the air is not: a man falling PAST a wall
  ' is beside it every frame.  Enabling the air case without the edge data
  ' smacks him into the wall under every ledge he reaches for, so he can never
  ' catch one - which is what this gate was put here for in the first place.
  ' Bit 6 of the frame's check byte is the on-the-ground mark, the same bit
  ' the floor test uses; every hanging and climbing pose has it clear.
  If cFalling Then Exit Sub
  If cPosn >= 1 Then
    If (frmb((cPosn - 1) * frmEntry + 4) And &H40) = 0 Then Exit Sub
  End If

  ' Test the block AHEAD of him, not the one he is in.  Letting him walk into a
  ' barrier and pushing him back out afterwards is what made him jitter against
  ' a wall and cross screen joins he should never have reached: by the time the
  ' correction ran he had already moved, and sometimes already changed screen.
  ' A running step is bigger than the gap to a block edge, so testing only
  ' whether he has REACHED the edge lets him skip straight over it and into the
  ' wall in one frame - where the floor test then treats the solid block as
  ' empty and he falls through it.  So first: has he already entered a barrier?
  ' The mirror is a barrier to everything except a man at a full run.
  If cID = 0 And cPosn >= 39 And cPosn <= 43 And (cFace And &H80) <> 0 Then
    If Unbroken(cBlockX) Then SmashMirror : Exit Sub
    If Unbroken(cBlockX - 1) Then
      cBlockX = cBlockX - 1
      SmashMirror
      Exit Sub
    End If
  End If
  ' A mirror he has already been through is just a hole in the wall.
  If BlockAt(cScrn, cBlockX, cBlockY) = T_MIRROR Then
    If Broken(cBlockX) Then Exit Sub
  End If
  t = BlockAt(cScrn, cBlockX, cBlockY)
  If t = T_GATE Then
    If GateOpen(cScrn, cBlockX, cBlockY) Then t = 0
  ElseIf t = T_SLICER Then
    If SlicerShut(cScrn, cBlockX, cBlockY) = 0 Then t = 0
  End If
  ' Barrier() returns a CLASS, not a flag, so it must be compared: AND is
  ' bitwise here, and 4 And 1 is 0.  That let him run straight into a solid
  ' block when facing right and fall through it.
  If Barrier(t) <> 0 And cBlockX >= 0 And cBlockX < COLS Then
    ' Push him back to the near edge of the block he came from.
    If (cFace And &H80) Then
      cX = 14 * (cBlockX + 1 + 4) + BLOCKLO     ' left edge of the block to his right
    Else
      cX = 14 * (cBlockX - 1 + 4) + BLOCKLO + 13 ' right edge of the block to his left
    End If
    RereadBlocks
    GoTo dobump
  End If

  If (cFace And &H80) Then ahead = cBlockX - 1 Else ahead = cBlockX + 1
  t = BlockAt(cScrn, ahead, cBlockY)
  If t = T_GATE Then
    If GateOpen(cScrn, ahead, cBlockY) Then t = 0
  ElseIf t = T_SLICER Then
    If SlicerShut(cScrn, ahead, cBlockY) = 0 Then t = 0
  End If
  If Barrier(t) = 0 Then Exit Sub

  ' At the edge of his own block with the wall next: stop him there.
  lo = 14 * (cBlockX + 4) + BLOCKLO
  hi = lo + 13
  If (cFace And &H80) Then
    If cX > lo Then Exit Sub                 ' still room to walk
    cX = lo
  Else
    If cX < hi Then Exit Sub
    cX = hi
  End If

dobump:

  ' COLL.S collide sorts the bump into one of two before anything else: it is
  ' a ground bump only if he has floor under him where he hit, and a ground
  ' bump taken more than fifteen pixels above the floor line is an air bump
  ' too - unless he is en garde, when a fighter is never thrown off his feet.
  ' The gate above keeps a freefall out of here, so the airbump label is only
  ' reached from a pose that carries the on-the-ground mark and is nonetheless
  ' well clear of the floor - the top of a jump.  It is written out in full
  ' because it is what COLL.S does, and because the rest of it becomes live
  ' the moment the collision is done from the image edges.
  floorline = floory(cBlockY + 1)
  If BlockAt(cScrn, cBlockX, cBlockY) = T_SPACE Then GoTo airbump
  If cSword <> 2 Then
    If floorline - cY >= 15 Then GoTo airbump
  End If
  ' GroundBump puts him on the floor line.  Coming down hard he is backed off
  ' five instead and left falling, for the floor test to land properly.
  cY = floorline
  If cYVel >= OOF_VELOCITY Then
    cX = AddCharX(-5)
    RereadBlocks
    Exit Sub
  End If
  cYVel = 0
  If cLife = 0 Then Exit Sub
  ' With the sword out the bump is the en-garde one, forward or back by which
  ' way he was moving (ENEMYCOLL / bumpengfwd).
  If cSword = 2 Then
    If cXVel < 0 Or cXVel > 127 Then
      cSeq = seqTab(SEQ_BUMPENGBACK)
      cX = AddCharX(1)                      ' :collback "lda #1 / jsr addcharx"
      RereadBlocks
    Else
      cSeq = seqTab(SEQ_BUMPENGFWD)
      BumpSound                             ' bumpengfwd falls into :doit
    End If
    lastWhat = "bump en garde" : nBumps = nBumps + 1
    cAction = 5
    Exit Sub
  End If
  ' Severity is chosen by the pose he hit it in: a jump or a fall is a hard
  ' bump, anything else a soft one.  Pose 24 is the other half of the stand
  ' jump and belongs with 25.
  If cPosn = 24 Or cPosn = 25 Or (cPosn >= 40 And cPosn <= 42) Or (cPosn >= 102 And cPosn <= 106) Then
    cSeq = seqTab(SEQ_HARDBUMP) : lastWhat = "hard bump" : nBumps = nBumps + 1
  Else
    cSeq = seqTab(SEQ_BUMP) : lastWhat = "bump" : nBumps = nBumps + 1
  End If
  cAction = 5
  BumpSound
  Exit Sub

airbump:
  ' AirBump: back off four.  If he is already in a freefall he simply rebounds
  ' off the wall with his drift killed; otherwise bumpfall takes him down from
  ' wherever he was.  Either way the wall makes a noise a guard can hear.
  cX = AddCharX(-4)
  RereadBlocks
  If cAction = ACT_FALLING Then
    cXVel = 0
  Else
    cSeq = seqTab(SEQ_BUMPFALL)
  End If
  lastWhat = "air bump" : nBumps = nBumps + 1
  BumpSound
End Sub

'-----------------------------------------------------------------------------
' Where did that leave him: on the ground, still falling, or landing?
Sub Settle
  Local INTEGER idx, t, spiked, ahead, behind, land, spkScrn, spkLoc
  ' Whether he is falling is our state, not the sequence's.  The sequence
  ' rewrites the action class as it runs, so testing that instead re-entered
  ' the fall every frame and restarted it, and he hung in the air.
  ' A sequence can put a character into a fall by itself, which is how the
  ' shadow drops into the room.  Without this he hung in the air for ever,
  ' because only stepping off an edge ever started one.
  If cFalling = 0 And cAction = ACT_FALLING Then cFalling = 1
  If cFalling = 0 Then
    ' The floor is only checked when he is ON it.  The original dispatches on
    ' the action class: 0, 1 and 7 are on the ground and get the floor test;
    ' 2 and 6 are hanging; 3 is mid-jump, where only the falling poses 102-105
    ' hand over to the fall.  Checking regardless started a freefall the moment
    ' a running jump crossed a gap, and the jump never landed.
    If cAction = 2 Or cAction = 6 Then Exit Sub
    If cAction = 3 Then
      If cPosn < 102 Or cPosn > 105 Then Exit Sub
    ElseIf cAction = 5 Then
      If cPosn <> 109 And cPosn <> 185 Then Exit Sub
    End If
    ' On the ground, the original consults the frame table first: bit 6 of
    ' the frame's collision byte is the "check the floor" mark, and frames
    ' that are mid-climb or mid-step do not carry it.  Without this gate the
    ' first frame of a climb was stepped straight off the empty block he was
    ' climbing out of.
    If cPosn >= 1 Then
      If (frmb((cPosn - 1) * frmEntry + 4) And &H40) = 0 Then Exit Sub
    End If
    CheckBridge
    t = BlockAt(cScrn, cBlockX, cBlockY)
    ' A solid block is not somewhere you fall from or stand on: you cannot be
    ' inside one at all, so you are pushed back out the way you came.  It is in
    ' the no-floor set for exactly that reason, and treating it as a fall makes
    ' him oscillate - step off, land, step off - once a frame.
    If noFloor(t) Then
      cFalling = 1 : cAction = ACT_FALLING : cYVel = 0
      ' CTRL.S startfall: "inc CharBlockY ;# of floor just below your feet",
      ' and then addslicers.  Without the increment the fall's first frame
      ' tests the floor he has just stepped off and puts him back on it, so a
      ' walk-off wobbled - step off, land, crouch, step off again - and once
      ' the landing nudge pulled him back off the lip it cancelled the fall
      ' for good and a ledge could not be walked off at all.
      cBlockY = cBlockY + 1
      AddSlicers cScrn, cBlockY
      cSeq = seqTab(FallSeq())
      ' CTRL.S startfall puts the sword away.  Left drawn, the landing runs
      ' softland's crouch with FightCtrl in charge, and FightCtrl takes no
      ' input from that pose: he crouches where he lands until something
      ' kills him.  Retreating off a ledge is the ordinary way out of a
      ' fight, so this was a soft-lock in the one move you most want.
      cSword = 0
      ' And it tells whoever he was fighting that he has gone over the edge.
      ' Nothing ever set this, so FollowKid never ran and no guard ever came
      ' down after him.
      If cID = 0 Then droppedOut = 1
      lastWhat = "step off" : nStepOff = nStepOff + 1
    End If
    Exit Sub
  End If

  ' SUBS.S GRAVITY begins "lda CharAction / cmp #4 / bne rts", so it does
  ' nothing at all until the fall is a freefall.  The first frames of a fall
  ' are action 3, where the sequence's own chy opcodes are what move him, and
  ' the port accelerated him through those as well: every fall started about
  ' thirty pixels too low and reached the floor a frame early.
  If cAction = ACT_FALLING Then
    If weightless Then
      cYVel = cYVel + WTLESS_ACCEL
      If cYVel > WTLESS_TERMVEL Then cYVel = WTLESS_TERMVEL
    Else
      cYVel = cYVel + FALL_ACCEL
      If cYVel > FALL_TERMVEL Then cYVel = FALL_TERMVEL
    End If
  End If
  ' SUBS.S ADDFALL adds the vertical velocity whatever the action, and carries
  ' the horizontal one only in a freefall.  The port set cXVel from the
  ' sequence and then never moved him with it, so every fall was dead
  ' vertical: the one pixel a frame that a step off an edge drifts was
  ' missing, and with it the ledge the seventh level opens by dropping onto.
  cY = (cY + cYVel) And &HFF
  If cAction = ACT_FALLING And cXVel <> 0 Then
    cX = AddCharX(cXVel)
    RereadBlocks
  End If
  idx = cBlockY + 1
  If idx > 4 Then Exit Sub
  ' CHECKFLOOR sends action 3 in the falling poses to fallon, not to falling:
  ' while the fall is still the sequence's, the only thing asked is whether
  ' there is a ledge to catch.  The floor plane is not tested until action 4.
  If cAction = 3 Then
    If cPosn >= 102 And cPosn <= 105 Then
      If btn < 0 And cLife And cYVel < GRAB_SPEED And cY + GRAB_LEAD >= floory(idx) Then
        nGates = nGates + 1
        TryGrab idx
      End If
    End If
    Exit Sub
  End If
  If cY < floory(idx) Then
    ' Still in the air.  Three gates before a ledge is even looked for: the
    ' button held, falling slowly enough to catch anything, and close enough to
    ' the floor line to be reaching for it.
    If btn < 0 And cLife And cYVel < GRAB_SPEED And cY + GRAB_LEAD >= floory(idx) Then
      nGates = nGates + 1
      TryGrab idx
    End If
    Exit Sub
  End If
  t = BlockAt(cScrn, cBlockX, cBlockY)
  ' CTRL.S falling: "jsr getunderft / cmp #block / bne :2 / jsr InsideBlock".
  ' A solid block underfoot is the special case - he is put out to one side of
  ' it rather than dropped through.  The port had a solid block in the no-floor
  ' set and let him fall through the wall.
  If t = T_BLOCK Then
    InsideBlock
    t = BlockAt(cScrn, cBlockX, cBlockY)
    ' "jsr cmpspace / bne hitflr": a solid block is not space, so if the nudge
    ' has not taken him clear of it he lands on top of it.  Falling through a
    ' wall is what the port did instead, because a solid block is in its
    ' no-floor set - it is there so that nobody can stand INSIDE one, which is
    ' a different question from whether you can come down on one.
    If t = T_BLOCK Then t = T_FLOOR
  End If
  If noFloor(t) Then
    cBlockY = (cBlockY + 1) And &HFF      ' through the floor plane
    lastWhat = "through"
    Exit Sub
  End If
  ' Sprung spikes under him take precedence over how hard he lands.
  ' CTRL.S hitflr, in its own order.  He is put on the floor line first, then
  ' the spikes underfoot are asked about, then how near the edge he came down.
  cY = floory(idx)
  cAction = 0 : cFalling = 0
  spiked = 0
  t = RdBlock(cScrn, cBlockX, cBlockY)
  ' RdBlock leaves the resolved place in tScrn/tBX/tBY, and the test for
  ' spikes BEHIND him below runs another one - so where the spikes are has
  ' to be kept here rather than read back afterwards.
  If t = T_SPIKES And cLife <> 0 Then spiked = 1 : spkScrn = tScrn : spkLoc = tBY * COLS + tBX
  If spiked = 0 Then
    ' "Has he landed too close to edge?" - within four pixels of an edge with
    ' nothing beyond it, he is moved three back off it.  Without this a landing
    ' right on the lip leaves him half over a drop.
    If (cFace And &H80) Then ahead = cBlockX - 1 Else ahead = cBlockX + 1
    If noFloor(BlockAt(cScrn, ahead, cBlockY)) Then
      If GetDist() < 4 Then cX = AddCharX(-3)
    End If
  End If
  ' "jsr addslicers ;trigger slicers on this level" - coming down on a row
  ' starts that row's blades, exactly as walking onto it does.
  AddSlicers cScrn, cBlockY
  ' Dead before he hits the ground: hardland, and nothing left to take off him.
  If cLife = 0 Then
    cSeq = seqTab(SEQ_HARDLAND) : lastWhat = "the body lands"
    cYVel = 0
    Exit Sub
  End If
  ' Well into the block, the spikes BEHIND him count too.
  If spiked = 0 And GetDist() >= 12 Then
    If (cFace And &H80) Then behind = cBlockX + 1 Else behind = cBlockX - 1
    If RdBlock(cScrn, behind, cBlockY) = T_SPIKES Then spiked = 1 : spkScrn = tScrn : spkLoc = tBY * COLS + tBX
  End If
  If spiked Then
    If GetSpikes(spkScrn, spkLoc) Then DoImpale : Exit Sub
  End If
  ' The severity, and who it applies to.  The shadow is spared a MEDIUM fall -
  ' "lda CharID / cmp #1 / beq :softland ;shad lands easy" - not a hard one.  A
  ' guard cannot survive a medium fall and takes the hard landing whole, sound
  ' and sequence and all, which is not the same as a medium one that kills.
  If cYVel < OOF_VELOCITY Then
    land = 0
  ElseIf cYVel < DEATH_VELOCITY Then
    land = 1
    If cID = 1 Then land = 0
    If cID >= 2 Then land = 2
  Else
    land = 2
  End If
  If land = 1 Then
    If DecStr(1) = 0 Then
      cLife = 0 : nDead = nDead + 1
      land = 3                      ' :hdland1 - the hard landing, already paid for
    End If
  End If
  If land = 2 Then
    If DecStr(100) = 0 Then cLife = 0 : nDead = nDead + 1
    land = 3
  End If
  If land = 0 Then
    ' ":softland ... cmp #2 / bcs :gd ;guard always lands en garde" - and at
    ' :gd the sword is SET, not merely tested.
    If cID >= 2 Or cSword = 2 Then
      cSword = 2
      cSeq = seqTab(SEQ_LANDENGARDE)
    Else
      cSeq = seqTab(SEQ_SOFTLAND)
    End If
    lastWhat = "soft land" : nSoft = nSoft + 1
  ElseIf land = 1 Then
    AddSound 5
    cSeq = seqTab(SEQ_MEDLAND) : lastWhat = "med land" : nMed = nMed + 1
  Else
    AddSound 5
    cSeq = seqTab(SEQ_HARDLAND) : lastWhat = "HARD land" : nHard = nHard + 1
  End If
  cYVel = 0
End Sub

'-----------------------------------------------------------------------------
Function StandCtrl() As INTEGER
  Local INTEGER d
  StandCtrl = 0
  If clrBtn < 0 And btn < 0 Then
    StandCtrl = TryPickup()
    If StandCtrl Then Exit Function
  End If
  ' CTRL.S standing, "Shadman only: down & fwd to go en garde" - a fresh down
  ' and a fresh forward from anyone who is not the player.  FinalShad uses it.
  If cID <> 0 Then
    If clrD < 0 And clrF < 0 Then StandCtrl = DoEngarde() : Exit Function
  End If
  ' With a sword and a guard in sight he draws it, or turns to face him.
  If cID = 0 And gotSword Then
    If offGuard = 0 Or btn < 0 Then
      If enemyAlert >= 2 Then
        d = OpDist()
        If d >= SWORDTHRESN Or d < SWORDTHRES Then
          heroic = 1
          If d >= 250 Then StandCtrl = DoTurn() Else StandCtrl = DoEngarde()
          Exit Function
        End If
      End If
      offGuard = 0
    End If
  End If
  If btn < 0 Then
    If clrB < 0 Then StandCtrl = DoTurn() : Exit Function
    If clrU < 0 Then clrU = 1 : StandCtrl = DoUp() : Exit Function
    ' ":2 lda clrD / bmi :down" - down with the button held is the same
    ' handler as down without it, and was missing here altogether.
    If clrD < 0 Then StandCtrl = DoDown() : Exit Function
    ' Button and forward together is the careful step, the only way to walk
    ' up to an edge without running off it.
    If jstkX < 0 And clrF < 0 Then StandCtrl = DoStepFwd() : Exit Function
    Exit Function
  End If
  If jstkY > 0 Then StandCtrl = DoDown() : Exit Function
  If jstkY < 0 Then StandCtrl = DoUp() : Exit Function
  If jstkX < 0 Then StandCtrl = DoStartrun() : Exit Function
  If jstkX > 0 Then StandCtrl = DoTurn() : Exit Function
End Function

'=============================================================================
' THE SECOND CHARACTER.  Records, the opponent's view, the guard's mind, the
' fight controls both use, and the blows.
'=============================================================================
Sub SaveChar(r() As INTEGER)
  r(0) = cPosn : r(1) = cX : r(2) = cY : r(3) = cFace : r(4) = cBlockX : r(5) = cBlockY
  r(6) = cAction : r(7) = cXVel : r(8) = cYVel : r(9) = cSeq : r(10) = cScrn : r(11) = cID
  r(12) = cSword : r(13) = cLife : r(14) = cFalling : r(15) = stunned
  ' The careful step's counter belongs to the character, not to the game.  The
  ' original keeps it in the character block, between the screen and the id; it
  ' was a bare global here, so the guard's turn wrote over the player's and the
  ' step machine lost its place whenever anyone else was on screen.
  r(16) = cRepeat
End Sub

Sub LoadChar(r() As INTEGER)
  cPosn = r(0) : cX = r(1) : cY = r(2) : cFace = r(3) : cBlockX = r(4) : cBlockY = r(5)
  cAction = r(6) : cXVel = r(7) : cYVel = r(8) : cSeq = r(9) : cScrn = r(10) : cID = r(11)
  cSword = r(12) : cLife = r(13) : cFalling = r(14) : stunned = r(15)
  cRepeat = r(16)
End Sub

Sub SetOp(r() As INTEGER)
  oPosn = r(0) : oX = r(1) : oY = r(2) : oFace = r(3) : oBlockX = r(4) : oBlockY = r(5)
  oAction = r(6) : oSword = r(12) : oLife = r(13) : opScrn = r(10)
End Sub

Sub LoadKidWOp
  LoadChar kRec() : SetOp gRec()
End Sub

Sub LoadShadWOp
  LoadChar gRec() : SetOp kRec()
End Sub

' The player's stick and fresh-press flags survive the guard's turn.
Sub SaveInput
  kJstkX = jstkX : kJstkY = jstkY : kBtn = btn
  kClrF = clrF : kClrB = clrB : kClrU = clrU : kClrD = clrD : kClrBtn = clrBtn
End Sub

Sub RestoreInput
  jstkX = kJstkX : jstkY = kJstkY : btn = kBtn
  clrF = kClrF : clrB = kClrB : clrU = kClrU : clrD = kClrD : clrBtn = kClrBtn
End Sub

' GETOPDIST: how far ahead the opponent is, in the character's own facing;
' negative is behind.  Facing each other, the opponent's anchor is his far
' side, so a width is added.  Different screens read as far away.
Function OpDist() As INTEGER
  Local INTEGER d
  If cScrn <> opScrn Then OpDist = 127 : Exit Function
  d = oX - cX
  ' CTRLSUBS.S GETOPDIST clamps the two directions separately: the ":neg"
  ' branch loads 127 and then negates it, so an opponent a long way BEHIND
  ' comes back as -127.  The port clamped both ways to +127, which reports
  ' a man far behind as a man far in front.
  If d > 127 Then d = 127
  If d < -127 Then d = -127
  If (cFace And &H80) Then d = -d
  If ((cFace Xor oFace) And &H80) Then
    If (d And &HFF) < 127 - ESTWIDTH Then d = d + ESTWIDTH
  End If
  OpDist = d And &HFF
End Function

' Signed reading of the same, for comparisons the original does unsigned.
Function OpDistS() As INTEGER
  OpDistS = Sgn8(OpDist())
End Function

' CHECKALERT: can the guard see him?  Same screen and row, both alive; then
' 2 for a clear path, 1 when a gap, a closed gate or a slicer lies between,
' 0 when a wall blocks the view.
Sub CheckAlert
  Local INTEGER xk, xg, x, xe, tt, s
  enemyAlert = 0
  If gdPresent = 0 Then Exit Sub
  If gRec(11) = MOUSE_ID Then Exit Sub    ' he is not an enemy
  ' MISC.S CHECKALERT: neither is a shadow, except on the twelfth level
  ' where he is the one you have come to meet.  Without this the kid drew
  ' his sword at the shadow on levels 4, 5 and 6, and on six that is
  ' exactly where the running jump the shadow copies has to be made.
  If gRec(11) = 1 And curLevel <> 12 Then Exit Sub
  If kRec(0) = 0 Or (kRec(0) >= 219 And kRec(0) < 229) Then Exit Sub
  If kRec(13) = 0 Or gRec(13) = 0 Then Exit Sub
  If kRec(10) <> gRec(10) Or kRec(5) <> gRec(5) Then Exit Sub
  enemyAlert = 2
  xk = blocks(gBlockEdge + kRec(4) + 5) + 7
  xg = blocks(gBlockEdge + gRec(4) + 5) + 7
  If xg < xk Then
    x = xg : xe = xk
  Else
    x = xk : xe = xg
  End If
  s = kRec(10)
  tt = RdBlock(s, (x - BLOCKLO) \ 14 - 4, kRec(5))
  If tt = T_SLICER Then x = x + 14
  tt = RdBlock(s, (xe - BLOCKLO) \ 14 - 4, kRec(5))
  If tt = T_GATE Then xe = xe - 14
  Do While x <= xe
    tt = RdBlock(s, (x - BLOCKLO) \ 14 - 4, kRec(5))
    If tt = T_BLOCK Or tt = 7 Or tt = 12 Then enemyAlert = 0 : Exit Sub
    If tt = T_LOOSE Or tt = T_SLICER Then
      enemyAlert = 1
    ElseIf tt = T_GATE Then
      ' gfightthres is 28*4, not 4*16: a gate has to be a good deal
      ' higher than that before it stops being in the way of a fight.
      If BSpec(tScrn, tBY * COLS + tBX) < 28 * 4 Then enemyAlert = 1
    ElseIf noFloor(tt) Then
      enemyAlert = 1
    End If
    x = x + 14
  Loop
End Sub

' AUTOCTRL: the guard's mind.  It presses the same stick and buttons the
' player has, then the shared controls do the rest.
Sub AutoCtrl
  clrF = 0 : clrB = 0 : clrU = 0 : clrD = 0 : clrBtn = 0 : jstkX = 0 : jstkY = 0 : btn = 0
  If justBlocked > 0 Then justBlocked = justBlocked - 1
  If gdTimer > 0 Then gdTimer = gdTimer - 1
  If refract > 0 Then refract = refract - 1
  If cID = 1 Then ShadowProg : Exit Sub
  If cID = MOUSE_ID Then MouseProg : Exit Sub
  If cSword < 2 Then GuardAlert Else GuardEnGarde
End Sub

' CTRL.S clrall: every press not yet acted on is forgotten, and the caller
' then marks the one it is acting on.
Sub ClrAll
  clrB = 0 : clrF = 0 : clrU = 0 : clrD = 0
End Sub

Sub AiFwd
  clrF = -1 : jstkX = -1
End Sub
Sub AiBack
  clrB = -1 : jstkX = 1
End Sub
Sub AiBlock
  clrU = -1 : jstkY = -1
End Sub
Sub AiTurn
  clrD = -1 : jstkY = 1
End Sub
Sub AiDropGuard
  clrD = -1 : AiBack
End Sub
Sub AiEngarde
  clrD = -1 : AiFwd
End Sub
Sub AiStrike
  clrBtn = -1 : btn = -1
End Sub

' Is the block at this column a mirror that has not been broken yet?
Function Unbroken(bx As INTEGER) As INTEGER
  Unbroken = 0
  If BlockAt(cScrn, bx, cBlockY) <> T_MIRROR Then Exit Function
  If BSpec(cScrn, cBlockY * COLS + bx) <> 86 Then Unbroken = 1
End Function

Function Broken(bx As INTEGER) As INTEGER
  Broken = 0
  If BSpec(cScrn, cBlockY * COLS + bx) = 86 Then Broken = 1
End Function

' DEADENEMY: on the last level before the tower, killing him is what opens
' the way out, by pressing the plate that the exit hangs on.
Sub DeadEnemy
  If curLevel <> 13 Then Exit Sub
  exitOpen = 1
  CueSong 9
  PushPP 24, 0
  lastWhat = "the way out opens"
End Sub

' AUTO.S stealsword: on the twelfth level, cutting right into screen 18 is
' the moment the shadow takes the sword.  The block simply goes from the room
' he has just left - REMOVEOBJ, the same as picking anything else up - and
' AddGuard's test for the sword being gone is what puts the shadow on that
' screen from then on.  Without this the sword stayed where it was, the
' shadow was never added, and the level could not be finished.
Sub StealSword
  If curLevel <> 12 Or cScrn <> 18 Then Exit Sub
  If BType(kSwordScrn, kSwordY * COLS + kSwordX) <> T_SWORD Then Exit Sub
  SetType kSwordScrn, kSwordY * COLS + kSwordX, T_FLOOR
  SetSpec kSwordScrn, kSwordY * COLS + kSwordX, 0
  lastWhat = "the sword is gone"
End Sub

' MIRAPPEAR: on the fourth level the mirror is not part of the room.  It is
' put there the moment the exit opens, which is what makes the way out lead
' past it.
Sub MirAppear
  If curLevel <> 4 Then Exit Sub
  SetType kMirScrn, kMirY * COLS + kMirX, T_MIRROR
  nMirrors = nMirrors + 1
End Sub

' SMASHMIRROR: running at the mirror carries him through it.  What comes
' out the other side is his reflection, which keeps going as a character of
' its own and takes most of his strength with it.
Sub SmashMirror
  Local INTEGER mx
  SetSpec cScrn, cBlockY * COLS + cBlockX, 86
  AddSound 6
  SaveChar kRec()
  ' Mirrored about the mirror's own line, and facing the other way.
  mx = blocks(gBlockEdge + cBlockX + 5) + 10
  cX = (mx * 2 - cX) And &HFF
  cFace = cFace Xor &HFF
  RereadBlocks
  cID = 1 : cSword = 0 : cLife = &HFF
  cAction = 1 : cFalling = 0 : stunned = 0 : cXVel = 0 : cYVel = 0
  guardProg = 3
  maxOppStr = maxKidStr : oppStr = maxKidStr : chgOppStr = 0
  alertGuard = 0 : refract = 0 : justBlocked = 0 : droppedOut = 0
  playCount = 0 : preRecPtr = 0
  If HEADLESS = 0 Then GuardPalette 3
  SaveChar gRec()
  gdPresent = 1 : nShadows = nShadows + 1
  LoadKidWOp
  ' He is left with almost nothing.
  kidStr = 1 : chgKidStr = 0
  lastWhat = "through the mirror"
End Sub

' SHADOWPROG: the shadow does something different on each of his levels.
' On four he simply runs away.  On five he is the thief, and plays back a
' recorded set of moves to steal the potion.  On six he is the one who
' jumps, and he takes the leap at the same moment the player does.
Sub ShadowProg
  Select Case curLevel
    Case 4  : Shad4
    Case 5  : Shad5
    Case 6  : Shad6
    Case 12 : Shad12
  End Select
End Sub

' Level four: he runs off to the right and is gone.
Sub Shad4
  If cScrn <> 4 Then Exit Sub
  If cX >= 80 Then AiFwd Else VanishShadow
End Sub

' Level six: the plunge.  When the player commits to a running jump on the
' left of the screen, the shadow jumps with him.
Sub Shad6
  If cScrn <> 1 Then Exit Sub
  If oPosn <> 43 Then Exit Sub
  If oX >= 128 Then Exit Sub
  AiStrike
  AiFwd
End Sub

' Level twelve: the last meeting.  While the sword is out he fights like
' anyone else.  Put it away and he puts his away too, and then walking into
' him is what ends it: the two become one and the strength of both is
' yours.  It cannot be done by force, only by refusing to fight.
Sub Shad12
  ' Waiting at the ceiling: nothing of him moves, not the sequence and not the
  ' fall, until the kid is left of x 150 - a little over half way across the
  ' room.  StepCharacter is what honours it; this is where it ends.
  If shadHold Then
    If kRec(1) >= 150 Then Exit Sub
    shadHold = 0
    lastWhat = "the shadow drops"
  End If
  ' AUTO.S FinalShad, ":cont lda CharSword / cmp #2 / bcs :fight" - his own
  ' sword is asked about before the kid's.
  '     :fight lda offguard / beq :1   ;has kid put up sword?
  '            lda refract / bne :1    ;yes--wait a moment--
  '            jmp DoDown              ;--then lower your guard
  '     :1 jmp EnGarde
  ' He lowers his guard by PRESSING DOWN, and the control machine then puts
  ' the sword up for him with the animation that goes with it.  The port took
  ' the sword out of his hand where he stood, with nothing to see, and did it
  ' whether or not the kid had put his own up first.
  '
  ' NOT REACHABLE TODAY, and worth knowing why: FinalShad's other branch is
  '     :hostile lda EnemyAlert / cmp #2 / bcc :2
  '              jsr getopdist / cmp #swordthres / bcs :2
  '              lda CharPosn / cmp #15 / bne rts / jmp DoEngarde ;draw on kid
  ' and this port answers the kid's drawn sword with GuardEnGarde instead,
  ' which is the routine for a character who already has his out.  So the
  ' shadow never draws on the twelfth level: he stands at pose 15 and is run
  ' through.  That is a separate finding and not one the review raises.
  If cSword = 2 Then
    If offGuard <> 0 And refract = 0 Then AiTurn : Exit Sub
    GuardEnGarde : Exit Sub
  End If
  If kRec(12) = 2 Then GuardEnGarde : Exit Sub
  If OpDistS() < 0 Then MergeShadow : Exit Sub
  If enemyAlert = 2 Then AiFwd
End Sub

' CTRL.S onground, the twelfth level.  Once the two are one, empty air on the
' top row of screen 2, and from column six of screen 13's top row, becomes
' floor as he comes to it.  That invisible bridge is the only way left from
' the room where they met to the last screen of the level, so without it the
' level cannot be finished however well it is played.  The block really is
' written, not merely treated as solid for a frame, which is what "creates
' floor on the fly" means: walk it once and it is there.
Sub CheckBridge
  Local INTEGER here
  If curLevel <> 12 Or mergeTimer >= 0 Then Exit Sub
  If cBlockY <> 0 Or cBlockX < 0 Or cBlockX >= COLS Then Exit Sub
  here = 0
  If cScrn = 2 Then here = 1
  If cScrn = 13 And cBlockX >= 6 Then here = 1
  If here = 0 Then Exit Sub
  If BType(cScrn, cBlockX) <> T_SPACE Then Exit Sub
  SetType cScrn, cBlockX, T_FLOOR
  BuildTypeGrid cScrn
  nBridge = nBridge + 1
  lastWhat = "the bridge holds"
End Sub

Sub MergeShadow
  CueSong 5
  mergeTimer = -1
  If maxKidStr < kMaxMaxStr Then maxKidStr = maxKidStr + 1
  kidStr = maxKidStr : chgKidStr = 0
  VanishShadow
  nMerges = nMerges + 1
  lastWhat = "the two become one"
End Sub

' Level five: the thief.  A recorded list of moves, each held until the
' frame it runs to, is replayed a frame at a time; he leaves at the left.
Sub Shad5
  Local INTEGER cmd
  If cScrn <> kFlaskScrn Then Exit Sub
  If playCount = 0 Then
    ' He waits at the gate until it has risen far enough to come through.
    If RdBlock(kFlaskScrn, 1, 0) = T_GATE Then
      If BSpec(tScrn, tBY * COLS + tBX) < 20 Then Exit Sub
    End If
    preRecPtr = 0
  End If
  cmd = PlayBack()
  Select Case cmd
    Case 1 : AiFwd
    Case 2 : AiBack
    Case 3 : AiBlock
    Case 4 : AiTurn
    Case 6 : AiStrike
  End Select
  If cX < 15 Then VanishShadow
End Sub

' One frame of the recorded list: pairs of a frame number and the move to
' hold until it.  The move is repeated until its frame arrives.
Function PlayBack() As INTEGER
  Local INTEGER at
  PlayBack = 0
  If playCount >= 254 Then Exit Function
  playCount = playCount + 1
  at = aShadProg5 + preRecPtr
  If playCount < blocks(at) Then
    If preRecPtr > 0 Then PlayBack = blocks(at - 1)
    Exit Function
  End If
  PlayBack = blocks(at + 1)
  preRecPtr = preRecPtr + 2
End Function

Sub VanishShadow
  gdPresent = 0 : oppStr = 0 : gRec(13) = 0
  lastWhat = "the shadow is gone"
End Sub

' Put the shadow on the screen from one of his three recorded positions.
Sub AddShadow(at As INTEGER)
  cPosn = blocks(at) : cX = blocks(at + 1) : cY = blocks(at + 2)
  cFace = blocks(at + 3) : cBlockX = Sgn8(blocks(at + 4)) : cBlockY = blocks(at + 5)
  cAction = blocks(at + 6)
  cSeq = seqTab(blocks(at + 7))
  cID = 1 : cSword = 0 : cLife = &HFF
  cFalling = 0 : stunned = 0 : cXVel = 0 : cYVel = 0
  guardProg = 3
  maxOppStr = kShadStr : oppStr = kShadStr : chgOppStr = 0
  alertGuard = 0 : refract = 0 : justBlocked = 0 : droppedOut = 0
  playCount = 0 : preRecPtr = 0
  ' AUTO.S FinalShad :hold.  On the twelfth level he is up at the ceiling when
  ' the room opens and stays there until the kid has come far enough in; the
  ' drop is a greeting, not something that happens while he is still at the
  ' door.  The port let him fall the moment the room was entered.
  shadHold = Choice(curLevel = 12, 1, 0)
  If HEADLESS = 0 Then GuardPalette 3
  SaveChar gRec()
  gdPresent = 1 : nShadows = nShadows + 1
End Sub

' Standing at ease: go en garde when the player is on top of you, or in
' sight once a sound has alerted you; turn if he is behind.
Sub GuardAlert
  Local INTEGER d, ok
  If oLife = 0 Then Exit Sub
  d = OpDist()
  ok = 0
  If oBlockY = cBlockY And d >= 248 Then
    ok = 1
  Else
    If alertGuard = 0 Then
      If d < 128 Then ok = 1 Else Exit Sub
    Else
      alertGuard = 0
      If d < 128 Then
        ok = 1
      ElseIf d >= 252 Then
        Exit Sub
      Else
        AiTurn : Exit Sub
      End If
    End If
  End If
  If enemyAlert = 0 Then Exit Sub
  AiEngarde
End Sub

' Sword drawn: keep your distance, close in, fight, or chase and drop down
' after him.
Sub GuardEnGarde
  Local INTEGER d
  If cPosn = 166 Or cPosn < 150 Then Exit Sub
  If enemyAlert < 2 Then
    If enemyAlert = 1 Then Exit Sub
    If droppedOut Then FollowKid Else AiDropGuard
    Exit Sub
  End If
  d = OpDist()
  If d < 128 And d >= 12 Then
    If oPosn >= 102 And oPosn < 118 Then
      If oAction = 5 Then Exit Sub
    End If
  End If
  If d >= TOOFAR Then
    If refract Then Exit Sub
    If cFace <> oFace Then
      If oPosn >= 7 And oPosn < 15 Then
        If d < RUNTHRES Then AiStrike
        Exit Sub
      End If
      If oPosn >= 34 And oPosn < 44 Then
        If d < JUMPTHRES Then AiStrike
        Exit Sub
      End If
    End If
    If FloorAhead(1) And FloorAhead(2) Then AiFwd Else AiBack
    Exit Sub
  End If
  If cSword = 2 Then
    If d < TOOCLOSE Then
      If cFace = oFace Then AiBack Else AiFwd
      Exit Sub
    End If
  Else
    If d < OFFGUARDTHRES Then
      If cFace = oFace Then AiBack Else AiFwd
      Exit Sub
    End If
  End If
  InRange
End Sub

' Is there floor n blocks ahead of him?
Function FloorAhead(n As INTEGER) As INTEGER
  Local INTEGER bx, tt
  If (cFace And &H80) Then bx = cBlockX - n Else bx = cBlockX + n
  tt = RdBlock(cScrn, bx, cBlockY)
  If noFloor(tt) Then FloorAhead = 0 Else FloorAhead = 1
End Function

Sub FollowKid
  Local INTEGER tt, tb
  If oAction = 2 Or oAction = 6 Then Exit Sub
  If (cFace And &H80) Then tb = cBlockX - 1 Else tb = cBlockX + 1
  ' A gate that has risen is not in his way.  Asking the raw block type made a
  ' guard turn back at an open gate instead of following through it, which is
  ' the same mistake the screen-crossing test made on the way into room 8.
  tt = RdBlock(cScrn, tb, cBlockY)
  If tt = T_GATE Then
    If GateOpen(cScrn, tb, cBlockY) Then tt = 0
  ElseIf tt = T_SLICER Then
    If SlicerShut(cScrn, tb, cBlockY) = 0 Then tt = 0
  End If
  If Barrier(tt) <> 0 Then droppedOut = 0 : AiBack : Exit Sub
  If noFloor(tt) = 0 Then AiFwd : Exit Sub
  tt = RdBlock(cScrn, tb, cBlockY + 1)
  If tt = T_GATE Then
    If GateOpen(cScrn, tb, cBlockY + 1) Then tt = 0
  End If
  If tt = T_SPIKES Or tt = T_LOOSE Or Barrier(tt) <> 0 Or noFloor(tt) Then droppedOut = 0 : AiBack : Exit Sub
  If cBlockY + 1 <> oBlockY Then droppedOut = 0 : AiBack : Exit Sub
  AiFwd
End Sub

Sub InRange
  Local INTEGER d
  If oSword = 2 Then GenFight : Exit Sub
  If refract Then Exit Sub
  d = OpDist()
  If d < STRIKERANGE2 Then AiStrike Else AiFwd
End Sub

Sub GenFight
  Local INTEGER d
  d = OpDist()
  If d >= BLOCKTHRES1 And d < BLOCKRANGE2 Then
    MaybeBlock
    If refract Then Exit Sub
    d = OpDist()
    If d >= STRIKERANGE1 And d < STRIKERANGE2 Then MaybeStrike : Exit Sub
  End If
  MaybeAdvance
End Sub

Sub MaybeAdvance
  If guardProg <> 0 And gdTimer <> 0 Then Exit Sub
  If Rnd8() < blocks(aAdvProb + guardProg) Then AiFwd
End Sub

Sub MaybeBlock
  If oPosn <> 152 And oPosn <> 153 And oPosn <> 162 Then Exit Sub
  If justBlocked Then
    If Rnd8() < blocks(aImpBlockProb + guardProg) Then AiBlock
  Else
    If Rnd8() < blocks(aBlockProb + guardProg) Then AiBlock
  End If
End Sub

Sub MaybeStrike
  If oPosn = 169 Or oPosn = 151 Then Exit Sub
  If cPosn = 161 Or cPosn = 150 Then
    If Rnd8() < blocks(aRestrikeProb + guardProg) Then AiStrike
  Else
    If Rnd8() < blocks(aStrikeProb + guardProg) Then AiStrike
  End If
End Sub

' GUARDCTRL: a guard standing at ease reacts to his own mind's 'engarde' or
' 'turn' presses.
Function GuardCtrl() As INTEGER
  GuardCtrl = 0
  If cPosn <> 166 Then Exit Function
  If clrD >= 0 Then Exit Function
  If clrF < 0 Then
    GuardCtrl = DoEngarde()
  Else
    clrD = 1 : GuardCtrl = SEQ_ALERTTURN
  End If
End Function

Function DoEngarde() As INTEGER
  ClearInput : clrF = 1 : clrBtn = 1
  cSword = 2
  nEngarde = nEngarde + 1
  If cID = 0 Then
    offGuard = 0 : DoEngarde = SEQ_ENGARDE
  Else
    DoEngarde = SEQ_GUARDENGARDE
  End If
  lastWhat = "en garde"
End Function

' FIGHTCTRL: sword drawn, on the ground.  Sheathe when the opponent is gone
' or far; otherwise strike on the button, block on up, advance and retreat
' on the stick, and drop guard on down from the ready poses.
Function FightCtrl() As INTEGER
  Local INTEGER d, under, dropgd
  FightCtrl = 0
  dropgd = 0
  under = RdBlock(cScrn, cBlockX, cBlockY)
  If under <> T_LOOSE And enemyAlert < 2 Then
    dropgd = 1
  Else
    d = OpDist()
    If d >= SWORDTHRES And d < 128 Then dropgd = 1
    If d >= 128 And d < 252 Then FightCtrl = SEQ_TURNENGARDE : Exit Function
  End If
  If dropgd Then
    If cID = 0 Then
      heroic = 0
      If cPosn <> 171 Then Exit Function
      cSword = 0
      FightCtrl = SEQ_RESHEATHE
      Exit Function
    End If
  End If
  ' on alert
  If cPosn = 161 Then
    If clrBtn >= 0 Then FightCtrl = SEQ_RETREAT : Exit Function
  End If
  If clrBtn < 0 Then
    If cID = 0 Then gdTimer = GDPATIENCE
    FightCtrl = DoStrike()
    If clrBtn = 1 Then Exit Function
  End If
  If clrD < 0 Then
    If cPosn = 158 Or cPosn = 170 Or cPosn = 171 Then
      clrD = 1 : cSword = 0
      If cID = 0 Then
        offGuard = 1 : refract = GRACEPERIOD : heroic = 0
        FightCtrl = SEQ_FASTSHEATHE
      Else
        FightCtrl = SEQ_GOALERTSTAND
      End If
    End If
    Exit Function
  End If
  If clrU < 0 Then FightCtrl = DoBlock() : Exit Function
  If clrF < 0 Then FightCtrl = DoAdvance() : Exit Function
  If clrB < 0 Then FightCtrl = DoRetreat()
End Function

Function DoStrike() As INTEGER
  DoStrike = 0
  If cPosn = 157 Or cPosn = 158 Or cPosn = 170 Or cPosn = 171 Or cPosn = 165 Then
    If cID = 0 Then DoStrike = SEQ_FASTSTRIKE Else DoStrike = SEQ_STRIKE
  ElseIf cPosn = 150 Or cPosn = 161 Then
    DoStrike = SEQ_BLOCKTOSTRIKE
  End If
  If DoStrike Then clrBtn = 1 : nStrikes = nStrikes + 1 : lastWhat = "strike"
End Function

Function DoBlock() As INTEGER
  Local INTEGER ok
  DoBlock = 0
  If cPosn = 167 Then clrU = 1 : DoBlock = SEQ_STRIKEBLOCK : Exit Function
  If cPosn <> 158 And cPosn <> 170 And cPosn <> 171 And cPosn <> 168 And cPosn <> 165 Then Exit Function
  If OpDist() >= BLOCKTHRES Then
    ' too far: the player wastes the block, a guard backs off instead
    If cID <> 0 Then DoBlock = DoRetreat() : Exit Function
    clrU = 1 : DoBlock = SEQ_READYBLOCK : nBlocks = nBlocks + 1
    Exit Function
  End If
  ok = 0
  If cID <> 0 Then
    If oPosn = 152 Then ok = 1
  Else
    If oPosn = 168 Then Exit Function
    If oPosn = 151 Or oPosn = 152 Or oPosn = 162 Then ok = 1
    If oPosn = 153 Then ok = 2
    If ok = 0 Then clrU = 1 : DoBlock = SEQ_READYBLOCK : nBlocks = nBlocks + 1 : Exit Function
  End If
  If ok = 0 Then Exit Function
  clrU = 1 : nBlocks = nBlocks + 1 : lastWhat = "block"
  If ok = 2 Then
    cSeq = seqTab(SEQ_READYBLOCK)
    If Advance() = 0 Then lastWhat = "STALLED"
    DoBlock = -1
  Else
    DoBlock = SEQ_READYBLOCK
  End If
End Function

Function DoRetreat() As INTEGER
  DoRetreat = 0
  If cPosn = 158 Or cPosn = 170 Or cPosn = 171 Then clrB = 1 : DoRetreat = SEQ_RETREAT
End Function

Function DoAdvance() As INTEGER
  DoAdvance = 0
  If cPosn = 158 Or cPosn = 170 Or cPosn = 171 Then
    clrF = 1
    If cID = 0 Then DoAdvance = SEQ_FASTADVANCE Else DoAdvance = SEQ_ADVANCE
  End If
End Function

' CHECKSTRIKE: for each character in turn, is his blade landing?  A blade
' at full stretch within range stabs, unless the other is blocking, which
' turns the blow aside and marks the striker as blocked.
Sub CheckStrike
  If gdPresent = 0 Then Exit Sub
  If kRec(0) = 0 Or (kRec(0) >= 219 And kRec(0) < 229) Then Exit Sub
  LoadShadWOp : TestStrike : SaveChar gRec() : kRec(0) = oPosn : kRec(6) = oAction
  LoadKidWOp : TestStrike : SaveChar kRec() : gRec(0) = oPosn : gRec(6) = oAction
End Sub

Sub TestStrike
  Local INTEGER d
  If cSword <> 2 Then Exit Sub
  If cBlockY <> oBlockY Then Exit Sub
  If cPosn <> 153 And cPosn <> 154 Then Exit Sub
  d = OpDist()
  If d < BLOCKRANGE2 Then
    If oPosn = 161 Or oPosn = 150 Then
      oPosn = 161
      If cID <> 0 Then justBlocked = BLOCKTIME
      cSeq = seqTab(SEQ_BLOCKEDSTRIKE)
      If Advance() = 0 Then lastWhat = "STALLED"
      lastWhat = "blocked"
      Exit Sub
    End If
  End If
  If cPosn <> 154 Then Exit Sub
  If oSword >= 2 Then
    If d < STRIKERANGE1 Then Exit Sub
  Else
    If d < OFFGUARDTHRES Then Exit Sub
  End If
  If d >= STRIKERANGE2 Then Exit Sub
  oAction = 99
End Sub

' The stabs land, the guard's first, and he needs a moment before the next.
Sub CheckStab
  If gdPresent = 0 Then Exit Sub
  If gRec(6) = 99 Then
    If kRec(6) = 99 Then kRec(6) = 1
    LoadShadWOp : StabChar : SaveChar gRec()
    refract = blocks(aRefracTimer + guardProg)
  End If
  If kRec(6) = 99 Then
    LoadKidWOp : StabChar : SaveChar kRec()
  End If
End Sub

Sub StabChar
  Local INTEGER dist, behind
  If cLife = 0 Then Exit Sub
  nStabs = nStabs + 1
  If cID = 4 Then
    cSeq = seqTab(SEQ_STABBED) : lastWhat = "rattled"   ' a skeleton has no life to lose
    cAction = 1 : cY = floory(cBlockY + 1) : cYVel = 0
    If Advance() = 0 Then lastWhat = "STALLED"
    Exit Sub
  End If
  ' MISC.S ":DL - stabbed when defenseless" takes all hundred points and then
  ' jumps to :killed, which is the very code an armed man reaches when his
  ' last point goes - edge test and all.  The port sent a defenceless man
  ' straight to stabkill, so being run through at the lip of a drop stood him
  ' up and killed him where he was instead of putting him over it.
  If cSword = 2 Then
    If DecStr(1) Then
      cSeq = seqTab(SEQ_STABBED) : lastWhat = "stabbed"
      cAction = 1
      cY = floory(cBlockY + 1) : cYVel = 0
      If Advance() = 0 Then lastWhat = "STALLED"
      Exit Sub
    End If
    lastWhat = "killed"
  Else
    If DecStr(100) = 0 Then cLife = cLife
    lastWhat = "run through"
  End If
  ' :killed - if he goes down at an edge, he is knocked off it.
  If (cFace And &H80) Then behind = cBlockX + 1 Else behind = cBlockX - 1
  dist = GetDist()
  If BlockAt(cScrn, behind, cBlockY) = T_SPACE And dist >= 4 Then
    MoveFwd dist - 14
    cBlockY = cBlockY + 1
    cSeq = seqTab(SEQ_FIGHTFALL) : cFalling = 1 : cAction = 4
    lastWhat = lastWhat + ", knocked off"
    If Advance() = 0 Then lastWhat = "STALLED"
    Exit Sub
  End If
  cSeq = seqTab(SEQ_STABKILL)
  cAction = 1
  cY = floory(cBlockY + 1) : cYVel = 0
  If Advance() = 0 Then lastWhat = "STALLED"
End Sub

' ADDGUARD: the guard of this screen, if there is one and he is not dead.
Sub AddGuard(scrn As INTEGER)
  Local INTEGER b, s
  s = scrn
  gdPresent = 0 : enemyAlert = 0
  ' The shadow comes before any ordinary guard, and only in his own places.
  If curLevel = 6 And s = 1 Then AddShadow aShad6a : CueSong 3 : Exit Sub
  If curLevel = 5 And s = kFlaskScrn Then
    If RdBlock(s, kFlaskX, kFlasky) = T_FLASK Then AddShadow aShad5 : Exit Sub
  End If
  ' AUTO.S ADDGUARD puts the shadow on the sword's screen once the sword has
  ' gone, and not once the two have become one.  Without the second test he
  ' was made again every time the room was entered, so the meeting could be
  ' had over and over and the level was never done with him.
  If curLevel = 12 And s = kSwordScrn And mergeTimer >= 0 Then
    If RdBlock(s, kSwordX, kSwordY) <> T_SWORD Then AddShadow aShad12 : Exit Sub
  End If
  If s < 1 Or s > 24 Then Exit Sub
  b = gdBlock(s)
  If b >= 30 Then Exit Sub
  cPosn = 0 : cBlockX = b Mod COLS : cBlockY = b \ COLS
  cY = floory(cBlockY + 1)
  cX = gdX(s) : RereadBlocks
  cFace = gdFace(s) : cScrn = s
  ' AUTO.S AddNormalGd: "lda level / cmp #3 / bne :3 / lda #4 ;skel".  The
  ' third level's guard is the skeleton, and he comes up with his sword out in
  ' landengarde rather than the alert stand.  The port made him an ordinary
  ' guard, so once that room had been left and revisited the skeleton was a
  ' mortal man.
  If curLevel = 3 Then cID = 4 Else cID = 2
  cFalling = 0 : stunned = 0 : cXVel = 0 : cYVel = 0 : cAction = 1
  If gdSeq(s) = 0 Then
    If cID = 4 Then
      cSword = 2 : cSeq = seqTab(SEQ_LANDENGARDE)
    Else
      cSword = 0 : cSeq = seqTab(SEQ_ALERTSTAND)
    End If
  Else
    cSeq = gdSeq(s) : cSword = 2
  End If
  If Advance() = 0 Then lastWhat = "STALLED"
  If cPosn = 185 Or cPosn = 177 Or cPosn = 178 Then
    cLife = 0 : oppStr = 0
  Else
    cLife = &HFF
    guardProg = gdProg(s) : If guardProg >= 12 Then guardProg = 3
    maxOppStr = blocks(aBasicStrength + curLevel) + blocks(aExtraStrength + guardProg)
    oppStr = maxOppStr
  End If
  alertGuard = 0 : refract = 0 : justBlocked = 0 : droppedOut = 0 : chgOppStr = 0
  guardColor = blocks(aBasicColor + curLevel) Xor blocks(aSpecialColor + guardProg)
  If HEADLESS = 0 Then GuardPalette guardColor
  SaveChar gRec()
  gdPresent = 1
End Sub

' KEEPTIME: one frame of the clock.  It does not run in the demo level or
' while he is dead, and it announces the time as each threshold is passed,
' but only when nothing else is on screen.
Sub KeepTime
  If curLevel = 0 Or kRec(13) = 0 Then Exit Sub
  ' TOPCTRL.S NextFrame:
  '     lda level / cmp #14 / bcs :stopped
  '                 cmp #13 / bcc :ticking
  '                 lda exitopen / bne :stopped
  '     :ticking jsr keeptime
  ' From the fourteenth level the clock has stopped for good, and on the
  ' thirteenth it stops the moment the vizier is dead.  The port had the
  ' second of those and kept counting through the tower.
  If curLevel >= 14 Then Exit Sub
  If curLevel = 13 And exitOpen Then Exit Sub
  frameCount = frameCount + 1
  GetMinLeft
  If nextTimeMsg > 17 Then Exit Sub
  If minLeft > timeMsg(nextTimeMsg) Then Exit Sub
  If msgTimer > 0 Then Exit Sub
  nextTimeMsg = nextTimeMsg + 1
  timeRequest = 2
End Sub

' How much is left.  The original counts frames in steps of a game minute;
' this divides, which gives the same reading at every whole minute.  The
' seconds only matter in the last minute, which is when it shows them.
Sub GetMinLeft
  Local INTEGER togo
  togo = GAMEMINUTES * FRAMESPERMIN - frameCount
  If togo < 0 Then togo = 0
  minLeft = (togo + FRAMESPERMIN - 1) \ FRAMESPERMIN
  secLeft = (togo * 60) \ FRAMESPERMIN
  If togo = 0 Then
    outOfTime = 1
    ' "lda level / cmp #13 / bcs :safe" - running out of time loses on levels
    ' one to twelve.  From the thirteenth on he has his one chance to finish
    ' and the clock cannot take it away from him.  The port ended the game at
    ' zero whatever level he was on.
    If curLevel < 13 And gameOver = 0 Then
      gameOver = 1
      message = MSG_TIME : msgText = "TIME UP" : msgTimer = 200
    End If
  End If
End Sub

' SHOWTIME: turn a request into the message itself, once the screen is free.
Sub ShowTime
  If timeRequest = 0 Then Exit Sub
  If kRec(13) = 0 Then Exit Sub
  If msgTimer > 0 Then Exit Sub
  GetMinLeft
  If minLeft >= 2 Then
    msgText = Str$(minLeft) + " MINUTES LEFT"
  ElseIf secLeft > 0 Then
    msgText = Str$(secLeft) + " SECONDS LEFT"
  Else
    msgText = "TIME UP"
  End If
  message = MSG_TIME : msgTimer = TIMEMSGTIMER
  timeRequest = 0 : nTimeMsgs = nTimeMsgs + 1
End Sub

' BONESRISE: level 3's skeleton.  With the exit open, stepping up to the
' bones on the first screen brings them to their feet, sword in hand.
' TOPCTRL.S misctimers, the eighth level.  Screen 16's way out is a gate the
' kid cannot reach the plate for: he is shut in.  A hundred and fifty frames
' after the level's exit opens, while he is in that room, a mouse is sent in;
' it runs to the plate, presses it and leaves, and that is how he gets out.
' The port had no timer, no mouse and no id for one, so the room was a trap.
Sub MiscTimers
  If curLevel <> 8 Or visScrn <> 16 Or exitOpen = 0 Then Exit Sub
  mouseTimer = mouseTimer + 1
  If mouseTimer = MOUSE_WAIT Then MouseRescue
End Sub

' MISC.S MOUSERESCUE.  He takes the one slot the game keeps for somebody
' other than the player, as he does in the original, and he is built the way
' the skeleton above is built.
Sub MouseRescue
  If gdPresent Then Exit Sub
  SaveChar kRec()
  cScrn = visScrn : cBlockY = 0 : cY = floory(1)
  cX = MOUSE_START : RereadBlocks
  cFace = &HFF : cID = MOUSE_ID : cSword = 0 : cAction = 1
  cFalling = 0 : stunned = 0 : cXVel = 0 : cYVel = 0
  cPosn = 0 : cSeq = seqTab(SEQ_MOUSERUN) : mouseTurned = 0
  If Advance() = 0 Then lastWhat = "STALLED"
  guardProg = 3 : cLife = &HFF
  maxOppStr = 1 : oppStr = 1 : chgOppStr = 0
  alertGuard = 0 : refract = 0 : justBlocked = 0 : droppedOut = 0
  SaveChar gRec()
  guardColor = blocks(aBasicColor + curLevel) Xor blocks(aSpecialColor + guardProg)
  If HEADLESS = 0 Then GuardPalette guardColor
  gdPresent = 1 : nMice = nMice + 1
  gdBlock(cScrn) = 255
  LoadKidWOp
  lastWhat = "a mouse comes in"
End Sub

' AUTO.S MouseProg: in at the right, round again at the plate, and out.  The
' turn is the sequence's own aboutface, so nothing here touches his facing.
Sub MouseProg
  If (cFace And &H80) Then
    ' Once, and only once: put back to the head of the turn every frame he
    ' stood on the plate for ever playing its first pose.
    If cX <= MOUSE_TURN And mouseTurned = 0 Then
      mouseTurned = 1
      cSeq = seqTab(SEQ_MOUSETURN)
    End If
    Exit Sub
  End If
  If cX >= MOUSE_START Then
    gdPresent = 0 : oppStr = 0 : gRec(13) = 0
    lastWhat = "the mouse is gone"
  End If
End Sub

Sub BonesRise
  Local INTEGER tt
  If curLevel <> 3 Or gdPresent Or visScrn <> 1 Or exitOpen = 0 Then Exit Sub
  If kRec(4) <> 2 And kRec(4) <> 3 Then Exit Sub
  tt = BType(1, 15)
  SetType 1, 15, T_FLOOR
  If tt <> 21 Then Exit Sub
  SaveChar kRec()
  cScrn = 1 : cBlockY = 1 : cY = floory(2) : cBlockX = 5
  cX = blocks(gBlockEdge + 5 + 5) + 14
  cFace = &HFF : cID = 4 : cSword = 2 : cAction = 1
  cFalling = 0 : stunned = 0 : cXVel = 0 : cYVel = 0
  cSeq = seqTab(SEQ_ARISE)
  If Advance() = 0 Then lastWhat = "STALLED"
  guardProg = SKELPROG : cLife = &HFF
  ' MISC.S BONESRISE sets OppStrength to 3 and leaves MaxOppStrength ALONE, so
  ' in the original the skeleton's meter is three of whatever the last guard
  ' the kid met was worth.  Not followed, deliberately: maxOppStr is also what
  ' caps a guard's strength when he is restored on re-entering his room, which
  ' is this port's own machinery and not the original's, and leaving it at a
  ' previous guard's figure would cap the skeleton at that instead of at his
  ' own three.  He is invincible anyway - STABCHAR spares CharID 4 - so the
  ' only thing the original's leftover changes is how wide his meter is drawn.
  maxOppStr = 3 : oppStr = 3 : chgOppStr = 0
  alertGuard = 0 : refract = 0 : justBlocked = 0 : droppedOut = 0
  SaveChar gRec()
  If HEADLESS = 0 Then GuardPalette 2
  gdPresent = 1 : nBones = nBones + 1
  gdBlock(1) = 255
  LoadKidWOp
End Sub

' CHECKGATE: a gate coming down on him shoves him aside.  It only reaches
' him when he is standing, turning or crouched, and only while the gate is
' still low enough to be in his way.
Sub CheckGate
  Local INTEGER gx, off
  gx = -1
  If cAction <> 7 And cPosn <> 15 Then
    If cPosn < 108 Or cPosn >= 111 Then gateLast = 0 : Exit Sub
  End If
  If RdBlock(cScrn, cBlockX, cBlockY) = T_GATE Then
    gx = cBlockX
  ElseIf RdBlock(cScrn, cBlockX - 1, cBlockY) = T_GATE Then
    ' INTERPRETATION.  COLL.S wants his collision edges to have overlapped
    ' both edges of the bars, which it gets from CDthisframe and CDlastframe -
    ' per-column collision maps the port does not keep.  What it can ask is
    ' how far into his own block he is standing: a man on the far side of the
    ' boundary is not touching the bars at all.  Without this, standing beside
    ' a shut gate on its right shoved him five pixels a frame with a smack on
    ' the wall every frame, for as long as he stood there.
    off = BaseX() - (14 * (cBlockX + 4) + BLOCKLO)
    If off > kGateMargin Then gateLast = 0 : Exit Sub
    gx = cBlockX - 1
  End If
  If gx < 0 Then gateLast = 0 : Exit Sub
  If GateOpen(cScrn, gx, cBlockY) Then gateLast = 0 : Exit Sub
  ' "lda CDthisframe,x / and CDlastframe,x / cmp #$ff" - both frames, or not
  ' yet.
  If gateLast = 0 Then gateLast = 1 : Exit Sub
  AddSound 13
  ' Away from the gate: out of its own column to the left, or clear of one
  ' standing to his left.
  If gx = cBlockX Then cX = (cX - 5) And &HFF Else cX = (cX + 5) And &HFF
  RereadBlocks
  nGateKnocks = nGateKnocks + 1
  lastWhat = "shoved by the gate"
End Sub

' ENEMYCOLL: with a sword drawn he can be pushed into a wall, because
' retreating does not test the ground behind him.  Finding himself inside
' one, he is put back outside it and bumps.
' COLL.S ENEMYCOLL takes exactly three things for something to back into: a
' solid block, a panel WITH floor, and a gate hanging low enough to bar him.
' The port asked Barrier(), which also says yes to a panel without floor, to
' a mirror and to a slicer - none of which ENEMYCOLL so much as looks at, so
' a fighter was stopped by things he should have been able to back through.
Function EnemyBarrier(bx As INTEGER) As INTEGER
  Local INTEGER t
  EnemyBarrier = 0
  t = RdBlock(cScrn, bx, cBlockY)
  If t = T_BLOCK Or t = 7 Then EnemyBarrier = 1 : Exit Function   ' 7 = panelwif
  If t = T_GATE Then
    If GateOpen(cScrn, bx, cBlockY) = 0 Then EnemyBarrier = 1
  End If
End Function

Sub EnemyColl
  Local INTEGER lo, bx
  If cSword <> 2 Or cAction <> 1 Or cLife = 0 Then Exit Sub
  bx = cBlockX
  If EnemyBarrier(bx) = 0 Then
    ' "* If facing R, check block behind too" - and only then, because the
    ' routine returns at "lda CharFace / bmi rts" for a left-facer.  The port
    ' never looked behind at all, so a right-facing fighter backed clean
    ' through the wall on his left.
    If (cFace And &H80) Then Exit Sub
    bx = cBlockX - 1
    If EnemyBarrier(bx) = 0 Then Exit Sub
  End If
  ' ":collide - put him right at edge".  DBarr2 gives the distance to the
  ' barrier, negative because it is behind him, and :collide negates it and
  ' hands it to addcharx - so he comes out of it the way he FACES.  Backing up
  ' moves him against his facing, so that is the way out.  The port had the
  ' two the wrong way round and pushed him further in, which is why he could
  ' stand inside a wall shuffling between two blocks for ever.
  '
  ' NO SCENARIO PINS THIS.  ENEMYCOLL wants action 1 - the en-garde advance
  ' and retreat - and in every room that can be built out of the shipped
  ' levels HitBarrier reaches him first and puts him in action 5, where this
  ' routine will not look at him.  Written from the source and left as read.
  lo = 14 * (bx + 4) + BLOCKLO
  If (cFace And &H80) Then cX = lo - 1 Else cX = lo + 14
  RereadBlocks
  cSeq = seqTab(SEQ_BUMPENGBACK) : cAction = 5
  If Advance() = 0 Then lastWhat = "STALLED"
  lastWhat = "backed into the wall"
  nBumps = nBumps + 1
End Sub

' FIRSTGUARD: with his sword sheathed he cannot walk through a guard who
' has his out: running into him is a bump.
Sub FirstGuard
  Local INTEGER d
  If gdPresent = 0 Or enemyAlert < 2 Then Exit Sub
  If cSword <> 0 Or oSword = 0 Or oAction >= 2 Then Exit Sub
  If cFace = oFace Then Exit Sub
  d = OpDist()
  If d < 241 Then Exit Sub                   ' only when overlapping him
  cY = floory(cBlockY + 1)
  cSeq = seqTab(SEQ_BUMP) : cAction = 5
  If Advance() = 0 Then lastWhat = "STALLED"
  lastWhat = "bumps into the guard" : nBumps = nBumps + 1
End Sub

' The guard's colours: the common guard and the special one wear different
' tunics, so the opponent's three palette entries are set for whoever is
' on the screen.
Sub GuardPalette(c As INTEGER)
  If c = 3 Then
    ' The shadow is the player's own colours, darkened.
    Map(8) = &H12283F : Map(9) = &H274662 : Map(10) = &H47647A
  ElseIf c = 2 Then
    Map(8) = &H807870 : Map(9) = &HC0B8A8 : Map(10) = &HF0ECE0
  ElseIf c = 0 Then
    Map(8) = &H5A2A7A : Map(9) = &H9050B0 : Map(10) = &HC090E0
  Else
    Map(8) = palette(8) : Map(9) = palette(9) : Map(10) = palette(10)
  End If
  Map Set
End Sub

' CUTGUARD: as the player leaves a screen, a guard with his sword drawn who
' is close behind follows him through, unless the new screen has a fresh
' guard of its own.  Returns 1 if the guard came along.  Otherwise he is
' left where he stood for the next visit.
Function CutGuard(news As INTEGER) As INTEGER
  Local INTEGER go
  CutGuard = 0
  If gdPresent = 0 Then Exit Function
  go = 0
  If cutDir >= 0 And gRec(13) <> 0 And gRec(12) = 2 Then
    If gdBlock(news) >= 30 Or gdSeq(news) <> 0 Then
      Select Case cutDir
        Case 0 : If gRec(1) < 256 - SCRNW - 25 Then go = 1
        Case 1 : If gRec(1) >= SCRNW + 25 Then go = 1
        Case 2 : If gRec(5) < 0 Then go = 1
        Case 3 : If gRec(5) >= ROWS Then go = 1
      End Select
    End If
  End If
  If go = 0 Then UpdateGuard : Exit Function
  gdBlock(news) = 255 : gdBlock(gRec(10)) = 255
  gRec(10) = news
  Select Case cutDir
    Case 0 : gRec(1) = gRec(1) + SCRNW : gRec(4) = gRec(4) + COLS
    Case 1 : gRec(1) = gRec(1) - SCRNW : gRec(4) = gRec(4) - COLS
    Case 2 : gRec(5) = gRec(5) + ROWS : gRec(2) = (gRec(2) + 3 * 63) And &HFF
    Case 3 : gRec(5) = gRec(5) - ROWS : gRec(2) = (gRec(2) - 3 * 63) And &HFF
  End Select
  nTransfers = nTransfers + 1
  CutGuard = 1
End Function

' Leaving a screen: the guard stays where he stood, alive or dead, for the
' next visit.
Sub UpdateGuard
  Local INTEGER s
  If gdPresent = 0 Then Exit Sub
  ' AUTO.S updateguard: "lda ShadID / cmp #1 / beq ]rts ;not for shadman /
  ' cmp #24 / beq ]rts ;or mouse".  Neither is kept for the next visit.  The
  ' port saved the shadow, so an ordinary guard turned up standing where the
  ' thief or the mirror shadow had been left.
  If gRec(11) = 1 Or gRec(11) = MOUSE_ID Then gdPresent = 0 : oppStr = 0 : Exit Sub
  s = gRec(10)
  ' "lda #0 ;arbitrary--ADDGUARD will reconstruct CharBlockX from CharX".
  ' Only the row is kept.  The port stored the column too, and a guard's
  ' column can be -2 to 11 because his x is clamped to the screen, so one left
  ' near an edge came back on the wrong row entirely.
  gdBlock(s) = gRec(5) * COLS
  gdX(s) = gRec(1) : gdFace(s) = gRec(3) : gdProg(s) = guardProg
  If gRec(13) = 0 Then gdSeq(s) = 0 Else gdSeq(s) = gRec(9)
  gdPresent = 0 : oppStr = 0
End Sub

' The level's guard table, from its start block: block, facing, skill, and
' the start x the original computes from the block's edge.
Sub InitGuards
  Local INTEGER s, b
  For s = 1 To 24
    b = level(OFF_INFO + 71 + s - 1)
    gdBlock(s) = b
    If b < 30 Then gdX(s) = blocks(gBlockEdge + (b Mod COLS) + 5) + 14 Else gdX(s) = 0
    gdFace(s) = level(OFF_INFO + 95 + s - 1)
    gdProg(s) = level(OFF_INFO + 167 + s - 1)
    gdSeq(s) = 0
  Next s
  gdPresent = 0
End Sub

' DOSTEPFWD: one careful step, up to whatever is ahead, which is the near
' edge of the block he stands in.  Already on that edge, the first press
' tests the ground with a foot and the second commits, off the edge or
' through into the next block.  A barrier is stepped up to without the test.
' COLL.S GETFWDDIST: how far a careful step goes, and what is in the way.
' Clear floor ahead is a full natural step of eleven; space or a loose board
' is a step to the edge of his own block, and then the foot test; a plate, the
' sword or a flask is a step to the edge with no foot test; a barrier is a
' step up to the barrier.  The port always stepped to the edge of its own
' block whatever was next, so on open floor every block boundary produced a
' foot test and no step was ever the right length.
'
' fwdKind comes back as 0 for the edge, 1 for a barrier, 2 for clear - the
' original's X register - and fwdType as the block it looked at.
Function GetFwdDist() As INTEGER
  Local INTEGER bx
  If (cFace And &H80) Then bx = cBlockX - 1 Else bx = cBlockX + 1
  fwdType = RdBlock(cScrn, bx, cBlockY)
  If fwdType = T_GATE Then
    If GateOpen(cScrn, bx, cBlockY) Then fwdType = T_SPACE
  ElseIf fwdType = T_SLICER Then
    If SlicerShut(cScrn, bx, cBlockY) = 0 Then fwdType = T_SPACE
  End If
  If Barrier(fwdType) <> 0 Then
    ' INTERPRETATION: the original measures to the barrier's own edge from the
    ' image (DBarr); the near edge of his block is as close as this gets.
    fwdKind = 1 : GetFwdDist = GetDist() : Exit Function
  End If
  If noFloor(fwdType) Or fwdType = T_LOOSE Then
    fwdKind = 0 : GetFwdDist = GetDist() : Exit Function
  End If
  If fwdType = T_PLATE Or fwdType = T_DPLATE Or fwdType = T_UPLATE Then
    fwdKind = 2 : GetFwdDist = GetDist() : Exit Function
  End If
  If fwdType = T_SWORD Or fwdType = T_FLASK Then
    fwdKind = 2 : GetFwdDist = GetDist() : Exit Function
  End If
  fwdKind = 2 : GetFwdDist = 11
End Function

Function DoStepFwd() As INTEGER
  Local INTEGER d
  clrF = 1 : clrBtn = 1
  nSteps = nSteps + 1
  d = GetFwdDist()
  If d <> 0 Then
    cRepeat = d
    DoStepFwd = SEQ_STEP1 - 1 + d
    lastWhat = "step " + Str$(d)
    Exit Function
  End If
  If fwdKind <> 1 And cRepeat <> 0 Then
    cRepeat = 0
    DoStepFwd = SEQ_TESTFOOT
    lastWhat = "tests the ground"
    Exit Function
  End If
  cRepeat = 11
  DoStepFwd = SEQ_STEP1 - 1 + 11
  lastWhat = "steps through"
End Function

' CTRL.S DoStartrun: within eight pixels of a solid barrier he steps instead
' of running, so that he walks up to a wall rather than slamming into it.  A
' slicer is excepted - running at one is how you get past it - and an open
' gate is not a barrier by the time the question is asked.
Function DoStartrun() As INTEGER
  DoStartrun = SEQ_STARTRUN
  If GetFwdDist() >= 8 Then Exit Function
  If fwdKind <> 1 Or fwdType = T_SLICER Then Exit Function
  If clrF >= 0 Then DoStartrun = 0 : Exit Function
  DoStartrun = DoStepFwd()
End Function

' How far he is from the front edge of the block he is in, 0 to 13.
' GETDIST: how far from his base X to the end of the block that point is in.
Function GetDist() As INTEGER
  Local INTEGER lo, bx, b
  bx = BaseX()
  If bx < BLOCKLO Then b = -4 Else b = (bx - BLOCKLO) \ 14 - 4
  lo = 14 * (b + 4) + BLOCKLO
  If (cFace And &H80) Then GetDist = bx - lo Else GetDist = lo + 13 - bx
End Function

' Move him n units the way he faces (negative is back).
Sub MoveFwd(n As INTEGER)
  If (cFace And &H80) Then cX = (cX - n) And &HFF Else cX = (cX + n) And &HFF
  RereadBlocks
End Sub

' DOJUMPUP: up alone while standing.  If there is a ledge above and in front
' he jumps and catches it; if there would be one from a block back, he is
' shifted back first (or jumps from the edge when there is no floor behind);
' otherwise a plain jump, touching a ceiling if there is one.
' CTRL.S standing :down.  With a cliff in front of him and his base within
' three pixels of the edge, down steps off it.  With a cliff behind and his
' base eight or more pixels into the block - which is to say within six of
' the edge behind him - down lowers him over THAT edge instead and he ends up
' hanging from it.  Anything else is a crouch.
'
' The port only ever crouched.  Getting down two storeys safely is done by
' climbing down and letting go, so without this every descent cost health or
' a life, and some of them cannot be made at all.
' Which landing a character comes down in.  A guard, or anyone with his sword
' out, lands en garde; only the player unarmed comes down into the crouch,
' which is FightCtrl's business and which a guard never gets out of - a guard
' who dropped a storey lay in it for the rest of the game.
' AUTO.S CUTGUARD: a guard who has gone past the foot of the screen is taken
' off it.  The port's CutGuard only ever dealt with the player leaving a room,
' so a guard who fell out of the bottom - alive or dead - kept falling with
' his y wrapping round, and was drawn again from the top of the same screen.
Sub CutFallenGuard
  If cID = 0 Or gdPresent = 0 Then Exit Sub
  If cBlockY < ROWS Then Exit Sub
  gdPresent = 0 : oppStr = 0 : gRec(13) = 0
  If cScrn >= 1 And cScrn <= 24 Then gdBlock(cScrn) = 255
  nGuardsGone = nGuardsGone + 1
  lastWhat = "over the edge and gone"
End Sub

' CTRL.S InsideBlock: he came down inside a wall, so put him out beside it.
' Forward if he is near the front edge and there is no wall that way, back
' otherwise; and if there is wall on both sides, back two ("what the hell",
' as the original has it).
Sub InsideBlock
  Local INTEGER d, ahead, behind
  If (cFace And &H80) Then
    ahead = cBlockX - 1 : behind = cBlockX + 1
  Else
    ahead = cBlockX + 1 : behind = cBlockX - 1
  End If
  d = GetDist()
  If d < 8 Then
    If BlockAt(cScrn, ahead, cBlockY) <> T_BLOCK Then
      MoveFwd d + 4
      lastWhat = "out of the wall"
      Exit Sub
    End If
  End If
  If BlockAt(cScrn, behind, cBlockY) = T_BLOCK Then
    MoveFwd -d - 7
  Else
    MoveFwd 7 - d
  End If
  lastWhat = "out of the wall"
End Sub

Function LandSeq() As INTEGER
  If cID >= 2 Or cSword = 2 Then LandSeq = SEQ_LANDENGARDE Else LandSeq = SEQ_SOFTLAND
End Function

' CTRL.S standing :up - the stairs first, then a standing jump if forward is
' held, then DoJumpup with its ledge logic.  Both branches of "standing" reach
' it: ":2 lda clrU / bmi :up" with the button down is the same label that
' :btnup uses.  The port answered a fresh up with the button held by returning
' jumpup on the spot, so a player who keeps the action key down never grabs a
' ledge and cannot climb a flight of stairs.
' CTRL.S :starting - poses 1 to 3, the first frames of a run.  Up and forward
' together is still a standing jump there, so a jump asked for a frame late is
' not thrown away.
Function StartingCtrl() As INTEGER
  StartingCtrl = 0
  If jstkY >= 0 Then Exit Function
  If jstkX >= 0 Then Exit Function
  StartingCtrl = SEQ_STANDJUMP
End Function

' CTRL.S :stjumpup - poses 67 to 69, the wind-up of a jump straight up.
' Forward, held or freshly pressed, makes it a standing jump instead.
Function StJumpUpCtrl() As INTEGER
  StJumpUpCtrl = 0
  If jstkX < 0 Or clrF < 0 Then StJumpUpCtrl = SEQ_STANDJUMP
End Function

' CTRL.S :turning - pose 48.  Forward still held, with no button and no up,
' turns the turn into a run rather than making him stand through it.
Function TurningCtrl() As INTEGER
  TurningCtrl = 0
  If btn < 0 Then Exit Function
  If jstkX >= 0 Then Exit Function
  If jstkY < 0 Then Exit Function
  TurningCtrl = SEQ_TURNRUN
End Function

' CTRL.S DoTurn.  Turning with an armed enemy BEHIND him draws the sword as
' he comes round:
'     lda gotsword / beq :1
'     lda EnemyAlert / cmp #2 / bcc :1
'     jsr getopdist / bpl :1
'     jsr getdist ;to EOB / cmp #2 / bcc :1
'     lda #2 / sta CharSword ;en garde
'     lda #0 / sta offguard
'     lda #turndraw
' The port turned and left the sword where it was, so he came round to face a
' drawn blade with nothing in his hand.
Function DoTurn() As INTEGER
  DoTurn = SEQ_TURN
  clrB = 1
  If gotSword = 0 Then Exit Function
  If enemyAlert < 2 Then Exit Function
  If OpDistS() >= 0 Then Exit Function
  If GetDist() < 2 Then Exit Function
  cSword = 2 : offGuard = 0
  DoTurn = SEQ_TURNDRAW
End Function

Function DoUp() As INTEGER
  If TryStairs() Then DoUp = SEQ_CLIMBSTAIRS : Exit Function
  If jstkX < 0 Then DoUp = SEQ_STANDJUMP : Exit Function
  DoUp = DoJumpup()
End Function

Function DoDown() As INTEGER
  Local INTEGER ahead, behind, under
  DoDown = 0
  clrD = 1
  If (cFace And &H80) Then
    ahead = cBlockX - 1 : behind = cBlockX + 1
  Else
    ahead = cBlockX + 1 : behind = cBlockX - 1
  End If
  ' A cliff in front, and his base within three pixels of the edge: he is
  ' NUDGED five pixels over it and the floor test does the rest.  The original
  ' starts no sequence here at all -
  '     lda #5 / jsr addcharx / sta CharX / jmp rereadblocks
  ' - which is why handing him stepfall did not work: the floor test found the
  ' floor he was still standing over and set him back down before the sequence
  ' could carry his base off it.
  If noFloor(BlockAt(cScrn, ahead, cBlockY)) Then
    If GetDist() < STEPDOWN_THRES Then
      MoveFwd 5
      lastWhat = "steps off"
      Exit Function
    End If
  End If
  ' A cliff behind, and his base eight or more pixels in: climb down it.
  If noFloor(BlockAt(cScrn, behind, cBlockY)) Then
    If GetDist() >= CLIMBDOWN_THRES Then
      under = BlockAt(cScrn, cBlockX, cBlockY)
      ' checkledge(getbehind, getunderft): the block behind has to be one he
      ' can hang in and the one he is standing in has to have the floor whose
      ' edge he hangs from.
      If CanGrabAt(BlockAt(cScrn, behind, cBlockY), cScrn, cBlockX, cBlockY) Then
        ' Facing left with a gate underfoot, it has to be high enough to get
        ' under, the same threshold the climb up the other way uses.
        If (cFace And &H80) = 0 Or RdBlock(cScrn, cBlockX, cBlockY) <> T_GATE Then
          MoveFwd GetDist() - 9
          DoDown = SEQ_CLIMBDOWN
          nClimbDown = nClimbDown + 1
          lastWhat = "climbs down"
          Exit Function
        ElseIf (BSpec(tScrn, tBY * COLS + tBX) >> 2) >= GCLIMBTHRES Then
          MoveFwd GetDist() - 9
          DoDown = SEQ_CLIMBDOWN
          nClimbDown = nClimbDown + 1
          lastWhat = "climbs down"
          Exit Function
        End If
      End If
    End If
  End If
  DoDown = SEQ_STOOP
  nStoop = nStoop + 1
End Function

Function DoJumpup() As INTEGER
  Local INTEGER ahead, behind, above, abovebeh, dist
  clrU = 1
  If (cFace And &H80) Then
    ahead = cBlockX - 1 : behind = cBlockX + 1
  Else
    ahead = cBlockX + 1 : behind = cBlockX - 1
  End If
  above = BlockAt(cScrn, cBlockX, cBlockY - 1)
  abovebeh = BlockAt(cScrn, behind, cBlockY - 1)
  If CanGrabAt(above, cScrn, ahead, cBlockY - 1) Then DoJumpup = DoJumphang() : Exit Function
  If CanGrabAt(abovebeh, cScrn, cBlockX, cBlockY - 1) Then
    dist = GetDist()
    If dist >= JUMPBACK_THRES Then
      If noFloor(BlockAt(cScrn, behind, cBlockY)) Then
        MoveFwd dist - 10
        DoJumpup = SEQ_JUMPBACKHANG : nJumpHang = nJumpHang + 1
        lastWhat = "jump back hang"
      Else
        MoveFwd dist - 14
        DoJumpup = DoJumphang()
      End If
      Exit Function
    End If
  End If
  DoJumpup = DoJumphigh()
End Function

' CTRL.S DoJumphigh: a jump straight up with no ledge to catch.  Whether he
' reaches the ceiling is asked of the block over his HAND, not the one over
' his feet:
'     jsr getbasex / clc / adc #jumpupangle / clc / adc ztemp / jsr getblockx
' with jumpupangle -6 and jumpupreach 0, so the hand is six pixels left of his
' base whichever way he faces.  getblockx is the plain conversion, while this
' port's block number is getblockxp with the angle of seven already taken off,
' so the same point is base+1 here.
'
' And the answer under a solid block is to touch the ceiling - "cmp #block /
' beq :jumpup" comes BEFORE the space test, because a solid block is one of
' the things cmpspace calls clear.  The port asked noFloor of the block over
' his feet, which says yes to a solid block, so he jumped high through the
' ceiling of a room he was standing under.
Function DoJumphigh() As INTEGER
  Local INTEGER d, t
  ' "jsr getfwddist / cmp #4 / bcs :ok / cpx #1 ;barrier? / bne :ok" - up
  ' against something, he is backed off three before he goes.
  d = GetFwdDist()
  If d < 4 And fwdKind = 1 Then MoveFwd d - 3
  t = RdBlock(cScrn, BlockOfX((BaseX() + 1) And &HFF), cBlockY - 1)
  If t = T_BLOCK Then
    DoJumphigh = SEQ_JUMPUP
  ElseIf noFloor(t) = 0 Then
    DoJumphigh = SEQ_JUMPUP
  Else
    DoJumphigh = SEQ_HIGHJUMP
  End If
End Function

' The jump that ends hanging from the ledge in front: a long one from four
' or more units back, which the sequence closes up, or a medium one.
Function DoJumphang() As INTEGER
  Local INTEGER dist
  dist = GetDist()
  If dist >= 4 Then
    MoveFwd dist - 4
    DoJumphang = SEQ_JUMPHANGLONG
  Else
    MoveFwd dist
    DoJumphang = SEQ_JUMPHANGMED
  End If
  nJumpHang = nJumpHang + 1
  lastWhat = "jump and hang"
End Function

' Up while standing at an exit that has risen far enough: the stairs.  The
' door counts if it is under him, behind him or in front of him; he is put
' at its edge, facing left as the climbing frames are drawn.
Function TryStairs() As INTEGER
  Local INTEGER k, bx, t
  TryStairs = 0
  For k = 0 To 2
    bx = cBlockX
    If k = 1 Then
      If (cFace And &H80) Then bx = cBlockX + 1 Else bx = cBlockX - 1
    ElseIf k = 2 Then
      If (cFace And &H80) Then bx = cBlockX - 1 Else bx = cBlockX + 1
    End If
    t = RdBlock(cScrn, bx, cBlockY)
    If t = T_EXIT Then
      If (BSpec(tScrn, tBY * COLS + tBX) >> 2) < kStairThres Then Exit Function
      cX = blocks(gBlockEdge + tBX + 5) + 10
      cFace = &HFF
      nStairs = nStairs + 1
      lastWhat = "stairs"
      TryStairs = 1
      Exit Function
    End If
  Next k
End Function

' Hanging from a ledge (poses 87-99).  Up climbs if the block above allows it:
' a mirror or slicer only from the left, a gate only from the right unless it
' is open enough.  Releasing the button drops: onto floor if there is any, into
' a fall if not, nudged clear first when hanging against a sheer face.  While
' hanging, a solid block or a panel behind straightens the hang, and a ledge
' that crumbles away drops him.
Function HangCtrl() As INTEGER
  Local INTEGER above, under, behind, st, aboveSt
  HangCtrl = 0
  ' CTRL.S hanging reads getabove for the ledge and getunderft for what he is
  ' hanging against, both at CharBlockX: the grab put his base on the ledge's
  ' own edge, so the block he is holding is the one directly overhead.  The
  ' port aligned his coordinate instead of his base and then went looking one
  ' block in front, which reads the gap for a left-facer and two past it for a
  ' right-facer - so a right-facing hang let go the moment anything two blocks
  ' along was empty, the sheer-face test never fired, and a plate could not be
  ' pressed by hanging from it.
  above = RdBlock(cScrn, cBlockX, cBlockY - 1)
  aboveSt = BSpec(tScrn, tBY * COLS + tBX)
  under = BlockAt(cScrn, cBlockX, cBlockY)
  If (cFace And &H80) Then behind = cBlockX + 1 Else behind = cBlockX - 1
  If stunned = 0 And jstkY < 0 Then
    ClearInput : clrU = 1 : clrBtn = 1
    If above = 13 Or above = 18 Then
      If (cFace And &H80) Then HangCtrl = SEQ_CLIMBUP Else HangCtrl = SEQ_CLIMBFAIL
    ElseIf above = 4 Then
      If (cFace And &H80) = 0 Then
        HangCtrl = SEQ_CLIMBUP
      Else
        st = aboveSt >> 2
        If st < GCLIMBTHRES Then HangCtrl = SEQ_CLIMBFAIL Else HangCtrl = SEQ_CLIMBUP
      End If
    Else
      HangCtrl = SEQ_CLIMBUP
    End If
    If HangCtrl = SEQ_CLIMBUP Then nClimb = nClimb + 1 Else nClimbFail = nClimbFail + 1
    Exit Function
  End If
  If btn > 0 Then
    ClearInput : clrD = 1
    If noFloor(BlockAt(cScrn, behind, cBlockY)) And noFloor(under) Then
      HangCtrl = SEQ_HANGFALL : nHangFall = nHangFall + 1 : Exit Function
    End If
    If under = 20 Or ((cFace And &H80) <> 0 And (under = 12 Or under = 7)) Then
      If (cFace And &H80) Then cX = (cX + 7) And &HFF Else cX = (cX - 7) And &HFF
      RereadBlocks
    End If
    HangCtrl = SEQ_HANGDROP : nDrop = nDrop + 1 : Exit Function
  End If
  If cAction <> 6 Then
    If under = 20 Or ((cFace And &H80) <> 0 And (under = 7 Or under = 12)) Then
      HangCtrl = SEQ_HANGSTRAIGHT : nHangStr = nHangStr + 1 : Exit Function
    End If
  End If
  If noFloor(above) Then
    ClearInput : clrD = 1
    HangCtrl = SEQ_HANGDROP : nDrop = nDrop + 1
  End If
End Function

' Crouched (pose 109).  Down released means stand up; down held with a fresh
' forward press means crawl.  A fresh button click tries a pickup, which needs
' objects and is not here yet.
Function CrouchCtrl() As INTEGER
  CrouchCtrl = 0
  If clrBtn < 0 Then
    CrouchCtrl = TryPickup()
    If CrouchCtrl Then Exit Function
  End If
  If jstkY <> 1 Then CrouchCtrl = SEQ_STANDUP : nStandup = nStandup + 1 : Exit Function
  If clrF < 0 Then clrF = 1 : CrouchCtrl = SEQ_CRAWL : nCrawl = nCrawl + 1
End Function

' Running.  A centred stick stops him, but only at two poses in the cycle -
' run-10 and run-14 - so a stop always lands on a foot.  Back turns him round
' at speed; up with a fresh press is a running jump; a fresh down is a dive
' and roll.  Otherwise he keeps running.
Function RunCtrl() As INTEGER
  RunCtrl = 0
  If jstkX = 0 Then
    ' CTRL.S ":rs jsr ]clr / sta clrF".  Stopping a run throws away every
    ' press that has not been acted on, and the same at ":runturn".  The port
    ' kept them, so a key still held as he stopped was still a fresh press
    ' afterwards: tap up too late in a run and he stopped and then jumped on
    ' the spot, off a press made while he was still running.
    If cPosn = 7 Or cPosn = 11 Then
      ClrAll : clrF = 1
      RunCtrl = SEQ_RUNSTOP
    End If
    Exit Function
  End If
  If jstkX > 0 Then
    ClrAll : clrB = 1
    RunCtrl = SEQ_RUNTURN : nRunTurn = nRunTurn + 1 : Exit Function
  End If
  If jstkY < 0 Then
    If clrU < 0 Then RunCtrl = DoRunjump()
    Exit Function
  End If
  If clrD < 0 Then clrD = 1 : RunCtrl = SEQ_DIVEROLL : nRoll = nRoll + 1
End Function

' DORUNJUMP.  A running jump is not "press up and leave the ground".  The
' original waits for the right moment and then nudges him onto it, which is why
' its jumps land and why pressing up at any other time appears to do nothing.
'
' Three things it does that this port did not.  He must be in FULL run, pose
' seven or later, so the press is ignored through the first half-second of
' acceleration.  It looks one block ahead for the edge he is running at.  And
' when it finds one it compares the distance he has to cover with the distance
' the jump covers, and if he is within a few pixels either way it shifts him by
' that difference so the take-off is right.  Too far away and it does nothing
' at all and tries again next frame, so you can press up early and he will
' leave the ground when he gets there.
'
Function DoRunjump() As INTEGER
  Local INTEGER n, px, bx, t, dist, diff
  DoRunjump = 0
  If cPosn < 7 Then Exit Function                  ' not yet at full speed
  n = 0
  px = AddCharX(RJCHANGE)
  bx = BlockOfX(px)
  Do
    If (cFace And &H80) Then bx = bx - 1 Else bx = bx + 1
    t = RdBlock(cScrn, bx, cBlockY)
    If t = T_SPIKES Then Exit Do
    If noFloor(t) Then Exit Do
    n = n + 1
    If n > RJLOOKAHEAD Then
      ' Nothing to aim at.  Jump anyway, which is what a long jump on a long
      ' floor is.
      clrU = 1 : nRunJump = nRunJump + 1
      DoRunjump = SEQ_RUNJUMP
      Exit Function
    End If
  Loop
  ' How far to the end of the floor, and how that compares with the jump.
  dist = DistFromX(px, bx) + 14 * n
  diff = dist - RJLEADDIST
  If diff >= RJMAXFUJFWD And diff < 128 Then Exit Function     ' too far off yet
  If diff < -RJMAXFUJBAK Then diff = -3                       ' too late; tidy it
  cX = (cX + Choice((cFace And &H80) <> 0, -(diff + RJCHANGE), diff + RJCHANGE)) And &HFF
  RereadBlocks
  clrU = 1 : nRunJump = nRunJump + 1
  DoRunjump = SEQ_RUNJUMP
End Function

' His position a frame from now, the way he faces.
Function AddCharX(n As INTEGER) As INTEGER
  If (cFace And &H80) Then AddCharX = (cX - n) And &HFF Else AddCharX = (cX + n) And &HFF
End Function

' The block a given x sits in, and how far that x is from its front edge.
Function BlockOfX(x As INTEGER) As INTEGER
  If x < BLOCKLO Then BlockOfX = -4 Else BlockOfX = (x - BLOCKLO) \ 14 - 4
End Function

Function DistFromX(x As INTEGER, bx As INTEGER) As INTEGER
  Local INTEGER lo, b
  b = BlockOfX(x)
  lo = 14 * (b + 4) + BLOCKLO
  If (cFace And &H80) Then DistFromX = x - lo Else DistFromX = lo + 13 - x
End Function

'-----------------------------------------------------------------------------
' STARTFALL's choice of sequence.  Walking off an edge is not one thing: the
' pose he is in when the floor runs out decides what the fall looks like and,
' more to the point, where he ends up.  A running jump crosses the edge at
' pose 44 and continues through the air; a plain freefall for that case drops
' him into the gap he was clearing, which is what made running jumps useless
' however well they were aimed.
'
' The default is a step off, not a freefall.  From CTRL.S, STARTFALL.
Function FallSeq() As INTEGER
  If cPosn = 9 Then FallSeq = SEQ_STEPFALL : Exit Function       ' run-12
  If cPosn = 13 Then FallSeq = SEQ_STEPFALL2 : Exit Function     ' run-16
  If cPosn = 26 Then FallSeq = SEQ_JUMPFALL : Exit Function      ' standjump-19
  If cPosn = 44 Then FallSeq = SEQ_RJUMPFALL : Exit Function     ' runjump-11
  If cPosn >= 81 And cPosn < 86 Then
    ' Letting go of a ledge: nudged clear of the face he was hanging on.
    cX = AddCharX(5)
    RereadBlocks
    FallSeq = SEQ_STEPFALL2
    Exit Function
  End If
  If cPosn >= 150 And cPosn < 180 Then FallSeq = SEQ_FIGHTFALL : Exit Function
  FallSeq = SEQ_STEPFALL
End Function

Function Advance() As INTEGER
  Local INTEGER n, op, d
  For n = 1 To GUARD
    If cSeq < 0 Or cSeq >= seqLen Then Advance = 0 : Exit Function
    op = seqb(cSeq) : cSeq = cSeq + 1
    If op < OP_LOW Then cPosn = op : Advance = 1 : Exit Function
    Select Case op
      Case OP_CHX
        d = Sgn8(seqb(cSeq)) : cSeq = cSeq + 1
        If (cFace And &H80) Then cX = (cX - d) And &HFF Else cX = (cX + d) And &HFF
      Case OP_CHY
        d = Sgn8(seqb(cSeq)) : cSeq = cSeq + 1
        cY = (cY + d) And &HFF
      Case OP_ABOUTFACE
        cFace = cFace Xor &HFF
      Case OP_GOTO
        cSeq = seqb(cSeq) Or (seqb(cSeq + 1) << 8)
      Case OP_UP
        cBlockY = (cBlockY - 1) And &HFF
        AddSlicers cScrn, cBlockY
      Case OP_DOWN
        cBlockY = (cBlockY + 1) And &HFF
        AddSlicers cScrn, cBlockY
      Case OP_ACT
        cAction = seqb(cSeq) : cSeq = cSeq + 1
      Case OP_SETFALL
        cXVel = seqb(cSeq) : cYVel = seqb(cSeq + 1) : cSeq = cSeq + 2
      Case OP_IFWTLESS
        If weightless Then cSeq = seqb(cSeq) Or (seqb(cSeq+1) << 8) Else cSeq = cSeq + 2
      Case OP_DIE
      Case OP_NEXTLEVEL
        levelDone = 1
      Case OP_JARD
        jarAbove = -1                   ' shake the loose floors on his row
      Case OP_JARU
        jarAbove = 1                    ' shake the loose floors above
      Case OP_EFFECT
        If seqb(cSeq) = 1 Then PotionEffect
        cSeq = cSeq + 1
      Case OP_TAP
        ' A sequence taps to make a noise and to draw attention: 0 is the
        ' attention alone, 1 a footstep, 2 the smack of hitting a wall.
        ' Skipping it is why running about was silent.
        If seqb(cSeq) = 1 Then AddSound 9
        If seqb(cSeq) = 2 Then AddSound 13
        alertGuard = 1
        cSeq = cSeq + 1
      Case Else
        Advance = 0 : Exit Function
    End Select
  Next n
  Advance = 0
End Function

'-----------------------------------------------------------------------------
' Draw the character for the current pose.  The frame's own displacement is a
' drawing offset and is applied here and nowhere else.
' Find the character's current image: which sheet, where, and how big.  The
' drawer needs all of it and the collision code needs the height, which is
' what decides whether a gate is open enough to pass under.
Function CharImg() As INTEGER
  Local INTEGER f, im, sw, tbl, img, rec, facing
  CharImg = 0
  ' The frame table's first row is frame ONE: the original subtracts one
  ' before multiplying by five.  Indexing by the frame number drew every pose
  ' with the NEXT frame's image and offsets, which nobody could see by eye.
  If cPosn < 1 Then Exit Function
  f = FrameRow() * frmEntry
  If f + frmEntry > frmLen Then Exit Function
  im = frmb(f) : sw = frmb(f + 1)
  If im = 0 Then Exit Function
  tbl = ((((sw And &HC0) >> 1) + (im And &H80)) >> 5)
  img = im And &H7F
  If img < 1 Then Exit Function
  ' Table 3 is whichever enemy set this level carries; a missing set falls
  ' back to the guard's.
  If tbl = 3 Then
    If tSet(chSet(curLevel)) <> 0 Then T_CH(3) = tSet(chSet(curLevel)) Else T_CH(3) = tSet(0)
  End If
  If T_CH(tbl) = 0 Then Exit Function
  If img > artCount(T_CH(tbl)) Then Exit Function
  If (cFace And &H80) Then facing = 0 Else facing = 1
  rec = (artFirst(T_CH(tbl)) + (img - 1) * artFacings(T_CH(tbl)) + facing) * 9 + artBase
  imSheet = secSheetOf(T_CH(tbl), img, facing)
  imSX = art(rec+1) Or (art(rec+2) << 8)
  imSY = art(rec+3) Or (art(rec+4) << 8)
  imW  = art(rec+5) Or (art(rec+6) << 8)
  imH  = art(rec+7) Or (art(rec+8) << 8)
  CharImg = 1
End Function

' Which row of the frame table this character's pose uses: an enemy's poses
' 150 to 189 and his falling frames come from the first alternate set.
Function FrameRow() As INTEGER
  FrameRow = FrameRowOf(cID, cPosn)
End Function

' CTRLSUBS.S usealtsets, as a function of who he is and what he is doing, so
' that a scenario can ask it of the opponent as well as of the player.
Function FrameRowOf(id As INTEGER, posn As INTEGER) As INTEGER
  Local INTEGER p
  p = posn
  FrameRowOf = p - 1
  If id = 0 Then Exit Function           ' only the player uses the main set
  ' and the mouse, whose poses are in it: sent down the guards' alternate set
  ' they land on rows that hold nothing and he is drawn as empty air.
  If id = MOUSE_ID Then Exit Function
  ' "cpx #2 / bcc :1" - the falling poses are swapped for the alternate set's
  ' own only from CharID 2 upward.  The shadow is 1 and keeps the main set's
  ' falling frames, which are the kid's, because he IS the kid; the port sent
  ' him down the guards' set and drew him as a guard for the whole of a fall.
  If id >= 2 Then
    If p >= 102 And p < 107 Then p = p + 70
  End If
  If p >= 150 And p < 190 Then FrameRowOf = frmCount + (p - 150)
End Function

' MISC.S REFLECTION.  Standing in the mirror's block, the kid's reflection is
' drawn in it: the same pose, mirrored about the mirror's own line and facing
' the other way, cropped so that only the part of him that is IN the mirror
' shows.
'     jsr getunderft / cmp #mirror / bne rts
'     jsr getreflect / lda dmirr / bmi rts
'     jsr setupchar
'     ldx CharBlockY / inx / lda BlockTop,x / cmp FCharY / bcs rts
'     sta FCharCU                       ;crop upper edge to the top of the row
'     lda CharBlockX / asl / asl / clc / adc #1 / sta FCharCL   ;and the left
'     jmp addreflobj
' The mirroring is the one SmashMirror already uses when the reflection comes
' to life and walks out of the glass.  getreflect itself is in none of the
' source files that can be reached, and dmirr with it; the left crop does the
' same work, since a kid on the wrong side of the glass has his reflection
' cropped away to nothing.
Sub DrawReflection
  Local INTEGER sx, sf, f, dx, px, py, cl, cu, w, h, ox, oy
  If curLevel <> 4 Then Exit Sub
  If BlockAt(cScrn, cBlockX, cBlockY) <> T_MIRROR Then Exit Sub
  sx = cX : sf = cFace
  cX = ((blocks(gBlockEdge + cBlockX + 5) + 10) * 2 - cX) And &HFF
  cFace = cFace Xor &HFF
  If CharImg() Then
    f = FrameRow() * frmEntry
    dx = Sgn8(frmb(f + 2))
    If (cFace And &H80) Then px = (cX - dx) And &HFF Else px = (cX + dx) And &HFF
    px = ORIGINX + ((px - 58) And &HFF) * 2
    If (cFace And &H80) = 0 Then px = px - imW + 1
    py = ORIGINY + ((cY + vertDist + Sgn8(frmb(f + 3))) And &HFF) - imH + 1
    cl = ORIGINX + cBlockX * BLOCKW
    cu = ORIGINY + blocks(gBlockBot + cBlockY)
    ox = 0 : oy = 0
    If px < cl Then ox = cl - px
    If py < cu Then oy = cu - py
    w = imW - ox : h = imH - oy
    If w > 0 And h > 0 Then
      px = px + ox : py = py + oy
      If px >= 0 And py >= 0 And px + w <= 320 And py + h <= 240 Then
        Blit Flash imSheet,2,imSX+ox,imSY+oy,px,py,w,h,TRANSP
      End If
    End If
  End If
  cX = sx : cFace = sf
End Sub

Sub DrawChar
  Local INTEGER f, dx, px, py, ax
  If CharImg() = 0 Then Exit Sub
  f = FrameRow() * frmEntry
  dx = Sgn8(frmb(f + 2))
  If (cFace And &H80) Then px = (cX - dx) And &HFF Else px = (cX + dx) And &HFF
  px = ORIGINX + ((px - 58) And &HFF) * 2
  ' The anchor is the image's LEFT edge facing left and its RIGHT edge facing
  ' right: a mirrored image extends the other way from the same point, which
  ' is what GETEDGES computes.  Drawing both from the left edge put a
  ' right-facing character a body's width too far along, behind his sword.
  ax = px
  If (cFace And &H80) = 0 Then px = ax - imW + 1
  ' The character's Y is a CENTRE PLANE, not his feet.  The source calls the
  ' constant "from bottom of block to center plane", and the floor line he is
  ' aligned to sits that far above the block bottom he actually stands on, so
  ' converting to feet is the one place it belongs.  Adding it at the drawing
  ' site rather than to CharY keeps the physics in the coordinate the floor
  ' test, the grab and the collisions all share.
  py = ORIGINY + ((cY + vertDist + Sgn8(frmb(f + 3))) And &HFF) - imH + 1
  If px >= 0 And py >= 0 And px + imW <= 320 And py + imH <= 240 Then
    Blit Flash imSheet,2,imSX,imSY,px,py,imW,imH,TRANSP
  End If
  DrawSword ax, py + imH - 1, frmb(f + 1) And &H3F
End Sub

' The sword is a separate picture, placed from the frame's sword entry
' relative to the character's drawn position, mirrored with him.  Drawn
' whenever it is out, and always for a living guard.
Sub DrawSword(cx As INTEGER, cybot As INTEGER, n As INTEGER)
  Local INTEGER e, img, dx, dy, tbl, rec, facing, sx, sy, w, h, px, py
  If n = 0 Then Exit Sub
  If cSword = 0 Then
    If cID = 2 And cLife <> 0 Then
      ' a living guard's sword is always out
    ElseIf cPosn >= 229 And cPosn < 238 Then
      ' sheathing
    Else
      Exit Sub
    End If
  End If
  e = frmSword + (n - 1) * 3
  If e + 2 >= frmLen Then Exit Sub
  img = frmb(e) : dx = Sgn8(frmb(e + 1)) : dy = Sgn8(frmb(e + 2))
  If img = 0 Then Exit Sub
  tbl = T_CH(2)
  If img > artCount(tbl) Then Exit Sub
  If (cFace And &H80) Then facing = 0 Else facing = 1
  rec = (artFirst(tbl) + (img - 1) * artFacings(tbl) + facing) * 9 + artBase
  sx = art(rec+1) Or (art(rec+2) << 8) : sy = art(rec+3) Or (art(rec+4) << 8)
  w = art(rec+5) Or (art(rec+6) << 8) : h = art(rec+7) Or (art(rec+8) << 8)
  ' ADDFCHARX: the offset is added facing right and subtracted facing left.
  If facing Then px = cx + dx - w + 1 Else px = cx - dx
  py = cybot + dy - h + 1
  If px < 0 Or py < 0 Or px + w > 320 Or py + h > 240 Then Exit Sub
  Blit Flash secSheetOf(tbl, img, facing),2,sx,sy,px,py,w,h,TRANSP
End Sub

' A gate is a barrier until it has risen far enough for his image to fit
' under it, with the original's margin of grace.
Function GateOpen(scrn As INTEGER, bx As INTEGER, by As INTEGER) As INTEGER
  Local INTEGER st, h
  GateOpen = 0
  If RdBlock(scrn, bx, by) <> T_GATE Then GateOpen = 1 : Exit Function
  st = BSpec(tScrn, tBY * COLS + tBX)
  If CharImg() Then h = imH Else h = 40
  If (st >> 2) + kGateMargin >= h Then GateOpen = 1
End Function

' A slicer is a wall only while its blade is down.  COLL.S CHECKCOLL tests
' the block state against slicerExt and lets a character walk through at any
' other point in the stroke; treating it as a permanent wall made every
' slicer corridor impassable, which stops levels 4 and 5 being finished.
' The top bit of the state is the bloodied flag, so mask it off.  Built like
' GateOpen above: RdBlock resolves across a screen edge and leaves the
' resolved location in tScrn/tBX/tBY for the spec read.
Function SlicerShut(scrn As INTEGER, bx As INTEGER, by As INTEGER) As INTEGER
  Local INTEGER st
  SlicerShut = 1
  If RdBlock(scrn, bx, by) <> T_SLICER Then Exit Function
  st = BSpec(tScrn, tBY * COLS + tBX)
  If (st And &H7F) <> kSlicerExt Then SlicerShut = 0
End Function

Function secSheetOf(tbl As INTEGER, img As INTEGER, facing As INTEGER) As INTEGER
  Local INTEGER rec
  rec = (artFirst(tbl) + (img - 1) * artFacings(tbl) + facing) * 9 + artBase
  secSheetOf = SLOT + art(rec) - 1
End Function

'-----------------------------------------------------------------------------
' Compose the fixed part of the screen once.  Everything that never changes
' while the character moves lives here, so a frame costs one buffer copy
' instead of ninety blits.
' Each screen names its four neighbours, in the order left, right, up, down.
' Confirmed twice: from the four accessors in the original, and from the data
' itself - a screen's right neighbour names it back as its left.
Sub ApplyPalette
  Local INTEGER i, n
  n = 0
  For i = 1 To 15
    If palette(i) <> 0 Then n = n + 1
  Next i
  ' Refuse an all-black palette.  Setting every entry to black makes the whole
  ' display invisible, console text included, which looks exactly like a crash
  ' and needs a hardware reset to recover from.  A missing or malformed data
  ' file should never be able to do that.
  If n = 0 Then
    Print "no palette in tables.idx - keeping the default colours"
    Exit Sub
  End If
  For i = 0 To 15
    Map(i) = palette(i)
  Next i
  Map Set
End Sub

'-----------------------------------------------------------------------------
' The block he would arrive in, with a gate that has risen far enough counted
' as clear.  The barrier check does this and the crossing test did not, so a
' gate standing on a room boundary stayed shut as far as crossing was
' concerned however far it had opened - and on the first level every gate that
' matters is on a boundary.
Function EntryBlock(bx As INTEGER, by As INTEGER) As INTEGER
  EntryBlock = BlockAt(cScrn, bx, by)
  If EntryBlock = T_GATE Then
    If GateOpen(cScrn, bx, by) Then EntryBlock = 0
  ElseIf EntryBlock = T_SLICER Then
    If SlicerShut(cScrn, bx, by) = 0 Then EntryBlock = 0
  End If
End Function

Sub GetScreens(scr As INTEGER)
  sLeft  = level(OFF_MAP + (scr - 1) * 4)
  sRight = level(OFF_MAP + (scr - 1) * 4 + 1)
  sUp    = level(OFF_MAP + (scr - 1) * 4 + 2)
  sDown  = level(OFF_MAP + (scr - 1) * 4 + 3)
End Sub

'-----------------------------------------------------------------------------
' Walking off an edge moves him to the adjoining screen.  Screen zero means
' there is nothing there, so he stays put and the edge behaves as a wall.
Function CrossScreen() As INTEGER
  Local INTEGER edge
  CrossScreen = 0
  ' You cannot cross into a wall.  A barrier in the entry block of the next
  ' screen behaves as a wall at THIS screen's edge: stop him here and bump.
  ' A hanging character is not standing in the block his feet are over: he is
  ' holding the ledge ABOVE and in front, and that is the block that has to be
  ' clear.  Testing the one at his feet refuses a grab made across a screen
  ' boundary, which is how you leave room 2 on the first level: the ledge is the
  ' pillar at the edge of the room next door and the wall below it is not in the
  ' way at all.  Action 2 is the hang.
  If cBlockX < 0 Then
    edge = EntryBlock(cBlockX, cBlockY)
    If sLeft = 0 Or (cAction <> 2 And Barrier(edge) <> 0) Then
      cBlockX = 0 : cX = 58 + ANGLE : WallBump : Exit Function
    End If
    cScrn = sLeft : cBlockX = cBlockX + COLS : cX = cX + SCRNW
    CrossScreen = 1 : cutDir = 0
  ElseIf cBlockX >= COLS Then
    edge = EntryBlock(cBlockX, cBlockY)
    If sRight = 0 Or (cAction <> 2 And Barrier(edge) <> 0) Then
      cBlockX = COLS - 1 : cX = 58 + ANGLE + 139 : WallBump : Exit Function
    End If
    cScrn = sRight : cBlockX = cBlockX - COLS : cX = cX - SCRNW
    CrossScreen = 1 : cutDir = 1
    StealSword
  ElseIf cBlockY >= ROWS Then
    ' The sixth level is finished by falling off its first screen.  AUTO.S
    ' cutchar refuses to cut down from there, so he never arrives anywhere -
    ' the shaft below is on the map and is what he appears to fall through,
    ' but he is gone before he reaches it - and TOPCTRL.S NextFrame sees his
    ' Y wrap and calls the level done.  Without this he crossed down, ran out
    ' of screens at the foot of the shaft and was killed by the landing.
    If curLevel = 6 And cScrn = 1 Then
      levelDone = 1 : lastWhat = "off the sixth level"
      Exit Function
    End If
    ' INTERPRETATION: the original has no case for this, because its levels
    ' always link a screen below wherever a fall is possible.  Falling out of
    ' the world is treated as fatal rather than left to count rows for ever.
    If sDown = 0 Then
      cBlockY = ROWS - 1 : cY = floory(ROWS) : cFalling = 0 : cAction = 0
      cSeq = seqTab(SEQ_HARDLAND) : lastWhat = "fell out of the world" : nHard = nHard + 1 : cLife = 0 : nDead = nDead + 1
      Exit Function
    End If
    cScrn = sDown : cBlockY = cBlockY - ROWS
    cY = (cY - 3 * 63) And &HFF
    CrossScreen = 1 : cutDir = 3
  ElseIf cBlockY < 0 Then
    If sUp = 0 Then Exit Function
    cScrn = sUp : cBlockY = cBlockY + ROWS
    cY = (cY + 3 * 63) And &HFF
    CrossScreen = 1 : cutDir = 2
  End If
  If CrossScreen Then
    GetScreens cScrn
    BuildTypeGrid cScrn
    ' Arriving: torches, slicers, and the guard - his own, or the one
    ' following.  This was missing, so nothing on a crossed-into screen
    ' ever came alive.
    EnterScreen cScrn, cBlockY
    cutDir = -1
    ComposeBackground
  End If
End Function

'-----------------------------------------------------------------------------
Sub ComposeBackground
  ' Ninety blits.  If the screen has not actually changed there is nothing to
  ' rebuild, and rebuilding it anyway is visible as a flash.
  If composedScrn = cScrn Then Exit Sub
  composedScrn = cScrn
  BuildTypeGrid cScrn
  FrameBuffer Write F
  CLS
  DrawScreen cScrn
  ' The background is kept the way it will be shown, so only the moving
  ' half has to be turned over each frame.
  If invert Then FlipBuffer
  FrameBuffer Write 2
End Sub

'-----------------------------------------------------------------------------
' Screens are numbered from ONE, and the blueprint's planes are laid out from
' screen one at offset zero (CALCBLUE subtracts one before multiplying by
' thirty).  Indexing them by the screen number itself read every room one
' screen along, so the geometry never matched the map of neighbours: the
' room he fell into was not the room the map said was below.
Sub BuildTypeGrid(scr As INTEGER)
  Local INTEGER r, c, t, base, row
  For r = 0 To 3
    For c = 0 To TPW - 1 : tp(r * TPW + c) = 0 : ts(r * TPW + c) = 0 : Next c
  Next r
  For r = 0 To ROWS - 1
    base = (scr - 1) * 30 + r * COLS : row = r * TPW
    For c = 0 To COLS - 1
      t = level(base + c) And &H1F
      If t >= blockTypes Then t = 0
      tp(row + c + 1) = t
      ' The state byte lives in a parallel plane of the blueprint.
      ts(row + c + 1) = level(720 + base + c)
    Next c
  Next r
End Sub

Sub DrawScreen(scr As INTEGER)
  Local INTEGER r, c, dy, x, yb, k, row, i
  For r = 0 To ROWS - 1
    dy = blocks(gBlockBot + r + 1) : yb = ORIGINY + dy : row = r * TPW
    For c = 0 To COLS - 1
      x = ORIGINX + c * BLOCKW : i = row + c
      ' Anything whose picture depends on its state is left out here and drawn
      ' every frame by DrawMovers: a gate's grille and corner, a loose floor
      ' entirely, a plate's floor and front.
      k = tp(i + TPW)
      If k <> T_GATE Then
        If secOn(k) Then Blit Flash secSheet(k),F,secSX(k),secSY(k),x+secDX(k),yb+secDY(k),secW(k),secH(k),TRANSP
      End If
      ' B section, from the block on the left - unless a solid block in this
      ' cell hides it.  Indexed by that neighbour's type AND its state.
      If tp(i + 1) <> 20 And tp(i) <> T_GATE And tp(i) <> T_LOOSE And tp(i) <> T_EXIT Then
        k = tp(i) * BSTATES + (ts(i) And 7)
        If bOn(k) Then Blit Flash bSheet(k),F,bSX(k),bSY(k),x+bDX(k),yb+bDY(k),bW(k),bH(k),TRANSP
      End If
      If Movable(tp(i + 1)) = 0 Then
        k = 64 + tp(i + 1)
        If secOn(k) Then Blit Flash secSheet(k),F,secSX(k),secSY(k),x+secDX(k),yb+secDY(k),secW(k),secH(k),TRANSP
        k = 96 + tp(i + 1)
        If secOn(k) Then Blit Flash secSheet(k),F,secSX(k),secSY(k),x+secDX(k),yb+secDY(k),secW(k),secH(k),TRANSP
      End If
    Next c
  Next r
End Sub

Sub DrawFront(scr As INTEGER)
  Local INTEGER r, c, dy, x, yb, k, row, n, tall
  For r = 0 To ROWS - 1
    dy = blocks(gBlockBot + r + 1) : yb = ORIGINY + dy : row = r * TPW
    For c = 0 To COLS - 1
      k = 128 + tp(row + c + 1)
      x = ORIGINX + c * BLOCKW
      ' Potions come in two bottles.  Kinds two, three and four use the taller
      ' one; kinds nought, one and five use the short one the tables name.  It
      ' hangs from the same line, so its bottom edge is taken from the short
      ' one's rather than from a table of its own.
      tall = 0
      If secOn(k) And tp(row + c + 1) = T_FLASK Then
        n = ts(row + c + 1) >> 5
        If n >= 2 And n <= 4 Then
          PutBg kSpecialFlask, x + secDX(k), yb + secDY(k) + secH(k) - 1
          tall = 1
        End If
      End If
      If secOn(k) And tall = 0 Then
        n = tp(row + c + 1)
        If n = 3 Or n >= 27 Then
          Blit Flash secSheet(k),2,secSX(k),secSY(k),x+secDX(k),yb+secDY(k),secW(k),secH(k),OPAQUE
        Else
          Blit Flash secSheet(k),2,secSX(k),secSY(k),x+secDX(k),yb+secDY(k),secW(k),secH(k),TRANSP
        End If
      End If
      If tp(row + c + 1) = T_SLICER Then
        n = SlicerFrame(level(OFF_SPEC + (scr - 1) * 30 + r * COLS + c))
        PutBg blocks(aSlicerFrnt + n), x, yb - 3
      End If
    Next c
  Next r
End Sub

'-----------------------------------------------------------------------------
' Everything whose picture depends on its state, drawn fresh every frame from
' the live blueprint: gates by how far open, spikes by how far out, loose floors
' by their wobble, plates by whether they are down, slicers by their stroke,
' torches by their flame, and any floor in mid-air.  Drawn in the original's
' order within a cell: the corner from below-left, the middle from the left,
' then this cell's front and floor.
Sub DrawMovers
  Local INTEGER r, c, base, x, yb, ay, dy, t, st, tl, sl, tbl, sbl, y, n, k, te
  For r = 0 To ROWS - 1
    yb = ORIGINY + blocks(gBlockBot + r + 1) : ay = yb - 3 : dy = yb
    base = (cScrn - 1) * 30 + r * COLS
    For c = 0 To COLS - 1
      x = ORIGINX + c * BLOCKW
      t = level(base + c) And &H1F : st = level(OFF_SPEC + base + c)
      If c > 0 Then
        tl = level(base + c - 1) And &H1F : sl = level(OFF_SPEC + base + c - 1)
      Else
        tl = -1 : sl = 0
      End If
      tbl = -1
      If r < ROWS - 1 And c > 0 Then
        tbl = level(base + COLS + c - 1) And &H1F : sbl = level(OFF_SPEC + base + COLS + c - 1)
      End If
      ' the corner of a gate below and to the left shows through an empty cell
      If tbl = T_GATE Then
        If t = T_SPACE Or t = 12 Or t = 9 Then DrawGateC x, dy, sbl
      End If
      ' the middle section belongs to the block on the left
      If t <> T_BLOCK Then
        Select Case tl
          Case T_GATE
            DrawGateB x, ay, dy, sl
          Case T_SPIKES
            PutBg blocks(aSpikeB + SpikeIdx(sl)), x, ay - 1
          Case T_LOOSE
            y = LooseY(sl)
            PutBg kLooseB, x, ay + Sgn8(blocks(aLooseBY + y))
          Case T_TORCH
            If sl <= kTorchLast Then PutBg blocks(aTorchFlame + sl), x + 7, ay - 43
          Case T_EXIT
            DrawExitB x, ay, dy, sl, c
        End Select
      End If
      ' this cell's own front and floor
      Select Case t
        Case T_LOOSE
          y = LooseY(st)
          PutBg blocks(aLooseD + y), x, dy
          PutBg blocks(aLooseA + y), x, ay
        Case T_PLATE, T_UPLATE, T_DPLATE
          te = t
          If t = T_PLATE And GetTimer(st) >= 2 Then te = T_DPLATE
          If t = T_UPLATE And GetTimer(st) >= 2 Then te = T_FLOOR
          PutBg blocks(pPieceD + te), x, dy
          PutBg blocks(pPieceA + te), x, ay + Sgn8(blocks(pPieceAY + te))
        Case T_SWORD
          If st = 1 Then
            PutSword kSwordGleam1, x, ay
          Else
            PutSword kSwordGleam0, x, ay
          End If
          nSwordsDrawn = nSwordsDrawn + 1
        Case T_SPIKES
          PutBg blocks(aSpikeA + SpikeIdx(st)), x, ay - 1
        Case T_SLICER
          n = SlicerFrame(st)
          If st And &H80 Then
            PutBg blocks(aSlicerBot2 + n), x, ay
          Else
            PutBg blocks(aSlicerBot + n), x, ay
          End If
          PutBg blocks(aSlicerTop + n), x, ay - blocks(aSlicerGap + n)
      End Select
    Next c
  Next r
  ' floors in mid-air on this screen: the falling frame of a loose floor
  For k = 0 To numMob - 1
    If mobScrn(k) = cScrn And mobType(k) = 0 Then
      x = ORIGINX + (mobX(k) >> 2) * BLOCKW : yb = ORIGINY + mobY(k)
      PutBg blocks(aLooseD + kFfalling), x, yb
      PutBg blocks(aLooseA + kFfalling), x, yb - 3
    End If
  Next k
End Sub

' Which of the spike pictures a state selects: the count on the way out and
' back, or fully out while the timer runs.
Function SpikeIdx(st As INTEGER) As INTEGER
  If st And &H80 Then SpikeIdx = kSpikeExt Else SpikeIdx = st
  If SpikeIdx > kSpikeRet Then SpikeIdx = 0
End Function

' Which of the loose-floor pictures: the wobble count, or the falling count.
Function LooseY(st As INTEGER) As INTEGER
  Local INTEGER y
  If st < &H80 Then
    y = st
  Else
    y = st And &H7F
    If y >= kFfalling + 1 Then y = 1
  End If
  If y > kFfalling Then y = kFfalling
  LooseY = y
End Function

Function SlicerFrame(st As INTEGER) As INTEGER
  Local INTEGER n
  n = st And &H7F
  If n >= kSlicerRet Then n = kSlicerRet
  SlicerFrame = blocks(aSlicerSeq + n) - 1
End Function

' The corner piece of a gate, seen through the cell above and to the right of
' it: one of eight pictures, by how far the grille has risen within a block.
Sub DrawGateC(x As INTEGER, dy As INTEGER, st As INTEGER)
  Local INTEGER p
  p = st : If p > kGMax Then p = kGMax
  p = p >> 2
  PutBg blocks(aGate8C + (p And 7)), x, dy
End Sub

' The grille: a bottom piece, as many eight-line middle pieces as fit up to the
' top of the block, and a part piece to finish.  Its bottom sits a quarter of
' the state above the floor line.
Sub DrawGateB(x As INTEGER, ay As INTEGER, dy As INTEGER, st As INTEGER)
  Local INTEGER p, gb, thr, yco, h
  p = st : If p > kGMax Then p = kGMax
  gb = ay - ((p >> 2) + 1)
  thr = dy - 62
  PutBg kGateBotORA, x, gb - 2
  yco = gb - 12
  Do
    If yco - ORIGINY >= 192 Then Exit Do
    If yco - 7 < thr Then Exit Do
    PutBg kGateB1, x, yco
    yco = yco - 8
  Loop
  h = yco - thr + 1
  If h >= 1 And h <= 8 Then PutBg blocks(aGate8B + h - 1), x, yco
End Sub

' The exit: stairs behind (not on the level's entrance), then the door in
' four-line strips from its bottom edge, which rises with the state, up to
' the top of the block, then the lintel.
Sub DrawExitB(x As INTEGER, ay As INTEGER, dy As INTEGER, st As INTEGER, c As INTEGER)
  Local INTEGER thr, yco
  If c >= 9 Then Exit Sub
  If cScrn <> startScrn Then PutBg &H6B, x + 7, ay - 12
  thr = dy - 67
  If thr < ORIGINY Then Exit Sub
  yco = ay - 14 - (st >> 2)
  Do
    PutBg &H6C, x + 7, yco
    yco = yco - 4
  Loop While yco >= thr
  PutBg &H6E, x + 7, ay - 64
End Sub

' The strength meter along the bottom: a mark for each unit he could have,
' filled for each he has.  The original's pictures come later; this is
' the information.
Sub DrawMeter
  Local INTEGER k, x, y
  y = ORIGINY + 193
  For k = 0 To maxKidStr - 1
    x = ORIGINX + k * 7
    If k < kidStr Then
      Box x, y, 6, 5, 1, &HFF0000, &HFF0000
    Else
      Box x, y, 6, 5, 1, &HFF0000
    End If
  Next k
  If gdPresent = 0 Then Exit Sub
  For k = 0 To maxOppStr - 1
    x = 320 - ORIGINX - 6 - k * 7
    If k < oppStr Then
      Box x, y, 6, 5, 1, &H0000FF, &H0000FF
    Else
      Box x, y, 6, 5, 1, &H0000FF
    End If
  Next k
End Sub

' The message across the foot of the screen: the level number on arrival,
' then the time remaining as the clock passes each mark.  The original
' draws these in the game's own font; this uses the display's, which is
' the one deliberate difference.
Sub DrawMessage
  ShowTime
  If msgTimer = 0 Then Exit Sub
  msgTimer = msgTimer - 1
  If message = MSG_LEVEL Then
    Text 160, ORIGINY + 176, "LEVEL " + Str$(msgLevel), "CT", 1, 1, Map(15)
  Else
    Text 160, ORIGINY + 176, msgText, "CT", 1, 1, Map(15)
  End If
End Sub

' Turn the buffer being written upside down, a pair of lines at a time.
' The potion flips the whole picture, sprites included, which on the
' original is a swap of the plotter's row table.  Here there is nothing to
' swap, so the finished raster is reversed: about sixteen milliseconds, and
' only while the effect is in force.
Sub FlipBuffer
  Local INTEGER y
  For y = 0 To 119
    Blit Read #1, 0, y, 320, 1
    Blit 0, 239 - y, 0, y, 320, 1
    Blit Write #1, 0, 239 - y
    Blit Close #1
  Next y
End Sub

' The title screen, if the player has one.  It is the game's own artwork,
' so it is not supplied here: drop a title.bmp beside the data and it is
' shown, leave it out and the game starts straight away.  A 640 by 480
' picture fits the screen with the seventh parameter, which bins pixels by
' two.  The game's palette is put aside while it is up, because the title
' is dithered against the display's own colours, not the dungeon's.
' THE INTERLUDES AND THE ENDINGS.
'
' The original animates the princess, the vizier and the mouse against a
' picture of her room: the picture is the background and the characters are
' drawn over it from the image tables, exactly as in the game.
'
' The picture in the release is the artist's composite, with the princess and
' the vizier already painted into it.  The clean room the engine drew over is
' on the shipped disk as a block number and is not among the files, so there
' is nothing to draw characters onto.  Putting them on top of this picture
' would give the room two princesses and two viziers.  So a scene is shown as
' the picture itself, held, which is what the interlude looks like anyway: the
' princess and the vizier, waiting, while the sand runs.
'
' If the picture is absent the scene is skipped and the game goes straight on.
Sub CutScene(lvl As INTEGER)
  ' The levels the original cuts away on the way into, from LOADNEXTLEVEL.
  If lvl <> 2 And lvl <> 4 And lvl <> 6 And lvl <> 8 And lvl <> 9 And lvl <> 12 Then Exit Sub
  ShowRoom ""
End Sub

Sub ShowRoom(caption As STRING)
  Local INTEGER t0
  If cutRoom = 0 Or HEADLESS Then Exit Sub
  FrameBuffer Write N
  CLS
  ' Blitted from an image slot rather than loaded straight to the screen.
  ' LOAD IMAGE matches every colour in the file against the palette that is
  ' live at the time, and the match is not exact, so the stone came out blue
  ' and the arches bluer.  A blit copies the pixel values as they are, which
  ' is how the character artwork already reaches the screen.
  Blit Flash cutSlot, N, 0, 0, ORIGINX, ORIGINY, ROOMW, ROOMH
  If caption <> "" Then Text 160, ORIGINY + 176, caption, "CT", 1, 1, Map(15)
  nCuts = nCuts + 1
  t0 = Timer
  Do While Timer - t0 < CUTMS
    If KeyDown(0) > 0 Then Exit Do
  Loop
  ' Let go of the key that cut the scene short, so it is not also read as a
  ' move the moment the level appears.  Bounded, in case one is stuck.
  t0 = Timer
  Do While KeyDown(0) > 0 And Timer - t0 < 2000 : Loop
  FrameBuffer Write 2
  CLS Map(TRANSP)
  composedScrn = -1
End Sub

Sub ShowTitle
  Local INTEGER t0, ok
  ok = 1
  Map Reset
  Map Set
  FrameBuffer Write N
  CLS
  On Error Skip 1
  Load Image home + "title.bmp", 0, 0, -1, 0, 0, 2
  If MM.ErrNo <> 0 Then ok = 0
  On Error Clear
  If ok Then
    t0 = Timer
    Do While Timer - t0 < 8000
      If KeyDown(0) > 0 Then Exit Do
    Loop
  End If
  ApplyPalette
  FrameBuffer Write 2
  CLS Map(TRANSP)
End Sub

' The room number, under the message line.  Nothing in the original shows this.
' It is here so that a player and whoever they are asking for help can name the
' same room: the rooms are numbered in the level data and the map, the routes
' and the positions of everything are all quoted in those numbers.
Sub DrawScrnNo(n As INTEGER)
  Text 160, ORIGINY + 186, "ROOM " + Str$(n), "CT", 7, 1, Map(15)
End Sub

' The frame number, top left, so a frame that looks wrong can be named and
' reproduced: the walkthrough is scripted, so frame N is always the same
' frame.  SNAPSHOT saves that frame's picture to the drive.
Sub DrawFrameNo(n As INTEGER)
  Text 2, 2, Str$(n), "LT", 7, 1, Map(15)
End Sub

' Which block types have state-dependent floor and front pieces.
Function Movable(t As INTEGER) As INTEGER
  Movable = 0
  If t = T_LOOSE Or t = T_PLATE Or t = T_UPLATE Or t = T_DPLATE Then Movable = 1
End Function

' Find a background image and blit it into F with its bottom line at ybot.
Sub PutBg(num As INTEGER, x As INTEGER, ybot As INTEGER)
  Local INTEGER py
  If num = 0 Then Exit Sub
  If BgImg(num) = 0 Then Exit Sub
  py = ybot - imH + 1
  If x < 0 Or py < 0 Or x + imW > 320 Or py + imH > 240 Then Exit Sub
  Blit Flash imSheet,2,imSX,imSY,x,py,imW,imH,TRANSP
End Sub

' THE SWORD ON THE FLOOR.  Drawn opaquely, because its picture is a strip of
' floor with the blade lying in it and the original stores rather than ORs it.
'
' It is also taken from the dungeon's second table by name rather than from
' whichever set the level uses.  The two sword pictures are only in that one:
' the palace table has a single-pixel placeholder where the first should be and
' nothing at all where the second should be.  That is not an oversight in the
' release.  The game has THREE scenery sets and the release carries two, so the
' level the sword lies on, which asks for the third, already falls back to the
' palace set here.  Taking the sword from the table that has it is the smaller
' of the two wrongs: the alternative is no sword at all.
Sub PutSword(num As INTEGER, x As INTEGER, ybot As INTEGER)
  Local INTEGER py
  If BgImgFrom(bg2Dun, num - &H80) = 0 Then Exit Sub
  py = ybot - imH + 1
  If x < 0 Or py < 0 Or x + imW > 320 Or py + imH > 240 Then Exit Sub
  Blit Flash imSheet,2,imSX,imSY,x,py,imW,imH,OPAQUE
End Sub

Function BgImg(num As INTEGER) As INTEGER
  If num < &H80 Then
    BgImg = BgImgFrom(T_BG1, num)
  Else
    BgImg = BgImgFrom(T_BG2, num - &H80)
  End If
End Function

Function BgImgFrom(tbl As INTEGER, img As INTEGER) As INTEGER
  Local INTEGER rec
  BgImgFrom = 0
  If tbl = 0 Then Exit Function
  If img < 1 Or img > artCount(tbl) Then Exit Function
  rec = (artFirst(tbl) + (img - 1) * artFacings(tbl)) * 9 + artBase
  imSheet = SLOT + art(rec) - 1
  imSX = art(rec+1) Or (art(rec+2) << 8)
  imSY = art(rec+3) Or (art(rec+4) << 8)
  imW  = art(rec+5) Or (art(rec+6) << 8)
  imH  = art(rec+7) Or (art(rec+8) << 8)
  If imW < 1 Or imH < 1 Then Exit Function
  BgImgFrom = 1
End Function

Sub BuildSections
  Local INTEGER t
  BuildBSections
  For t = 0 To blockTypes - 1
    AddSection 0, t, blocks(pPieceC + t), 0, 0
    AddSection 2, t, blocks(pPieceD + t), 0, 0
    AddSection 3, t, blocks(pPieceA + t), 0, -3 + Sgn8(blocks(pPieceAY + t))
    AddSection 4, t, blocks(pFrontI + t), Sgn8(blocks(pFrontX + t)), -3 + Sgn8(blocks(pFrontY + t))
  Next t
End Sub

' The middle section, resolved for every (neighbour type, neighbour state).
' It is the one section chosen by a decision tree rather than a table lookup.
Sub BuildBSections
  Local INTEGER t, st, k, num, yoff, p, s2
  For t = 0 To blockTypes - 1
    For st = 0 To BSTATES - 1
      k = t * BSTATES + st
      bOn(k) = 0
      num = 0 : yoff = 0
      If t = 0 Then
        If st <= nBpans Then
          num = blocks(vSpaceB + st) : yoff = Sgn8(blocks(vSpaceBY + st))
        End If
      ElseIf t = 1 Then
        If st <= nBpans Then
          num = blocks(vFloorB + st) : yoff = Sgn8(blocks(vFloorBY + st))
        End If
      ElseIf t = 20 Then
        s2 = st
        If s2 >= nBlox Then s2 = 0
        num = blocks(vBlockB + s2)
      Else
        p = blocks(pPieceB + t)
        If p = 0 Then
          If bgPalace <> 0 Then
            num = blocks(pStripe + t) : yoff = -32
          End If
        ElseIf p = panelB0 Then
          If st < nPans Then num = blocks(vPanelB + st)
        Else
          num = p : yoff = Sgn8(blocks(pPieceBY + t))
        End If
      End If
      If num <> 0 Then AddB k, num, -3 + yoff
    Next st
  Next t
End Sub

'-----------------------------------------------------------------------------
Sub AddB(k As INTEGER, num As INTEGER, yoff As INTEGER)
  Local INTEGER tbl, img, rec, sx, sy, w, h
  If num < &H80 Then
    tbl = T_BG1 : img = num
  Else
    tbl = T_BG2 : img = num - &H80
  End If
  If img < 1 Or img > artCount(tbl) Then Exit Sub
  rec = (artFirst(tbl) + (img - 1) * artFacings(tbl)) * 9 + artBase
  sx = art(rec+1) Or (art(rec+2) << 8)
  sy = art(rec+3) Or (art(rec+4) << 8)
  w  = art(rec+5) Or (art(rec+6) << 8)
  h  = art(rec+7) Or (art(rec+8) << 8)
  If w < 1 Or h < 1 Then Exit Sub
  bOn(k) = 1
  bSheet(k) = SLOT + art(rec) - 1
  bSX(k) = sx : bSY(k) = sy : bW(k) = w : bH(k) = h
  bDX(k) = 0
  bDY(k) = yoff - h + 1
End Sub

'-----------------------------------------------------------------------------
Sub AddSection(kind As INTEGER, t As INTEGER, num As INTEGER, xoff As INTEGER, yoff As INTEGER)
  Local INTEGER tbl, img, rec, sx, sy, w, h, k
  k = kind * 32 + t : secOn(k) = 0
  If num = 0 Then Exit Sub
  ' 'tab' is the TAB function, so the table variable is 'tbl'.
  If num < &H80 Then
    tbl = T_BG1 : img = num
  Else
    tbl = T_BG2 : img = num - &H80
  End If
  If img < 1 Or img > artCount(tbl) Then Exit Sub
  rec = (artFirst(tbl) + (img - 1) * artFacings(tbl)) * 9 + artBase
  sx = art(rec+1) Or (art(rec+2) << 8)
  sy = art(rec+3) Or (art(rec+4) << 8)
  w  = art(rec+5) Or (art(rec+6) << 8)
  h  = art(rec+7) Or (art(rec+8) << 8)
  If w < 1 Or h < 1 Then Exit Sub
  secOn(k) = 1
  secSheet(k) = SLOT + art(rec) - 1
  secSX(k) = sx : secSY(k) = sy : secW(k) = w : secH(k) = h
  secDX(k) = xoff : secDY(k) = yoff - h + 1
End Sub

'-----------------------------------------------------------------------------
' A real joystick is screen-relative: left is left whichever way he faces.
' The engine's jstkX is facing-relative, as the original's is after FACEJSTK,
' so a stick reading is mirrored when he faces right.  Without this, holding
' right after a turn to the right is read as "back" and he turns again.
Sub SetInputLR(lf As INTEGER, rt As INTEGER, up As INTEGER, dn As INTEGER, b As INTEGER)
  If (cFace And &H80) Then SetInput lf, rt, up, dn, b Else SetInput rt, lf, up, dn, b
End Sub

Sub SetInput(fwd As INTEGER, bk As INTEGER, up As INTEGER, dn As INTEGER, b As INTEGER)
  If fwd Then jstkX = -1 Else If bk Then jstkX = 1 Else jstkX = 0
  If up Then jstkY = -1 Else If dn Then jstkY = 1 Else jstkY = 0
  If b Then btn = -1 Else btn = 1
  If fwd And Not pF Then clrF = -1
  If bk And Not pB Then clrB = -1
  If up And Not pU Then clrU = -1
  If dn And Not pD Then clrD = -1
  If b And Not pBtn Then clrBtn = -1
  If Not fwd Then clrF = 1
  If Not bk Then clrB = 1
  If Not up Then clrU = 1
  If Not dn Then clrD = 1
  If Not b Then clrBtn = 1
  pF = fwd : pB = bk : pU = up : pD = dn : pBtn = b
End Sub

' A read that runs off an edge comes from the adjoining screen, which is what
' lets a floor continue past a screen boundary.  Without this every edge reads
' as empty and the character falls through the join.
Function BlockAt(scrn As INTEGER, bx As INTEGER, by As INTEGER) As INTEGER
  ' bx/by are copied to locals FIRST.  MMBasic passes a bare variable by
  ' reference, so reassigning a parameter writes back into the caller - and this
  ' function reassigns to resolve across a screen edge.  Left unguarded, merely
  ' READING a neighbouring block teleported the character to it.
  Local INTEGER s, lx, ly
  s = scrn : lx = bx : ly = by
  If lx < 0 Then
    s = level(OFF_MAP + (s - 1) * 4) : lx = lx + COLS
  ElseIf lx >= COLS Then
    s = level(OFF_MAP + (s - 1) * 4 + 1) : lx = lx - COLS
  End If
  If s = 0 Then BlockAt = 0 : Exit Function
  If ly < 0 Then
    s = level(OFF_MAP + (s - 1) * 4 + 2) : ly = ly + ROWS
  ElseIf ly >= ROWS Then
    s = level(OFF_MAP + (s - 1) * 4 + 3) : ly = ly - ROWS
  End If
  If s = 0 Or s > 24 Then BlockAt = 0 : Exit Function
  BlockAt = level((s - 1) * 30 + ly * COLS + lx) And &H1F
End Function

Function Pad$(t As STRING, w As INTEGER)
  Pad$ = Space$(Max(0, w - Len(t))) + t
End Function

' Panels and gates, mirrors and slicers, and solid blocks are all barriers.
Function Barrier(t As INTEGER) As INTEGER
  Select Case t
    Case 7, 12, 4  : Barrier = 1      ' panel with or without floor, gate
    Case 13, 18    : Barrier = 3      ' mirror, slicer
    Case 20        : Barrier = 4      ' solid block
    Case Else      : Barrier = 0
  End Select
End Function

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
  If scrn = cScrn Then composedScrn = -1      ' the background is stale
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

' MOVER.S CLOSEEXIT: open the exit all the way and let it slam shut.
Sub CloseExit(scrn As INTEGER, loc As INTEGER)
  oScrn = scrn : oLoc = loc
  SetSpec scrn, loc, EXITOPENVAL
  oDir = 3                                  ' coming down fast
  AddTrob
End Sub

' TOPCTRL.S entrance: the door the player came in by slams shut behind him as
' the level starts.  It looks through all thirty blocks of his own screen for
' the exit and closes that one.  The port never did it, so on any level whose
' first screen has a door it simply stood open for the rest of the game.
Sub Entrance
  Local INTEGER loc
  For loc = 29 To 0 Step -1
    If BType(cScrn, loc) = T_EXIT Then
      CloseExit cScrn, loc
      Exit Sub
    End If
  Next loc
End Sub

' CUESONG: ask for a tune.  music/<name>.wav beside the rest of the data.
' A missing file is not an error, it is the ordinary case: the game is
' playable with no music at all.
Sub CueSong(n As INTEGER)
  If n < 1 Or n > 16 Then Exit Sub
  If HEADLESS Then Exit Sub
  nCues = nCues + 1
  On Error Skip 2
  Play Stop
  Play Wav home + "music/" + musName(n) + ".wav"
  On Error Clear
End Sub

Sub AddSound(n As INTEGER)
  nSounds = nSounds + 1
  ' One thing at a time on the output: a tune outranks a click.
  If MM.Info$(Sound) = "WAV" Then Exit Sub
  ' The coverage pass fires hundreds of these and draws nothing, so it
  ' runs silent as well as blind.
  If soundOn = 0 Or HEADLESS Then Exit Sub
  If n < 0 Or n > 19 Then Exit Sub
  If sndF(n) = 0 Then Exit Sub
  Play Tone sndF(n), sndF(n), sndD(n)
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

' A sword on the floor gleams every few seconds.  Its state is a countdown, set
' to something short the first time so the screen does not open with every
' sword flashing at once.
Sub TrigSword(scrn As INTEGER, loc As INTEGER)
  oLoc = loc : oScrn = scrn : oDir = 1
  SetSpec scrn, loc, Rnd8() And &H1F
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
  ' The princess's room is the end of the game.
  If curLevel = 14 And s = 5 And gameOver = 0 Then
    gameOver = 2
    message = MSG_TIME : msgText = "YOU WIN" : msgTimer = 200
  End If
  ' TOPCTRL.S NextFrame: the twelfth level is over on reaching screen 23, the
  ' room the bridge leads to.  There is no exit door and no stairs to climb,
  ' so without this there was no way to leave the level at all.
  If curLevel = 12 And s = 23 Then levelDone = 1
  SaveChar kRec()
  If CutGuard(s) = 0 Then AddGuard s
  LoadKidWOp
  For loc = 0 To 29
    If BType(s, loc) = T_TORCH Then TrigTorch s, loc
    If BType(s, loc) = T_SWORD Then TrigSword s, loc
  Next loc
  AddSlicers s, row
  Crumble s
  Milestone3 s
End Sub

' AUTO.S milestone3: reaching screen 7 on the third level - the screen to the
' right of the gate - sets the checkpoint and banks what his strength has
' grown to, so that a death after it does not take that back either.
Sub Milestone3(s As INTEGER)
  If curLevel <> 3 Or s <> 7 Then Exit Sub
  If milestone Then Exit Sub
  milestone = 1
  origStrength = maxKidStr
End Sub

' SUBS.S CRUMBLE.  On the thirteenth level, arriving at screen 16 or screen 23
' brings down the loose floors on the bottom row of the screen ABOVE, columns
' two to seven, each after a delay of its own.  It is the ceiling coming in
' behind him as he runs.  The port had none of it.
Sub Crumble(s As INTEGER)
  Local INTEGER above, c
  If curLevel <> 13 Then Exit Sub
  If s <> 16 And s <> 23 Then Exit Sub
  above = level(OFF_MAP + (s - 1) * 4 + 2)
  If above < 1 Or above > 24 Then Exit Sub
  For c = 7 To 2 Step -1
    If BType(above, 2 * COLS + c) = T_LOOSE Then
      BreakLoose above, 2 * COLS + c, Rnd8() And &H1F
    End If
  Next c
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
    Case T_SWORD  : AnimSword
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

' One tick of the countdown.  At one the sword shows its gleaming picture, and
' at zero the count is reset to between forty and a hundred and three frames.
Sub AnimSword
  If oDir < 0 Then Exit Sub
  If oScrn <> visScrn Then StopObj : Exit Sub
  oState = (oState - 1) And &HFF
  If oState = 0 Then oState = (Rnd8() And &H3F) + 40
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
      AddSound 15
    End If
    Exit Sub
  End If
  If oState = &HFF Then StopObj : AddSound 2 : Exit Sub
  oState = (oState + Sgn8(blocks(aGateInc + x))) And &HFF
  If x = 0 Then
    If oState <= kGMin Then StopObj : AddSound 2 : Exit Sub
    If oState >= kGMax Then Exit Sub          ' the open timer running down
    AddSound 12
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
  Local INTEGER x, old
  If oDir < 0 Then Exit Sub
  ' MOVER.S animexit ":downfast" - a direction of three or more is the exit
  ' coming down fast, which is how the player's own entrance slams behind him.
  ' It accelerates through the same velocity table the gates use.
  If oDir >= 3 Then
    x = oDir
    If x < kMaxGateVel Then x = x + 1 : oDir = x
    old = oState
    oState = (old - blocks(aGateVel + x)) And &HFF
    If old <= blocks(aGateVel + x) Then
      StopObj
      oState = 0
      AddSound 15
    End If
    Exit Sub
  End If
  AddSound 11
  oState = (oState + 4) And &HFF
  If oState >= EXITOPENVAL Then
    StopObj
    AddSound 2
    exitOpen = 1
    CueSong 8                             ' the stairs are revealed
    MirAppear
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
  ' MOVER.S crushchar: a man at a run gets out from under it.  "lda level /
  ' cmp #13 / beq :1" makes the thirteenth level the exception - there the
  ' ceiling is coming down on purpose and nobody outruns it.
  If curLevel <> 13 Then
    If cPosn >= 5 And cPosn < 15 Then Exit Sub
  End If
  nCrush = nCrush + 1
  If cLife = 0 Then Exit Sub
  ' He is set down on the floor first - "lda FloorY,x / sta CharY" - and then
  ' takes the hit, and it is the crush sequence he plays, or a hard landing if
  ' it has killed him.  The port debited his strength and left him standing
  ' there in whatever he happened to be doing.
  cY = floory(cBlockY + 1)
  cAction = 0 : cFalling = 0 : cYVel = 0
  If DecStr(1) = 0 Then
    cLife = 0 : nDead = nDead + 1
    cSeq = seqTab(SEQ_HARDLAND)
  Else
    cSeq = seqTab(SEQ_CRUSH)
  End If
  ' AnimMobs runs before GameFrame reloads the character from his record, so
  ' anything set here is thrown away unless it is written back.  That is why
  ' the port's version appeared to work at all: the strength change goes
  ' through a separate variable and survived, and nothing else did.
  If cID = 0 Then SaveChar kRec() Else SaveChar gRec()
  lastWhat = "crushed"
End Sub


'=============================================================================
' THE CHARACTER MEETS THE MOVING PARTS.  These are the checks the original runs
' after the character has moved, in its order: plate under him, spikes he is
' over, spikes he has landed in, floors his landing jarred.
'=============================================================================

' DECSTR: take strength.  Taking as much as he has, or more, kills him and
' returns 0; otherwise the loss is queued for the end of the frame.
' Anyone who is not the player debits his OWN meter.  The test used to be
' "two or more", which left the shadow - who is one - taking his damage out
' of the player's meter, on every level he appears on.
Function DecStr(n As INTEGER) As INTEGER
  If cID >= 1 Then
    If n >= oppStr Then
      chgOppStr = -oppStr : DecStr = 0
    Else
      chgOppStr = -n : DecStr = 1
    End If
    Exit Function
  End If
  If n >= kidStr Then
    chgKidStr = -kidStr
    DecStr = 0
  Else
    chgKidStr = -n
    DecStr = 1
  End If
End Function

' CHGMETERS: a rise past the maximum is ignored, not clamped.  Reaching zero
' is death.
Sub ChgMeters
  Local INTEGER n
  ' MISC.S UNHOLY, and the twelfth level's half of TOPCTRL.S chgmeters: the
  ' kid and his shadow are one person, so a blow to either is felt by both,
  ' and the shadow's death is his.  Without it the meeting could be settled
  ' with the sword, which is the one thing it must not be: the level is
  ' finished by putting the sword away and walking into him.
  If curLevel = 12 And gdPresent And gRec(11) = 1 And mergeTimer >= 0 Then
    If chgOppStr < 0 And chgKidStr = 0 Then chgKidStr = chgOppStr
    If chgKidStr < 0 And chgOppStr = 0 Then chgOppStr = chgKidStr
  End If
  If chgOppStr <> 0 Then
    n = oppStr + chgOppStr
    If n <= maxOppStr Then oppStr = n
    chgOppStr = 0
    If oppStr <= 0 Then
      oppStr = 0
      If gRec(13) <> 0 Then
        gRec(13) = 0 : nGuardsDead = nGuardsDead + 1 : CueSong 7
        DeadEnemy
      End If
      If cID >= 1 Then cLife = 0
      ' Killing the shadow is killing himself.
      If curLevel = 12 And gRec(11) = 1 And mergeTimer >= 0 Then chgKidStr = -kidStr
    End If
  End If
  If chgKidStr = 0 Then Exit Sub
  n = kidStr + chgKidStr
  If n <= maxKidStr Then kidStr = n
  chgKidStr = 0
  If kidStr <= 0 Then
    kidStr = 0
    If cID = 0 Then
      If cLife <> 0 Then cLife = 0 : nDead = nDead + 1 : lastWhat = lastWhat + " DEAD"
    Else
      If kRec(13) <> 0 Then kRec(13) = 0 : nDead = nDead + 1
    End If
  End If
End Sub

' TRYPICKUP: a fresh press of the button with a flask or the sword in front
' of him.  One underfoot counts too, if there is room to step back off it.
' Standing, he crouches first; crouched, he takes it.  Returns the sequence
' to start, or zero.
Function TryPickup() As INTEGER
  Local INTEGER t, bx, potion
  TryPickup = 0
  t = RdBlock(cScrn, cBlockX, cBlockY)
  If t = T_FLASK Or t = T_SWORD Then
    If (cFace And &H80) Then bx = cBlockX + 1 Else bx = cBlockX - 1
    t = RdBlock(cScrn, bx, cBlockY)
    If noFloor(t) Then Exit Function            ' CMPSPACE: nothing to stand on
    If (cFace And &H80) Then cX = (cX + 14) And &HFF Else cX = (cX - 14) And &HFF
    RereadBlocks
  End If
  If (cFace And &H80) Then bx = cBlockX - 1 Else bx = cBlockX + 1
  t = RdBlock(cScrn, bx, cBlockY)
  If t <> T_FLASK And t <> T_SWORD Then Exit Function
  If cPosn <> 109 Then
    clrD = 1
    TryPickup = SEQ_STOOP
    Exit Function
  End If
  ' The kind of potion is the block's modifier byte itself - MISC.S compares
  ' lastpotion against 1 to 5 directly.  Shifting it down five bits turned
  ' every flask in the game into type zero, which is no effect at all: the
  ' float potion on level 7 carries a 3, the poison on level 2 a 5.
  If t = T_SWORD Then potion = -1 Else potion = BSpec(tScrn, tBY * COLS + tBX)
  ' REMOVEOBJ: the block becomes plain floor and the press is used up.
  lastPotion = potion
  clrBtn = 1
  SetType tScrn, tBY * COLS + tBX, T_FLOOR
  SetSpec tScrn, tBY * COLS + tBX, 0
  nPotions = nPotions + 1
  If t = T_SWORD Then TryPickup = SEQ_PICKUPSWORD Else TryPickup = SEQ_DRINK
  lastWhat = "pick up " + Str$(potion)
End Function

' POTIONEFFECT, run by the drinking sequence's effect instruction.  MISC.S
' returns at once unless CharID is zero: a potion works on the player and on
' nobody else.  The port had no such test, so when the thief drank the flask
' on level 5 the kid got the benefit of it.
Sub PotionEffect
  If cID <> 0 Then Exit Sub
  Select Case lastPotion
    Case -1
      gotSword = 1
      CueSong 4
    Case 1                                  ' heal one
      If kidStr < maxKidStr Then chgKidStr = 1
    Case 2                                  ' a bigger meter, and full
      CueSong 11
      If maxKidStr < kMaxMaxStr Then maxKidStr = maxKidStr + 1
      chgKidStr = maxKidStr - kidStr
    Case 3                                  ' float
      weightless = kWtlessTimer
    Case 4                                  ' the screen turns upside down
      invert = 1 - invert
      composedScrn = -1
      nInverts = nInverts + 1
    Case 5                                  ' poison
      chgKidStr = -1
  End Select
  lastWhat = lastWhat + " effect " + Str$(lastPotion)
End Sub

' CHECKPRESS: is he standing on a plate, or on a loose floor?  Hanging, the
' block he is holding counts instead; touching the ceiling breaks a loose floor
' above him.  Only frames marked as touching the floor press anything.
Sub CheckPress
  Local INTEGER t, hanging, loc
  hanging = 0
  If (cPosn >= 87 And cPosn < 100) Or (cPosn >= 135 And cPosn < 141) Then hanging = 1
  If hanging Then
    t = RdBlock(cScrn, cBlockX, cBlockY - 1)
  Else
    If cAction <> 7 And cAction <> 5 Then
      If cAction >= 2 Then Exit Sub
    End If
    If cPosn = 79 Then
      t = RdBlock(cScrn, cBlockX, cBlockY - 1)
      If t = T_LOOSE Then BreakLoose tScrn, tBY * COLS + tBX, 1 : nLooseTrig = nLooseTrig + 1
      Exit Sub
    End If
    If cPosn < 1 Then Exit Sub
    If (frmb((cPosn - 1) * frmEntry + 4) And &H40) = 0 Then Exit Sub
    t = RdBlock(cScrn, cBlockX, cBlockY)
  End If
  loc = tBY * COLS + tBX
  If t = T_UPLATE Or t = T_PLATE Then
    If cLife And &H80 Then PushPP tScrn, loc Else JamPP tScrn, loc
    nPlates = nPlates + 1
    lastWhat = lastWhat + " plate"
  ElseIf t = T_LOOSE Then
    alertGuard = 1
    BreakLoose tScrn, loc, 1
    nLooseTrig = nLooseTrig + 1
  End If
End Sub

' CHECKSPIKES: spikes under him spring; so do spikes under an empty block he
' is passing over.  The original scans the blocks between his image's edges;
' this uses the block he is in, which is the common case.
' CTRLSUBS.S CHECKSPIKES:
'     lda rightej / jsr getblockxp / bmi rts / sta tempright
'     lda leftej / jsr getblockxp
'     :loop sta blockx / jsr sub / lda blockx / cmp tempright / beq rts
'           clc / adc #1 / jmp :loop
' It walks every column his IMAGE covers, left edge to right, and looks down
' each one.  The port looked down the single column his base was in, so a
' foot out over a spike block was not over it, and spikes he was plainly
' standing on the edge of did not go off.
Sub CheckSpikes
  Local INTEGER bx, a, w2, lb, rb, f
  lb = cBlockX : rb = cBlockX
  If CharImg() Then
    ' GETEDGES, in this port's terms: the drawing anchor is the coordinate
    ' displaced by the frame's own dx, the image runs right from it facing
    ' left and left from it facing right, and a screen pixel is half a game
    ' unit.  (GETEDGES also narrows a frame marked thin by three bits on each
    ' side; that mark is not carried in this port's frame table.)
    f = FrameRow() * frmEntry
    a = Sgn8(frmb(f + 2))
    w2 = imW \ 2
    If (cFace And &H80) Then
      a = (cX - a) And &HFF
      lb = BlockOfX(a) : rb = BlockOfX((a + w2 - 1) And &HFF)
    Else
      a = (cX + a) And &HFF
      lb = BlockOfX((a - w2 + 1) And &HFF) : rb = BlockOfX(a)
    End If
  End If
  If rb < 0 Then Exit Sub
  If lb > rb Then lb = rb
  For bx = lb To rb
    SpikeColumn bx
  Next bx
End Sub

' CHECKSPIKES "sub": down one column from his own row, through empty space,
' until something is found.  Spikes go off; anything solid stops the look.
Sub SpikeColumn(bx As INTEGER)
  Local INTEGER t, by
  by = cBlockY
  Do
    t = RdBlock(cScrn, bx, by)
    If t = T_SPIKES Then
      TrigSpikes tScrn, tBY * COLS + tBX
      nSpikesTrig = nSpikesTrig + 1
      Exit Sub
    End If
    If t <> T_SPACE Then Exit Sub
    If tScrn = 0 Or tScrn <> cScrn Then Exit Sub
    by = by + 1
    If by >= ROWS Then Exit Sub
  Loop
End Sub

' CHECKIMPALE: running into springing spikes, or landing in sprung ones.
Sub CheckImpale
  Local INTEGER t, g
  t = RdBlock(cScrn, cBlockX, cBlockY)
  If t <> T_SPIKES Then Exit Sub
  If cPosn < 7 Then Exit Sub
  g = GetSpikes(tScrn, tBY * COLS + tBX)
  If cPosn < 15 Then
    If g = 2 Then DoImpale
  ElseIf cPosn = 43 Or cPosn = 26 Then
    If g <> 0 Then DoImpale
  End If
End Sub

' The spikes jam with him on them; he is centred on the pit and dies there.
Sub DoImpale
  Local INTEGER loc
  loc = tBY * COLS + tBX
  JamSpikes tScrn, loc
  cY = floory(cBlockY + 1)
  cX = blocks(gBlockEdge + tBX + 5) + 10
  If (cFace And &H80) Then cX = cX - 8 Else cX = cX + 8
  cYVel = 0 : cFalling = 0 : cAction = 0
  If DecStr(100) = 0 Then cLife = 0
  cSeq = seqTab(SEQ_IMPALE)
  If Advance() = 0 Then lastWhat = "STALLED"
  lastWhat = "IMPALED"
  nImpaled = nImpaled + 1 : nDead = nDead + 1
End Sub

'=============================================================================
Function Sgn8(v As INTEGER) As INTEGER
  If v > 127 Then Sgn8 = v - 256 Else Sgn8 = v
End Function

'-----------------------------------------------------------------------------
Sub LoadSheets
  Local INTEGER n
  Local STRING f
  nSheets = 0
  For n = 1 To 4
    f = home + "sheet" + Str$(n) + ".bmp"
    If Dir$(f, File) <> "" Then
      Flash Load Image SLOT + n - 1, f, O
      nSheets = nSheets + 1
    End If
  Next n
  Print "  artwork "; Str$(nSheets); " sheets in slots "; Str$(SLOT); "-"; Str$(SLOT+nSheets-1)
  ' The room the interludes play in, if this copy carries it.  It goes in the
  ' slot after the sheets, which is free: there are five and the game needs
  ' four.
  cutSlot = SLOT + 4
  If Dir$(home + "cutroom.bmp", File) <> "" Then
    On Error Skip 1
    Flash Load Image cutSlot, home + "cutroom.bmp", O
    If MM.ErrNo = 0 Then cutRoom = 1
    On Error Clear
  End If
  If cutRoom Then Print "  scenes  princess's room in slot "; Str$(cutSlot) Else Print "  scenes  no room picture - skipped"
End Sub

' Point the drawing at this level's tiles.  Rebuilding the section tables is
' what changes the look, so it is done only when the set actually changes.
Sub SelectBackground(n As INTEGER)
  Local INTEGER want
  want = 0
  If n >= 0 And n <= 14 Then want = bgSet(n)
  If want = usingBg Then Exit Sub
  usingBg = want
  If want = 0 Then
    T_BG1 = bg1Dun : T_BG2 = bg2Dun : bgPalace = 0
  Else
    T_BG1 = bg1Pal : T_BG2 = bg2Pal
    If want = 1 Then bgPalace = 1 Else bgPalace = 0
  End If
  If T_BG1 <> 0 Then BuildSections
  composedScrn = -1
End Sub

' The effects, then a one millisecond tone to find out whether this board
' can play anything at all.
Sub LoadSounds
  Local INTEGER i, n
  n = 80
  LoadBytes home + "sounds.dat", n, snd()
  For i = 0 To 19
    sndF(i) = snd(i * 4) Or (snd(i * 4 + 1) << 8)
    sndD(i) = snd(i * 4 + 2) Or (snd(i * 4 + 3) << 8)
  Next i
  soundOn = 1
  ' MM.ERRNO keeps its last value, so it has to be cleared before it can be
  ' read as the result of this one probe.
  On Error Clear
  On Error Skip 1
  Play Tone 1000, 1000, 1
  If MM.ErrNo <> 0 Then soundOn = 0
  On Error Clear
  If soundOn = 0 Then
    Print "  no audio configured - running silent"
  Else
    Print "  sound on, "; Str$(sndF(9)); " Hz footstep"
  End If
End Sub

Sub LoadArt
  Local INTEGER n, tables, i, off
  n = artLen
  Open home + "art.bin" For Input As #1
  Memory Input #1, n, packed()
  Close #1
  Memory Unpack packed(), art(), n, 8
  tables = art(0) Or (art(1) << 8)
  artBase = 2 + tables * 7
  For i = 0 To tables - 1
    off = 2 + i * 7
    artCount(i + 1) = art(off) Or (art(off+1) << 8)
    artFacings(i + 1) = art(off+2)
    artFirst(i + 1) = art(off+3) Or (art(off+4) << 8) Or (art(off+5) << 16)
  Next i
End Sub

Sub LoadLevel(n As INTEGER)
  curLevel = n
  levelDone = 0
  SelectBackground n
  If origStrength = 0 Then origStrength = kInitMaxStr
  maxKidStr = origStrength : kidStr = maxKidStr : chgKidStr = 0
  deadTimer = 0 : weightless = 0 : mergeTimer = 0 : mouseTimer = 0
  ' Every level begins with its way out shut.  Nothing put this back, so
  ' once any level's exit had been opened every level after it started
  ' with exitOpen set - which on the third level raises the skeleton the
  ' moment the room is entered, and on the eighth would send the mouse in
  ' before the door is open.
  exitOpen = 0
  Open home + "levels.dat" For Input As #1
  Seek #1, n * LEVELBYTES + 1
  Memory Input #1, LEVELBYTES, packed()
  Close #1
  Memory Unpack packed(), level(), LEVELBYTES, 8
  startScrn = level(OFF_INFO + 64)
  InitGuards
End Sub

Sub LoadBytes(path As STRING, n As INTEGER, dst() As INTEGER)
  Open path For Input As #1
  Memory Input #1, n, packed()
  Close #1
  Memory Unpack packed(), dst(), n, 8
End Sub

Sub ReadLayout
  Local STRING l, k
  Local INTEGER i, v
  Open home + "tables.idx" For Input As #2
  Do While Not Eof(#2)
    Line Input #2, l
    If l <> "" And Left$(l, 1) <> "#" Then
      i = Instr(l, " ")
      If i > 1 Then
        k = Left$(l, i - 1) : v = Val(Mid$(l, i + 1))
        Select Case k
          Case "block_piecea"  : pPieceA = v
          Case "block_pieceay" : pPieceAY = v
          Case "block_pieceb"  : pPieceB = v
          Case "block_pieceby" : pPieceBY = v
          Case "block_piecec"  : pPieceC = v
          Case "block_pieced"  : pPieceD = v
          Case "block_fronti"  : pFrontI = v
          Case "block_fronty"  : pFrontY = v
          Case "block_frontx"  : pFrontX = v
          Case "block_types"   : blockTypes = v
          Case "geom_BlockBot" : gBlockBot = v
          Case "geom_BlockAy"        : gBlockAy = v
          Case "geom_BlockEdge"      : gBlockEdge = v
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
          Case "anim_spikea"         : aSpikeA = v
          Case "anim_spikeb"         : aSpikeB = v
          Case "anim_loosea"         : aLooseA = v
          Case "anim_looseby"        : aLooseBY = v
          Case "anim_loosed"         : aLooseD = v
          Case "anim_gate8c"         : aGate8C = v
          Case "anim_gate8b"         : aGate8B = v
          Case "anim_slicerseq"      : aSlicerSeq = v
          Case "anim_slicertop"      : aSlicerTop = v
          Case "anim_slicerbot"      : aSlicerBot = v
          Case "anim_slicerbot2"     : aSlicerBot2 = v
          Case "anim_slicergap"      : aSlicerGap = v
          Case "anim_slicerfrnt"     : aSlicerFrnt = v
          Case "anim_torchflame"     : aTorchFlame = v
          Case "const_Ffalling"      : kFfalling = v
          Case "const_looseb"        : kLooseB = v
          Case "const_gatebotORA"    : kGateBotORA = v
          Case "const_gateB1"        : kGateB1 = v
          Case "const_gatemargin"    : kGateMargin = v
          Case "const_stairthres"    : kStairThres = v
          Case "frames_swordtab"     : frmSword = v
          Case "anim_strikeprob"     : aStrikeProb = v
          Case "anim_restrikeprob"   : aRestrikeProb = v
          Case "anim_blockprob"      : aBlockProb = v
          Case "anim_impblockprob"   : aImpBlockProb = v
          Case "anim_advprob"        : aAdvProb = v
          Case "anim_refractimer"    : aRefracTimer = v
          Case "anim_specialcolor"   : aSpecialColor = v
          Case "anim_extrastrength"  : aExtraStrength = v
          Case "anim_basicstrength"  : aBasicStrength = v
          Case "anim_basiccolor"     : aBasicColor = v
          Case "const_initmaxstr"    : kInitMaxStr = v
          Case "const_maxmaxstr"     : kMaxMaxStr = v
          Case "const_wtlesstimer"   : kWtlessTimer = v
          Case "const_deadenough"    : kDeadEnough = v
          Case "geom_FloorY"   : gFloorY = v
          Case "block_bstripe" : pStripe = v
          Case "var_panelb"    : vPanelB = v
          Case "var_spaceb"    : vSpaceB = v
          Case "var_spaceby"   : vSpaceBY = v
          Case "var_floorb"    : vFloorB = v
          Case "var_floorby"   : vFloorBY = v
          Case "var_blockb"    : vBlockB = v
          Case "const_numpans" : nPans = v
          Case "const_numblox" : nBlox = v
          Case "const_numbpans": nBpans = v
          Case "const_panelb0" : panelB0 = v
          Case "const_VertDist": vertDist = v
          Case "seq_len"       : seqLen = v
          Case "blocks_len"   : blocksLen = v
          Case "seq_count"     : seqCount = v
          Case "frames_len"    : frmLen = v
          Case "frames_count"  : frmCount = v
          Case "frames_entry"  : frmEntry = v
          Case "art_len"       : artLen = v
          Case "pal_0"         : palette(0) = v
          Case "pal_1"         : palette(1) = v
          Case "pal_2"         : palette(2) = v
          Case "pal_3"         : palette(3) = v
          Case "pal_4"         : palette(4) = v
          Case "pal_5"         : palette(5) = v
          Case "pal_6"         : palette(6) = v
          Case "pal_7"         : palette(7) = v
          Case "pal_8"         : palette(8) = v
          Case "pal_9"         : palette(9) = v
          Case "pal_10"        : palette(10) = v
          Case "pal_11"        : palette(11) = v
          Case "pal_12"        : palette(12) = v
          Case "pal_13"        : palette(13) = v
          Case "pal_14"        : palette(14) = v
          Case "pal_15"        : palette(15) = v
          Case "tab_BGTAB1DUN" : bg1Dun = v : T_BG1 = v
          Case "tab_BGTAB2DUN" : bg2Dun = v : T_BG2 = v
          Case "tab_BGTAB1PAL" : bg1Pal = v
          Case "tab_BGTAB2PAL" : bg2Pal = v
          Case "anim_bgset1"   : aBgSet = v
          Case "anim_chset"    : aChSet = v
          Case "anim_shadpos6a" : aShad6a = v
          Case "anim_shadpos5"  : aShad5 = v
          Case "anim_shadpos12" : aShad12 = v
          Case "anim_ShadProg5" : aShadProg5 = v
          Case "const_flaskscrn": kFlaskScrn = v
          Case "const_flaskx"   : kFlaskX = v
          Case "const_flasky"   : kFlaskY = v
          Case "const_swordscrn": kSwordScrn = v
          Case "const_swordx"   : kSwordX = v
          Case "const_swordy"   : kSwordY = v
          Case "const_specialflask" : kSpecialFlask = v
          Case "const_swordgleam0"  : kSwordGleam0 = v
          Case "const_swordgleam1"  : kSwordGleam1 = v
          Case "const_shadstrength" : kShadStr = v
          Case "const_mirscrn"  : kMirScrn = v
          Case "const_mirx"     : kMirX = v
          Case "const_miry"     : kMirY = v
          Case "tab_CHTAB1"    : T_CH(0) = v
          Case "tab_CHTAB2"    : T_CH(1) = v
          Case "tab_CHTAB3"    : T_CH(2) = v
          Case "tab_CHTAB4GD"  : T_CH(3) = v : tSet(0) = v : tSet(4) = v
          Case "tab_CHTAB4SKEL": tSet(1) = v
          Case "tab_CHTAB4SHAD": tSet(2) = v
          Case "tab_CHTAB4FAT" : tSet(3) = v
          Case "tab_CHTAB4VIZ" : tSet(5) = v
          Case "tab_CHTAB5"    : T_CH(4) = v
        End Select
      End If
    End If
  Loop
  Close #2
End Sub

'=============================================================================
' The scenario harness.
'
' A file called scen.txt beside this program turns the engine into a test rig:
' the game is not played at all, the scenarios in that file are, with nothing
' drawn and no pacing, and what each one expected is checked when it ends.
'
' The point is that a reported bug and the fix for it can each be demonstrated
' without anybody watching a screen, and that writing a new case costs a few
' lines of text rather than an edit and a two-minute upload of this program.
' The engine goes to the board once; the scenarios go as often as needed.
'
' Directives are obeyed in the order they are read, one to a line.  Blank
' lines, and anything after an apostrophe, are ignored.
'
'   FIND lvl type         where every block of that type is on that level
'   SHOW lvl scrn         one screen's blocks, and what it joins onto
'   GDTAB lvl            where the level says each screen's guard stands
'   SCEN name...          begin a scenario
'   AT lvl scrn bx by     put him there: fresh level, counters back to zero
'   START lvl             begin the level where the level itself begins
'   SWORD n               he is carrying the sword
'   FACE n                255 looking left, 0 looking right
'   X n                   put him at that coordinate within his row
'   FLOAT n               the float potion is in him for n more frames
'   OPEN n                the level's way out is open
'   SEED n                the random seed, so that a run repeats
'   CLOCK n               wind the game clock on to frame n
'   SPEC scrn bx by v     hold one block's modifier byte at a value
'   TYPE scrn bx by v     put a block type where the scenario needs one
'   BREAK scrn bx by      bring that loose floor down
'   ENTER                 arrive at the screen he is on, again
'   TRACE n               from here on, print a line for every frame
'   RUN codes             one character a frame; use as many lines as needed
'   WANT what [op] n      an expectation, checked at END
'   WANTBLOCK s bx by ty  what the blueprint should say, checked here
'   END                   check the expectations and say how it went
'
' Input codes are the ones ApplyCode knows: . nothing  > forward  < back
' ^ up  v down  F forward and button  B button  U up and button  J jump.
' WANT's operators are EQ NE LT GT LE GE, EQ if left out, and "what" is any
' of the names ScenValue knows - one it does not know stops the run rather
' than quietly reading zero.
'
' FIND and SHOW are how a scenario gets written at all: no game data leaves
' the board, so the blueprint cannot be read from the host, and guessing at
' the geometry is how every early scenario went wrong.
Sub RunScenarios
  ' The expectation arrays are given a length: a string array otherwise takes
  ' 256 bytes an element, and these two would be eight kilobytes between them
  ' for names that are never more than a word long.
  ' wOp holds the operator, but when the operator is left out it holds the
  ' value instead, so two characters is not enough: WANT X 163 overran it.
  Local STRING wName(15) LENGTH 12, wOp(15) LENGTH 12
  Local STRING ln, w, nm, codes
  Local INTEGER wVal(15), nw, i, got, ok, tests, fails, tr, bwant

  HEADLESS = 1
  Print
  Print "--- scenarios, nothing drawn"
  ' The states a scenario needs in order to write a SPEC line, since they come
  ' from the converted layout and not from anything the host can see.
  Print "  slicer: shut at " + Str$(kSlicerExt) + ", back at " + Str$(kSlicerRet) + ", cycle " + Str$(kSliceTimer)
  Open home + "scen.txt" For Input As #3
  Do While Not Eof(#3)
    Line Input #3, ln
    ln = ScClean$(ln)
    w = UCase$(ScWord$(ln, 1))
    Select Case w
      Case ""
        ' a blank line, or a line that was nothing but a comment
      Case "FIND"
        ScFind ScNum(ScWord$(ln, 2)), ScNum(ScWord$(ln, 3))
      Case "SHOW"
        ScShow ScNum(ScWord$(ln, 2)), ScNum(ScWord$(ln, 3))
      Case "GDTAB"
        ' What the level says about its guards.  It is the only way to tell
        ' whether the engine ends up putting one where the blueprint asked
        ' for him, which is the whole of finding 2.5.
        LoadLevel ScNum(ScWord$(ln, 2)) : loadedLevel = curLevel
        Print "gdtab: level " + Str$(curLevel)
        For i = 1 To 24
          If gdBlock(i) < 30 Then
            Print "  scr " + Str$(i) + " blk " + Str$(gdBlock(i) Mod COLS) + "," + Str$(gdBlock(i) \ COLS) + " x " + Str$(gdX(i)) + " face " + Str$(gdFace(i)) + " prog " + Str$(gdProg(i))
          End If
        Next i
      Case "SCEN"
        nm = ScRest$(ln, 2) : nw = 0 : tr = 0
        Print
        Print "scen: " + nm
      Case "AT"
        ResetCharAt ScNum(ScWord$(ln,2)), ScNum(ScWord$(ln,3)), ScNum(ScWord$(ln,4)), ScNum(ScWord$(ln,5))
        ScZero
      Case "START"
        ' The level's own opening: the screen, block and facing out of its
        ' INFO block, which is what AT cannot give because it has to be told
        ' where to put him and always faces him left.
        StartLevel ScNum(ScWord$(ln, 2))
        ScZero
        Print "      start blk " + Str$(cBlockX) + "," + Str$(cBlockY) + " lvl " + Str$(curLevel) + " scr " + Str$(cScrn) + " facing " + Str$(cFace)
      Case "SPEC"
        ScSpec ScNum(ScWord$(ln,2)), ScNum(ScWord$(ln,3)), ScNum(ScWord$(ln,4)), ScNum(ScWord$(ln,5))
      Case "BREAK"
        ' Bring a loose floor down, which is otherwise only ever asked for by
        ' standing on one or by the shake of a hard landing.
        BreakLoose ScNum(ScWord$(ln,2)), ScNum(ScWord$(ln,4)) * COLS + ScNum(ScWord$(ln,3)), 1
        Print "  break scr " + ScWord$(ln,2) + " blk " + ScWord$(ln,3) + "," + ScWord$(ln,4)
      Case "TYPE"
        ' Put a block where the scenario needs one, which is how a room is set
        ' up as it would be some way into the level without playing up to it.
        SetType ScNum(ScWord$(ln,2)), ScNum(ScWord$(ln,4)) * COLS + ScNum(ScWord$(ln,3)), ScNum(ScWord$(ln,5))
        BuildTypeGrid cScrn
        Print "  type scr " + ScWord$(ln,2) + " blk " + ScWord$(ln,3) + "," + ScWord$(ln,4) + " set to " + ScWord$(ln,5)
      Case "ENTER"
        ' Arrive at the screen he is already on: torches, slicers and whoever
        ' is waiting.  After a TYPE that is what makes the room take effect,
        ' and coming back to a room is a thing worth testing by itself.
        SaveChar kRec()
        EnterScreen cScrn, cBlockY
        LoadKidWOp
        Print "  entered scr " + Str$(cScrn) + ", shadows so far " + Str$(nShadows)
      Case "SWORD" : gotSword = ScNum(ScWord$(ln, 2))
      Case "X"
        ' Where in the block he is standing.  AT always puts him in the
        ' middle, and how far he is from an edge is exactly what several of
        ' the control rules turn on.
        cX = ScNum(ScWord$(ln, 2)) And &HFF
        RereadBlocks
        SaveChar kRec()
        Print "  x set to " + Str$(cX) + ", block " + Str$(cBlockX) + ", " + Str$(GetDist()) + " from the edge ahead"
      Case "FACE"
        ' Which way he is looking, without spending frames turning him:
        ' 255 for left, 0 for right.  AT always faces him left, and half
        ' of what a scenario wants to ask about depends on the other way.
        cFace = ScNum(ScWord$(ln, 2)) And &HFF
        RereadBlocks
        SaveChar kRec()
      Case "FLOAT" : weightless = ScNum(ScWord$(ln, 2))
      Case "OPEN"  : exitOpen = ScNum(ScWord$(ln, 2))
      Case "SEED"  : rndSeed  = ScNum(ScWord$(ln, 2))
      Case "CLOCK" : frameCount = ScNum(ScWord$(ln, 2)) : GetMinLeft
      Case "TRACE" : tr       = ScNum(ScWord$(ln, 2))
      Case "RUN"
        codes = ScRest$(ln, 2)
        For i = 1 To Len(codes)
          ' A later RUN line after the level ended plays nothing, quietly.
          If levelDone Or gameOver Then Exit For
          ApplyCode Mid$(codes, i, 1)
          GameFrame
          scenFrames = scenFrames + 1
          If tr Then Print "      f" + Str$(scenFrames) + " " + Mid$(codes, i, 1) + " " + ScState$()
          ' The play loop leaves a finished level at once, so there is nothing
          ' truthful to be learnt from the frames after it, and a character
          ' who has fallen off the world keeps falling into blocks that are
          ' not there.  Stop where the game would have stopped.
          If levelDone Or gameOver Then
            Print "  stopped at frame " + Str$(scenFrames) + ": " + Choice(levelDone, "the level ended", "the game ended")
            Exit For
          End If
        Next i
      Case "WANT"
        If nw < 15 Then
          nw = nw + 1
          wName(nw) = UCase$(ScWord$(ln, 2))
          wOp(nw) = UCase$(ScWord$(ln, 3))
          ' The operator may be left out, in which case word three is the value.
          If Len(wOp(nw)) = 2 And Instr("EQ NE LT GT LE GE", wOp(nw)) > 0 Then
            wVal(nw) = ScNum(ScWord$(ln, 4))
          Else
            wVal(nw) = ScNum(wOp(nw)) : wOp(nw) = "EQ"
          End If
        Else
          Print "  ?? too many expectations in one scenario"
        End If
      Case "WANTBLOCK"
        ' What the blueprint should say, checked where it is written rather
        ' than saved up for END: a scenario that changes a room wants to know
        ' that at the point it changed it.
        got = BType(ScNum(ScWord$(ln,2)), ScNum(ScWord$(ln,4)) * COLS + ScNum(ScWord$(ln,3)))
        bwant = ScNum(ScWord$(ln, 5))
        tests = tests + 1
        nm = "block " + ScWord$(ln,2) + " " + ScWord$(ln,3) + "," + ScWord$(ln,4)
        If got = bwant Then
          Print "  ok:   " + nm + " is type " + Str$(got)
        Else
          Print "  FAIL: " + nm + " should be type " + Str$(bwant) + " but is " + Str$(got)
          fails = fails + 1
        End If
      Case "WANTSPEC"
        ' The same, for the modifier byte: how far a gate or an exit has got.
        got = BSpec(ScNum(ScWord$(ln,2)), ScNum(ScWord$(ln,4)) * COLS + ScNum(ScWord$(ln,3)))
        bwant = ScNum(ScWord$(ln, 5))
        tests = tests + 1
        nm = "spec " + ScWord$(ln,2) + " " + ScWord$(ln,3) + "," + ScWord$(ln,4)
        If got = bwant Then
          Print "  ok:   " + nm + " is " + Str$(got)
        Else
          Print "  FAIL: " + nm + " should be " + Str$(bwant) + " but is " + Str$(got)
          fails = fails + 1
        End If
      Case "END"
        Print "  ran " + Str$(scenFrames) + " frames, ended " + ScState$()
        For i = 1 To nw
          got = ScenValue(wName(i))
          ok = 0
          Select Case wOp(i)
            Case "EQ" : If got =  wVal(i) Then ok = 1
            Case "NE" : If got <> wVal(i) Then ok = 1
            Case "LT" : If got <  wVal(i) Then ok = 1
            Case "GT" : If got >  wVal(i) Then ok = 1
            Case "LE" : If got <= wVal(i) Then ok = 1
            Case "GE" : If got >= wVal(i) Then ok = 1
          End Select
          tests = tests + 1
          If ok Then
            Print "  ok:   " + wName(i) + " " + wOp(i) + " " + Str$(wVal(i))
          Else
            Print "  FAIL: " + wName(i) + " " + wOp(i) + " " + Str$(wVal(i)) + " but it is " + Str$(got)
            fails = fails + 1
          End If
        Next i
        nw = 0
      Case Else
        Print "  ?? " + ln
    End Select
  Loop
  Close #3
  Print
  Print "SCENARIOS: " + Str$(tests - fails) + " of " + Str$(tests) + " checks passed"
  If fails = 0 Then Print "PASS" Else Print "FAIL"
  FrameBuffer Close
  ' Put the colours back, or the console is left unreadable.
  Map Reset
  Map Set
  Option Console Both
End Sub

' What a scenario is allowed to ask about.  An unknown name is an error and
' not a zero: a misspelt expectation that quietly passes is worse than none.
Function ScenValue(what As STRING) As INTEGER
  Select Case what
    Case "BX"      : ScenValue = cBlockX
    Case "BY"      : ScenValue = cBlockY
    Case "SCRN"    : ScenValue = cScrn
    Case "LEVEL"   : ScenValue = curLevel
    Case "X"       : ScenValue = cX
    Case "Y"       : ScenValue = cY
    Case "FACE"    : ScenValue = cFace
    Case "POSN"    : ScenValue = cPosn
    Case "SEQ"     : ScenValue = cSeq
    Case "ACTION"  : ScenValue = cAction
    Case "FALLING" : ScenValue = cFalling
    Case "XVEL"    : ScenValue = cXVel
    Case "YVEL"    : ScenValue = cYVel
    Case "FLOAT"   : ScenValue = weightless
    Case "ALIVE"   : ScenValue = Choice(cLife <> 0, 1, 0)
    Case "STR"     : ScenValue = kidStr
    Case "MAXSTR"  : ScenValue = maxKidStr
    Case "OPPSTR"  : ScenValue = oppStr
    Case "OPPPOSN" : ScenValue = gRec(0)
    Case "OPPX"    : ScenValue = gRec(1)
    Case "OPPY"    : ScenValue = gRec(2)
    Case "OPPFALL" : ScenValue = gRec(14)
    Case "MAXOPP"  : ScenValue = maxOppStr
    Case "SWORD"   : ScenValue = gotSword
    Case "DRAWN"   : ScenValue = cSword
    Case "DROPPED" : ScenValue = droppedOut
    Case "ALERT"   : ScenValue = enemyAlert
    Case "NOISE"   : ScenValue = alertGuard
    Case "OPPROW"  : ScenValue = FrameRowOf(gRec(11), gRec(0))
    Case "OPDIST"  : ScenValue = OpDistS()
    Case "OPEN"    : ScenValue = exitOpen
    Case "OVER"    : ScenValue = gameOver
    Case "DONE"    : ScenValue = levelDone
    Case "FRAMES"  : ScenValue = scenFrames
    ' The counters, all of them zeroed by AT, so these say what this scenario
    ' did rather than what the whole run has done so far.
    Case "BUMPS"   : ScenValue = nBumps
    Case "FELL"    : ScenValue = nStepOff
    Case "LANDED"  : ScenValue = nSoft + nMed + nHard
    Case "HARD"    : ScenValue = nHard
    Case "GRABS"   : ScenValue = nGrabs
    Case "GATES"   : ScenValue = nGates
    Case "CROSS"   : ScenValue = nCross
    Case "DEATHS"  : ScenValue = nDead
    Case "IMPALED" : ScenValue = nImpaled
    Case "PLATES"  : ScenValue = nPlates
    Case "STEPS"   : ScenValue = nSteps
    Case "CLIMBS"  : ScenValue = nClimb
    Case "CLIMBDOWN" : ScenValue = nClimbDown
    Case "JUMPHANG" : ScenValue = nJumpHang
    Case "POTIONS" : ScenValue = nPotions
    Case "STOOPS"  : ScenValue = nStoop
    Case "DIST"    : ScenValue = GetDist()
    Case "FWDDIST" : ScenValue = GetFwdDist()
    Case "FWDKIND" : ScenValue = fwdKind
    Case "FWDTYPE" : ScenValue = fwdType
    Case "REPEAT"  : ScenValue = cRepeat
    Case "KNOCKS"  : ScenValue = nGateKnocks
    Case "CLOCK"   : ScenValue = frameCount
    Case "MILESTONE" : ScenValue = milestone
    Case "CRUSHED" : ScenValue = nCrush
    Case "MINLEFT" : ScenValue = minLeft
    Case "BASEX"   : ScenValue = BaseX()
    Case "DROPS"   : ScenValue = nDrop
    Case "STRIKES" : ScenValue = nStrikes
    Case "GUARDS"  : ScenValue = nGuardsDead
    Case "SHADOWS" : ScenValue = nShadows
    Case "MERGES"  : ScenValue = nMerges
    Case "BRIDGE"  : ScenValue = nBridge
    Case "MICE"    : ScenValue = nMice
    Case "OPPID"   : ScenValue = Choice(gdPresent, gRec(11), -1)
    Case "OPPBY"   : ScenValue = gRec(5)
    Case "GONE"    : ScenValue = nGuardsGone
    Case Else      : Error "unknown expectation " + what
  End Select
End Function

' Put a block's modifier byte where a scenario needs it, and take that block
' out of the list of moving things so that nothing winds it on again.  A
' slicer left to itself cycles, which is right for the game and useless for a
' test: this is how a scenario says "shut, and staying shut".
Sub ScSpec(s As INTEGER, bx As INTEGER, by As INTEGER, v As INTEGER)
  Local INTEGER k, loc
  loc = by * COLS + bx
  SetSpec s, loc, v
  For k = 0 To numTrans - 1
    If trLoc(k) = loc And trScrn(k) = s Then trDir(k) = -1
  Next k
  Print "  spec scr " + Str$(s) + " blk " + Str$(bx) + "," + Str$(by) + " held at " + Str$(v)
End Sub

' AT starts a scenario from a known slate.
Sub ScZero
  scenFrames = 0
  ' A scenario that ended the game must not decide the ones after it.
  gameOver = 0 : message = 0 : msgTimer = 0 : weightless = 0
  nBumps = 0 : nStepOff = 0 : nSoft = 0 : nMed = 0 : nHard = 0
  nGrabs = 0 : nGates = 0 : nCross = 0 : nDead = 0 : nImpaled = 0
  nPlates = 0 : nSteps = 0 : nClimb = 0 : nDrop = 0 : nClimbDown = 0 : nStoop = 0 : nJumpHang = 0 : nPotions = 0 : nGateKnocks = 0 : nCrush = 0
  nStrikes = 0 : nGuardsDead = 0 : nShadows = 0 : nMerges = 0 : nBridge = 0 : nMice = 0 : nGuardsGone = 0
End Sub

' One line saying where he is and what he is doing, the same shape every
' frame, so that two runs can be compared with a text diff.
Function ScState$() As STRING
  Local STRING t
  t = "blk " + Str$(cBlockX) + "," + Str$(cBlockY) + " scr " + Str$(cScrn)
  t = t + " x " + Str$(cX) + " y " + Str$(cY) + " posn " + Str$(cPosn)
  t = t + " act " + Str$(cAction) + " fall " + Str$(cFalling)
  t = t + " yv " + Str$(cYVel) + " sw " + Str$(cSword) + " life " + Str$(cLife) + " d " + Str$(GetDist())
  ' What the frame decided, which is usually the thing being looked for.
  If lastWhat <> "" Then t = t + " [" + lastWhat + "]"
  ' Whoever else is on the screen, since half of what a scenario asks about
  ' is what he is doing: pose, x, block, sword, life and what is left of him.
  If gdPresent Then
    t = t + " dist " + Str$(OpDistS())
    t = t + " | gd id " + Str$(gRec(11)) + " p" + Str$(gRec(0)) + " x" + Str$(gRec(1))
    t = t + " blk" + Str$(gRec(4)) + "," + Str$(gRec(5)) + " fall" + Str$(gRec(14))
    t = t + " sw" + Str$(gRec(12)) + " life" + Str$(gRec(13)) + " str" + Str$(oppStr)
  End If
  ScState$ = t
End Function

' Where every block of a type is on a level.
Sub ScFind(lv As INTEGER, ty As INTEGER)
  Local INTEGER s, loc, n
  LoadLevel lv : loadedLevel = lv
  Print "find: level " + Str$(lv) + " type " + Str$(ty)
  For s = 1 To 24
    For loc = 0 To 29
      If BType(s, loc) = ty Then
        Print "  scr " + Str$(s) + " blk " + Str$(loc Mod COLS) + "," + Str$(loc \ COLS) + " spec " + Str$(BSpec(s, loc))
        n = n + 1
      End If
    Next loc
  Next s
  Print "  " + Str$(n) + " of them"
End Sub

' One screen laid out: the block types, the modifier bytes beneath them, and
' the four screens this one joins onto.
Sub ScShow(lv As INTEGER, s As INTEGER)
  Local INTEGER r, c
  Local STRING t, u
  LoadLevel lv : loadedLevel = lv
  Print "show: level " + Str$(lv) + " screen " + Str$(s)
  For r = 0 To ROWS - 1
    t = "  row " + Str$(r) + " type" : u = "          spec"
    For c = 0 To COLS - 1
      t = t + Pad$(Str$(BType(s, r * COLS + c)), 4)
      u = u + Pad$(Str$(BSpec(s, r * COLS + c)), 4)
    Next c
    Print t
    Print u
  Next r
  t = "  left " + Str$(level(OFF_MAP + (s - 1) * 4))
  t = t + " right " + Str$(level(OFF_MAP + (s - 1) * 4 + 1))
  t = t + " up " + Str$(level(OFF_MAP + (s - 1) * 4 + 2))
  t = t + " down " + Str$(level(OFF_MAP + (s - 1) * 4 + 3))
  Print t
End Sub

' ---- the small amount of text handling the directives need ------------------

' Tabs count as spaces, and a comment is everything after an apostrophe that
' is not inside quotes - the same rule the build script uses on this file.
Function ScClean$(s As STRING) As STRING
  Local INTEGER i, q
  Local STRING t, ch
  For i = 1 To Len(s)
    ch = Mid$(s, i, 1)
    If ch = Chr$(34) Then q = 1 - q
    If ch = Chr$(39) And q = 0 Then Exit For
    If ch = Chr$(9) Or ch = Chr$(13) Then ch = " "
    t = t + ch
  Next i
  ' trailing spaces would otherwise be frames of no input
  Do While Len(t) > 0 And Right$(t, 1) = " "
    t = Left$(t, Len(t) - 1)
  Loop
  ScClean$ = t
End Function

' The nth space-separated word, or "" past the end of the line.
Function ScWord$(s As STRING, n As INTEGER) As STRING
  Local INTEGER i, k, st, l
  l = Len(s) : i = 1
  ScWord$ = ""
  Do
    Do
      If i > l Then Exit Function
      If Mid$(s, i, 1) <> " " Then Exit Do
      i = i + 1
    Loop
    st = i
    Do
      If i > l Then Exit Do
      If Mid$(s, i, 1) = " " Then Exit Do
      i = i + 1
    Loop
    k = k + 1
    If k = n Then ScWord$ = Mid$(s, st, i - st) : Exit Function
  Loop
End Function

' The line from its nth word on, with the spacing inside it kept.
Function ScRest$(s As STRING, n As INTEGER) As STRING
  Local INTEGER i, k, l
  l = Len(s) : i = 1
  ScRest$ = ""
  Do
    Do
      If i > l Then Exit Function
      If Mid$(s, i, 1) <> " " Then Exit Do
      i = i + 1
    Loop
    k = k + 1
    If k = n Then ScRest$ = Mid$(s, i) : Exit Function
    Do
      If i > l Then Exit Function
      If Mid$(s, i, 1) = " " Then Exit Do
      i = i + 1
    Loop
  Loop
End Function

' Val on a word, as an integer.
Function ScNum(s As STRING) As INTEGER
  If s = "" Then ScNum = 0 Else ScNum = Val(s)
End Function
