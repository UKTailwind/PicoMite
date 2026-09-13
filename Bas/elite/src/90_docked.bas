' =====================================================================
'  Docked: status, inventory, the market and the equipment shop
'
'  Everything here is a screen over state that already exists.  The market
'  prices were fixed the moment we arrived and do not move while we trade;
'  buying and selling adjust the hold and the till and nothing else.  The
'  equipment on offer depends on the system's technology level, which is
'  why the good lasers are only sold in the developed systems and why a
'  poor agricultural world is a bad place to be caught short.
' =====================================================================

SUB EquipTable
  LOCAL INTEGER i
  RESTORE dat_equip
  FOR i = 0 TO NEQUIP - 1
    READ eqName$(i), eqPrice(i), eqTech(i)
  NEXT i
END SUB

' --- status
SUB StatusScreen
  LOCAL INTEGER y
  CLS
  GotoSystem gGal, homeSys
  SysData
  TEXT VCX, 4, "COMMANDER JAMESON", "CT", 7, 1, cWhite
  LINE 0, 19, SCRW - 1, 19, 1, cWhite
  y = 28
  DataLine y, "Present System", SysName$() : y = y + 11
  DataLine y, "Hyperspace Arm", SysName2$(selSys) : y = y + 11
  DataLine y, "Condition", CondName$() : y = y + 11
  DataLine y, "Fuel", STR$(pFuel / 10) + " Light Years" : y = y + 11
  DataLine y, "Cash", STR$(cashTenths / 10) + " Cr" : y = y + 11
  DataLine y, "Legal Status", LegalName$() : y = y + 11
  DataLine y, "Rating", RankName$() : y = y + 14
  TEXT 20, y, "Equipment:", "LT", 7, 1, cWhite : y = y + 11
  IF eqOwned(1) THEN TEXT 30, y, "Large Cargo Bay", "LT", 7, 1, cYellow : y = y + 10
  IF eqOwned(2) THEN TEXT 30, y, "E.C.M. System", "LT", 7, 1, cYellow : y = y + 10
  IF eqOwned(EQ_SCOOPS) THEN TEXT 30, y, "Fuel Scoops", "LT", 7, 1, cYellow : y = y + 10
  IF eqOwned(EQ_POD) THEN TEXT 30, y, "Escape Pod", "LT", 7, 1, cYellow : y = y + 10
  IF eqOwned(EQ_BOMB) THEN TEXT 30, y, "Energy Bomb", "LT", 7, 1, cYellow : y = y + 10
  IF eqOwned(EQ_ENERGY) THEN TEXT 30, y, "Energy Unit", "LT", 7, 1, cYellow : y = y + 10
  IF eqOwned(EQ_DOCK) THEN TEXT 30, y, "Docking Computer", "LT", 7, 1, cYellow : y = y + 10
  IF eqOwned(EQ_GALHYP) THEN TEXT 30, y, "Galactic Hyperdrive", "LT", 7, 1, cYellow : y = y + 10
  LOCAL INTEGER v
  FOR v = 0 TO 3
    IF lasView(v) <> 0 THEN
      TEXT 30, y, ViewWord$(v) + LaserWord$(lasView(v)), "LT", 7, 1, cYellow
      y = y + 10
    ENDIF
  NEXT v
END SUB

FUNCTION ViewWord$(v AS INTEGER)
  SELECT CASE v
    CASE 0 : ViewWord$ = "Fore "
    CASE 1 : ViewWord$ = "Aft "
    CASE 2 : ViewWord$ = "Left "
    CASE ELSE : ViewWord$ = "Right "
  END SELECT
END FUNCTION

FUNCTION LaserWord$(p AS INTEGER)
  IF p >= 128 THEN LaserWord$ = "Beam Laser" ELSE LaserWord$ = "Pulse Laser"
END FUNCTION

