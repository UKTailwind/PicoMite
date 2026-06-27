' ============================================================
' Raycaster Demo for PicoMite MMBasic (RP2350)
' Continuous auto-play loop with animated sliding door
' Demonstrates: RAY MOVE, RAY TURN, RAY CELL, RAY CAST,
'               RAY SPRITE, RAY MINIMAP, RAY DOOR
' Press ESC at any time to quit
' ============================================================
OPTION EXPLICIT
MODE 2
CLS

' ---- Map dimensions ----
CONST MAP_W = 57
CONST MAP_H = 51
CONST FOV = 66

' ---- Start position ----
CONST START_X = 25.5
CONST START_Y = 23.5
CONST START_A = 0

' ---- Variables ----
DIM INTEGER x%, y%, i%, cx%, cy%, door_x%, door_y%, door_wall%
DIM k$
DIM FLOAT moveSpeed, rotSpeed

moveSpeed = 0.2
rotSpeed = 5.625   ' exactly 90 degrees per 16 steps

' ---- Door animation state ----
DIM FLOAT door_offset!, door_target!, door_step!
DIM INTEGER door_animating%
door_step! = 0.1   ' offset change per frame (10 frames to fully open/close)

' ---- Read map data into string array (1 byte/cell vs 8 for integer) ----
' Characters: '0'=empty, '1'-'5'=wall types (pre-varied for visual interest)
DIM m$(MAP_H - 1) LENGTH MAP_W
m$(0) = "123451234512345123451234512345123451234512345123451234512"
m$(1) = "234512345123451234512345100000000002345123451234512345123"
m$(2) = "345120000234512340103451200000000003451234512345123451234"
m$(3) = "451230000345123000000000000000000004002345123451234512345"
m$(4) = "512340000451234000000000400000000005003451234512345123451"
m$(5) = "123450234512345100451234500000000001234512345123451234512"
m$(6) = "234510000000001200512345100000000002345123451234512345123"
m$(7) = "345120000000012300103451234510045123451234512345123451234"
m$(8) = "451230000000000000004512345120051234512345123451234512345"
m$(9) = "512340000000000000005123451230012345123451234512345123451"
m$(10) = "100000000000040000001234512340023451234512345123451234512"
m$(11) = "234510000000051234512345123450034512345123451234512345123"
m$(12) = "300000451234012345123451234510045123451234512345123451234"
m$(13) = "400234510045123451234512000000000234512345123451234512345"
m$(14) = "502345120051234512345123001230012345123451234512345123451"
m$(15) = "100451230012345123451234002340023451234512345123451234512"
m$(16) = "200000040023451234512345003450034512345123451234512345123"
m$(17) = "300000450034512345123451234510045123451234512345123451234"
m$(18) = "400204012340123451234512345120050204002345123051234502345"
m$(19) = "500340000000034512345123051200002340123451234010000000451"
m$(20) = "100400000000045123451234000000000000200500040020000000002"
m$(21) = "200510000000051234512340000000000000045123451030000000123"
m$(22) = "300020000000002345123450000000000000000000000040000000004"
m$(23) = "400030000000023451234510000000000000000000000000000000045"
m$(24) = "500040000000034512345120000000000000020000000010000000001"
m$(25) = "100450000000045123451230000000000000004510045120000000012"
m$(26) = "200510000000051234512345120400004012345100001030000000123"
m$(27) = "300123450230512345123451234512045103001200010040123401034"
m$(28) = "400034510045123451234512345120001234512340003001200500045"
m$(29) = "500345120051234512345123451200002345123400030010005003001"
m$(30) = "100400030012345123451234512340003451234510005123451234512"
m$(31) = "200000040023451234512345123400034512345100050030000305123"
m$(32) = "300123450034512045123451234510045123451230000000000001234"
m$(33) = "400030500045120451204012345100050234512300000000000012345"
m$(34) = "500000000000000000000123451230002345123450230010305103451"
m$(35) = "100000000000000000000004512300003451234512345123451234512"
m$(36) = "200000000000000200000345123400034512345123451234512345123"
m$(37) = "345023401234012345123451234510005123451234512345123451234"
m$(38) = "451234512300100001234512345023051230512345123451234512345"
m$(39) = "512345123400000512345123000000000000123451234512345123451"
m$(40) = "123451234510345123451234000040020000034512345123451234512"
m$(41) = "234512345120051234512340000000000000345123451234512345123"
m$(42) = "345123451230012345123451234510045123451234512345123451234"
m$(43) = "451234512345123451234512000020050000512345123451234512345"
m$(44) = "512345123451234512345123000000000000023451234512345123451"
m$(45) = "123451234512345123451234000040020000234512345123451234512"
m$(46) = "234512345123451234512345123450034512345123451234512345123"
m$(47) = "345123451234512345123451000000000000451234512345123451234"
m$(48) = "451234512345123451234512000000000000512345123451234512345"
m$(49) = "512345123451234512345123001030500040123451234512345123451"
m$(50) = "123451234512345123451234512345123451234512345123451234512"

