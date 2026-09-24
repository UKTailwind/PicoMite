# bench_gfx.py - MicroPython twin of bench_gfx.bas (Phase 1 graphics pairs:
# H4, E8, E9), for Peter's PICO_COMPUTER_3 port (the PC2 on COM9).
# Each test times N passes of a loop holding one statement, subtracts the
# empty loop, and prints: BENCH label us_per_stmt cycles_per_stmt
# with the SAME label bench_gfx.bas prints for the same test, so the two pair
# by label. Module-level tests mimic BASIC (globals, LOAD_NAME); the local.*
# tests run the same statements on locals inside a def (LOAD_FAST), pairing
# with the .bas SUB-on-LOCALs tests.
#
# Drawing goes through a plain framebuf.FrameBuffer over hdmi.framebuffer(),
# so every drawing method is C, as every MMBasic drawing command is. (hdmi.fb()
# returns pcgfx.Display, whose line() is a Python wrapper; the py.display.*
# tests price that separately and have no MMBasic twin.)
# The screen is switched to RGB640_4 - 640x480, 4 bits a pixel, packed two to
# a byte in SRAM - the same geometry, depth and memory as MMBasic MODE 3 on
# the PC3, and the mode it was in is put back at the end. The on-screen
# console (and its 500 ms cursor-blink timer) is off while the tests run, so
# results go to the serial port only and nothing else draws on the screen.
import gc, machine, os, hdmi, framebuf
from time import ticks_us, ticks_diff

PSECT = 0          # 0 = all sections, 1 = H4 only, 2 = E8 only, 3 = E9 only
SWITCH_MODE = True # False: draw in whatever mode is running (not 4 bpp: unfair E9)
N = 20000
NE = 2000
PASSES = 5
MHZ = machine.freq() / 1000000
npix = PASSES * 4096

x = 10; y = 20; c = 1; u = 17; v = 27; s = 8; p = 50; q = 50; r = 4
a = 0; b = 7
wht = 15           # white in a 4 bpp palette mode
base = 0

# ---- display set-up: RGB640_4, console to serial only ---------------------
# (done inside the try below, so the finally undoes whatever part of it ran)
_MODES = {(640, 480, 8): hdmi.RGB640, (640, 480, 4): hdmi.RGB640_4,
          (320, 240, 16): hdmi.RGB320, (320, 240, 8): hdmi.RGB320_8,
          (1024, 600, 4): hdmi.RGB1024, (512, 300, 16): hdmi.RGB512}
orig_mode = _MODES.get((hdmi.width(), hdmi.height(), hdmi.bpp()))
clk = machine.freq() // 1000000
try:
    import pcconsole
except ImportError:
    pcconsole = None
con_on = False
switched = False


def emit(name, us):
    print("BENCH %s %.3f %d" % (name, us, round(us * MHZ)))


def rep(name, us_total):
    # N passes at module level, against the module-level empty loop
    emit(name, (us_total - base) / N)


def repn(name, us_total, n):
    # n passes at module level: the empty loop scaled from N to n passes
    emit(name, (us_total - base * n / N) / n)


def reps(name, us_total, lbase):
    # N passes inside a def, against that def's own empty loop
    emit(name, (us_total - lbase) / N)


def local_h4():
    d = fb
    lx = 10; ly = 20; lc = 1; lu = 17; lv = 27; ls = 8; lp = 50; lq = 50; lr = 4
    gc.collect(); lt = ticks_us()
    for li in range(N):
        pass
    lbase = ticks_diff(ticks_us(), lt)
    emit("local.ctl.loop.empty", lbase / N)

    gc.collect(); lt = ticks_us()
    for li in range(N):
        d.pixel(10, 20, 1)
    reps("local.H4.pixel.lit", ticks_diff(ticks_us(), lt), lbase)

    gc.collect(); lt = ticks_us()
    for li in range(N):
        d.pixel(lx, ly, lc)
    reps("local.H4.pixel.var", ticks_diff(ticks_us(), lt), lbase)

    gc.collect(); lt = ticks_us()
    for li in range(N):
        d.line(10, 20, 17, 27, 1)
    reps("local.H4.line.lit", ticks_diff(ticks_us(), lt), lbase)

    gc.collect(); lt = ticks_us()
    for li in range(N):
        d.line(lx, ly, lu, lv, lc)
    reps("local.H4.line.var", ticks_diff(ticks_us(), lt), lbase)

    gc.collect(); lt = ticks_us()
    for li in range(N):
        d.rect(10, 20, 8, 8, 1)
    reps("local.H4.box.lit", ticks_diff(ticks_us(), lt), lbase)

    gc.collect(); lt = ticks_us()
    for li in range(N):
        d.rect(lx, ly, ls, ls, lc)
    reps("local.H4.box.var", ticks_diff(ticks_us(), lt), lbase)

    gc.collect(); lt = ticks_us()
    for li in range(N):
        d.ellipse(50, 50, 4, 4, 1)
    reps("local.H4.circle.lit", ticks_diff(ticks_us(), lt), lbase)

    gc.collect(); lt = ticks_us()
    for li in range(N):
        d.ellipse(lp, lq, lr, lr, lc)
    reps("local.H4.circle.var", ticks_diff(ticks_us(), lt), lbase)


