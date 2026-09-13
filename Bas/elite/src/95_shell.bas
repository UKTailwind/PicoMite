' =====================================================================
'  The game shell: docked, in flight, and the screens either side
'
'  Two states with a loop each.  In flight the frame loop runs as fast as
'  it can and the whole universe moves; docked, nothing moves at all, so
'  the screen is only repainted when a key has changed something and the
'  loop can afford to block on the keyboard between times.
'
'  The function keys follow the original's docked menu, shifted one place
'  because the BBC had an f0 and a PC keyboard does not:
'
'        docked                        in flight
'    F1  launch                    F1  front view
'    F2  buy cargo                 F2  rear view
'    F3  sell cargo                F3  left view
'    F4  equip ship                F4  right view
'    F5  galactic chart            F5  galactic chart
'    F6  short range chart         F6  short range chart
'    F7  data on system            F7  data on system
'    F8  market prices             F8  market prices
'    F9  status                    F9  status
'    F10 inventory                 F10 inventory
'
'  The six information screens are the same code in both states, so they
'  run as a small modal loop of their own and hand control back where it
'  came from.  Modal is a departure: the original keeps flying while you
'  read the chart, and you can be shot at while you do it.  Ours cannot
'  afford that - the long range chart walks all 256 systems and generates
'  each one's seeds to place its dot, which is far too much work to repeat
'  every frame, and it has nowhere to keep the finished picture.
' =====================================================================

SUB RunGame
  quitGame = 0
  frames = 0
  tFlight = 0
  DO
    IF docked THEN
      RunDocked
    ELSE
      RunFlight
    ENDIF
  LOOP UNTIL quitGame OR dead
END SUB

' --- in flight
SUB RunFlight
  LOCAL FLOAT t0
  t0 = TIMER
  DO
    ReadKeys
    IF kQuit THEN quitGame = 1 : EXIT DO
    UpdatePlayer
    IF kFire THEN FireLaser
    IF kTarget THEN TargetMissile
    IF kMissile THEN LaunchMissile
    IF kECM THEN FireECM
    IF kDock THEN dockComp = 1 - dockComp
    IF dockComp THEN DockingComputer
    IF kJump THEN JumpAway
    IF kChart > 0 THEN
      ' The information screens stop the clock as well as the universe,
      ' so the time spent reading one is not counted as time flying.
      tStage = TIMER
      InfoScreen kChart
      t0 = t0 + TIMER - tStage
    ENDIF
    IF lasTimer > 0 THEN lasTimer = lasTimer - 1
    IF lasFlash > 0 THEN lasFlash = lasFlash - 1
    tStage = TIMER
    MoveShips
    Missiles
    Tactics
    ECMService
    Recharge
    StationCheck
    StationPolice
    SpawnTraffic
    DockCheck
    prof(5) = prof(5) + TIMER - tStage
    DrawFrame
    IF demoMode THEN DemoCaption
    FRAMEBUFFER COPY F, N, B
    IF kPause THEN PauseGame
    mcnt = (mcnt + 1) AND 255
    frames = frames + 1
  LOOP UNTIL dead OR docked
  tFlight = tFlight + TIMER - t0
END SUB

' Stop the world.  The frame that is already on the screen stays there, so
' this is also how to look at something for longer than it lasts.
SUB PauseGame
  LOCAL INTEGER k
  TEXT VCX, VIEWH - 12, "PAUSED", "CT", 7, 1, cWhite
  FRAMEBUFFER COPY F, N
  k = WaitKey(0)
END SUB

' Hyperspace.  Only outside the safe zone, only if the tank will cover it,
' and never to the system we are already sitting in.
SUB JumpAway
  IF inSafe THEN EXIT SUB
  IF selSys = homeSys THEN EXIT SUB
  IF CanReach(selSys) = 0 THEN EXIT SUB
  Hyperspace selSys
END SUB