FUNCTION CondName$()
  IF docked THEN
    CondName$ = "Docked"
  ELSEIF pEnergy < 128 THEN
    CondName$ = "Red"
  ELSE
    CondName$ = "Green"
  ENDIF
END FUNCTION

' The name of a system without disturbing where the seeds are sitting.
FUNCTION SysName2$(n AS INTEGER)
  LOCAL INTEGER keep
  keep = homeSys
  GotoSystem gGal, n
  SysName2$ = SysName$()
  GotoSystem gGal, keep
  SysData
END FUNCTION

' --- what is in the hold
SUB InventoryScreen
  LOCAL INTEGER i, y
  CLS
  TEXT VCX, 4, "INVENTORY", "CT", 7, 1, cWhite
  LINE 0, 19, SCRW - 1, 19, 1, cWhite
  y = 28
  DataLine y, "Fuel", STR$(pFuel / 10) + " Light Years" : y = y + 11
  DataLine y, "Cash", STR$(cashTenths / 10) + " Cr" : y = y + 14
  FOR i = 0 TO NGOODS - 1
    IF cargo(i) > 0 THEN
      TEXT 20, y, mkName$(i), "LT", 7, 1, cWhite
      TEXT 180, y, STR$(cargo(i)) + " " + mkUnit$(i), "LT", 7, 1, cYellow
      y = y + 10
    ENDIF
  NEXT i
  IF y = 53 THEN TEXT 20, y, "Nothing in the hold.", "LT", 7, 1, cDim
END SUB

' --- the market
SUB MarketScreen(sel AS INTEGER)
  LOCAL INTEGER i, y, c
  CLS
  GotoSystem gGal, homeSys
  SysData
  TEXT VCX, 3, SysName$() + " MARKET PRICES", "CT", 7, 1, cWhite
  LINE 0, 15, SCRW - 1, 15, 1, cWhite
  TEXT 20, 18, "PRODUCT", "LT", 7, 1, cDim
  TEXT 150, 18, "UNIT", "LT", 7, 1, cDim
  TEXT 190, 18, "PRICE", "LT", 7, 1, cDim
  TEXT 238, 18, "FOR SALE", "LT", 7, 1, cDim
  TEXT 292, 18, "HELD", "LT", 7, 1, cDim
  y = 29
  FOR i = 0 TO NGOODS - 1
    c = cWhite
    IF i = sel THEN
      c = cYellow
      BOX 16, y - 1, 292, 9, 0, cSel, cSel
    ENDIF
    TEXT 20, y, mkName$(i), "LT", 7, 1, c
    TEXT 150, y, mkUnit$(i), "LT", 7, 1, c
    TEXT 190, y, PriceStr$(mkPrice(i)), "LT", 7, 1, c
    TEXT 245, y, STR$(mkStock(i)), "LT", 7, 1, c
    TEXT 300, y, STR$(cargo(i)), "LT", 7, 1, c
    y = y + 9
  NEXT i
  TEXT 20, y + 4, "Cash: " + STR$(cashTenths / 10) + " Cr", "LT", 7, 1, cWhite
  TEXT 180, y + 4, "Hold: " + STR$(HoldUsed()) + "/" + STR$(holdSize), "LT", 7, 1, cWhite
END SUB

FUNCTION HoldUsed() AS INTEGER
  LOCAL INTEGER i, t
  t = 0
  FOR i = 0 TO NGOODS - 1
    ' Gold, platinum and gems are carried in the hold's odd corners and
    ' do not count against the tonnage.
    IF i < 13 THEN t = t + cargo(i)
  NEXT i
  HoldUsed = t
END FUNCTION

' Buy one unit, if it is for sale, affordable, and there is room.
SUB BuyOne(i AS INTEGER)
  IF mkStock(i) <= 0 THEN EXIT SUB
  IF cashTenths < mkPrice(i) THEN EXIT SUB
  IF i < 13 AND HoldUsed() >= holdSize THEN EXIT SUB
  cashTenths = cashTenths - mkPrice(i)
  cargo(i) = cargo(i) + 1
  mkStock(i) = mkStock(i) - 1
