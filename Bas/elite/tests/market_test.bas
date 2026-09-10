' market_test.bas - does the board price Elite's market, exactly?
'
' Every economy against every possible market byte, all seventeen goods,
' reduced to one number.  elite_tools/market_ref.py computes the same one
' in Python.  Then Lave's own market, which anyone can check by eye.
'
' Assemble with:  cat market_test.bas ../src/71_market.bas > run.bas
OPTION EXPLICIT
OPTION BASE 0
OPTION DEFAULT NONE

CONST NGOODS = 17
CONST HMOD = 2147483647
DIM mkName$(NGOODS-1) LENGTH 14, mkUnit$(NGOODS-1) LENGTH 2
DIM INTEGER mkBase(NGOODS-1), mkFact(NGOODS-1), mkQty(NGOODS-1), mkMask(NGOODS-1)
DIM INTEGER mkPrice(NGOODS-1), mkStock(NGOODS-1)

DIM INTEGER eco, r, i, h, t0

LoadMarket
PRINT "Elite market check, 8 economies x 256 bytes x 17 goods"
t0 = TIMER
h = 0
FOR eco = 0 TO 7
  FOR r = 0 TO 255
    MakeMarket eco, r
    FOR i = 0 TO NGOODS - 1
      h = (h * 31 + mkPrice(i)) MOD HMOD
      h = (h * 31 + mkStock(i)) MOD HMOD
    NEXT i
  NEXT r
NEXT eco
PRINT "checksum "; h
PRINT "expected 566388444"
IF h = 566388444 THEN PRINT "MATCH - the market is Elite's" ELSE PRINT "** MISMATCH **"
PRINT "took"; (TIMER - t0) / 1000; " seconds"
PRINT

' Lave is a Rich Agricultural system, economy 5.  Its food costs a few
' credits and it stocks no computers, narcotics or firearms at all.
PRINT "Lave, market byte 0:"
MakeMarket 5, 0
FOR i = 0 TO NGOODS - 1
  PRINT "  "; mkName$(i); SPACE$(14 - LEN(mkName$(i)));
  PRINT PriceStr$(mkPrice(i)); " Cr";
  PRINT SPACE$(3); STR$(mkStock(i)); " "; mkUnit$(i)
NEXT i
PRINT "market check done"
END