' --- docked
'
' The repaint is the expensive thing here, not the input, so it happens
' once per key rather than once per pass.  DockKey blocks until there is
' something to do, which leaves the machine idle instead of spinning.
SUB RunDocked
  LOCAL INTEGER k, dirty
  dscreen = SCR_STATUS
  dsel = 0
  dirty = 1
  DO
    IF dirty THEN
      DrawDocked
      FRAMEBUFFER COPY F, N
      dirty = 0
    ENDIF
    k = DockKey()
    dirty = 1
    SELECT CASE k
      CASE 27                                  ' escape leaves the game
        quitGame = 1
        EXIT SUB
      CASE 145                                 ' F1: launch
        LaunchTunnel
        docked = 0
        dockComp = 0
        msLock = -1
        LaunchState
        EXIT SUB
      CASE 146 : dscreen = SCR_MARKET : dbuy = 1 : dsel = 0
      CASE 147 : dscreen = SCR_MARKET : dbuy = 0 : dsel = 0
      CASE 148 : dscreen = SCR_EQUIP  : dsel = 0
      CASE 149 : dscreen = SCR_LONG
      CASE 150 : dscreen = SCR_SHORT
      CASE 151 : dscreen = SCR_DATA
      CASE 152 : dscreen = SCR_MARKET : dbuy = 1 : dsel = 0
      CASE 153 : dscreen = SCR_STATUS
      CASE 154 : dscreen = SCR_INVENT
      CASE 128 : DockUp
      CASE 129 : DockDown
      CASE 130 : DockLeft
      CASE 131 : DockRight
      CASE 32, 13                              ' space or return: do it
        DockAct
      CASE 70, 102                             ' F: fill the tank
        IF dscreen = SCR_EQUIP THEN BuyFuel
      CASE 83, 115                             ' S: save the commander
        SaveCommander CMDRFILE
        dscreen = SCR_STATUS
      CASE 76, 108                             ' L: load one back
        IF LoadCommander(CMDRFILE) THEN dscreen = SCR_STATUS
      CASE ELSE
        dirty = 0
    END SELECT
  LOOP
END SUB

' What space does depends on which screen is showing.
SUB DockAct
  SELECT CASE dscreen
    CASE SCR_MARKET
      IF dbuy THEN BuyOne dsel ELSE SellOne dsel
    CASE SCR_EQUIP
      BuyShopItem
  END SELECT
END SUB

' One key, waited for.  Docked there is nothing to animate, so blocking
' here costs nothing.  KEYDOWN is not used because every call to it
' empties the console buffer INKEY$ reads from.
FUNCTION DockKey() AS INTEGER
  IF demoMode THEN DockKey = DemoKey() : EXIT FUNCTION
  DockKey = WaitKey(0)
END FUNCTION

SUB DrawDocked
  SELECT CASE dscreen
    CASE SCR_STATUS
      StatusScreen
      DockFooter "F1 launch   S save   L load"
    CASE SCR_INVENT
      InventoryScreen
      DockFooter "F1 launch   F2 buy   F3 sell"
    CASE SCR_MARKET
      MarketScreen dsel
      IF dbuy THEN
        DockFooter "up/down choose   SPACE buy one   F3 sell"
      ELSE
        DockFooter "up/down choose   SPACE sell one   F2 buy"
      ENDIF
    CASE SCR_EQUIP
      EquipScreen dsel
      DockFooter "up/down choose   SPACE buy   F fill the tank"
    CASE SCR_LONG
      ChartLong
      DockFooter "arrows move the cursor   F7 data   F1 launch"
    CASE SCR_SHORT
      ChartShort
      DockFooter "arrows move the cursor   F7 data   F1 launch"
    CASE SCR_DATA
      SysDataScreen
      DockFooter "F5 galactic chart   F6 short range   F1 launch"
  END SELECT
END SUB

' The footer says what the keys do - except while the demo is playing,
' when the only key that matters is any of them.
SUB DockFooter(t$)
  IF demoMode THEN
    TEXT VCX, SCRH - 9, "DEMONSTRATION - PRESS ANY KEY TO PLAY", "CT", 7, 1, cGrey
  ELSE
    TEXT VCX, SCRH - 9, t$, "CT", 7, 1, cGrey
  ENDIF
END SUB

