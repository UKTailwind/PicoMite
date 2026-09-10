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
  IF eqOwned(4) THEN TEXT 30, y, "Fuel Scoops", "LT", 7, 1, cYellow : y = y + 10
  IF eqOwned(6) THEN TEXT 30, y, "Docking Computer", "LT", 7, 1, cYellow : y = y + 10
  IF lasPower >= 128 THEN TEXT 30, y, "Beam Laser", "LT", 7, 1, cYellow : y = y + 10
END SUB

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
  IF y = 53 THEN TEXT 20, y, "Nothing in the hold.", "LT", 7, 1, cGrey
END SUB

' --- the market
SUB MarketScreen(sel AS INTEGER)
  LOCAL INTEGER i, y, c
  CLS
  GotoSystem gGal, homeSys
  SysData
  TEXT VCX, 3, SysName$() + " MARKET PRICES", "CT", 7, 1, cWhite
  LINE 0, 15, SCRW - 1, 15, 1, cWhite
  TEXT 20, 18, "PRODUCT", "LT", 7, 1, cGrey
  TEXT 150, 18, "UNIT", "LT", 7, 1, cGrey
  TEXT 190, 18, "PRICE", "LT", 7, 1, cGrey
  TEXT 245, 18, "FOR SALE", "LT", 7, 1, cGrey
  TEXT 300, 18, "HELD", "LT", 7, 1, cGrey
  y = 29
  FOR i = 0 TO NGOODS - 1
    c = cWhite
    IF i = sel THEN
      c = cYellow
      BOX 16, y - 1, 292, 9, 0, RGB(32, 32, 64), RGB(32, 32, 64)
    ENDIF
    TEXT 20, y, mkName$(i), "LT", 7, 1, c
    TEXT 150, y, mkUnit$(i), "LT", 7, 1, c
    TEXT 190, y, PriceStr$(mkPrice(i)), "LT", 7, 1, c
    TEXT 250, y, STR$(mkStock(i)), "LT", 7, 1, c
    TEXT 302, y, STR$(cargo(i)), "LT", 7, 1, c
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
      IF eqOwned(i) THEN c = cGrey
      IF i = sel THEN
        c = cYellow
        BOX 16, y - 1, 292, 9, 0, RGB(32, 32, 64), RGB(32, 32, 64)
      ENDIF
      TEXT 20, y, eqName$(i), "LT", 7, 1, c
      TEXT 220, y, STR$(eqPrice(i)) + " Cr", "LT", 7, 1, c
      y = y + 9
    ENDIF
  NEXT i
  TEXT 20, y + 5, "Cash: " + STR$(cashTenths / 10) + " Cr", "LT", 7, 1, cWhite
END SUB

SUB BuyEquip(i AS INTEGER)
  IF eqOwned(i) THEN EXIT SUB
  IF eqTech(i) > sysTech + 1 THEN EXIT SUB
  IF cashTenths < eqPrice(i) * 10 THEN EXIT SUB
  cashTenths = cashTenths - eqPrice(i) * 10
  eqOwned(i) = 1
  SELECT CASE i
    CASE 0 : IF pMissl < 4 THEN pMissl = pMissl + 1 : eqOwned(0) = 0
    CASE 1 : holdSize = 35
    CASE 3 : lasPower = 143 OR 128           ' beam laser
    CASE 5 : energyUnit = 1
  END SELECT
END SUB

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
  PRINT #fn, "elite-commander 1"
  PRINT #fn, gGal
  PRINT #fn, homeSys
  PRINT #fn, cashTenths
  PRINT #fn, pFuel
  PRINT #fn, holdSize
  PRINT #fn, kills
  PRINT #fn, legal
  PRINT #fn, pMissl
  PRINT #fn, lasPower
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
  IF LEFT$(hd$, 16) <> "elite-commander " THEN
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
  INPUT #fn, lasPower
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
DATA "Beam Laser",1000,4
DATA "Fuel Scoops",525,5
DATA "Energy Unit",1500,8
DATA "Docking Computer",1500,9
DATA "Galactic Hyperdrive",5000,10
