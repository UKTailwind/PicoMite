"""make_games.py - build the Phase 1 game workloads (H3/H4) from the repo, into this directory.

    python make_games.py [exile] [elite] [pop]      (no argument: all three)

Nothing in the repository is written.  Every output lands beside this script:

  exile/  gbxphys.bas  gbxphysp.bas   the faithful player kernel in BASIC (Bas/exile/physics.bas)
          gbxcsub.bas  gbxcsubp.bas   the same kernel as a CSUB        (Bas/exile/physcsub.bas)
          gbx_feed.txt                all 18 feed files in one, read before the timed region
          gbx_world.bin               the 65,536 bytes of world_types.bin the kernel reads
          gbx_tables.bin              tables.bin, for the CSUB
  elite/  gbelite.bas  gbelitep.bas   DEMOSCENE 1 (flight and combat), fixed tick, seeded
          gbelite_lib.bas             its library (declarations + ship data)
          src/ data/                  the patched source copies the two are built from
  pop/    gbpop.bas    gbpopp.bas     Prince of Pico, scenario harness, stripped by build.py
          gbpop.bas.map               stripped line -> source line
          gbscen.txt                  every scen/*.txt except discover*, joined

The ...p.bas twin of each program differs in one line only: GBPROF = 1, which
starts the PC sampler straight after the TIMER = 0 that opens the timed region.
"""
import importlib.util
import json
import os
import re
import shutil
import sys

# The repository's build.py files are loaded as modules below; never leave a
# __pycache__ entry behind in the repository for them.
sys.dont_write_bytecode = True

REPO = r"D:/Dropbox/PicoMite/PicoMite"
HERE = os.path.dirname(os.path.abspath(__file__))
# The sampler's two tables are entries * 8 bytes each (pcs_ent_t, core/MMBasic.c
# PcsStart).  Bas/bench/README.md suggests 8192 on an RP2350, but 8192 makes two
# 64 KB tables, under half the 152 KB HDMIUSB heap, so GetMemory takes them from
# SRAM first (core/Memory.c:2146-2148) - 128 KB the timing run does not lose, and
# Elite's per-ship Draw3D meshes (4 KB each, created in flight) and PoP's temp
# strings would then spill to PSRAM in the profiled run only.  16384 makes each
# table 128 KB, over half the heap, so both go straight to PSRAM and the profiled
# run's SRAM differs from the timing run's only by the ~16 KB [PERF] counters.
PCS_ENTRIES = 16384
NL = "\n"


def rd(path):
    with open(path, encoding="utf-8", newline="") as fh:
        return fh.read().replace("\r\n", "\n")


def wr(path, text, crlf=False):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if crlf:
        text = text.replace("\n", "\r\n")
    with open(path, "w", encoding="utf-8", newline="") as fh:
        fh.write(text)


def sub_once(text, old, new, what):
    n = text.count(old)
    if n != 1:
        raise SystemExit("%s: expected exactly one '%s', found %d" % (what, old.strip()[:60], n))
    return text.replace(old, new)


def cut_block(text, start, what):
    """Remove the block that begins with the line `start` and ends with the next 'End Sub'."""
    i = text.find(NL + start + NL)
    if i < 0:
        raise SystemExit("%s: '%s' not found" % (what, start))
    j = text.find(NL + "End Sub" + NL, i + 1)
    if j < 0:
        raise SystemExit("%s: no End Sub after '%s'" % (what, start))
    return text[:i + 1] + text[j + len(NL + "End Sub" + NL):], i + 1


def prof_twin(text, what):
    return sub_once(text, "Const GBPROF = 0", "Const GBPROF = 1", what)


def check_lines(text, what, limit=240):
    for n, ln in enumerate(text.split(NL), 1):
        if len(ln) > limit:
            raise SystemExit("%s line %d is %d characters" % (what, n, len(ln)))


# =====================================================================
#  Exile: the player kernel, interpreted and as a CSUB
# =====================================================================
EX_COMPARE = ['px', 'py', 'vx', 'vy', 'flags', 'state', 'sprite', 'energy', 'jetpack',
              'angle', 'facing', 'immob', 'thrust_immob', 'jet_ok', 'timer', 'frame', 'palette']

