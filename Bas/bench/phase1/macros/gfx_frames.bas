' gfx_frames.bas - Phase 1 macro: a drawing-heavy game frame, fixed work.
' Written for the RP2040 PicoMiteVGA (MODE 2, 320x240, 16 colours); it runs
' unchanged on the PC3 HDMIUSB build at 640x480 as well.
'
' Each frame is the Elite port's cockpit frame (Bas/elite/elite.bas
' DrawFrame, :683) with its routines carried over as they are written there:
' DrawStardust (front view, :844), SpaceFurniture (:748), the planet's
' HalfCircle (:812), DrawDash with Bar, Pointer, MissileBlocks, DrawScanner,
' DrawCompass and Bulbs (:1030-1124), ViewName and DrawMessage. The full
' Elite does not fit an RP2040, so three things are stood in for:
'  - the ships are three of Bas/3ddemo.bas's Elite meshes (Viper, Cobra,
'    Sidewinder) drawn as wireframe in BASIC: rotate, project and one LINE
'    per edge, where the full game hands them to Draw3D;
'  - the dashboard's static part is redrawn every frame (the full game
'    caches it and copies it back with MEMORY COPY);
'  - RND is replaced by a fixed table rb(), so every run draws the same
'    frames and the read-back checksum is repeatable.
' Then FRAMEBUFFER COPY F, N, without waiting for the frame: no pacing.
' First runs (pre-review file): RP2040 VGA 315 MHz 68.7 ms a frame (68.7 s
' for 1000); PC3 HDMIUSB 378 MHz 25.9 ms (25.9 s).
Option Explicit
Const PROFILE% = 0             ' 1 = run the PC sampler over the timed region
Const FBUF% = 1                ' 1 = draw in FRAMEBUFFER F and copy it to the screen
                               ' each frame, as games do; 0 = draw on the screen
                               ' (frees 38,400 bytes of RP2040 heap for the sampler)
Const NFRAMES% = 1000
' layout, from Bas/elite/elite_lib.bas:4-10 and :160-194
Const SCRW = 320, SCRH = 240, VIEWH = 176, VCX = 160, VCY = 88, VPLANE = 256
Const DASHY = 176, NSTAR = 18, NSEG = 16
Const DL = 20, DR = 260, DW = 40
Const SCX = 155, SCY = 204, SCA = 86, SCB = 18, SCDOTX = 154
Const SCXDIV = 204.8, SCZDIV = 1024, SCYDIV = 512, SCTOP = 178, SCBOT = 230
Const BULBX = 64, BULBY = 232, CPX = 244, CPY = 187, CPR = 9
Const MSGX = 90, MSGY = 160
Const NSHIP = 3, NOBJ = 4
Const NV = 53, NE = 72         ' mesh totals; gen_gfx.py checks them

Dim INTEGER cWhite, cCyan, cYellow, cRed, cGreen, cBlack, cDim, cMagenta, cBlue
Dim FLOAT ctab(NSEG - 1), stab(NSEG - 1)
Dim FLOAT stX(NSTAR - 1), stY(NSTAR - 1), stZ(NSTAR - 1)
Dim INTEGER spx(4 * NSTAR - 1), spy(4 * NSTAR - 1), spc(4 * NSTAR - 1)
Dim INTEGER rb(258), rk
Dim FLOAT alp, bet, dSpeed, pa
Dim FLOAT mvx(NV - 1), mvy(NV - 1), mvz(NV - 1)
Dim INTEGER psx(NV - 1), psy(NV - 1), mea(NE - 1), meb(NE - 1)
Dim INTEGER shV0(NSHIP - 1), shNV(NSHIP - 1), shE0(NSHIP - 1), shNE(NSHIP - 1)
Dim INTEGER shCol(NSHIP - 1)
Dim FLOAT shX(NSHIP - 1), shY(NSHIP - 1), shZ(NSHIP - 1)
Dim FLOAT shYaw(NSHIP - 1), shPit(NSHIP - 1), shRol(NSHIP - 1)
Dim FLOAT shDy(NSHIP - 1), shDp(NSHIP - 1), shDr(NSHIP - 1)
Dim FLOAT obX(NOBJ - 1), obY(NOBJ - 1), obZ(NOBJ - 1)
Dim INTEGER obCol(NOBJ - 1)
Dim INTEGER DLY(5), DRY(6)
Dim LLAB$(5) LENGTH 2
Dim RLAB$(6) LENGTH 2
Dim INTEGER nfr, lasFlash, pEnergy, pFsh, pAsh, pFuel, pCabT, pLasT, pAltit
Dim INTEGER pMissl, ecmOn, inSafe, dSpd, rollv, pitchv
Dim INTEGER pcsn, seed, ks, qx, qy, ck
Dim FLOAT mhz, el, bl, us, tm0, tm1, tMv, tSd, tPl, tSh, tDa, tCp

mhz = Val(MM.Info(CPUSPEED)) / 1000000
Print "BENCHSTART "; MM.Device$; " "; MM.Ver; " "; mhz; " MHz"
Mode 2
If MM.HRes <> SCRW Or MM.VRes <> SCRH Then Error "needs MODE 2 at 320x240 (a 640x480 display)"
Setup
LoadMeshes
InitStardust
If FBUF% Then
  Framebuffer Create
  Framebuffer Write F
EndIf
' the sampler's tables: the largest power of two up to 8192 entries (16
' bytes each) that leaves 20 KB of heap for OPTION PROFILING's counters.
' The PC3 needs 8192: the first board runs sampled 2,700-5,900 distinct PCs.
' RP2040: 1024 with FBUF% = 1 (the framebuffer takes 38,400 B of the heap)
pcsn = 8192
Do While pcsn > 1024 And pcsn * 16 + 20480 > MM.Info(HEAP)
  pcsn = pcsn \ 2
Loop
' the empty frame loop, subtracted as bench_micro.bas does
tm0 = Timer
For nfr = 1 To NFRAMES%
Next nfr
bl = Timer - tm0
Timer = 0
If PROFILE% Then Option profiling on, sample, pcsn
For nfr = 1 To NFRAMES%
  tm0 = Timer
  MoveObjects
  Cls
  tm1 = Timer : tMv = tMv + tm1 - tm0
  DrawStardust
  tm0 = Timer : tSd = tSd + tm0 - tm1
  DrawPlanet
  tm1 = Timer : tPl = tPl + tm1 - tm0
  For ks = 0 To NSHIP - 1
    DrawMesh ks
  Next ks
  tm0 = Timer : tSh = tSh + tm0 - tm1
  SpaceFurniture
  DrawDash
  ViewName
  DrawMessage
  tm1 = Timer : tDa = tDa + tm1 - tm0
  If FBUF% Then Framebuffer Copy F, N
  tCp = tCp + Timer - tm1
Next nfr
el = Timer
If FBUF% Then
  Framebuffer Write N
  Framebuffer Close F
EndIf
' read back every 4th pixel of every 4th row of the last frame
ck = 0
For qy = 0 To SCRH - 1 Step 4
  For qx = 0 To SCRW - 1 Step 4
    ck = ck + Pixel(qx, qy) * (1 + ((qx + 3 * qy) And 15))
  Next qx
Next qy
us = (el - bl) * 1000 / NFRAMES%
Print "BENCH gfx_elite_frame_us_per_frame "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
us = tMv * 1000 / NFRAMES%
Print "BENCH gfx_elite_move_cls_us_per_frame "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
us = tSd * 1000 / NFRAMES%
Print "BENCH gfx_elite_stardust_us_per_frame "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
us = tPl * 1000 / NFRAMES%
Print "BENCH gfx_elite_planet_us_per_frame "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
us = tSh * 1000 / NFRAMES%
Print "BENCH gfx_elite_ships_us_per_frame "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
us = tDa * 1000 / NFRAMES%
Print "BENCH gfx_elite_dash_us_per_frame "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
us = tCp * 1000 / NFRAMES%
Print "BENCH gfx_elite_fbcopy_us_per_frame "; Str$(us, 0, 3); " "; Str$(us * mhz, 0, 0)
Print "CHECK gfx_elite "; Str$(ck)
Print "ELAPSED gfx_elite "; Str$(el, 0, 3); " ms for "; Str$(NFRAMES%); " frames, FBUF%="; Str$(FBUF%)
Print "BENCHEND"
End

Sub Setup
  Local INTEGER i
  cWhite = RGB(255, 255, 255) : cCyan = RGB(0, 255, 255) : cYellow = RGB(255, 255, 0)
  cRed = RGB(255, 0, 0) : cGreen = RGB(0, 255, 0) : cBlack = RGB(0, 0, 0)
  cDim = RGB(0, 128, 0) : cMagenta = RGB(255, 0, 255) : cBlue = RGB(0, 0, 255)
  For i = 0 To NSEG - 1
    ctab(i) = Cos(2 * Pi * i / NSEG)
    stab(i) = Sin(2 * Pi * i / NSEG)
  Next i
  ' the stand-in for RND * 256: bytes from a 31-bit LCG
  seed = 12345
  For i = 0 To 258
    seed = (seed * 1103515245 + 12345) And &H7FFFFFFF
    rb(i) = (seed \ 65536) And 255
  Next i
  rk = 0
  DLY(0) = 178 : DLY(1) = 186 : DLY(2) = 194
  DLY(3) = 202 : DLY(4) = 210 : DLY(5) = 218
  DRY(0) = 178 : DRY(1) = 185 : DRY(2) = 193 : DRY(3) = 202
  DRY(4) = 210 : DRY(5) = 218 : DRY(6) = 226
  LLAB$(0) = "FS" : LLAB$(1) = "AS" : LLAB$(2) = "FU"
  LLAB$(3) = "CT" : LLAB$(4) = "LT" : LLAB$(5) = "AL"
  RLAB$(0) = "SP" : RLAB$(1) = "RL" : RLAB$(2) = "DC"
  RLAB$(3) = "1" : RLAB$(4) = "2" : RLAB$(5) = "3" : RLAB$(6) = "4"
  ' flight: roll, pitch and speed as Elite's alp2 * alp1, bet2 * bet1, dSpeed
  alp = 4 : bet = -2 : dSpeed = 12
  ' the ships: where they sit, their colour and how fast they turn
  shX(0) = -150 : shY(0) = 25 : shZ(0) = 650 : shCol(0) = cCyan
  shDy(0) = Rad(1) : shDp(0) = Rad(2) : shDr(0) = Rad(0.5)
  shX(1) = 0 : shY(1) = -10 : shZ(1) = 1000 : shCol(1) = cYellow
  shDy(1) = Rad(-0.7) : shDp(1) = Rad(1.3) : shDr(1) = Rad(0.9)
  shX(2) = 190 : shY(2) = -30 : shZ(2) = 750 : shCol(2) = cMagenta
  shDy(2) = Rad(1.6) : shDp(2) = Rad(-0.8) : shDr(2) = Rad(1.1)
  ' the scanner's objects: the three ships and the planet
  For i = 0 To NSHIP - 1
    obX(i) = 20 * shX(i) : obY(i) = 20 * shY(i) : obZ(i) = 8 * shZ(i)
    obCol(i) = shCol(i)
  Next i
  obX(3) = 3000 : obY(3) = 2000 : obZ(3) = 9000 : obCol(3) = cGreen
End Sub

Sub LoadMeshes
  Local INTEGER s, kk, vb0, eb0, nvs, nes, ia, ib
  Local FLOAT x, y, z
  vb0 = 0 : eb0 = 0
  Restore
  For s = 0 To NSHIP - 1
    Read nvs, nes
    shV0(s) = vb0 : shNV(s) = nvs : shE0(s) = eb0 : shNE(s) = nes
    For kk = 0 To nvs - 1
      Read x, y, z
      mvx(vb0 + kk) = 2 * x : mvy(vb0 + kk) = 2 * y : mvz(vb0 + kk) = 2 * z
    Next kk
    For kk = 0 To nes - 1
      Read ia, ib
      mea(eb0 + kk) = vb0 + ia : meb(eb0 + kk) = vb0 + ib
    Next kk
    vb0 = vb0 + nvs : eb0 = eb0 + nes
  Next s
  If vb0 <> NV Or eb0 <> NE Then Error "mesh DATA does not match NV and NE"
End Sub

' Elite's MoveShips and UpdatePlayer, reduced to deterministic drift
Sub MoveObjects
  Local INTEGER n
  For n = 0 To NOBJ - 1
    obZ(n) = obZ(n) - 40
    If obZ(n) < -12000 Then obZ(n) = 12000
    obX(n) = obX(n) + 13 * (n - 1.5)
    If Abs(obX(n)) > 12000 Then obX(n) = -0.9 * obX(n)
  Next n
  lasFlash = ((nfr And 7) = 0)
  pEnergy = (nfr * 3) And 255
  pFsh = (nfr * 5) And 255 : pAsh = 255 - pFsh
  pFuel = 70 - (nfr And 63) : pCabT = (nfr * 2) And 255
  pLasT = (nfr * 7) And 255 : pAltit = 255 - pCabT
  pMissl = (nfr \ 16) And 3 : ecmOn = nfr And 16 : inSafe = nfr And 32
  dSpd = 20 + (nfr And 15)
  rollv = ((nfr \ 4) And 15) - 8 : pitchv = ((nfr \ 8) And 15) - 8
End Sub

Sub InitStardust
  Local INTEGER i
  For i = 0 To NSTAR - 1
    stX(i) = SdSM(rb(3 * i))
    stY(i) = SdSM(rb(3 * i + 1))
    stZ(i) = 1 + rb(3 * i + 2)
  Next i
  For i = 0 To 4 * NSTAR - 1 : spc(i) = cWhite : Next i
End Sub

Function SdSM(b As FLOAT) As FLOAT
  Local INTEGER v
  v = b
  If (v And 128) <> 0 Then SdSM = -(v And 127) Else SdSM = (v And 127)
End Function

' Elite's DrawStardust, front view (elite.bas:844-938)
Sub DrawStardust
  Local INTEGER i, zh, np, sx, sy, sy2
  Local FLOAT q, x, y, z, a, b, qb, sp
  a = alp
  b = bet
  sp = dSpeed
  np = 0
  Array Set -1, spx()
  For i = 0 To NSTAR - 1
    x = stX(i) : y = stY(i) : z = stZ(i)
    zh = z
    q = (Int(64 * sp / zh)) Or 1
    z = z - sp / 4
    y = y + Fix(y) * q / 256
    x = x + Fix(x) * q / 256
    y = y - a * Fix(x) / 256
    x = x + a * Fix(y) / 256
    qb = Int(Abs(b) * Int(Abs(y)) / 256)
    x = x + 2 * qb * qb / 256
    y = y - b
    If Abs(x) >= 120 Or Abs(y) >= 120 Or z < 16 Then
      rk = (rk + 3) And 255
      y = SdSM(rb(rk) Or 4)
      x = SdSM(rb(rk + 1) Or 8)
      z = rb(rk + 2) Or 144
    EndIf
    stX(i) = x : stY(i) = y : stZ(i) = z
    If Abs(y) < VCY Then
      zh = z
      sx = VCX + Fix(x)
      sy = VCY - Fix(y)
      spx(np) = sx : spy(np) = sy : np = np + 1
      If zh < 144 Then
        spx(np) = sx + 1 : spy(np) = sy : np = np + 1
        If zh < 80 Then
          If (sy And 7) = 0 Then sy2 = sy + 1 Else sy2 = sy - 1
          spx(np) = sx : spy(np) = sy2 : np = np + 1
          spx(np) = sx + 1 : spy(np) = sy2 : np = np + 1
        EndIf
      EndIf
    EndIf
  Next i
  Pixel spx(), spy(), spc()
End Sub

' the planet: Elite's outline and two half-ellipse meridians (elite.bas:764-829)
Sub DrawPlanet
  Local INTEGER cx, cy, r
  Local FLOAT vnx, vny, vnz, vrx, vry, vrz, vsx, vsy, vsz
  cx = 250 : cy = 48 : r = 30
  pa = pa + 0.02
  vnx = Cos(pa) : vny = 0.3 : vnz = Sin(pa)
  vsx = -0.8 * Sin(pa) : vsy = 0.6 : vsz = 0.8 * Cos(pa)
  vrx = 0.2 : vry = 0.9 : vrz = -0.4
  Circle cx, cy, r, 1, 1, cGreen, -1
  HalfCircle cx, cy, r, vnx, vny, vnz, vrx, vry, vrz
  HalfCircle cx, cy, r, vsx, vsy, vsz, vrx, vry, vrz
End Sub

Sub HalfCircle(cx As INTEGER, cy As INTEGER, r As INTEGER, ax As FLOAT, ay As FLOAT, az As FLOAT, bx As FLOAT, by As FLOAT, bz As FLOAT)
  Local INTEGER kk, px, py, lx, ly, have
  Local FLOAT c, sn, pz
  have = 0
  For kk = 0 To NSEG
    c = ctab(kk And (NSEG - 1))
    sn = stab(kk And (NSEG - 1))
    pz = az * c + bz * sn
    If pz <= 0 Then
      px = cx + r * (ax * c + bx * sn)
      py = cy - r * (ay * c + by * sn)
      If have Then Line lx, ly, px, py, 1, cGreen
      lx = px : ly = py : have = 1
    Else
      have = 0
    EndIf
  Next kk
End Sub

' one ship as wireframe: rotate and project every vertex, one LINE per edge
Sub DrawMesh(n As INTEGER)
  Local INTEGER kk, k1, ee, c
  Local FLOAT cy, sy, cp, sp, cr, sr, x, y, z, rz
  Local FLOAT m00, m01, m02, m10, m11, m12, m20, m21, m22
  Local FLOAT ox, oy, oz
  shYaw(n) = shYaw(n) + shDy(n)
  shPit(n) = shPit(n) + shDp(n)
  shRol(n) = shRol(n) + shDr(n)
  cy = Cos(shYaw(n)) : sy = Sin(shYaw(n))
  cp = Cos(shPit(n)) : sp = Sin(shPit(n))
  cr = Cos(shRol(n)) : sr = Sin(shRol(n))
  m00 = cy * cr + sy * sp * sr : m01 = sr * cp : m02 = cy * sp * sr - sy * cr
  m10 = sy * sp * cr - cy * sr : m11 = cr * cp : m12 = sr * sy + cy * sp * cr
  m20 = sy * cp : m21 = -sp : m22 = cy * cp
  ox = shX(n) : oy = shY(n) : oz = shZ(n)
  k1 = shV0(n) + shNV(n) - 1
  For kk = shV0(n) To k1
    x = mvx(kk) : y = mvy(kk) : z = mvz(kk)
    rz = m20 * x + m21 * y + m22 * z + oz
    psx(kk) = VCX + VPLANE * (m00 * x + m01 * y + m02 * z + ox) / rz
    psy(kk) = VCY - VPLANE * (m10 * x + m11 * y + m12 * z + oy) / rz
  Next kk
  c = shCol(n)
  k1 = shE0(n) + shNE(n) - 1
  For ee = shE0(n) To k1
    Line psx(mea(ee)), psy(mea(ee)), psx(meb(ee)), psy(meb(ee)), 1, c
  Next ee
End Sub

' elite.bas:748
Sub SpaceFurniture
  If lasFlash Then
    Line 40, VIEWH - 2, VCX - 4 + rb(rk) / 32, VCY, 1, cRed
    Line SCRW - 40, VIEWH - 2, VCX - 4 + rb(rk + 1) / 32, VCY, 1, cRed
  EndIf
  Line 0, 0, SCRW - 2, 0, 1, cWhite
  Box 0, 0, 2, VIEWH, 0, cWhite, cWhite
  Box SCRW - 2, 0, 2, VIEWH, 0, cWhite, cWhite
  Text 6, 4, Str$(nfr), "LT", 7, 1, cWhite
  Line VCX - 25, VCY, VCX - 12, VCY, 1, cWhite
  Line VCX + 12, VCY, VCX + 25, VCY, 1, cWhite
  Line VCX, VCY - 20, VCX, VCY - 10, 1, cWhite
  Line VCX, VCY + 10, VCX, VCY + 20, 1, cWhite
End Sub

' elite.bas:1008, drawn every frame here
Sub DashStatic
  Local INTEGER i
  Box 0, DASHY, SCRW, SCRH - DASHY, 0, cBlack, cBlack
  Line 0, DASHY, SCRW - 1, DASHY, 1, cCyan
  For i = 0 To 5
    Text 17, DLY(i) - 1, LLAB$(i), "RT", 7, 1, cWhite
    Box DL, DLY(i) - 1, DW + 2, 5, 1, cDim, -1
  Next i
  For i = 0 To 6
    Text 303, DRY(i) - 1, RLAB$(i), "LT", 7, 1, cWhite
    Box DR, DRY(i) - 1, DW + 2, 5, 1, cDim, -1
  Next i
  Circle CPX, CPY, CPR + 2, 1, 1.25, cDim, -1
End Sub

' elite.bas:1030
Sub DrawDash
  Local INTEGER i, ev
  DashStatic
  Bar DR, DRY(0), dSpd \ 2, 14, cRed, cYellow
  Pointer DR, DRY(1), 8 + rollv
  Pointer DR, DRY(2), 8 + pitchv
  For i = 0 To 3
    ev = (pEnergy \ 4) - (3 - i) * 16
    If ev < 0 Then ev = 0
    If ev > 16 Then ev = 16
    Bar DR, DRY(3 + i), ev, 3, cYellow, cRed
  Next i
  Bar DL, DLY(0), pFsh \ 16, 3, cYellow, cRed
  Bar DL, DLY(1), pAsh \ 16, 3, cYellow, cRed
  Bar DL, DLY(2), pFuel \ 4, 3, cYellow, cRed
  Bar DL, DLY(3), pCabT \ 16, 11, cRed, cYellow
  Bar DL, DLY(4), pLasT \ 16, 11, cRed, cYellow
  Bar DL, DLY(5), pAltit \ 16, 99, cRed, cYellow
  MissileBlocks
  DrawScanner
  DrawCompass
  Bulbs
End Sub

Sub Bar(x As INTEGER, y As INTEGER, lv As INTEGER, t1 As INTEGER, hi As INTEGER, lo As INTEGER)
  Local INTEGER w, c, v
  v = lv
  If v < 0 Then v = 0
  If v > 16 Then v = 16
  c = lo
  If v >= t1 Then c = hi
  w = v * 2.5
  Box x + 1, y, DW, 3, 0, cBlack, cBlack
  If w > 0 Then Box x + 1, y, w, 3, 0, c, c
End Sub

Sub Pointer(x As INTEGER, y As INTEGER, p As INTEGER)
  Local INTEGER v
  v = p
  If v < 0 Then v = 0
  If v > 15 Then v = 15
  Box x + 1, y, DW, 3, 0, cBlack, cBlack
  Box x + 1 + v * 2.5, y, 3, 3, 0, cYellow, cYellow
End Sub

Sub MissileBlocks
  Local INTEGER i, c
  For i = 0 To 3
    c = cBlack
    If i < pMissl Then c = cGreen
    Box 20 + i * 10, 225, 7, 5, 0, c, c
  Next i
End Sub

Sub DrawScanner
  Local INTEGER n, px, py, bse, c
  Box SCX - SCA, SCY - SCB - 9, 2 * SCA, 2 * SCB + 18, 0, cBlack, cBlack
  Circle SCX, SCY, SCB, 1, SCA / SCB, cCyan, -1
  For n = 0 To NOBJ - 1
    If Abs(obX(n)) < 16384 And Abs(obY(n)) < 16384 And Abs(obZ(n)) < 16384 Then
      px = SCDOTX + obX(n) / SCXDIV
      bse = SCY - obZ(n) / SCZDIV
      py = bse - obY(n) / SCYDIV
      If py < SCTOP Then py = SCTOP
      If py > SCBOT Then py = SCBOT
      If px > SCX - SCA And px < SCX + SCA Then
        c = obCol(n)
        Line px, bse, px, py, 1, c
        Box px, py - 1, 5, 2, 0, c, c
      EndIf
    EndIf
  Next n
End Sub

Sub DrawCompass
  Local INTEGER px, py
  Local FLOAT m
  m = Sqr(obX(3) * obX(3) + obY(3) * obY(3) + obZ(3) * obZ(3))
  If m < 1 Then Exit Sub
  Box CPX - CPR - 4, CPY - CPR - 3, 2 * CPR + 9, 2 * CPR + 7, 0, cBlack, cBlack
  Circle CPX, CPY, CPR + 2, 1, 1.25, cDim, -1
  px = CPX + CPR * 1.25 * obX(3) / m
  py = CPY - CPR * obY(3) / m
  If obZ(3) >= 0 Then
    Box px, py, 3, 2, 0, cYellow, cYellow
  Else
    Box px, py, 3, 1, 0, cGreen, cGreen
  EndIf
End Sub

Sub Bulbs
  Box BULBX, BULBY, 9, 7, 0, cBlack, cBlack
  Box BULBX + 13, BULBY, 9, 7, 0, cBlack, cBlack
  If ecmOn Then Text BULBX + 2, BULBY, "E", "LT", 7, 1, cYellow
  If inSafe Then Text BULBX + 15, BULBY, "S", "LT", 7, 1, cGreen
End Sub

Sub ViewName
  Local vn$
  Select Case nfr And 3
    Case 0 : vn$ = "Front view"
    Case 1 : vn$ = "Rear view"
    Case 2 : vn$ = "Left view"
    Case 3 : vn$ = "Right view"
  End Select
  Text 110, 8, vn$, "LT", 7, 1, cWhite
End Sub

Sub DrawMessage
  If (nfr And 63) < 20 Then Text MSGX, MSGY, "ENERGY LOW", "LT", 7, 1, cWhite
End Sub

' ---- mesh data: generated by gen_gfx.py from Bas/3ddemo.bas ----
' Viper: 15 vertices, 20 edges
Data 15, 20
Data 0,0,72, 0,16,24, 0,-16,24, 48,0,-24, -48,0,-24
Data 24,-16,-24, -24,-16,-24, 24,16,-24, -24,16,-24, -32,0,-24
Data 32,0,-24, 8,8,-24, -8,8,-24, -8,-8,-24, 8,-8,-24
Data 1,7, 7,8, 1,8, 0,1, 4,8, 0,4, 0,3, 3,7, 4,6, 2,6
Data 0,2, 2,5, 3,5, 5,6, 9,12, 12,13, 9,13, 11,14, 10,11, 10,14
' Cobra: 28 vertices, 37 edges
Data 28, 37
Data 32,0,76, -32,0,76, 0,26,24, -120,-3,-8, 120,-3,-8
Data -88,16,-40, 88,16,-40, 128,-8,-40, -128,-8,-40, 0,26,-40
Data -32,-24,-40, 32,-24,-40, -36,8,-40, -8,12,-40, 8,12,-40
Data 36,8,-40, 36,-12,-40, 8,-16,-40, -8,-16,-40, -36,-12,-40
Data 0,0,76, 0,0,90, -80,-6,-40, -80,6,-40, -88,0,-40
Data 80,6,-40, 88,0,-40, 80,-6,-40
Data 1,2, 0,1, 0,2, 2,5, 1,5, 0,6, 2,6, 3,5, 1,3, 0,4
Data 4,6, 2,9, 5,9, 6,9, 5,8, 3,8, 4,7, 6,7, 7,11, 10,11
Data 8,10, 12,13, 13,18, 18,19, 12,19, 14,15, 15,16, 16,17, 14,17, 22,24
Data 23,24, 22,23, 25,26, 26,27, 25,27, 1,10, 0,11
' Sidewinder: 10 vertices, 15 edges
Data 10, 15
Data -32,0,36, 32,0,36, 64,0,-28, -64,0,-28, 0,16,-28
Data 0,-16,-28, -12,6,-28, 12,6,-28, 12,-6,-28, -12,-6,-28
Data 0,1, 1,4, 0,4, 3,4, 0,3, 1,2, 2,4, 2,5, 3,5, 6,7
Data 7,8, 8,9, 6,9, 0,5, 1,5
' ---- end of mesh data ----
