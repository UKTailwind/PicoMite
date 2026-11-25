"""affine_span_model.py - a pixel-by-pixel model of FRAMEBUFFER AFFINE and SPAN, exactly as
docs/FRAMEBUFFER_AFFINE_SPAN_Design.md states their rules, and pictures of the design's examples made with it.

  python affine_span_model.py OUTDIR      writes model_*.png into OUTDIR (it needs Pillow)

  affine(src, dst, u, v, du, dv, step, first, left, width, edge, transparent)
  span(src, dst, left, right, step, first, fill, transparent, outside)

Images are lists of rows of pixel values. WRAP = None for the edge argument (the source wraps round)."""
import math, os
from PIL import Image, ImageDraw

import sys

here = sys.argv[1] if len(sys.argv) > 1 else "."
W, H = 320, 240
FP = 65536
WRAP, LEAVE = None, -1


def affine(src, dst, u, v, du, dv, step=1, first=0, left=0, width=None, edge=WRAP, transparent=None):
    sw, sh = len(src[0]), len(src)
    if width is None:
        width = W - left
    n = len(u)
    assert len(v) == n and len(du) == n and len(dv) == n
    for b in range(n):
        for j in range(step):
            y = first + b * step + j
            if y < 0 or y >= H:
                continue
            for i in range(width):
                x = left + i
                if x < 0 or x >= W:
                    continue
                U = u[b] - j * dv[b] + i * du[b]
                V = v[b] + j * du[b] + i * dv[b]
                sx, sy = U // FP, V // FP          # floor
                if edge is WRAP:
                    sx %= sw
                    sy %= sh
                elif not (0 <= sx < sw and 0 <= sy < sh):
                    if edge != LEAVE:
                        dst[y][x] = edge
                    continue
                c = src[sy][sx]
                if c != transparent:
                    dst[y][x] = c


def span(src, dst, left, right, step=1, first=0, fill=None, transparent=None, outside=0):
    n = len(left)
    assert len(right) == n
    for b in range(n):
        lo, hi = max(left[b], 0), min(right[b], W - 1)
        for j in range(step):
            y = first + b * step + j
            if y < 0 or y >= H:
                continue
            for x in range(W):
                inside = lo <= x <= hi
                if inside != bool(outside):
                    if src[y][x] != transparent:
                        dst[y][x] = src[y][x]
                elif fill is not None:
                    dst[y][x] = fill


def rnd(x):  # MATH's float to integer: to the nearest, halves away from zero
    return int(x + 0.5) if x >= 0 else -int(-x + 0.5)


def load(name):
    im = Image.open(os.path.join(here, name)).convert("RGB")
    p = im.load()
    return [[p[x, y] for x in range(im.width)] for y in range(im.height)]


def save(img, name):
    im = Image.new("RGB", (len(img[0]), len(img)))
    im.putdata([c for row in img for c in row])
    im.save(os.path.join(here, name))


def blank(c):
    return [[c] * W for _ in range(H)]