EX_HEAD = """' ---- gamebench (Phase 1, H3).  Written by make_games.py: see NOTES.md ----
' The local/global variable split is set explicitly, to the RP2350 default:
' OPTION LOCAL VARIABLES outlives the program that sets it (until a reset),
' and Elite's library and Prince of Pico both set 128, which would change the
' hash tables this kernel's lookups probe, and so its time, with run order.
' It must come before the first variable (CONST included) exists.
Option Local Variables 256
' GBPROF 1 starts the PC sampler straight after the TIMER = 0 that opens the
' timed region.  GBREPS plays the whole scenario set that many times.
Const GBPROF = 0
Const GBREPS = %(reps)d
Const GBNS = %(ns)d, GBNT = %(nt)d
Dim gbFd(16, GBNT - 1), gbEx(16, GBNS - 1), gbV(16)
Dim gbNm$(GBNS - 1) LENGTH 24
Dim gbSX(GBNS - 1), gbSY(GBNS - 1), gbN(GBNS - 1), gbT0(GBNS - 1), gbBad(GBNS - 1)
Dim gbGood, gbMiss, gbNote, gbFlt
"""

# The harness shared by both programs: load the feed, time an identical loop
# without the kernel call (GAMENULL), then the timed loop with it (GAMEBENCH).
EX_MAIN = """' ===================================================================
' the gamebench harness (make_games.py) - replaces Main and PrintState.
' The feed is read into gbFd() before anything is timed; the timed loop
' then does exactly what the old harness did around each tick except the
' file read and the PRINT of every tick, and checks each scenario's end
' state against the game's own (the last tick of out/traces/<name>.json).
' ===================================================================
Sub Main
  Local Float mhz, tNull, tRun
  Local n
  homeDir$ = MM.Info(Path) : If homeDir$ = "NONE" Then homeDir$ = "A:/"
  mhz = Val(MM.Info(CPUSPEED)) / 1000000
  Print "GAMESTART %(name)s " + MM.Device$ + " " + Str$(MM.Ver) + " " + Str$(mhz) + " MHz prof=" + Str$(GBPROF) + " reps=" + Str$(GBREPS)
%(load)s
  GbLoad
  n = GBNT * GBREPS
  ' calibration: the same loop with the kernel call left out, so that
  ' GAMEKERNEL = GAMEBENCH - GAMENULL is the kernel alone
  Timer = 0
  GbRunAll 0
  tNull = Timer
  Timer = 0
  If GBPROF Then Option Profiling On, Sample, %(pcs)d
  GbRunAll 1
  tRun = Timer
  Print "GAMEBENCH %(name)s " + Str$(n) + " " + Str$(tRun, 0, 1) + " " + Str$(tRun / n, 0, 4)
  Print "GAMENULL %(name)s " + Str$(n) + " " + Str$(tNull, 0, 1) + " " + Str$(tNull / n, 0, 4)
  Print "GAMEKERNEL %(name)s " + Str$(n) + " " + Str$(tRun - tNull, 0, 1) + " " + Str$((tRun - tNull) / n, 0, 4)
  GbReport
End Sub

' every scenario, GBREPS times; doK = 0 is the calibration pass.  Both passes
' must execute the same harness statements, so the unread-feed check is one
' unconditional statement: as an IF it ran its THEN on every tick of the
' calibration pass (the kernel never consumed the feed there) and on none of
' the timed pass, which made GAMEKERNEL too small.
Sub GbRunAll(doK)
%(runall)s
End Sub

' the end state of scenario s, in the order run_phystest.py compares it
Sub GbEnd(s, doK)
  Local k, bad
%(endstate)s
  bad = 0
  For k = 0 To 16
    If gbV(k) <> gbEx(k, s) Then bad = 1
  Next k
  If doK Then
    If bad Then gbBad(s) = gbBad(s) + 1 Else gbGood = gbGood + 1
  EndIf
End Sub

Sub GbLoad
  Local s, t, k, sx, sy, n, ns, nt
  Local nm$
  Open homeDir$ + "gbx_feed.txt" For Input As #1
  Input #1, ns, nt
  If ns <> GBNS Or nt <> GBNT Then Print "gbx_feed.txt does not match this program" : End
  k = 0
  For s = 0 To GBNS - 1
    Input #1, nm$, sx, sy, n
    gbNm$(s) = nm$ : gbSX(s) = sx : gbSY(s) = sy : gbN(s) = n : gbT0(s) = k
    For t = 1 To n
      Input #1, gbFd(0,k), gbFd(1,k), gbFd(2,k), gbFd(3,k), gbFd(4,k), gbFd(5,k), gbFd(6,k), gbFd(7,k), gbFd(8,k), gbFd(9,k), gbFd(10,k), gbFd(11,k), gbFd(12,k), gbFd(13,k), gbFd(14,k), gbFd(15,k), gbFd(16,k)
      k = k + 1
    Next t
  Next s
  Close #1
  If k <> GBNT Then Print "gbx_feed.txt holds"; k; " ticks, not"; GBNT : End
  Restore GbExpect
  For s = 0 To GBNS - 1
    For k = 0 To 16 : Read gbEx(k, s) : Next k
  Next s
End Sub

Sub GbReport
  Local s
  For s = 0 To GBNS - 1
    If gbBad(s) Then Print "GAMEDIFF %(name)s " + gbNm$(s) + ": end state differs in " + Str$(gbBad(s)) + " of " + Str$(GBREPS) + " passes"
  Next s
  Print "GAMECHECK %(name)s " + Str$(gbGood) + " of " + Str$(GBNS * GBREPS) + " end states match, unread feed " + Str$(gbMiss) + ", faults " + Str$(gbFlt) + ", notes " + Str$(gbNote)
  If gbGood = GBNS * GBREPS And gbMiss = 0 And gbFlt = 0 Then Print "GAMECHECK PASS" Else Print "GAMECHECK FAIL"
End Sub
"""

