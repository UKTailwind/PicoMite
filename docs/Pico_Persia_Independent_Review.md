# Pico Persia: independent review of the port against Mechner's source

---

## 1. Findings that stop a level being completed

### 1.1 Slicers are permanent walls, and never cut
`Barrier()` (3443) returns 3 for type 18 unconditionally, and `HitBarrier`
(1109), `CrossScreen` (2772), `DoStepFwd` and `FollowKid` all treat that as a
wall. The original (`COLL.S CHECKCOLL :slicer`, 562) treats a slicer as a
barrier only while its state equals `slicerExt` (closed); otherwise you pass
through it. It also has `CHECKSLICE`/`CHECKSLICE2` (1186, 1266), which halve a
character whose collision edges overlap a closed slicer. The port has neither.

Consequence: the kid can never pass a slicer block. Level 4 needs the slicer at
screen 12 (3,0) passed to reach the up-plate that opens the gate on screen 14;
level 5 needs the two slicers on screen 24 row 1 passed to reach the plates
that open the thief's gate; every later level has slicers in corridors. Also,
once passable, they must kill when closed.

### 1.2 Level 6: the plunge does not end the level
`TOPCTRL.S NextFrame` (541-558) ends level 6 when the kid falls off screen 1
(`KidY < 20` after wrapping), and `AUTO.S cutchar :CUTDOWN` (1565) refuses to
cut down from level 6 screen 1. Screen 1's map entry has screen 3 below it, so
the port's `CrossScreen` (2772) simply moves him to screen 3 and the level
cannot end. The port has no level-6 special case at all.

### 1.3 Level 7 starts with a fatal fall, and the float potion does nothing
Two independent omissions.

(a) `SUBS.S ADDFALL` (1651) adds `CharXVel` to `CharX` every freefall frame.
The port's `Settle` (2233) applies only the vertical velocity; `cXVel` is set
by `OP_SETFALL` and never used for movement. On level 7 the kid starts on
screen 17 (2,2) over empty space and falls; in the original the 1-pixel-a-frame
drift of `stepfall` carries his base into column 1 just before he passes row 0
of screen 1, where there is floor (soft landing, velocity 21). Without the
drift he stays in column 2, which is empty on screens 1 and 3, and lands two
screens down at terminal velocity: dead on arrival. The drift also affects
every "jump short and grab" across a gap.

(b) `SUBS.S GRAVITY` (1618) uses acceleration 1 and terminal velocity 4 while
`weightless` is set, and caps normal falls at 33. The port always adds 3 with
no cap. The float potion is on level 7 screen 1 (8,2) and exists to survive the
long drop that follows. In the port it changes nothing.

### 1.4 Level 8: there is no mouse
`TOPCTRL.S misctimers` (1661) counts `exitopen` up on screen 16 and after 150
frames calls `MISC.S MOUSERESCUE` (352), which puts a character with ID 24 at
X 200 running left; `AUTO.S MouseProg` (198) turns him round at X 166 and
vanishes him at 200. On the way he presses the up-plate at (7,0), which is
how the player gets out. The port has no ID 24, no timer and no rescue.

### 1.5 Level 12 cannot be finished, for five separate reasons
1. `AUTO.S stealsword` (1605): cutting right into screen 18 removes the sword
   from screen 15 (1,0). The port never removes it, and `AddGuard` (1989) only
   adds the shadow when the sword is gone, so the shadow never appears.
2. `CTRL.S onground` (326-352): on level 12 after the merge (`mergetimer`
   negative), standing on empty space in row 0 of screen 2, or of screen 13 at
   column 6 or beyond, creates floor on the fly. That invisible bridge is the
   only way left from screen 15 to screen 23. The port has no `mergetimer` and
   no bridge.
3. `TOPCTRL.S NextFrame` (562-570): reaching screen 23 ends the level. The
   port's `EnterScreen` (3791) only knows about level 14 screen 5.
