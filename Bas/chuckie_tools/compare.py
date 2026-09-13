"""Draw the extracted maps the way the game draws them and stack each one
under the screenshot it came from, so the trace can be judged by eye."""
import glob
import os
import sys

from PIL import Image

from gen_levels import build_all, LEVEL_OF, COLS, ROWS

BLACK = (0, 0, 0)
GREEN = (0, 255, 0)
MAGENTA = (255, 0, 255)
YELLOW = (255, 255, 0)
CYAN = (0, 255, 255)
WHITE = (255, 255, 255)


def render(g, top=16):
    im = Image.new('RGB', (320, 256), BLACK)
    px = im.load()

    def box(x, y, w, h, col):
        for yy in range(max(0, y), min(256, y + h)):
            for xx in range(max(0, x), min(320, x + w)):
                px[xx, yy] = col

    for r in range(ROWS):
        y = top + r * 8
        for c in range(COLS):
            x = c * 16
            ch = g[r][c]
            if ch == '#':          # a '+' crossing draws as ladder only
                for k in (0, 2, 4):
                    box(x, y + k, 16, 1, GREEN)
                box(x + 10, y, 2, 1, BLACK)
                box(x + 2, y + 2, 2, 1, BLACK)
                box(x + 6, y + 4, 2, 1, BLACK)
    for r in range(ROWS):
        y = top + r * 8
        for c in range(COLS):
            x = c * 16
            if g[r][c] in 'H+':
                box(x + 2, y, 2, 8, MAGENTA)
                box(x + 12, y, 2, 8, MAGENTA)
                box(x + 2, y + 3, 12, 1, MAGENTA)
    for r in range(ROWS):
        y = top + r * 8
        for c in range(COLS):
            x = c * 16
            ch = g[r][c]
            if ch == 'E':
                box(x + 2, y + 2, 12, 6, YELLOW)
            elif ch == 'S':
                box(x + 1, y + 4, 14, 4, MAGENTA)
            elif ch in '12345':
                box(x + 1, y - 12, 14, 20, CYAN)
            elif ch == 'P':
                box(x, y - 10, 16, 18, WHITE)
            elif ch in 'LK':
                box(x + 7, y, 2, 8, (0, 85, 170))
    return im


if __name__ == '__main__':
    levels = build_all()
    shots = {}
    for f in sorted(glob.glob('lvl_*.png')):
        k = os.path.basename(f)[4:12]
        if LEVEL_OF.get(k, 99) <= 8:
            shots[LEVEL_OF[k]] = f
    want = [int(a) for a in sys.argv[1:] if a.isdigit()] or sorted(levels)
    out = Image.new('RGB', (320 * 2 * len(want), 256 * 2), BLACK)
    for i, lvl in enumerate(want):
        src = Image.open(shots[lvl]).convert('RGB')
        out.paste(src.resize((640, 512), Image.NEAREST), (i * 640, 0))
        # blank the shot's status panel so the two line up visually
        mine = render(levels[lvl][0])
        out.paste(mine.resize((640, 512), Image.NEAREST), (i * 640, 0))
        out.paste(src.resize((640, 512), Image.NEAREST), (i * 640, 0))
    # side by side instead: original left, trace right
    out = Image.new('RGB', (640 * len(want), 512), BLACK)
    for i, lvl in enumerate(want):
        src = Image.open(shots[lvl]).convert('RGB').resize((320, 256), Image.NEAREST)
        mine = render(levels[lvl][0])
        pair = Image.new('RGB', (640, 256), BLACK)
        pair.paste(src, (0, 0))
        pair.paste(mine, (320, 0))
        out.paste(pair.resize((640, 512), Image.NEAREST), (i * 640, 0))
    out = out.resize((640 * len(want), 512))
    out.save('compare.png')
    print('compare.png', out.size, 'levels', want)