EX_RUNALL_BASIC = """  Local r, s, tk, kmask
  For r = 1 To GBREPS
    For s = 0 To GBNS - 1
      InitPlayer
      If gbSX(s) >= 0 Then ps(0) = gbSX(s) : ps(2) = gbSY(s) : pf(0) = &H80 : pf(2) = 0 : vel(0) = 0 : vel(2) = 0
      For tk = gbT0(s) To gbT0(s) + gbN(s) - 1
        kmask = gbFd(0, tk) : relTY = gbFd(1, tk) : feedN = gbFd(2, tk) : feedI = 0
        feedA(0) = gbFd(3, tk) : feedV(0) = gbFd(4, tk) : feedA(1) = gbFd(5, tk) : feedV(1) = gbFd(6, tk)
        feedA(2) = gbFd(7, tk) : feedV(2) = gbFd(8, tk)
        wlYF(0) = gbFd(9, tk) : wlYF(1) = gbFd(10, tk) : wlYF(2) = gbFd(11, tk) : wlYF(3) = gbFd(12, tk)
        wlY(0) = gbFd(13, tk) : wlY(1) = gbFd(14, tk) : wlY(2) = gbFd(15, tk) : wlY(3) = gbFd(16, tk)
        If doK Then DoTick kmask
        gbMiss = gbMiss + doK * (feedI < feedN)
      Next tk
      GbEnd s, doK
    Next s
  Next r"""

EX_END_BASIC = """  gbV(0) = ps(0) * 256 + pf(0) : gbV(1) = ps(2) * 256 + pf(2) : gbV(2) = vel(0) : gbV(3) = vel(2)
  gbV(4) = oFlags And &HFE : gbV(5) = oState : gbV(6) = oSpr : gbV(7) = oEnergy
  gbV(8) = jetHi * 256 + jetLo : gbV(9) = pAngle : gbV(10) = pFacing : gbV(11) = immob
  gbV(12) = tImmob : gbV(13) = jetOk : gbV(14) = oTimer : gbV(15) = frm : gbV(16) = oPal"""

EX_RUNALL_CSUB = """  Local r, s, tk
  For r = 1 To GBREPS
    For s = 0 To GBNS - 1
      InitState
      If gbSX(s) >= 0 Then st(S_X) = gbSX(s) : st(S_Y) = gbSY(s) : st(S_XF) = &H80 : st(S_YF) = 0 : st(S_VX) = 0 : st(S_VY) = 0
      For tk = gbT0(s) To gbT0(s) + gbN(s) - 1
        st(S_KMASK) = gbFd(0, tk) : st(S_RELTY) = gbFd(1, tk) : st(S_FEEDN) = gbFd(2, tk)
        st(S_FEEDA0) = gbFd(3, tk) : st(S_FEEDV0) = gbFd(4, tk) : st(S_FEEDA1) = gbFd(5, tk) : st(S_FEEDV1) = gbFd(6, tk)
        st(S_FEEDA2) = gbFd(7, tk) : st(S_FEEDV2) = gbFd(8, tk)
        st(S_WL0) = gbFd(9, tk) : st(S_WL1) = gbFd(10, tk) : st(S_WL2) = gbFd(11, tk) : st(S_WL3) = gbFd(12, tk)
        st(S_WL4) = gbFd(13, tk) : st(S_WL5) = gbFd(14, tk) : st(S_WL6) = gbFd(15, tk) : st(S_WL7) = gbFd(16, tk)
        If doK Then ExileUpdate st(), world(), tbl()
        If st(S_FAULT) Then GbFault doK
        gbMiss = gbMiss + doK * (st(S_FEEDI) < st(S_FEEDN))
      Next tk
      GbEnd s, doK
    Next s
  Next r"""