def local_e8():
    d = fb
    lcw = wht
    gc.collect(); lt = ticks_us()
    for li in range(PASSES):
        for lpy in range(64):
            for lpx in range(64):
                pass
    lb8 = ticks_diff(ticks_us(), lt)
    emit("local.E8.loop64.empty", lb8 / npix)

    gc.collect(); lt = ticks_us()
    for li in range(PASSES):
        for lpy in range(64):
            for lpx in range(64):
                d.pixel(lpx, lpy, lcw)
    lt8 = ticks_diff(ticks_us(), lt)
    emit("local.E8.pixel64.gross", lt8 / npix)
    emit("local.E8.pixel64.net", (lt8 - lb8) / npix)


try:
    if pcconsole is not None:
        con_on = pcconsole.size() is not None
        pcconsole.console("serial")
    if SWITCH_MODE and orig_mode is not None and orig_mode != hdmi.RGB640_4 and clk in (252, 315, 378):
        hdmi.deinit()
        switched = True    # set first: the finally re-inits orig_mode even if this init fails
        hdmi.init(hdmi.RGB640_4, clk)
    W = hdmi.width(); H = hdmi.height(); BPP = hdmi.bpp()
    FMT = framebuf.GS4_HMSB if BPP == 4 else (framebuf.GS8 if BPP == 8 else framebuf.RGB565)
    fb = framebuf.FrameBuffer(hdmi.framebuffer(), W, H, FMT)
    dfb = hdmi.fb()    # pcgfx.Display over the same buffer (py.display.* only)

    _u = os.uname()
    print("BENCHSTART %s %s %d MHz" % (_u.machine.replace(" ", "_"), _u.release, round(MHZ)))

    # ---- controls: the empty loop every module-level test subtracts, and a=b
    gc.collect(); t0 = ticks_us()
    for i in range(N):
        pass
    base = ticks_diff(ticks_us(), t0)
    emit("ctl.loop.empty", base / N)

    gc.collect(); t0 = ticks_us()
    for i in range(N):
        a = b
    rep("ctl.assign", ticks_diff(ticks_us(), t0))

    if PSECT in (0, 1):
        # ---- H4: the same call with literal and with variable arguments
        gc.collect(); t0 = ticks_us()
        for i in range(N):
            fb.pixel(10, 20, 1)
        rep("H4.pixel.lit", ticks_diff(ticks_us(), t0))

        gc.collect(); t0 = ticks_us()
        for i in range(N):
            fb.pixel(x, y, c)
        rep("H4.pixel.var", ticks_diff(ticks_us(), t0))

        gc.collect(); t0 = ticks_us()
        for i in range(N):
            fb.pixel(x, 20, 1)
        rep("H4.pixel.var_x", ticks_diff(ticks_us(), t0))

        gc.collect(); t0 = ticks_us()
        for i in range(N):
            fb.pixel(10, 20, c)
        rep("H4.pixel.var_c", ticks_diff(ticks_us(), t0))

        # same-length twins, as in the .bas: lit_3digit - lit_offscreen is the
        # C write (setpixel plus the colour argument, which framebuf converts
        # only for an on-screen pixel)
        gc.collect(); t0 = ticks_us()
        for i in range(N):
            fb.pixel(100, 200, 1)
        rep("H4.pixel.lit_3digit", ticks_diff(ticks_us(), t0))

        gc.collect(); t0 = ticks_us()
        for i in range(N):
            fb.pixel(999, 999, 1)
        rep("H4.pixel.lit_offscreen", ticks_diff(ticks_us(), t0))

        gc.collect(); t0 = ticks_us()
        for i in range(N):
            fb.line(10, 20, 17, 27, 1)
        rep("H4.line.lit", ticks_diff(ticks_us(), t0))

        gc.collect(); t0 = ticks_us()
        for i in range(N):
            fb.line(x, y, u, v, c)
        rep("H4.line.var", ticks_diff(ticks_us(), t0))

        gc.collect(); t0 = ticks_us()
        for i in range(N):
            fb.rect(10, 20, 8, 8, 1)
        rep("H4.box.lit", ticks_diff(ticks_us(), t0))

        gc.collect(); t0 = ticks_us()
        for i in range(N):
            fb.rect(x, y, s, s, c)
        rep("H4.box.var", ticks_diff(ticks_us(), t0))

        gc.collect(); t0 = ticks_us()
        for i in range(N):
            fb.ellipse(50, 50, 4, 4, 1)
        rep("H4.circle.lit", ticks_diff(ticks_us(), t0))

        gc.collect(); t0 = ticks_us()
        for i in range(N):
            fb.ellipse(p, q, r, r, c)
        rep("H4.circle.var", ticks_diff(ticks_us(), t0))

        # Python only: the same calls through hdmi.fb()'s pcgfx.Display
        gc.collect(); t0 = ticks_us()
        for i in range(N):
            dfb.pixel(x, y, c)
        rep("py.display.pixel.var", ticks_diff(ticks_us(), t0))

        gc.collect(); t0 = ticks_us()
        for i in range(N):
            dfb.line(x, y, u, v, c)
        rep("py.display.line.var", ticks_diff(ticks_us(), t0))

    if PSECT in (0, 2):
        # ---- E8: pixel() over a 64x64 region, per pixel
        gc.collect(); t0 = ticks_us()
        for i in range(PASSES):
            for py in range(64):
                for px in range(64):
                    pass
        e8b = ticks_diff(ticks_us(), t0)
        emit("E8.loop64.empty", e8b / npix)

        gc.collect(); t0 = ticks_us()
        for i in range(PASSES):
            for py in range(64):
                for px in range(64):
                    fb.pixel(px, py, wht)
        e8t = ticks_diff(ticks_us(), t0)
        emit("E8.pixel64.gross", e8t / npix)
        emit("E8.pixel64.net", (e8t - e8b) / npix)

    if PSECT in (0, 3):
        # ---- E9: large shapes, where the time should be in C; each has a
        # small twin, so large minus small is the drawing alone
        gc.collect(); t0 = ticks_us()
        for i in range(NE):
            fb.fill(0)
        repn("E9.cls.%dx%d" % (W, H), ticks_diff(ticks_us(), t0), NE)

        gc.collect(); t0 = ticks_us()
        for i in range(NE):
            fb.fill_rect(0, 0, 8, 8, wht)
        repn("E9.boxfill.8x8", ticks_diff(ticks_us(), t0), NE)

        gc.collect(); t0 = ticks_us()
        for i in range(NE):
            fb.fill_rect(0, 0, 320, 240, wht)
        repn("E9.boxfill.320x240", ticks_diff(ticks_us(), t0), NE)

        gc.collect(); t0 = ticks_us()
        for i in range(NE):
            fb.line(0, 0, 7, 7, wht)
        repn("E9.line.8x8", ticks_diff(ticks_us(), t0), NE)

        gc.collect(); t0 = ticks_us()
        for i in range(NE):
            fb.line(0, 0, 319, 239, wht)
        repn("E9.line.320x240", ticks_diff(ticks_us(), t0), NE)

        gc.collect(); t0 = ticks_us()
        for i in range(NE):
            fb.ellipse(160, 120, 4, 4, wht, True)
        repn("E9.circlefill.r4", ticks_diff(ticks_us(), t0), NE)

        gc.collect(); t0 = ticks_us()
        for i in range(NE):
            fb.ellipse(160, 120, 100, 100, wht, True)
        repn("E9.circlefill.r100", ticks_diff(ticks_us(), t0), NE)

        if W >= 640 and H >= 480:
            gc.collect(); t0 = ticks_us()
            for i in range(NE):
                fb.fill_rect(0, 0, 640, 480, wht)
            repn("E9.boxfill.640x480", ticks_diff(ticks_us(), t0), NE)

            gc.collect(); t0 = ticks_us()
            for i in range(NE):
                fb.line(0, 0, 639, 479, wht)
            repn("E9.line.640x480", ticks_diff(ticks_us(), t0), NE)

    # ---- the same calls inside a def, on locals (bench_gfx.bas: in a SUB)
    if PSECT in (0, 1):
        local_h4()
    if PSECT in (0, 2):
        local_e8()
    print("BENCHEND")
finally:
    if switched:
        hdmi.deinit()
        hdmi.init(orig_mode, clk)
    if pcconsole is not None and con_on:
        pcconsole.console()