4. `AUTO.S ADDGUARD` (1756-1779) adds the shadow once, gated on `exitopen` and
   `mergetimer`. The port re-adds him every time screen 15 is entered, even
   after the merge.
5. `MISC.S UNHOLY` (450) and the level-12 half of `TOPCTRL.S chgmeters`
   (1313-1329): damage to either of kid and shadow is mirrored, and if the
   shadow dies the kid dies. The port has neither, so the shadow can simply be
   killed. Related: `DecStr` (4131) sends a character with ID 1 down the kid's
   branch, so damage to the shadow debits the kid's meter directly.

Also missing: `FinalShad :hold` (338) keeps the shadow at the ceiling until the
kid is left of X 150; the port drops him at once.

---

## 2. Defects that make later levels unplayable or unfair

### 2.1 No climb-down, no step-off-with-down
`CTRL.S standing :down` (1183): with a cliff in front and within 3 pixels,
"down" steps off; with a cliff behind and 8 or more pixels in, "down" turns
into `climbdown` (hang from the ledge behind you, via `checkledge` and the
gate threshold); otherwise crouch. The port's `StandCtrl` (1284) maps down to
`SEQ_STOOP` only. Descending two storeys safely is done by climbing down and
dropping; without it every descent costs health or a life.

### 2.2 Stepping off an edge while en garde soft-locks the kid
`CTRL.S startfall` (356) zeroes `CharSword` (and sets `droppedout` for the
player). The port's step-off in `Settle` (1230-1236) leaves `cSword` at 2, so
the fall ends in `SOFTLAND`'s crouch loop (pose 109) with `FightCtrl` in
charge, which accepts no input from pose 109. The kid crouches until killed.
Retreating off a ledge is the normal way out of a fight.

Same root: `droppedOut` is never set to 1 anywhere, so `FollowKid` never runs
and guards never come down after the kid.

### 2.3 Landing rules for armed characters
`CTRL.S hitflr :softland` (219): a guard, or anyone with the sword out, lands
in `landengarde`; the shadow always soft-lands; a guard cannot survive a
medium fall. The port always uses `SOFTLAND` (1264), so a guard who drops a
storey lands in the kid's crouch loop and, with `FightCtrl` in charge, never
gets up.

### 2.4 Dead characters stop obeying physics
`StepCharacter` (949-953) exits before `Settle` when `cLife = 0`. In the
original a dead character still runs `animchar`, `gravity`, `addfall`,
`checkfloor` (`hitflr` sends him to `hardland` if dead). Consequences:
- A guard killed at an edge (`StabChar` "knocked off", 1975) hangs in mid-air
  in pose 106 for ever.
- A guard who falls off the screen bottom is never removed: `AUTO.S CUTGUARD`
  (1397) removes him at Y >= 215 with `deadenemy` (and drops the skeleton onto
  screen 3). The port's `CutGuard` only handles the kid leaving. A live guard
  falling past row 2 keeps falling with `cY` wrapping, so he is drawn again
  from the top of the screen.

### 2.5 Two X-coordinate conventions are in use at once
The original computes a character's block with `GETBLOCKXP` (`CTRLSUBS.S`
560): subtract the 7-pixel perspective `angle`, then `BlockTable`. The port's
`RereadBlocks` (1033), `GetDist` (2280), `BlockOfX`, `DistFromX`, `TryGrab`'s
alignment and `HitBarrier`'s edges all use `(x - 2) \ 14 - 4`, i.e. no angle.
The kid's start is placed 7 pixels left to compensate (`58 + bx*14 + 7`,
lines 347 and 722; the original uses `BlockEdge + angle + 7`). But
`InitGuards` (2231, `BlockEdge + 14`), `BonesRise` (2093), `TryStairs` (2360),
`DoImpale` (4312) and `SmashMirror` (1500) copy the original's absolute
constants unchanged.