END SUB

' Sell one, at the same price the system is asking - the profit is in
' carrying it somewhere else, not in haggling.
SUB SellOne(i AS INTEGER)
  IF cargo(i) <= 0 THEN EXIT SUB
  cashTenths = cashTenths + mkPrice(i)
  cargo(i) = cargo(i) - 1
  mkStock(i) = mkStock(i) + 1
END SUB

' --- the equipment shop
SUB EquipScreen(sel AS INTEGER)
  LOCAL INTEGER i, y, c
  CLS
  GotoSystem gGal, homeSys
  SysData
  TEXT VCX, 3, "EQUIP SHIP", "CT", 7, 1, cWhite
  LINE 0, 15, SCRW - 1, 15, 1, cWhite
  y = 22
  TEXT 20, y, "Fuel", "LT", 7, 1, cWhite
  TEXT 220, y, PriceStr$((70 - pFuel) * 2) + " Cr", "LT", 7, 1, cYellow
  y = y + 11
  FOR i = 0 TO NEQUIP - 1
    ' Only what this system is advanced enough to sell.
    IF eqTech(i) <= sysTech + 1 THEN
      c = cWhite
      IF eqOwned(i) THEN c = cDim
      IF i = sel THEN
        c = cYellow
        BOX 16, y - 1, 292, 9, 0, cSel, cSel
      ENDIF
      TEXT 20, y, eqName$(i), "LT", 7, 1, c
      TEXT 220, y, STR$(eqPrice(i)) + " Cr", "LT", 7, 1, c
      y = y + 9
    ENDIF
  NEXT i
  TEXT 20, y + 5, "Cash: " + STR$(cashTenths / 10) + " Cr", "LT", 7, 1, cWhite
END SUB

SUB BuyEquip(i AS INTEGER)
  LOCAL INTEGER v
  IF eqTech(i) > sysTech + 1 THEN EXIT SUB
  IF cashTenths < eqPrice(i) * 10 THEN EXIT SUB
  ' A laser is not owned once and for all: there is a mount for each of the
  ' four views and one can be bought for each, which is why these two rows
  ' never grey out until every mount is full.
  IF i = EQ_PULSE OR i = EQ_BEAM THEN
    v = AskView()
    IF v < 0 THEN EXIT SUB
    IF i = EQ_PULSE THEN
      ' A pulse will not go where anything is already mounted.
      IF lasView(v) <> 0 THEN Sfx SFX_BOOP : EXIT SUB
      lasView(v) = LAS_PULSE
    ELSE
      ' A beam will replace a pulse, and the original hands back what the
      ' pulse cost when it does.  Only another beam is refused.
      IF lasView(v) >= 128 THEN Sfx SFX_BOOP : EXIT SUB
      IF lasView(v) <> 0 THEN cashTenths = cashTenths + eqPrice(EQ_PULSE) * 10
      lasView(v) = LAS_BEAM
    ENDIF
    cashTenths = cashTenths - eqPrice(i) * 10
    EXIT SUB
  ENDIF
  IF eqOwned(i) THEN EXIT SUB
  ' A missile is the other thing that can be bought again and again, up to
  ' the four the racks hold.
  IF i = 0 THEN
    IF pMissl >= 4 THEN EXIT SUB
    cashTenths = cashTenths - eqPrice(i) * 10
    pMissl = pMissl + 1
    EXIT SUB
  ENDIF
  cashTenths = cashTenths - eqPrice(i) * 10
  eqOwned(i) = 1
  SELECT CASE i
    CASE 1 : holdSize = 35
    CASE EQ_ENERGY : energyUnit = 1
  END SELECT
END SUB