EX_END_CSUB = """  gbV(0) = st(S_X) * 256 + st(S_XF) : gbV(1) = st(S_Y) * 256 + st(S_YF) : gbV(2) = st(S_VX) : gbV(3) = st(S_VY)
  gbV(4) = st(S_FLAGS) And &HFE : gbV(5) = st(S_STATE) : gbV(6) = st(S_SPRITE) : gbV(7) = st(S_ENERGY)
  gbV(8) = st(S_JETHI) * 256 + st(S_JETLO) : gbV(9) = st(S_ANGLE) : gbV(10) = st(S_FACING) : gbV(11) = st(S_IMMOB)
  gbV(12) = st(S_TIMMOB) : gbV(13) = st(S_JETOK) : gbV(14) = st(S_TIMER) : gbV(15) = st(S_FRAME) : gbV(16) = st(S_PALETTE)"""

EX_FAULT_CSUB = """
' fault 3 is a tile whose collision routine is not modelled, which the old
' harness noted and carried on from; anything else it stopped on
Sub GbFault(doK)
  If st(S_FAULT) = 3 Then gbNote = gbNote + doK Else gbFlt = gbFlt + doK
End Sub
"""

EX_LOAD_BASIC = """  LoadTables
  LoadWorld"""

EX_LOAD_CSUB = """  Open homeDir$ + "gbx_world.bin" For Input As #3
  MEMORY INPUT 3, 65536, world()
  Close #3
  Open homeDir$ + "gbx_tables.bin" For Input As #3
  MEMORY INPUT 3, TABLES_BYTES, tbl()
  Close #3"""


def exile_feed_and_expect():
    out = os.path.join(REPO, "Bas", "exile_tools", "out")
    names = [n.strip() for n in rd(os.path.join(out, "phys", "phys_list.txt")).split(NL) if n.strip()]
    feed, expect, total = [], [], 0
    for nm in names:
        lines = [l for l in rd(os.path.join(out, "phys", "phys_%s.txt" % nm)).split(NL) if l.strip()]
        sx, sy, n = (int(v) for v in lines[0].split(","))
        ticks = lines[1:]
        if len(ticks) != n:
            raise SystemExit("phys_%s.txt: header says %d ticks, file has %d" % (nm, n, len(ticks)))
        for t in ticks:
            if len(t.split(",")) != 17:
                raise SystemExit("phys_%s.txt: a tick without 17 values: %s" % (nm, t))
        feed.append("%s,%d,%d,%d" % (nm, sx, sy, n))
        feed.extend(ticks)
        total += n
        tr = json.load(open(os.path.join(out, "traces", nm + ".json")))
        if len(tr["ticks"]) != n:
            raise SystemExit("%s: trace has %d ticks, feed %d" % (nm, len(tr["ticks"]), n))
        idx = {f: i for i, f in enumerate(tr["fields"])}
        last = tr["ticks"][-1]
        v = [last[idx[f]] for f in EX_COMPARE]
        v[2] &= 255                 # the trace holds vx, vy signed; the kernel holds the byte
        v[3] &= 255
        v[4] &= 0xFE                # run_phystest.py compares flags without bit 0
        expect.append((nm, v))
    feedtext = "%d,%d" % (len(names), total) + NL + NL.join(feed) + NL
    return names, total, feedtext, expect