So: the kid is drawn 7 pixels (14 display pixels) left of where the original
draws him relative to the scenery; guards are drawn where the original draws
them but their block arithmetic is 7 pixels off; and `OpDist` between kid and
guard is biased by 7 pixels, in opposite directions for the two of them.
Recommended fix: adopt the original convention everywhere (block from
`base - 9`, block front edge at `14*(b+4)+9`, kid start `+14`).

### 2.6 The ledge reach goes the wrong way
`CTRL.S fallon` (268-273): `grabreach = -8` through `addcharx` moves the kid 8
pixels BACK before testing for a ledge. `TryGrab` (1055) moves him 8 pixels
forward (`cX - 8` when facing left is forward). The comment above it says
backwards; the code says forwards. The grab window is shifted 16 pixels.

### 2.7 While hanging, the wrong column is examined
In the original the hang frames' `Fdx` (7 down to -1, footmark 0) put the
base under the ledge, so `CharBlockX` IS the ledge column and `hanging`
(1408) uses `getabove` / `getunderft` / `getbehind` directly. The port aligns
`cX` rather than the base to the block edge (1071), so during the hang cycle
`cBlockX` alternates between the ledge column (poses 87-92) and the gap
column (93-99) for a left-facer, and sits in the ledge column throughout for a
right-facer. `HangCtrl` (2375) then takes `ahead = cBlockX +/- 1` as the ledge:
- facing right, "ledge gone, drop" (`noFloor(above)`, 2424) looks two blocks
  past the gap, so a right-facing hang lets go whenever that block is empty;
- the hangstraight / sheer-face tests use `under = (cBlockX, y)`, the gap, so
  the -7 nudge on dropping beside a wall never happens, and `FallSeq`'s +5
  then walks the base into the wall column;
- `CheckPress` while hanging (4245) reads above the gap, so a plate cannot be
  pressed by hanging from it;
- the gate-height read at 2397 indexes `level(720 + ...)` with `cBlockY - 1`
  and `cBlockX` raw, wrong on the top row and off-screen.
Fix: align the base (`MoveFwd GetDist()`) on the grab as the original does,
then use `cBlockX` as the ledge column exactly as `hanging` does.

### 2.8 CheckAlert treats the shadow as an enemy
`MISC.S CHECKALERT` (827-835): a shadow (ID 1) gives alert 0 unless the level
is 12. The port's version (1393) has no ID test, so on levels 4, 5 and 6 the
kid auto-draws (`StandCtrl` 1293) whenever the shadow is on his row within 90
pixels. On level 6 that is exactly where the running jump the shadow reacts
to has to be made. Also the gate threshold is `4*16` (1423); the original's
`gfightthres` is `28*4`.

### 2.9 Careful step semantics
`COLL.S GETFWDDIST` (822): next block clear floor -> a full 11-pixel step;
space or loose -> step to the edge, then `testfoot`, then step through;
plate, sword, flask -> step to the edge with no foot test; barrier -> step up
to the barrier's edge (image-based). The port's `DoStepFwd` (2248) always
steps to the end of the current block and then foot-tests, whatever is next,
so on open floor every block boundary produces a foot test and the step
length is wrong. `DoStartrun`'s "step instead of run when within 8 pixels of
a barrier" (1537) is also absent.

### 2.10 Up with the button held is a plain jump
`CTRL.S standing :2` (1102-1115): button down + fresh up goes to the same
`:up` handler as button up (stairs check, then up+forward standjump, then
`DoJumpup` with the ledge logic). `StandCtrl` (1310) returns `SEQ_JUMPUP`
directly, so a player who holds the action key while pressing up never grabs
a ledge and cannot climb stairs.

### 2.11 Control dispatch gaps
`CTRL.S GENCTRL` (668-731):
- action 5 (bump) and 4 accept no input; the port lets `StandCtrl` run during
  the bump's poses 50-52;