' Which mount?  The original puts up the four views and waits for a number.
FUNCTION AskView() AS INTEGER
  LOCAL INTEGER k
  DO
    BOX 44, 92, 232, 52, 1, cWhite, cBlack
    TEXT VCX, 102, "WHICH MOUNT?", "CT", 7, 1, cWhite
    TEXT VCX, 122, "F1 fore  F2 aft  F3 left  F4 right", "CT", 7, 1, cYellow
    FRAMEBUFFER COPY F, N
    k = DockKey()
    IF k = 27 THEN AskView = -1 : EXIT FUNCTION
  LOOP UNTIL k >= 145 AND k <= 148
  AskView = k - 145
END FUNCTION

SUB BuyFuel
  LOCAL INTEGER cost
  cost = (70 - pFuel) * 2
  IF cost <= 0 THEN EXIT SUB
  IF cashTenths < cost THEN
    ' Buy what we can afford.
    pFuel = pFuel + cashTenths \ 2
    cashTenths = cashTenths MOD 2
  ELSE
    cashTenths = cashTenths - cost
    pFuel = 70
  ENDIF
END SUB

' --- the commander, as plain text so it can be read and edited
'
' One value to a line.  Writing several to a line with PRINT separates
' them with spaces, while INPUT expects commas, and the mismatch loses
' everything after the first field on each line - which is exactly what
' the first version of this did.
SUB SaveCommander(f$)
  LOCAL INTEGER i, fn
  fn = 1
  OPEN f$ FOR OUTPUT AS #fn
  PRINT #fn, "elite-commander 2"
  PRINT #fn, gGal
  PRINT #fn, homeSys
  PRINT #fn, cashTenths
  PRINT #fn, pFuel
  PRINT #fn, holdSize
  PRINT #fn, kills
  PRINT #fn, legal
  PRINT #fn, pMissl
  FOR i = 0 TO 3 : PRINT #fn, lasView(i) : NEXT i
  PRINT #fn, energyUnit
  FOR i = 0 TO NGOODS - 1 : PRINT #fn, cargo(i) : NEXT i
  FOR i = 0 TO NEQUIP - 1 : PRINT #fn, eqOwned(i) : NEXT i
  CLOSE #fn
END SUB

FUNCTION LoadCommander(f$) AS INTEGER
  LOCAL INTEGER i, fn
  LOCAL hd$
  LoadCommander = 0
  IF DIR$(f$, FILE) = "" THEN EXIT FUNCTION
  fn = 1
  OPEN f$ FOR INPUT AS #fn
  LINE INPUT #fn, hd$
  IF hd$ <> "elite-commander 2" THEN
    CLOSE #fn
    EXIT FUNCTION
  ENDIF
  INPUT #fn, gGal
  INPUT #fn, homeSys
  INPUT #fn, cashTenths
  INPUT #fn, pFuel
  INPUT #fn, holdSize
  INPUT #fn, kills
  INPUT #fn, legal
  INPUT #fn, pMissl
  FOR i = 0 TO 3 : INPUT #fn, lasView(i) : NEXT i
  INPUT #fn, energyUnit
  FOR i = 0 TO NGOODS - 1 : INPUT #fn, cargo(i) : NEXT i
  FOR i = 0 TO NEQUIP - 1 : INPUT #fn, eqOwned(i) : NEXT i
  CLOSE #fn
  selSys = homeSys
  GotoSystem gGal, homeSys
  SysData
  homeX = sysX : homeY = sysY * 2
  curX = homeX : curY = homeY
  mkByte = 0
  MakeMarket sysEco, mkByte
  LoadCommander = 1
END FUNCTION

dat_equip:
' name, price in credits, technology level needed to sell it
DATA "Missile",30,1
DATA "Large Cargo Bay",400,4
DATA "E.C.M. System",600,3
DATA "Extra Pulse Lasers",400,4
DATA "Extra Beam Lasers",1000,4
DATA "Fuel Scoops",525,5
DATA "Escape Pod",600,6
DATA "Energy Bomb",900,7
DATA "Energy Unit",1500,8
DATA "Docking Computer",1500,9
DATA "Galactic Hyperdrive",5000,10
