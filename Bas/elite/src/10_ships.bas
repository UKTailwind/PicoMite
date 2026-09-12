' =====================================================================
'  Ship blueprints, the universe slot table, and the Draw3D object pool
' =====================================================================

' Read every blueprint once: the header statistics, and the mesh (which
' we throw away again) so we can record each ship's size.  Slow, but it
' happens once at start up and it means nothing else has to guess.
SUB LoadStats
  LOCAL INTEGER b
  FOR b = 0 TO NBP - 1
    LoadMesh b
  NEXT b
  tBp(1) = 0 : tBp(2) = 1 : tBp(3) = 2 : tBp(4) = 3 : tBp(5) = 4
  tBp(6) = 5 : tBp(7) = 4 : tBp(8) = 6 : tBp(9) = 7 : tBp(10) = 8
  tBp(11) = 9 : tBp(12) = 10 : tBp(13) = 11
END SUB

' Load blueprint b into the scratch mesh buffers and fill in its stats.
' The DATA layout is fixed by elite_tools/blueprints.py:
'   name, nv, nf, nfv, nf0, nv0, canisters, area, bounty, visdist,
'   energy, speed, laser, missiles, gun vertex, explosion count
'   then nv vertices, nf face vertex counts, nf host faces,
'   nf0 stored normals, nfv face vertex indices.
SUB LoadMesh(b AS INTEGER)
  LOCAL INTEGER j, k
  SELECT CASE b
    CASE 0  : RESTORE dat_sidewinder
    CASE 1  : RESTORE dat_viper
    CASE 2  : RESTORE dat_mamba
    CASE 3  : RESTORE dat_python
    CASE 4  : RESTORE dat_cobra_mk_3
    CASE 5  : RESTORE dat_thargoid
    CASE 6  : RESTORE dat_coriolis
    CASE 7  : RESTORE dat_missile
    CASE 8  : RESTORE dat_asteroid
    CASE 9  : RESTORE dat_canister
    CASE 10 : RESTORE dat_thargon
    CASE 11 : RESTORE dat_escape_pod
  END SELECT
  READ bName$(b), bNv(b), bNf(b), bNfv(b), bNf0(b), bNv0(b)
  READ bCan(b), bArea(b), bBty(b), bVis(b), bEne(b), bSpd(b)
  READ bLas(b), bMis(b), bGun(b), bExp(b)
  FOR j = 0 TO bNv(b) - 1 : READ mV(0, j), mV(1, j), mV(2, j) : NEXT j
  FOR j = 0 TO bNf(b) - 1 : READ mFc(j) : NEXT j
  FOR j = 0 TO bNf(b) - 1 : READ mHost(j) : NEXT j
  FOR j = 0 TO bNf0(b) - 1 : READ mNrm(0, j), mNrm(1, j), mNrm(2, j) : NEXT j
  FOR j = 0 TO bNfv(b) - 1 : READ mF(j) : NEXT j
  ' Ships are white lines, as on the BBC; the fill colours only matter
  ' when the solid renderer is switched on.
  FOR j = 0 TO bNf(b) - 1
    mEc(j) = 0
    mFl(j) = 1 + (mHost(j) MOD 6)
  NEXT j
  bSize(b) = 0
  FOR j = 0 TO bNv0(b) - 1
    FOR k = 0 TO 2
      IF ABS(mV(k, j)) > bSize(b) THEN bSize(b) = ABS(mV(k, j))
    NEXT k
  NEXT j
END SUB

' ------------------------------------------------------- the slot table
SUB ClearSlots
  LOCAL INTEGER n
  CloseAll
  FOR n = 0 TO NSLOT - 1
    sTyp(n) = 0 : sObj(n) = 0
  NEXT n
  nUsed = 0
END SUB

' Create a ship of type t at x, y, z with orientation q (5 elements) and
' return its slot, or -1 if the bubble is full.  Slot 0 is reserved for
' the planet and slot 1 for the sun or the station, exactly as FRIN.
FUNCTION NewShip(t AS INTEGER, x AS FLOAT, y AS FLOAT, z AS FLOAT, q() AS FLOAT) AS INTEGER
  LOCAL INTEGER n, i, first
  first = 2
  IF t = T_PLANET OR t = T_CRATER THEN first = 0
  IF t = T_SUN OR t = T_STATION THEN first = 1
  n = -1
  IF first < 2 THEN
    IF sTyp(first) = 0 THEN n = first
  ELSE
    FOR i = 2 TO NSLOT - 1
      IF sTyp(i) = 0 THEN n = i : EXIT FOR
    NEXT i
  ENDIF
  NewShip = n
  IF n < 0 THEN EXIT FUNCTION

  sTyp(n) = t
  sX(n) = x : sY(n) = y : sZ(n) = z
  MATH INSERT sQ(), , n, q()
  sQ(4, n) = 1                      ' Draw3D scales by this squared
  sObj(n) = 0
  sSpd(n) = 0 : sAcc(n) = 0 : sRol(n) = 0 : sPit(n) = 0
  sFlg(n) = 0 : sAI(n) = 0
  ' Every field the slot carries has to be cleared, not most of them.  A
  ' slot that last held something which blew up keeps its explosion
  ' counter, and the next ship to be given that slot is killed on its
  ' first frame - no cloud, no kill, just gone - which is exactly what
  ' the second ship in every fight used to do.
  sExp(n) = 0 : sTgt(n) = -1
  IF t < T_PLANET THEN
    sBp(n) = tBp(t)
    sEne(n) = bEne(sBp(n))
    GetObject n
  ELSE
    sBp(n) = -1                     ' planet and sun are drawn by hand
    sEne(n) = 0
  ENDIF
  IF n >= nUsed THEN nUsed = n + 1