- poses 1-3 (`starting`): up + forward -> standing jump; poses 67-69
  (`stjumpup`): forward -> standing jump; pose 48 (`turning`): forward held
  -> `turnrun`. None are in the port (`StepCharacter` 964-972), so a jump
  pressed a frame late is lost and turn-then-run waits for the full turn;
- shadow: down + forward -> `DoEngarde` (1043-1051), used by `FinalShad`.

### 2.12 Potion effects apply to whoever drinks
`MISC.S POTIONEFFECT` (257) returns at once unless `CharID = 0`. The port's
`PotionEffect` (4215) has no such test. On level 5 the thief drinks the
"bigger meter" potion at screen 24 (3,0) and the kid gets the upgrade.

### 2.13 Guards saved and restored wrongly
- `AUTO.S updateguard` (1343) skips IDs 1 and 24. The port's `UpdateGuard`
  (2219) saves the shadow, so a normal guard appears where the thief or the
  mirror shadow was left.
- `AUTO.S AddNormalGd` (1856-1863) makes the level-3 guard ID 4 (skeleton,
  sword out, `landengarde`). The port's `AddGuard` always uses ID 2, so the
  skeleton, once left and revisited, is a mortal guard.
- `updateguard` stores block `BlockY*10 + 0`; the port stores
  `gRec(5)*COLS + gRec(4)`, and `gRec(4)` can be -2..11 because guards are
  clamped to X 40..215, so a guard left near a screen edge comes back on the
  wrong row.
- The clamp itself (985-986) also stops the level-4 shadow, who must run off
  to the right until X wraps below 80 and vanishes (`ShadLevel4`, 308); he
  sticks at 215 for ever.

### 2.14 CheckGate shoves without the straddle test
`COLL.S CHECKGATE` (1319) shoves only when the character's collision edges
have overlapped both edges of the bars for two frames AND the gate is too low.
The port (2110) shoves whenever a standing/crouching character is in the gate
block or the block to its right and the gate is not open enough, so standing
beside a closed gate on its right is pushed away 5 pixels a frame with a
smack-wall sound each frame.

### 2.15 Level-14 timer and end-of-time rules
`TOPCTRL.S` (576-596): the clock stops on level 14 and on level 13 once the
vizier is dead, and time running out never loses on level 13 or later. The
port's `GetMinLeft` (2049) ends the game at zero on any level, and `KeepTime`
(2033) keeps counting on level 14.

---

## 3. Moderate

- **Falling into a solid block.** `CTRL.S falling :1` (88-93): a solid block
  under a falling character calls `InsideBlock` to push him out sideways.
  `Settle` (1247) treats it as no floor and falls through it.
- **Gravity during the fall's first four frames.** The port sets `cFalling`
  on the step-off frame and adds gravity from then on, while the sequence's
  `chy` also moves him; the original applies gravity only in action 4. Falls
  are about 30 px lower after four frames and about a frame shorter. No
  terminal velocity cap.
- **Crush by a falling floor.** `MOVER.S crushchar` (1942): running poses
  5-14 escape (except level 13); on the ground the character is aligned to the
  floor, loses 1 and plays `crush` (medland), or `hardland` if it kills. The
  port's `CheckCrush` (4111) just debits strength.
- **Mid-air wall hits.** `HitBarrier` exits unless the frame has the ground
  mark; the original bumps in the air too (`AirBump`/`hardbump`), and stops
  the character where his image edge meets the bar (`BarL`/`BarR`), not where
  `cX` meets the block edge. A running jump into a wall now lands inside it
  and is pushed out a frame later.
- **Level 3 checkpoint.** `AUTO.S milestone3` (1589) and `SUBS.S STARTKID
  :special3` (1483): after screen 7 a death restarts at screen 2 block 6 with
  the loose floor at screen 7 (4,0) removed and strength banked. Missing.