def exile_program(src, head_after, main_text, fault_text, expect, what):
    text = rd(src)
    text = sub_once(text, head_after + NL, head_after + NL + EX_HEAD_FILLED + NL, what)
    text, at = cut_block(text, "Sub Main", what)
    text, _ = cut_block(text, "Sub PrintState(tk)", what)
    text = text[:at] + main_text + fault_text + NL + text[at:]
    text = text.replace('"world_types.bin"', '"gbx_world.bin"')
    if "world_types.bin" in text:
        raise SystemExit(what + ": world_types.bin still named")
    data = ["", "' ---- gamebench: each scenario's end state as the game left it (out/traces), in feed order",
            "GbExpect:"]
    for nm, v in expect:
        data.append("' " + nm)
        data.append("Data " + ",".join(str(x) for x in v))
    text = text.rstrip(NL) + NL + NL.join(data) + NL
    check_lines(text, what)
    return text


def make_exile():
    global EX_HEAD_FILLED
    dst = os.path.join(HERE, "exile")
    os.makedirs(dst, exist_ok=True)
    names, total, feedtext, expect = exile_feed_and_expect()
    wr(os.path.join(dst, "gbx_feed.txt"), feedtext)
    out = os.path.join(REPO, "Bas", "exile_tools", "out")
    world = open(os.path.join(out, "world_types.bin"), "rb").read()[:65536]
    open(os.path.join(dst, "gbx_world.bin"), "wb").write(world)
    shutil.copyfile(os.path.join(out, "phys", "tables.bin"), os.path.join(dst, "gbx_tables.bin"))

    for prog, src, reps, runall, endstate, load, fault, name in (
            ("gbxphys", "physics.bas", 1, EX_RUNALL_BASIC, EX_END_BASIC, EX_LOAD_BASIC, "", "exile_phys"),
            ("gbxcsub", "physcsub.bas", 15, EX_RUNALL_CSUB, EX_END_CSUB, EX_LOAD_CSUB, EX_FAULT_CSUB, "exile_csub")):
        EX_HEAD_FILLED = EX_HEAD % {"reps": reps, "ns": len(names), "nt": total}
        main_text = EX_MAIN % {"name": name, "load": load, "pcs": PCS_ENTRIES,
                               "runall": runall, "endstate": endstate}
        text = exile_program(os.path.join(REPO, "Bas", "exile", src), "Option CONSOLE SERIAL",
                             main_text, fault, expect, prog)
        wr(os.path.join(dst, prog + ".bas"), text, crlf=True)
        wr(os.path.join(dst, prog + "p.bas"), prof_twin(text, prog), crlf=True)
        print("exile: %s.bas (+p) %d bytes, %d lines" % (prog, len(text), text.count(NL)))
    print("exile: gbx_feed.txt %d bytes, %d scenarios, %d ticks; gbx_world.bin %d; gbx_tables.bin %d"
          % (len(feedtext), len(names), total, len(world), os.path.getsize(os.path.join(dst, "gbx_tables.bin"))))


EX_HEAD_FILLED = ""


# =====================================================================
#  Elite: DEMOSCENE 1, flight and combat, drawn
# =====================================================================
EL_FRAMES = 600             # DEMOSCENE 1 was measured over 300; its script runs on to frame 999
EL_SEED = 20260924
RND_RE = re.compile(r"(?<![A-Za-z0-9_.])RND(?![A-Za-z0-9_$%!])", re.I)


def split_comment(line):
    q = False
    for i, ch in enumerate(line):
        if ch == '"':
            q = not q
        elif ch == "'" and not q:
            return line[:i], line[i:]
    return line, ""


def replace_rnd(text):
    """RND is reseeded from the TRNG every 100 calls on the RP2350 and RANDOMIZE
    is a no-op there, so a seeded MATH(RAND) stands in for it (code only)."""
    out, n = [], 0
    for ln in text.split(NL):
        code, com = split_comment(ln)
        code2, k = RND_RE.subn("Math(Rand)", code)
        n += k
        out.append(code2 + com)
    return NL.join(out), n