' ---- Set up framebuffer & raycaster ----
FRAMEBUFFER CREATE
FRAMEBUFFER WRITE F
RAY MAP MAP_W, MAP_H, m$()
RAY COLOUR 12, 3, 8, 1, 1, 3

' ---- Load 4bpp RGB121 sprite images via SPRITE LOADARRAY ----
CONST C_BLK = &h000000  ' index 0 - transparent
CONST C_RED = &hFF0000  ' index 8
CONST C_GRN = &h00FF00  ' index 6
CONST C_BLU = &h0000FF  ' index 1
CONST C_YEL = &hFFFF00  ' index 14
CONST C_CYN = &h00FFFF  ' index 7
CONST C_MAG = &hFF00FF  ' index 9
CONST C_WHT = &hFFFFFF  ' index 15

SPRITE SET TRANSPARENT 0  ' index 0 (black) is transparent
DIM INTEGER spr%(63)   ' reusable 8x8 pixel array

' Sprite 1: Red cross on black (transparent) background
FOR i% = 0 TO 63: spr%(i%) = C_BLK: NEXT i%
FOR i% = 0 TO 7
  spr%(i% * 8 + 3) = C_RED: spr%(i% * 8 + 4) = C_RED
  spr%(3 * 8 + i%) = C_RED: spr%(4 * 8 + i%) = C_RED
NEXT i%
SPRITE LOADARRAY 1, 8, 8, spr%()

' Sprite 2: Yellow diamond on black background
FOR i% = 0 TO 63: spr%(i%) = C_BLK: NEXT i%
FOR y% = 0 TO 7: FOR x% = 0 TO 7
  cx% = ABS(x% - 3): cy% = ABS(y% - 3)
  IF cx% + cy% <= 3 THEN spr%(y% * 8 + x%) = C_YEL
NEXT x%: NEXT y%
SPRITE LOADARRAY 2, 8, 8, spr%()

' Sprite 3: Green/cyan vertical stripes
FOR i% = 0 TO 63
  IF (i% MOD 8) MOD 2 = 0 THEN spr%(i%) = C_GRN ELSE spr%(i%) = C_CYN
NEXT i%
SPRITE LOADARRAY 3, 8, 8, spr%()

' Sprite 4: Magenta/blue checkerboard
FOR y% = 0 TO 7: FOR x% = 0 TO 7
  IF (x% + y%) MOD 2 = 0 THEN spr%(y% * 8 + x%) = C_MAG ELSE spr%(y% * 8 + x%) = C_BLU
NEXT x%: NEXT y%
SPRITE LOADARRAY 4, 8, 8, spr%()

' Sprite 5: White ring on black background
FOR i% = 0 TO 63: spr%(i%) = C_BLK: NEXT i%
FOR i% = 2 TO 5: spr%(0 * 8 + i%) = C_WHT: spr%(7 * 8 + i%) = C_WHT: NEXT i%
FOR i% = 2 TO 5: spr%(i% * 8 + 0) = C_WHT: spr%(i% * 8 + 7) = C_WHT: NEXT i%
spr%(1 * 8 + 1) = C_WHT: spr%(1 * 8 + 6) = C_WHT
spr%(6 * 8 + 1) = C_WHT: spr%(6 * 8 + 6) = C_WHT
SPRITE LOADARRAY 5, 8, 8, spr%()

