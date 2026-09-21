"""rooms.py - draw every room of every level to a picture file.

    python rooms.py --data <converted data directory> --out <picture directory>

It reads the same files the engine does and composes each screen the same way
`DrawScreen` and `DrawFront` do in player.bas, so what comes out is the room as
the game draws it: the scenery only, with no characters and none of the moving
parts that the engine redraws every frame.

    --level N     just that level, otherwise all fourteen
    --scale N     N times bigger, nearest neighbour (default 2)
    --sheet       also write one contact sheet per level

Names are `level01_screen01.png` and so on, and a screen the level never uses -
the blueprint carries twenty-four whatever the level needs - is still drawn,
because "screen 9 is empty" is itself worth being able to see.

**The pictures are the game's own artwork** and are no more yours to pass on
than the converted data they come from; see "Keep what comes out to yourself"
in README.md.
"""
import argparse
import os
import sys

from PIL import Image

# The screen, from player.bas.
ROWS, COLS, TPW = 3, 10, 12
BLOCKW, ORIGINX, ORIGINY = 28, 20, 24
WIDTH, HEIGHT = 320, 240
TRANSP = 11
BSTATES = 8
T_FLASK, T_SLICER = 10, 18
LEVELBYTES = 2304
LEVELS = 14        # blocks 1..14; block 0 is not one of the game's


def sgn8(b):
    return b - 256 if b > 127 else b


def read_tables(path):
    """tables.idx is `name value` a line, and is where every offset comes from."""
    t = {}
    with open(path, encoding="latin-1") as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            k, _, v = line.partition(" ")
            try:
                t[k] = int(v)
            except ValueError:
                pass
    return t


class Art:
    """art.bin, as LoadArt reads it: a table count, then a record per image."""

    def __init__(self, path):
        d = open(path, "rb").read()
        self.d = d
        ntab = d[0] | (d[1] << 8)
        self.base = 2 + ntab * 7
        self.count, self.facings, self.first = {}, {}, {}
        for i in range(ntab):
            o = 2 + i * 7
            self.count[i + 1] = d[o] | (d[o + 1] << 8)
            self.facings[i + 1] = d[o + 2]
            self.first[i + 1] = d[o + 3] | (d[o + 4] << 8) | (d[o + 5] << 16)

    def rec(self, tbl, img):
        """sheet, x, y, w, h for image `img` of table `tbl`, or None."""
        if tbl not in self.count or img < 1 or img > self.count[tbl]:
            return None
        r = (self.first[tbl] + (img - 1) * self.facings[tbl]) * 9 + self.base
        d = self.d
        if r + 9 > len(d):
            return None
        sheet = d[r]
        sx = d[r + 1] | (d[r + 2] << 8)
        sy = d[r + 3] | (d[r + 4] << 8)
        w = d[r + 5] | (d[r + 6] << 8)
        h = d[r + 7] | (d[r + 8] << 8)
        if w < 1 or h < 1:
            return None
        return sheet, sx, sy, w, h


class Sheets:
    """The artwork.  A pixel's VALUE is its colour index, which is what the
    board blits; the BMP's own palette is not used."""

    def __init__(self, data):
        self.px, self.size = {}, {}
        for n in range(1, 5):
            p = os.path.join(data, "sheet%d.bmp" % n)
            if not os.path.isfile(p):
                continue
            im = Image.open(p)
            if im.mode != "P":
                im = im.convert("P")
            self.px[n] = im.load()
            self.size[n] = im.size

    def blit(self, dst, sheet, sx, sy, dx, dy, w, h, transparent=True):
        if sheet not in self.px:
            return
        src = self.px[sheet]
        sw, sh = self.size[sheet]
        for yy in range(h):
            ty = dy + yy
            if ty < 0 or ty >= HEIGHT or sy + yy < 0 or sy + yy >= sh:
                continue
            row = dst[ty]
            for xx in range(w):
                tx = dx + xx
                if tx < 0 or tx >= WIDTH or sx + xx < 0 or sx + xx >= sw:
                    continue
                v = src[sx + xx, sy + yy]
                if transparent and v == TRANSP:
                    continue
                row[tx] = v