- **Level 13 entrance.** `SUBS.S CRUMBLE` (108): entering screens 16 or 23
  breaks loose floors 2-7 on the bottom row of the screen above. Missing.
- **Start facing and sequence.** `STARTKID` (1516) inverts the facing byte and
  starts every normal level with `turn`, level 1 with `stepfall` and level 13
  with `running`. The port uses the raw byte with `stand`: level 1 starts
  facing left instead of right; level 13 starts standing, facing right,
  instead of running left.
- **The entrance slam.** `TOPCTRL.S entrance` (1361): the exit on the start
  screen opens fully and slams shut at level start. Missing (cosmetic).
- **`gotSword` after a level-1 death.** `RESTART` (330-335) clears it on
  level 1; the port keeps it, so after picking up the sword and dying the kid
  restarts armed with the sword also lying on the floor.
- **OpDist clamp.** `GETOPDIST` (2052-2076) returns -127 when the opponent is
  more than 127 pixels behind; `OpDist` (1373) returns +127 for that case.
- **Turn-and-draw.** `DoTurn` (1563): turning with an armed enemy behind uses
  `turndraw`. Missing.
- **StabChar of a defenceless character** still uses the knock-off-the-edge
  branch in the original (`:DL` jumps to `:killed`); the port goes straight to
  `STABKILL`.
- **Landing details** from `hitflr` (150-176): nudge back 3 if within 4 px of
  an edge; check the block behind for spikes when 12+ px in; `addslicers` on
  landing and in `startfall`. All missing.
- **Skeleton strength.** `BONESRISE` leaves `MaxOppStr` alone and `STABCHAR`
  keeps the skeleton invincible only for ID 4; the port re-adds him as ID 2
  (see 2.13).

## 4. Minor

- `FrameRow` (2638) applies the 102-106 -> 172-176 substitution to the shadow
  (ID 1); `usealtsets` (1685) does it only for ID >= 2.
- `CanGrab` ignores a loose floor that is already falling (`CHECKLEDGE` 1841).
- `DoJumphigh` (1880) reads the block above the hand position and touches the
  ceiling under a solid block; `DoJumpup` (2327) does `HIGHJUMP` under one.
- `WallBump`/`dobump` omit pose 24 from the hard-bump list and never set
  `alertguard` (`BumpSound`, 704).
- `RunCtrl` stop does not clear the press flags (`:rs jsr ]clr`).
- `CheckSpikes` scans one column; the original scans from left to right image
  edge (`CHECKSPIKES`, 1868).
- `EnemyColl` counts `panelwof`, mirror and slicer as things to be backed
  into, and ignores the block behind when facing right (`ENEMYCOLL`, 1412).
- `Shad12`'s sheathe sets `cSword = 0` with no `resheathe` animation.
- The mirror reflection (`MISC.S REFLECTION`) is not drawn.

---

## 5. Where each level stops, on this reading

| Level | Blocked by |
|-------|------------|
| 2, 3 | Playable in principle; 2.1, 2.2, 2.5-2.9 will bite. Skeleton revisits (2.13). |
| 4 | Slicer on screen 12 (1.1); shadow stuck at X 215 and treated as an enemy (2.8, 2.13). |
| 5 | Slicers on screen 24 (1.1); thief's potion goes to the kid (2.12). |
| 6 | Plunge does not end the level (1.2); kid auto-draws on the shadow (2.8). |
| 7 | Dead on arrival (1.3a); float potion inert (1.3b). |
| 8 | No mouse (1.4). |
| 9-11 | Slicers (1.1); climb-down (2.1). |
| 12 | Five omissions (1.5). |
| 13 | Timer rules (2.15); crumble trap absent. |
| 14 | Clock keeps running and can lose (2.15). |

Suggested order of work: 1.1, 1.3a (x velocity), 2.2, 2.1, 2.5, 1.3b, then
the per-level specials (1.2, 1.4, 1.5), then the rest.
