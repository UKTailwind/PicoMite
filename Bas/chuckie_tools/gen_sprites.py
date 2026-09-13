"""Extract Chuckie Egg sprites from the BBC-Micro title screen PNG and emit
MMBasic DATA lines: width, height, colour-index, hex-bitmap string.

Bitmap string = `height` rows concatenated; each row is ceil(width/4) hex
digits, most-significant bit = leftmost pixel.
"""
from PIL import Image

SRC = r'D:/Dropbox/PicoMite/PicoMite/Bas/84383.png'
BG = (0, 96, 96)

im = Image.open(SRC).convert('RGB')
px = im.load()


def grab(x0, y0, w, h):
    return [[px[x0 + x, y0 + y] != BG for x in range(w)] for y in range(h)]


def trim_rows(rows, top, bottom):
    return rows[top:len(rows) - bottom]


def mirror(rows):
    return [list(reversed(r)) for r in rows]


def to_hex(rows, w):
    nyb = (w + 3) // 4
    out = []
    for r in rows:
        bits = 0
        for i in range(nyb * 4):
            bits <<= 1
            if i < w and r[i]:
                bits |= 1
        out.append(('%0*X' % (nyb, bits)))
    return ''.join(out)


def art(rows):
    return '\n'.join(''.join('#' if c else '.' for c in r) for r in rows)


# ---------------------------------------------------------------- Harry 16x18
HARRY = {}
for name, x in (('CLIMB1', 98), ('CLIMB2', 116),
                ('RUNL', 134), ('STAND', 152), ('RUNR', 170)):
    HARRY[name] = grab(x, 257, 16, 18)

# ------------------------------------------------------- Hen 14 wide, 20 tall
# rows 0..1 of the 22-row band are blank -> trim to 20
HEN_WALK1 = trim_rows(grab(87, 276, 14, 22), 2, 0)
HEN_WALK2 = trim_rows(grab(103, 276, 14, 22), 2, 0)
# Pecking hen: the sheet's head-down poses stretch 26-28px wide because the
# neck is extended, too wide for a game sprite, so the walking hen's body is
# redrawn here with the head lowered to the ground.
HEN_PECK_ART = [
    "..............",
    "..............",
    "..............",
    "..............",
    "..............",
    "..............",
    "..............",
    "..............",
    "..####........",
    "########......",
    "##############",
    "##############",
    "##############",
    "..##########..",
    "....####..##..",
    "....##....##..",
    "....##..######",
    "....##..######",
    "....##........",
    "....####......",
]

# --------------------------------------------------------------- Duck 31 x 22
DUCK1 = grab(112, 299, 31, 22)
DUCK2 = grab(145, 299, 31, 22)

# ------------------------------------------------------------ hand-drawn bits
# Egg and grain traced from the BBC screenshot (Bas/ChuckieEgg-AnF.png).
# The original screen is 160 pixels across where ours is 320, so each
# horizontal pixel is doubled; vertically the two are the same.
EGG_NAT = [
    '.###..',
    '##.##.',
    '#.####',
    '######',
    '#####.',
    '.###..',
]
SEED_NAT = [
    '...#...',
    '..#.#..',
    '.#.#.#.',
    '#.#.#.#',
]
# The lift is a 20 x 4 bar, the size the BBC draws it.
LIFT = [
    '####################',
    '####################',
    '.##################.',
    '.##################.',
]


def from_art(lines):
    return [[c == '#' for c in ln] for ln in lines]


def widen(lines):
    """Double each pixel horizontally: BBC 160-wide art onto our 320."""
    return [[c == '#' for c in ln for _ in (0, 1)] for ln in lines]


PAL = {'BLACK': 0, 'BLUE': 1, 'MYRTLE': 2, 'COBALT': 3, 'MIDGREEN': 4,
       'CERULEAN': 5, 'GREEN': 6, 'CYAN': 7, 'RED': 8, 'MAGENTA': 9,
       'RUST': 10, 'FUCHSIA': 11, 'BROWN': 12, 'LILAC': 13, 'YELLOW': 14,
       'WHITE': 15}

OUT = [
    # (basic-name, sprite-number, rows, colour)
    ('SP_STAND',  1, HARRY['STAND'],  'CYAN'),
    ('SP_RUN1',   2, HARRY['RUNR'],   'CYAN'),
    ('SP_RUN2',   3, HARRY['RUNL'],   'CYAN'),
    ('SP_CLIMB1', 4, HARRY['CLIMB1'], 'CYAN'),
    ('SP_CLIMB2', 5, HARRY['CLIMB2'], 'CYAN'),
    ('SP_DUCK1',  6, DUCK1,           'YELLOW'),
    ('SP_DUCK2',  7, DUCK2,           'YELLOW'),
    ('SP_HEN1',   8, HEN_WALK1,       'CYAN'),
    ('SP_HEN2',   9, HEN_WALK2,       'CYAN'),
    ('SP_HEN3',  10, from_art(HEN_PECK_ART), 'CYAN'),
    ('SP_LIFT',  11, from_art(LIFT),  'YELLOW'),
    # the eggs are yellow for the first six floors and white after that,
    # when the status panel turns red as well
    ('SP_EGGA',  12, widen(EGG_NAT),  'YELLOW'),
    ('SP_EGGB',  13, widen(EGG_NAT),  'WHITE'),
    ('SP_SEED',  14, widen(SEED_NAT), 'MAGENTA'),
]

if __name__ == '__main__':
    for name, n, rows, col in OUT:
        h = len(rows)
        w = len(rows[0])
        print("DATA %d,%d,%d,\"%s\"   ' %s" % (w, h, PAL[col], to_hex(rows, w), name))
    print()
    print('--- previews ---')
    for name, n, rows, col in OUT:
        print('###', name, len(rows[0]), 'x', len(rows))
        print(art(rows))