class Renderer:
    def __init__(self, data):
        self.t = read_tables(os.path.join(data, "tables.idx"))
        self.blocks = open(os.path.join(data, "blocks.dat"), "rb").read()
        self.levels = open(os.path.join(data, "levels.dat"), "rb").read()
        self.art = Art(os.path.join(data, "art.bin"))
        self.sheets = Sheets(data)
        self.pal = [self.t.get("pal_%d" % i, 0) for i in range(16)]
        self.ntypes = self.t["block_types"]
        self.bg_of_level = [self.blocks[self.t["anim_bgset1"] + i] for i in range(15)]
        self.sec = {}
        self.bsec = {}

    # ---- the section tables, as BuildSections and BuildBSections build them --
    def add_section(self, kind, t, num, xoff, yoff):
        k = kind * 32 + t
        self.sec.pop(k, None)
        if num == 0:
            return
        tbl, img = (self.bg1, num) if num < 0x80 else (self.bg2, num - 0x80)
        r = self.art.rec(tbl, img)
        if r is None:
            return
        sheet, sx, sy, w, h = r
        self.sec[k] = (sheet, sx, sy, w, h, xoff, yoff - h + 1)

    def add_b(self, k, num, yoff):
        tbl, img = (self.bg1, num) if num < 0x80 else (self.bg2, num - 0x80)
        r = self.art.rec(tbl, img)
        if r is None:
            return
        sheet, sx, sy, w, h = r
        self.bsec[k] = (sheet, sx, sy, w, h, 0, yoff - h + 1)

    def build_sections(self, level):
        """SelectBackground then BuildSections: which artwork tables this level
        uses, and whether it is the striped palace wall."""
        b = self.blocks
        t = self.t
        want = self.bg_of_level[level - 1] if 1 <= level <= 15 else 0
        if want == 0:
            self.bg1, self.bg2, self.palace = t["tab_BGTAB1DUN"], t["tab_BGTAB2DUN"], 0
        else:
            self.bg1, self.bg2 = t["tab_BGTAB1PAL"], t["tab_BGTAB2PAL"]
            self.palace = 1 if want == 1 else 0

        self.sec, self.bsec = {}, {}
        # BuildBSections
        n_bpans, n_blox, n_pans = t["const_numbpans"], t["const_numblox"], t["const_numpans"]
        panel_b0 = t["const_panelb0"]
        for ty in range(self.ntypes):
            for st in range(BSTATES):
                num, yoff = 0, 0
                if ty == 0:
                    if st <= n_bpans:
                        num = b[t["var_spaceb"] + st]
                        yoff = sgn8(b[t["var_spaceby"] + st])
                elif ty == 1:
                    if st <= n_bpans:
                        num = b[t["var_floorb"] + st]
                        yoff = sgn8(b[t["var_floorby"] + st])
                elif ty == 20:
                    num = b[t["var_blockb"] + (st if st < n_blox else 0)]
                else:
                    p = b[t["block_pieceb"] + ty]
                    if p == 0:
                        if self.palace:
                            num, yoff = b[t["block_bstripe"] + ty], -32
                    elif p == panel_b0:
                        if st < n_pans:
                            num = b[t["var_panelb"] + st]
                    else:
                        num, yoff = p, sgn8(b[t["block_pieceby"] + ty])
                if num:
                    self.add_b(ty * BSTATES + st, num, -3 + yoff)
        # BuildSections
        for ty in range(self.ntypes):
            self.add_section(0, ty, b[t["block_piecec"] + ty], 0, 0)
            self.add_section(2, ty, b[t["block_pieced"] + ty], 0, 0)
            self.add_section(3, ty, b[t["block_piecea"] + ty], 0,
                             -3 + sgn8(b[t["block_pieceay"] + ty]))
            self.add_section(4, ty, b[t["block_fronti"] + ty],
                             sgn8(b[t["block_frontx"] + ty]),
                             -3 + sgn8(b[t["block_fronty"] + ty]))

    # ---- one screen ---------------------------------------------------------
    def type_grid(self, level, scrn):
        """BuildTypeGrid: the types and states of one screen, with the one-cell
        left margin and the spare row below that DrawScreen indexes into."""
        # LoadLevel seeks to n * LEVELBYTES, so level 1 is the SECOND block of
        # levels.dat.  Block 0 is there too and is not one of the game's levels.
        base = level * LEVELBYTES
        tp = [0] * (4 * TPW)
        ts = [0] * (4 * TPW)
        for r in range(ROWS):
            o = base + (scrn - 1) * 30 + r * COLS
            for c in range(COLS):
                v = self.levels[o + c] & 0x1F
                tp[r * TPW + c + 1] = v if v < self.ntypes else 0
                ts[r * TPW + c + 1] = self.levels[base + 720 + (scrn - 1) * 30 + r * COLS + c]
        return tp, ts

    def draw(self, level, scrn):
        b, t = self.blocks, self.t
        tp, ts = self.type_grid(level, scrn)
        dst = [[0] * WIDTH for _ in range(HEIGHT)]
        put = self.sheets.blit

        def sec(k, x, yb, opaque=False):
            s = self.sec.get(k)
            if s:
                sh, sx, sy, w, h, dx, dy = s
                put(dst, sh, sx, sy, x + dx, yb + dy, w, h, not opaque)

        # DrawScreen
        for r in range(ROWS):
            yb = ORIGINY + b[t["geom_BlockBot"] + r + 1]
            row = r * TPW
            for c in range(COLS):
                x = ORIGINX + c * BLOCKW
                i = row + c
                if tp[i + TPW] != 4:                      # a gate's own picture is animated
                    sec(tp[i + TPW], x, yb)
                if tp[i + 1] != 20 and tp[i] not in (4, 11, 16):
                    k = tp[i] * BSTATES + (ts[i] & 7)
                    s = self.bsec.get(k)
                    if s:
                        sh, sx, sy, w, h, dx, dy = s
                        put(dst, sh, sx, sy, x + dx, yb + dy, w, h, True)
                if tp[i + 1] not in (5, 6, 11, 15):  # Movable(): loose floor, the three plates
                    sec(64 + tp[i + 1], x, yb)
                    sec(96 + tp[i + 1], x, yb)

        # DrawFront
        for r in range(ROWS):
            yb = ORIGINY + b[t["geom_BlockBot"] + r + 1]
            row = r * TPW
            for c in range(COLS):
                x = ORIGINX + c * BLOCKW
                ty = tp[row + c + 1]
                k = 128 + ty
                s = self.sec.get(k)
                tall = False
                if s and ty == T_FLASK:
                    n = ts[row + c + 1] >> 5
                    if 2 <= n <= 4:
                        self.put_bg(dst, t["const_specialflask"],
                                    x + s[5], yb + s[6] + s[4] - 1)
                        tall = True
                if s and not tall:
                    sec(k, x, yb, opaque=(ty == 3 or ty >= 27))
        return dst

    def put_bg(self, dst, num, x, ybot):
        """PutBg: a background image placed by its bottom edge."""
        if not num:
            return
        tbl, img = (self.bg1, num) if num < 0x80 else (self.bg2, num - 0x80)
        r = self.art.rec(tbl, img)
        if r is None:
            return
        sheet, sx, sy, w, h = r
        self.sheets.blit(dst, sheet, sx, sy, x, ybot - h + 1, w, h, True)

    def image(self, level, scrn, scale=2):
        dst = self.draw(level, scrn)
        im = Image.new("RGB", (WIDTH, HEIGHT))
        px = im.load()
        pal = [((v >> 16) & 255, (v >> 8) & 255, v & 255) for v in self.pal]
        for y in range(HEIGHT):
            row = dst[y]
            for x in range(WIDTH):
                px[x, y] = pal[row[x] & 15]
        if scale > 1:
            im = im.resize((WIDTH * scale, HEIGHT * scale), Image.NEAREST)
        return im


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data", required=True, help="the converted data directory")
    ap.add_argument("--out", required=True, help="where to write the pictures")
    ap.add_argument("--level", type=int, default=0, help="just this level")
    ap.add_argument("--scale", type=int, default=2)
    ap.add_argument("--sheet", action="store_true", help="a contact sheet per level too")
    a = ap.parse_args(argv)

    for f in ("tables.idx", "blocks.dat", "levels.dat", "art.bin", "sheet1.bmp"):
        if not os.path.isfile(os.path.join(a.data, f)):
            raise SystemExit("%s is not in %s - point --data at the converter's "
                             "output" % (f, a.data))
    os.makedirs(a.out, exist_ok=True)
    r = Renderer(a.data)
    levels = [a.level] if a.level else range(1, LEVELS + 1)
    n = 0
    for lv in levels:
        r.build_sections(lv)
        shots = []
        for sc in range(1, 25):
            im = r.image(lv, sc, a.scale)
            im.save(os.path.join(a.out, "level%02d_screen%02d.png" % (lv, sc)))
            shots.append(im)
            n += 1
        print("level %2d: 24 screens" % lv, flush=True)
        if a.sheet:
            w, h = shots[0].size
            sheet = Image.new("RGB", (w * 4, h * 6))
            for i, im in enumerate(shots):
                sheet.paste(im, ((i % 4) * w, (i // 4) * h))
            sheet.save(os.path.join(a.out, "level%02d.png" % lv))
    print("%d pictures in %s" % (n, a.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