def make_elite():
    esrc = os.path.join(REPO, "Bas", "elite")
    dst = os.path.join(HERE, "elite")
    if os.path.isdir(os.path.join(dst, "src")):
        shutil.rmtree(os.path.join(dst, "src"))
    shutil.copytree(os.path.join(esrc, "src"), os.path.join(dst, "src"))
    os.makedirs(os.path.join(dst, "data"), exist_ok=True)
    for f in ("ships.bas", "tokens.bas"):
        shutil.copyfile(os.path.join(esrc, "data", f), os.path.join(dst, "data", f))

    # load the repo's build.py as a module and point it at the copies
    spec = importlib.util.spec_from_file_location("elite_build",
                                                  os.path.join(REPO, "Bas", "elite_tools", "build.py"))
    eb = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(eb)

    def build(out, outlib, libname):
        eb.SRC = os.path.join(dst, "src")
        eb.DATA = os.path.join(dst, "data", "ships.bas")
        eb.TOKENS = os.path.join(dst, "data", "tokens.bas")
        eb.OUT = out
        eb.OUTLIB = outlib
        eb.LIBNAME = libname
        argv = sys.argv
        sys.argv = ["build.py"]
        try:
            eb.main()
        finally:
            sys.argv = argv

    # 1. the unmodified sources must rebuild the repo's own elite.bas and elite_lib.bas
    ref, reflib = os.path.join(dst, "_ref_elite.bas"), os.path.join(dst, "_ref_elite_lib.bas")
    build(ref, reflib, "elite_lib.bas")
    same = rd(ref) == rd(os.path.join(esrc, "elite.bas")) and rd(reflib) == rd(os.path.join(esrc, "elite_lib.bas"))
    print("elite: unpatched rebuild %s the repo's elite.bas / elite_lib.bas"
          % ("MATCHES" if same else "DIFFERS FROM"))
    os.remove(ref)
    os.remove(reflib)

    # 2. the patches
    s = os.path.join(dst, "src")
    p = os.path.join(s, "00_main.bas")
    t = rd(p)
    t = sub_once(t, "IF MM.INFO(PSRAM SIZE) > 0 THEN OPTION TRACECACHE ON 256 ELSE OPTION TRACECACHE ON 128\n",
                 "' gamebench: no trace cache - the interpreter is what is being measured\n", "00_main")
    t = sub_once(t, "OPTION CACHE SUB DrawStardust, DrawScanner\n",
                 "' gamebench: (OPTION CACHE SUB removed with the trace cache)\n", "00_main")
    t = sub_once(t, "CONST DEMOFRAMES = 0 ", "CONST DEMOFRAMES = %d " % EL_FRAMES, "00_main")
    t = sub_once(t, "CONST PROFILE = 1 ", "CONST PROFILE = 0 ", "00_main")
    t = sub_once(t, "CONST SOUNDON = 1\n",
                 "CONST SOUNDON = 0                  ' gamebench: silent - no OPTION AUDIO needed, nothing waits on a queue\n",
                 "00_main")
    t = sub_once(t, "CONST TICKMAX = 0.5 ",
                 "' gamebench: every frame is worth a fixed GBTICK, and gbNow is a clock that\n"
                 "' advances by the 40 ms that tick stands for, so the game does the same work\n"
                 "' frame for frame on every build however fast the build is.\n"
                 "CONST GBTICK = 0.5\nDIM FLOAT gbNow\nCONST TICKMAX = 0.5 ", "00_main")
    wr(p, t)

    p = os.path.join(s, "45_sound.bas")
    t = rd(p)
    t = sub_once(t, "SUB NextTick\n  LOCAL FLOAT now\n  now = TIMER\n"
                    "  tick = (now - tickPrev) * TICKRATE / 1000\n"
                    "  IF tick > TICKMAX THEN tick = TICKMAX\n"
                    "  IF tick < 0 THEN tick = 0\n"
                    "  tickPrev = now\n",
                 "SUB NextTick\n  tick = GBTICK\n  gbNow = gbNow + GBTICK * 1000 / TICKRATE\n  tickPrev = gbNow\n",
                 "45_sound")
    t = sub_once(t, "SUB ResetTick\n  tickPrev = TIMER\n", "SUB ResetTick\n  tickPrev = gbNow\n", "45_sound")
    wr(p, t)

    p = os.path.join(s, "46_message.bas")
    t = rd(p)
    t = sub_once(t, "  msgUntil = TIMER + MSGTIME\n", "  msgUntil = gbNow + MSGTIME\n", "46_message")
    t = sub_once(t, "  IF TIMER > msgUntil THEN\n", "  IF gbNow > msgUntil THEN\n", "46_message")
    wr(p, t)

    p = os.path.join(s, "09_run.bas")
    t = rd(p)
    t = sub_once(t, "SetupScreen\nLoadStats\n",
                 "' ---- gamebench (Phase 1, H3/H4), make_games.py: see NOTES.md\n"
                 "' GBPROF 1 starts the PC sampler straight after the TIMER = 0 that opens the\n"
                 "' timed region.  RND is replaced by a seeded MATH(RAND) throughout: on the\n"
                 "' RP2350 RND reseeds itself from the hardware generator every 100 calls.\n"
                 "CONST GBPROF = 0\n"
                 "MATH RANDOMIZE %d\n"
                 'PRINT "GAMESTART elite_demo " + MM.DEVICE$ + " " + STR$(MM.VER) + " " + '
                 'STR$(VAL(MM.INFO(CPUSPEED)) / 1000000) + " MHz prof=" + STR$(GBPROF) + " frames=" + STR$(DEMOFRAMES)\n'
                 "SetupScreen\nLoadStats\n" % EL_SEED, "09_run")
    t = sub_once(t, "  frames = 0\n  tFrame = TIMER\n",
                 "  frames = 0\n  TIMER = 0\n  IF GBPROF THEN OPTION PROFILING ON, SAMPLE, %d\n"
                 "  tFrame = TIMER\n" % PCS_ENTRIES, "09_run")
    t = sub_once(t, "    IF frames = 40 OR frames = 80 OR frames = 120 OR frames = 250 THEN SaveShot frames\n",
                 "    ' gamebench: no screenshots\n", "09_run")
    t = sub_once(t, "IF dead THEN DeathScreen : PAUSE 1500\n",
                 "' gamebench: no death screen and no pause - the sampler is still running\n", "09_run")
    t = sub_once(t, "RestoreScreen\nframeMs = 0\n",
                 "RestoreScreen\n"
                 "IF frames > 0 THEN\n"
                 '  PRINT "GAMEBENCH elite_demo " + STR$(frames) + " " + STR$(tFlight, 0, 1) + " " + STR$(tFlight / frames, 0, 3)\n'
                 "ENDIF\n"
                 'PRINT "GAMECHECK elite_demo frames " + STR$(frames) + " shots " + STR$(shots) + " hits " + STR$(hits)'
                 ' + " kills " + STR$(kills);\n'
                 'PRINT " energy " + STR$(pEnergy) + " fuel " + STR$(pFuel)'
                 ' + " slots " + STR$(nUsed) + " dead " + STR$(dead) + " witch " + STR$(inWitch)'
                 ' + " objs " + STR$(maxObj)\n'
                 "frameMs = 0\n", "09_run")
    t = sub_once(t, "\n ENDIF\nENDIF\nEND\n", "\n ENDIF\nENDIF\nOPTION CONSOLE SERIAL\nEND\n", "09_run")
    wr(p, t)

    nr = 0
    for fn in sorted(os.listdir(s)):
        if fn.endswith(".bas") and fn != "00_main.bas":
            p = os.path.join(s, fn)
            t, k = replace_rnd(rd(p))
            if k:
                wr(p, t)
                nr += k
    print("elite: %d RND replaced by Math(Rand)" % nr)

    # 3. build the bench variant
    out, outlib = os.path.join(dst, "gbelite.bas"), os.path.join(dst, "gbelite_lib.bas")
    build(out, outlib, "gbelite_lib.bas")
    prog = rd(out)
    check_lines(prog, "gbelite.bas", 250)
    wr(os.path.join(dst, "gbelitep.bas"), sub_once(prog, "CONST GBPROF = 0", "CONST GBPROF = 1", "gbelite"))
    for f in ("gbelite.bas", "gbelitep.bas", "gbelite_lib.bas"):
        print("elite: %s %d bytes" % (f, os.path.getsize(os.path.join(dst, f))))