END FUNCTION

' Remove a slot.  The original shuffles the table down to close the gap
' so the loop over ships never sees a hole; we do the same, because the
' AI and the scanner both walk the table in order.
SUB KillShip(n AS INTEGER)
  LOCAL INTEGER i
  DropObject n
  IF n < 2 THEN
    ' The planet and the sun / station keep their reserved slots.
    sTyp(n) = 0 : sObj(n) = 0
    EXIT SUB
  ENDIF
  FOR i = n TO nUsed - 2
    CopySlot i, i + 1
  NEXT i
  sTyp(nUsed - 1) = 0
  sObj(nUsed - 1) = 0
  sExp(nUsed - 1) = 0
  sTgt(nUsed - 1) = -1
  nUsed = nUsed - 1
END SUB

SUB CopySlot(d AS INTEGER, s AS INTEGER)
  LOCAL INTEGER i
  sTyp(d) = sTyp(s) : sBp(d) = sBp(s) : sObj(d) = sObj(s)
  sX(d) = sX(s) : sY(d) = sY(s) : sZ(d) = sZ(s)
  MATH SLICE sQ(), , s, qA()
  MATH INSERT sQ(), , d, qA()
  sSpd(d) = sSpd(s) : sAcc(d) = sAcc(s)
  sRol(d) = sRol(s) : sPit(d) = sPit(s)
  sEne(d) = sEne(s) : sAI(d) = sAI(s) : sFlg(d) = sFlg(s)
  sExp(d) = sExp(s) : sTgt(d) = sTgt(s)
  IF sObj(d) > 0 THEN objOwn(sObj(d)) = d
  sTyp(s) = 0 : sObj(s) = 0
  sExp(s) = 0 : sTgt(s) = -1
END SUB

' ------------------------------------------------- Draw3D object pool
' A slot only needs an object while its ship is close enough to be drawn
' as a mesh; there are fewer objects than slots, so they are handed out
' on a first come basis and the rest of the bubble shows up as dots.
SUB GetObject(n AS INTEGER)
  LOCAL INTEGER o, b
  IF sObj(n) > 0 THEN EXIT SUB
  FOR o = 1 TO maxObj
    IF objOwn(o) < 0 THEN
      b = sBp(n)
      LoadMesh b
      IF solidMode THEN
        Draw3D CREATE o, bNv(b), bNf(b), 1, mV(), mFc(), mF(), col(), mEc(), mFl()
      ELSE
        Draw3D CREATE o, bNv(b), bNf(b), 1, mV(), mFc(), mF(), col(), mEc()
      ENDIF
      objOwn(o) = n
      sObj(n) = o
      EXIT SUB
    ENDIF
  NEXT o
END SUB

SUB DropObject(n AS INTEGER)
  IF sObj(n) <= 0 THEN EXIT SUB
  Draw3D CLOSE sObj(n)
  objOwn(sObj(n)) = -1
  sObj(n) = 0
END SUB

' ----------------------------------------------------- view transform
' Front leaves the universe alone; rear turns it through 180 degrees
' about the vertical axis; left and right through plus and minus 90.
' Written out rather than done with a quaternion because it runs for
' every slot every frame.
SUB ViewXform(n AS INTEGER)
  SELECT CASE vw
    CASE 0 : tx = sX(n)  : ty = sY(n) : tz = sZ(n)
    CASE 1 : tx = -sX(n) : ty = sY(n) : tz = -sZ(n)
    CASE 2 : tx = sZ(n)  : ty = sY(n) : tz = -sX(n)
    CASE 3 : tx = -sZ(n) : ty = sY(n) : tz = sX(n)
  END SELECT
END SUB

' The ship's orientation seen from the current view.  Leaves the result
' in qC() ready for Draw3D ROTATE.
' MATH SLICE lifts one ship's quaternion out of the 5 x NSLOT table in a
' single call, and a whole-array assignment copies one in a single memcpy;
' both replace five interpreted statements, which is what this costs when
' it runs for every ship every frame.
SUB ViewOrient(n AS INTEGER)
  MATH SLICE sQ(), , n, qA()
  IF vw = 0 THEN
    qC() = qA()
  ELSE
    MATH SLICE vwQ(), , vw, qB()
    MATH Q_MULT qB(), qA(), qC()
  ENDIF
  qC(4) = 1
END SUB
