' =====================================================================
'  The market
'
'  Prices are not stored either.  Each system draws one random byte when
'  the player arrives, and that byte together with the system's economy
'  fixes the price and the quantity of all seventeen commodities until
'  the player leaves and comes back.
'
'  The arithmetic is eight bit and it wraps, which is exactly why
'  narcotics and furs swing so wildly from system to system while food
'  and minerals barely move.  The wrap is reproduced, not tidied away.
'
'    price    = ((base + (rnd AND mask) + economy * factor) AND 255) * 4
'    quantity = base_qty + (rnd AND mask) - economy * factor
'               floored at 0, then AND 63
'
'  Price is in tenths of a credit.  A negative factor means the goods get
'  cheaper and more plentiful the more agricultural the system is.
' =====================================================================

SUB LoadMarket
  LOCAL INTEGER i
  RESTORE dat_market
  FOR i = 0 TO NGOODS - 1
    READ mkName$(i), mkBase(i), mkFact(i), mkUnit$(i), mkQty(i), mkMask(i)
  NEXT i
END SUB

' Work out the whole market for an economy and a market byte, leaving it
' in mkPrice() and mkStock().
SUB MakeMarket(eco AS INTEGER, mb AS INTEGER)
  LOCAL INTEGER i, q
  FOR i = 0 TO NGOODS - 1
    mkPrice(i) = ((mkBase(i) + (mb AND mkMask(i)) + eco * mkFact(i)) AND 255) * 4
    q = mkQty(i) + (mb AND mkMask(i)) - eco * mkFact(i)
    IF q < 0 THEN q = 0
    mkStock(i) = q AND 63
  NEXT i
END SUB

' Price as the game prints it: a credits and tenths string.
FUNCTION PriceStr$(p AS INTEGER)
  PriceStr$ = STR$(p \ 10) + "." + STR$(p MOD 10)
END FUNCTION

dat_market:
' name, base price, economic factor, unit, base quantity, mask
DATA "Food",19,-2,"t",6,1
DATA "Textiles",20,-1,"t",10,3
DATA "Radioactives",65,-3,"t",2,7
DATA "Slaves",40,-5,"t",226,31
DATA "Liquor/Wines",83,-5,"t",251,15
DATA "Luxuries",196,8,"t",54,3
DATA "Narcotics",235,29,"t",8,120
DATA "Computers",154,14,"t",56,3
DATA "Machinery",117,6,"t",40,7
DATA "Alloys",78,1,"t",17,31
DATA "Firearms",124,13,"t",29,7
DATA "Furs",176,-9,"t",220,63
DATA "Minerals",32,-1,"t",53,3
DATA "Gold",97,-1,"kg",66,7
DATA "Platinum",171,-2,"kg",55,31
DATA "Gem-Stones",45,-1,"g",250,15
DATA "Alien items",53,15,"t",192,7