def testpic():
    im = Image.new("RGB", (W, H), (0, 0, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([85, 45, 234, 194], fill=(255, 255, 255))
    d.rectangle([88, 48, 231, 191], fill=(255, 0, 0))
    d.rectangle([88, 48, 159, 119], fill=(0, 170, 0))
    d.rectangle([160, 120, 231, 191], fill=(255, 0, 255))
    for k in range(9):
        d.line([88 + k * 18, 48, 88 + k * 18, 191], fill=(255, 255, 0))
        d.line([88, 48 + k * 18, 231, 48 + k * 18], fill=(255, 255, 0))
    d.ellipse([120, 80, 200, 160], fill=(0, 80, 255), outline=(255, 255, 255), width=2)
    d.polygon([(160, 52), (150, 70), (170, 70)], fill=(255, 255, 255))
    d.text((138, 100), "PicoMite", fill=(255, 255, 255))
    d.text((92, 178), "bottom left", fill=(255, 255, 255))
    p = im.load()
    return [[p[x, y] for x in range(W)] for y in range(H)]


def trackmap():
    im = Image.new("RGB", (W, H), (30, 140, 50))
    d = ImageDraw.Draw(im)
    for y in range(0, H, 20):                       # two greens, so the ground's movement shows
        for x in range(0, W, 20):
            if (x // 20 + y // 20) & 1:
                d.rectangle([x, y, x + 19, y + 19], fill=(40, 160, 60))
    d.ellipse([30, 30, 289, 209], outline=(230, 230, 230), width=44)   # kerb
    d.ellipse([34, 34, 285, 205], outline=(110, 110, 110), width=36)   # road
    for k in range(0, 36, 6):                       # the start line
        d.rectangle([156, 34 + k, 163, 34 + k + 2], fill=(255, 255, 255))
    d.rectangle([140, 100, 180, 140], fill=(40, 80, 220))               # a pond
    d.text((120, 112), "", fill=(255, 255, 255))
    p = im.load()
    return [[p[x, y] for x in range(W)] for y in range(H)]


if __name__ == "__main__":
    pic = testpic()
    # 1. a turn and zoom of the whole picture: one element, step = the screen's height
    a, z, cx, cy, sx, sy = math.radians(30), 1.5, 160, 120, 160, 120
    du = [rnd(FP * math.cos(a) / z)]
    dv = [rnd(-FP * math.sin(a) / z)]
    u = [FP * sx - cx * du[0] + cy * dv[0]]
    v = [FP * sy - cx * dv[0] - cy * du[0]]
    out = blank((0, 0, 0))
    affine(pic, out, u, v, du, dv, step=H)
    save(out, "model_turnzoom_wrap.png")
    out = blank((0, 0, 0))
    affine(pic, out, u, v, du, dv, step=H, edge=(0, 0, 64))
    save(out, "model_turnzoom_edge.png")
    # 2. a floor: the map seen from a camera above it
    world = trackmap()
    save(world, "model_map.png")
    for tag, (camx, camy, deg) in {"a": (160, 215, -90), "b": (262, 120, -60), "c": (60, 60, 20)}.items():
        hz, hgt, D = 90, 24.0, 160.0
        n = H - 1 - hz
        zt = [hgt / (k + 1) for k in range(n)]
        ca, sa = math.cos(math.radians(deg)), math.sin(math.radians(deg))
        du = [rnd(t * -FP * sa) for t in zt]
        dv = [rnd(t * FP * ca) for t in zt]
        u = [rnd(t * FP * (D * ca + 160 * sa)) + rnd(FP * camx) for t in zt]
        v = [rnd(t * FP * (D * sa - 160 * ca)) + rnd(FP * camy) for t in zt]
        out = [[(90, 150, 240)] * W for _ in range(H)]
        affine(world, out, u, v, du, dv, step=1, first=hz + 1)
        save(out, "model_floor_%s.png" % tag)
    # 3. a turned sprite: a 48x48 cell alone on its buffer, drawn through a 69x69 window with the rest left alone
    sheet = blank((0, 0, 0))
    for yy in range(48):
        for xx in range(48):
            sheet[100 + yy][100 + xx] = (240, 200, 40) if (xx // 8 + yy // 8) & 1 else (200, 40, 40)
    for yy in range(8, 16):
        for xx in range(20, 28):
            sheet[100 + yy][100 + xx] = (255, 255, 255)   # a mark at its top
    out = load("model_floor_a.png")
    B = 69
    for k, (px, py, deg) in enumerate([(60, 60, 0), (160, 60, 25), (260, 60, 45), (60, 170, 90), (160, 170, 200), (300, 220, 300)]):
        a = math.radians(deg)
        du = [rnd(FP * math.cos(a))]
        dv = [rnd(-FP * math.sin(a))]
        c = B // 2
        u = [FP * 124 - c * du[0] + c * dv[0]]
        v = [FP * 124 - c * dv[0] - c * du[0]]
        affine(sheet, out, u, v, du, dv, step=B, first=py - c, left=px - c, width=B, edge=LEAVE, transparent=(0, 0, 0))
    save(out, "model_sprites.png")
    # 4. the per-row window: an iris, a spotlight and a slanted wipe
    cxs, cys, rad = 200, 110, 80
    hw = [math.sqrt(max(rad * rad - (y - cys) ** 2, 0)) for y in range(H)]
    left = [rnd(-h + cxs) for h in hw]
    right = [rnd(h + cxs - 1) for h in hw]
    out = load("model_floor_b.png")
    scene = load("model_floor_b.png")
    span(scene, out, left, right, fill=(0, 0, 0))
    save(out, "model_iris.png")
    dark = [[(r // 4, g // 4, b // 4) for (r, g, b) in row] for row in scene]
    out = [row[:] for row in dark]
    span(scene, out, left, right)
    save(out, "model_spot.png")
    out = [row[:] for row in scene]
    left = [0] * H
    right = [rnd(y * 0.5 + 100) for y in range(H)]
    span(pic, out, left, right)
    save(out, "model_wipe.png")
    out = [row[:] for row in scene]
    span(pic, out, [rnd(-h + cxs) for h in hw], [rnd(h + cxs - 1) for h in hw], outside=1)
    save(out, "model_outside.png")
    print("done")
