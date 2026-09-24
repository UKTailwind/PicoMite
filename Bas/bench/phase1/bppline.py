# bppline.py - time a 640x480 diagonal LINE in MicroPython's framebuf at 4 and
# at 8 bits a pixel, printing the depth each run actually used.
import hdmi, framebuf, machine
from time import ticks_us, ticks_diff
clk = machine.freq() // 1000000
orig = (hdmi.width(), hdmi.height(), hdmi.bpp())
print("BENCHSTART orig mode", orig, clk, "MHz")
NE = 2000
for mode, name in ((hdmi.RGB640_4, "RGB640_4"), (hdmi.RGB640, "RGB640")):
    hdmi.deinit()
    hdmi.init(mode, clk)
    W, H, BPP = hdmi.width(), hdmi.height(), hdmi.bpp()
    FMT = framebuf.GS4_HMSB if BPP == 4 else (framebuf.GS8 if BPP == 8 else framebuf.RGB565)
    fb = framebuf.FrameBuffer(hdmi.framebuffer(), W, H, FMT)
    c = 15 if BPP == 4 else 255
    t0 = ticks_us()
    for i in range(NE):
        fb.line(0, 0, 639, 479, c)
    us = ticks_diff(ticks_us(), t0) / NE
    t0 = ticks_us()
    for i in range(NE):
        fb.line(0, 0, 7, 7, c)
    us8 = ticks_diff(ticks_us(), t0) / NE
    t0 = ticks_us()
    for i in range(NE // 10):
        fb.fill_rect(0, 0, 640, 480, c)
    usf = ticks_diff(ticks_us(), t0) / (NE // 10)
    print("BENCH %s bpp=%d fmt=%s line640x480 %.1f us %d cyc | line8x8 %.1f us | fillrect640x480 %.1f us %d cyc"
          % (name, BPP, "GS4_HMSB" if FMT == framebuf.GS4_HMSB else "GS8", us, us * clk, us8, usf, usf * clk))
hdmi.deinit()
hdmi.init(hdmi.RGB640 if orig[2] == 8 else hdmi.RGB640_4, clk)
print("BENCHEND")