# =====================================================================
#  Prince of Pico: the scenario harness, headless
# =====================================================================
def make_pop():
    sg = os.path.join(REPO, "Bas", "sandglass_tools")
    dst = os.path.join(HERE, "pop")
    os.makedirs(dst, exist_ok=True)
    t = rd(os.path.join(sg, "player.bas"))
    what = "player.bas"
    t = sub_once(t, "Option Local Variables 128\n",
                 "Option Local Variables 128\n"
                 "' ---- gamebench (Phase 1, H3), make_games.py: see NOTES.md.  These must come\n"
                 "' after OPTION LOCAL VARIABLES, which has to run before any variable exists.\n"
                 "Const GBPROF = 0\n"
                 "Dim INTEGER gbFrames\n"
                 "Dim FLOAT gbMs, gbT, gbGf\n", what)
    t = sub_once(t, 'If Dir$(home + "scen.txt", FILE) <> "" Then RunScenarios : End\n',
                 'If Dir$(home + "gbscen.txt", FILE) <> "" Then RunScenarios : End\n'
                 'Print "gamebench: gbscen.txt is not beside the program, so there is nothing to run" : End\n', what)
    t = sub_once(t, '  Open home + "scen.txt" For Input As #3\n',
                 '  Open home + "gbscen.txt" For Input As #3\n'
                 '  Print "GAMESTART pop_scen " + MM.Device$ + " " + Str$(MM.Ver) + " " + '
                 'Str$(Val(MM.Info(CPUSPEED)) / 1000000) + " MHz prof=" + Str$(GBPROF)\n'
                 "  ' gamebench: the scenario report is switched off for the timed region, or\n"
                 "  ' the run would be timing the serial port rather than the engine\n"
                 "  Option Console None\n"
                 "  Timer = 0\n"
                 "  If GBPROF Then Option Profiling On, Sample, %d\n" % PCS_ENTRIES, what)
    # GAMEFRAME: the GameFrame calls alone, apart from the scenario set-up
    # (level loads, directive parsing) that the whole timed region also holds
    t = sub_once(t, "          ApplyCode Mid$(codes, i, 1)\n          GameFrame\n          scenFrames = scenFrames + 1\n",
                 "          ApplyCode Mid$(codes, i, 1)\n"
                 "          gbT = Timer\n"
                 "          GameFrame\n"
                 "          gbGf = gbGf + Timer - gbT\n"
                 "          scenFrames = scenFrames + 1 : gbFrames = gbFrames + 1\n", what)
    t = sub_once(t, '  Loop\n  Close #3\n  Print\n  Print "SCENARIOS: ',
                 "  Loop\n  Close #3\n  gbMs = Timer\n  Option Console Serial\n"
                 '  If gbFrames > 0 Then Print "GAMEBENCH pop_scen " + Str$(gbFrames) + " " + Str$(gbMs, 0, 1)'
                 ' + " " + Str$(gbMs / gbFrames, 0, 3)\n'
                 '  If gbFrames > 0 Then Print "GAMEFRAME pop_scen " + Str$(gbFrames) + " " + Str$(gbGf, 0, 1)'
                 ' + " " + Str$(gbGf / gbFrames, 0, 3)\n'
                 '  Print "GAMECHECK pop_scen " + Str$(tests - fails) + " of " + Str$(tests) + " checks passed"\n'
                 '  Print\n  Print "SCENARIOS: ', what)
    t = sub_once(t, "  Map Set\n  Option Console Both\nEnd Sub\n", "  Map Set\n  Option Console Serial\nEnd Sub\n", what)
    src = os.path.join(dst, "gbpop_src.bas")
    wr(src, t)

    spec = importlib.util.spec_from_file_location("pop_build", os.path.join(sg, "build.py"))
    pb = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(pb)
    out = os.path.join(dst, "gbpop.bas")
    pb.main([src, "-o", out])
    body = rd(out)
    check_lines(body, "gbpop.bas", 250)
    wr(os.path.join(dst, "gbpopp.bas"), prof_twin(body, "gbpop"))

    # the regression set, joined as run_scentest.py joins it
    scen = ""
    sdir = os.path.join(sg, "scen")
    names = sorted(f for f in os.listdir(sdir) if f.endswith(".txt") and not f.startswith("discover"))
    frames = 0
    for n in names:
        body = rd(os.path.join(sdir, n))
        scen += NL + "' ---- " + n + NL + body + NL
        for ln in body.split(NL):
            code = ln.split("'")[0].strip()
            if code.upper().startswith("RUN "):
                frames += len(code[4:].replace(" ", ""))
    wr(os.path.join(dst, "gbscen.txt"), scen)
    print("pop: gbpop.bas %d bytes (+p), gbscen.txt %d bytes, %d scenario files, at most %d frames"
          % (os.path.getsize(out), len(scen), len(names), frames))


def main():
    what = sys.argv[1:] or ["exile", "elite", "pop"]
    for w in what:
        {"exile": make_exile, "elite": make_elite, "pop": make_pop}[w]()


if __name__ == "__main__":
    main()