' ---- Place billboard sprites in the open area ----
' RAY SPRITE id, spritenum, x!, y!
RAY SPRITE 0, 1, 32.5, 23.5   ' Red cross, ahead east
RAY SPRITE 1, 2, 32.5, 24.5   ' Yellow diamond, southeast
RAY SPRITE 2, 3, 26.5, 24.5   ' Green stripes, southwest
RAY SPRITE 3, 4, 26.5, 23.5   ' Checkerboard, behind west
RAY SPRITE 4, 5, 29.5, 22.5   ' White ring, just north

' ---- Pre-programmed input sequence (round trip) ----
DIM seq$
seq$ = "P5W22P3O1P10W15P5D16W5P5W5D16W30D16W5P5W5P5D16W15P3C1P10A16W5P5D32W5D16W22D16D16P5"

DIM INTEGER curpos%, reps%, r%
DIM cmd$

' ============================================================
' MAIN LOOP - runs continuously until ESC
' ============================================================
DO
  ' ---- Reset camera to start position ----
  RAY CAMERA START_X, START_Y, START_A, FOV

  ' ---- Rebuild door wall (north-south at x=30, y=19..24) ----
  RAY CELL 30, 19, 3
  RAY CELL 30, 20, 3
  RAY CELL 30, 21, 3
  RAY CELL 30, 22, 3
  RAY CELL 30, 23, 31   ' door (type 31 has door flag by default)
  RAY CELL 30, 24, 3

  ' ---- Reset door animation state ----
  door_offset! = 0.0
  door_target! = 0.0
  door_animating% = 0
  door_x% = 30: door_y% = 23: door_wall% = 31
  RAY DOOR CLEAR

  ' ---- Execute the sequence ----
  curpos% = 1
  DO WHILE curpos% <= LEN(seq$)
    cmd$ = MID$(seq$, curpos%, 1)
    curpos% = curpos% + 1

    ' Read repeat count
    reps% = 0
    DO WHILE curpos% <= LEN(seq$)
      k$ = MID$(seq$, curpos%, 1)
      IF k$ >= "0" AND k$ <= "9" THEN
        reps% = reps% * 10 + VAL(k$)
        curpos% = curpos% + 1
      ELSE
        EXIT DO
      ENDIF
    LOOP
    IF reps% = 0 THEN reps% = 1

    ' Execute reps% times
    FOR r% = 1 TO reps%
      k$ = INKEY$
      IF k$ = CHR$(27) THEN GOTO Done

      SELECT CASE cmd$
        CASE "W": RAY MOVE moveSpeed
        CASE "S": RAY MOVE -moveSpeed
        CASE "A": RAY TURN -rotSpeed
        CASE "D": RAY TURN rotSpeed
        CASE "O"
          ' Start door open animation
          door_target! = 1.0
          door_animating% = 1
          RAY DOOR door_x%, door_y%, door_offset!
        CASE "C"
          ' Start door close animation
          door_target! = 0.0
          door_animating% = 1
        CASE "P"
          ' Pause = render only, no movement
      END SELECT

      ' ---- Update door animation every frame ----
      IF door_animating% THEN
        IF door_target! > door_offset! THEN
          door_offset! = door_offset! + door_step!
          IF door_offset! >= door_target! THEN
            door_offset! = door_target!
            door_animating% = 0
          ENDIF
        ELSEIF door_target! < door_offset! THEN
          door_offset! = door_offset! - door_step!
          IF door_offset! <= door_target! THEN
            door_offset! = door_target!
            door_animating% = 0
            IF door_offset! <= 0.0 THEN
              RAY DOOR CLOSE door_x%, door_y%
            ENDIF
          ENDIF
        ELSE
          door_animating% = 0
        ENDIF
        IF door_offset! > 0.0 THEN
          RAY DOOR door_x%, door_y%, door_offset!
        ENDIF
      ENDIF

      ' ---- Render frame ----
      RAY RENDER
      RAY MINIMAP 2, 2, 48
      LINE MM.HRes\2 - 4, MM.VRes\2, MM.HRes\2 + 4, MM.VRes\2,, RGB(WHITE)
      LINE MM.HRes\2, MM.VRes\2 - 4, MM.HRes\2, MM.VRes\2 + 4,, RGB(WHITE)
      FRAMEBUFFER COPY F, N
      PAUSE 50
    NEXT r%
  LOOP

  ' Brief pause before restarting the loop
  PAUSE 500
LOOP

Done:
RAY CLOSE
FRAMEBUFFER CLOSE
CLS
PRINT "Raycaster demo ended."
END