' The four arrows mean different things on different screens: a row on a
' list, a light year on a chart.
SUB DockUp
  IF dscreen = SCR_MARKET OR dscreen = SCR_EQUIP THEN
    dsel = dsel - 1
    IF dsel < 0 THEN dsel = 0
  ELSE
    curY = curY - 4 : ChartMoved
  ENDIF
END SUB

SUB DockDown
  LOCAL INTEGER nrows
  IF dscreen = SCR_MARKET THEN
    nrows = NGOODS - 1
  ELSEIF dscreen = SCR_EQUIP THEN
    nrows = ShopCount() - 1
  ELSE
    curY = curY + 4 : ChartMoved
    EXIT SUB
  ENDIF
  dsel = dsel + 1
  IF dsel > nrows THEN dsel = nrows
  IF dsel < 0 THEN dsel = 0
END SUB

SUB DockLeft
  IF dscreen <> SCR_MARKET AND dscreen <> SCR_EQUIP THEN
    curX = curX - 2 : ChartMoved
  ENDIF
END SUB

SUB DockRight
  IF dscreen <> SCR_MARKET AND dscreen <> SCR_EQUIP THEN
    curX = curX + 2 : ChartMoved
  ENDIF
END SUB

' Keep the cursor on the map and pick out whatever system is nearest it.
SUB ChartMoved
  IF curX < 0 THEN curX = 0
  IF curX > 255 THEN curX = 255
  IF curY < 0 THEN curY = 0
  IF curY > 255 THEN curY = 255
  FindSystem curX, curY
END SUB

' --- the equipment shop
'
' The shop only lists what this system is advanced enough to sell, so the
' row the cursor is on is not the item's number and has to be counted back.
FUNCTION ShopCount() AS INTEGER
  LOCAL INTEGER i, k
  GotoSystem gGal, homeSys
  SysData
  k = 0
  FOR i = 0 TO NEQUIP - 1
    IF eqTech(i) <= sysTech + 1 THEN k = k + 1
  NEXT i
  ShopCount = k
END FUNCTION

SUB BuyShopItem
  LOCAL INTEGER i, k
  GotoSystem gGal, homeSys
  SysData
  k = 0
  FOR i = 0 TO NEQUIP - 1
    IF eqTech(i) <= sysTech + 1 THEN
      IF k = dsel THEN BuyEquip i : EXIT SUB
      k = k + 1
    ENDIF
  NEXT i
END SUB

' --- the information screens, from either state
'
' which: 1 galactic chart, 2 short range chart, 3 system data,
'        4 market prices, 5 status, 6 inventory - F5 to F10 in order.
SUB InfoScreen(which AS INTEGER)
  LOCAL INTEGER k, w, dirty
  w = which
  dirty = 1
  DO
    IF dirty THEN
      SELECT CASE w
        CASE 1 : ChartLong
        CASE 2 : ChartShort
        CASE 3 : SysDataScreen
        CASE 4 : MarketScreen(-1)
        CASE 5 : StatusScreen
        CASE 6 : InventoryScreen
      END SELECT
      DockFooter "F5-F10 screens   arrows move   F1-F4 back"
      FRAMEBUFFER COPY F, N
      dirty = 0
    ENDIF
    k = DockKey()
    dirty = 1
    SELECT CASE k
      CASE 128 : curY = curY - 4 : ChartMoved
      CASE 129 : curY = curY + 4 : ChartMoved
      CASE 130 : curX = curX - 2 : ChartMoved
      CASE 131 : curX = curX + 2 : ChartMoved
      CASE 149 TO 154 : w = k - 148
      ' A view key leaves the screen and selects that view, as it does in
      ' the original.
      CASE 145 TO 148 : vw = k - 145 : EXIT SUB
      CASE 13, 27 : EXIT SUB
      CASE ELSE : dirty = 0
    END SELECT
  LOOP
END SUB

' --- starting
'
' A new commander begins docked at Lave with a hundred credits, three
' missiles and a pulse laser, which is where every game of Elite starts.
SUB NewGame
  NewCommander
  ClearSlots
  docked = 1
  dscreen = SCR_STATUS
  dbuy = 1
END SUB
