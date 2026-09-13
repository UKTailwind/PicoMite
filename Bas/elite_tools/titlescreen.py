"""Draw the Elite title screen: a hidden-line Cobra Mk III over a starfield.

The picture is generated from our own ship data - Bas/elite/data/ships.json,
which blueprints.py extracts from the published 6502 source - and the lettering
is drawn here as bars.  Nothing copyrighted is copied in; this is the original
title screen's design rendered from the same numbers the game flies.

  python titlescreen.py            writes elite/data/title.jpg and title.png

MODE 2 is 320x240 in 16 colours, so the image is drawn at that size and kept to
flat blacks and bright lines, which is what survives both the JPEG and the
palette.
"""
import json, math, os, random

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SHIPS = os.path.normpath(os.path.join(HERE, "..", "elite", "data", "ships.json"))
OUT = os.path.normpath(os.path.join(HERE, "..", "elite", "data"))

W, H = 320, 240
WHITE = (255, 255, 255)
CYAN = (0, 255, 255)
YELLOW = (255, 255, 0)
GREY = (128, 128, 128)
DIM = (96, 96, 96)


def rotate(v, pitch, yaw):
    """Pitch about x, then yaw about y, both in degrees."""
    x, y, z = v
    a = math.radians(pitch)
    y, z = y * math.cos(a) - z * math.sin(a), y * math.sin(a) + z * math.cos(a)
    a = math.radians(yaw)
    x, z = x * math.cos(a) + z * math.sin(a), -x * math.sin(a) + z * math.cos(a)
    return (x, y, z)


def draw_ship(d, ship, cx, cy, scale, pitch, yaw, dist, colour):
    """A hidden-line wireframe: the edges of the faces that point our way.

    Visibility is the blueprint's own face normal against the direction from
    the face to the camera, which is how the game decides it too.
    """
    verts = [rotate(v, pitch, yaw) for v in ship["vertices"]]
    norms = [rotate(n, pitch, yaw) for n in ship["normals"]]
    focal = 256.0

    def project(v):
        z = v[2] + dist
        if z < 1:
            z = 1
        return (cx + focal * scale * v[0] / z, cy - focal * scale * v[1] / z)

    def visible(poly):
        host = poly.get("host")
        if host is None or host >= len(norms):
            return True
        n = norms[host]
        if n == (0.0, 0.0, 0.0):
            return True
        # The camera sits at -dist on the z axis, so the direction from a
        # point on the face to the camera is (-x, -y, -dist - z).  The face
        # is ours if its normal leans that way.
        v = verts[poly["vertices"][0]]
        to_cam = (-v[0], -v[1], -dist - v[2])
        return n[0] * to_cam[0] + n[1] * to_cam[1] + n[2] * to_cam[2] > 0

    drawn = set()
    for poly in ship["polygons"]:
        if not visible(poly):
            continue
        vs = poly["vertices"]
        n = len(vs)
        if poly.get("kind") == "line":
            pairs = [(vs[0], vs[1])]
        else:
            pairs = [(vs[i], vs[(i + 1) % n]) for i in range(n)]
        for a, b in pairs:
            key = (min(a, b), max(a, b))
            if key in drawn:
                continue
            drawn.add(key)
            d.line([project(verts[a]), project(verts[b])], fill=colour, width=1)


# --- the lettering, drawn as bars so it stays crisp in sixteen colours
def bars(letter, x, y, w, h, t):
    """Return the rectangles that make up one capital, on a w by h cell."""
    r = []
    if letter == "E":
        r = [(x, y, x + t, y + h), (x, y, x + w, y + t),
             (x, y + h // 2 - t // 2, x + w - t, y + h // 2 - t // 2 + t),
             (x, y + h - t, x + w, y + h)]
    elif letter == "L":
        r = [(x, y, x + t, y + h), (x, y + h - t, x + w, y + h)]
    elif letter == "I":
        r = [(x + w // 2 - t // 2, y, x + w // 2 - t // 2 + t, y + h)]
    elif letter == "T":
        r = [(x, y, x + w, y + t),
             (x + w // 2 - t // 2, y, x + w // 2 - t // 2 + t, y + h)]
    return r


def title(d, text, cy, cell_w, cell_h, thick, gap, colour):
    total = len(text) * cell_w + (len(text) - 1) * gap
    x = (W - total) // 2
    for ch in text:
        for r in bars(ch, x, cy, cell_w, cell_h, thick):
            d.rectangle(r, fill=colour)
        x += cell_w + gap


def main():
    ships = json.load(open(SHIPS))
    img = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(img)

    # A starfield, thinned out behind the lettering so nothing reads as dirt.
    random.seed(1984)
    for _ in range(110):
        x, y = random.randrange(W), random.randrange(H)
        if 8 < y < 56 or y > 196:
            continue
        d.point((x, y), fill=random.choice([WHITE, WHITE, CYAN]))

    title(d, "ELITE", 14, 26, 34, 6, 14, WHITE)
    d.line([(24, 60), (W - 24, 60)], fill=CYAN)

    draw_ship(d, ships["COBRA_MK_3"], W // 2, 126, 1.55, -26, 34, 400, WHITE)

    d.line([(24, 190), (W - 24, 190)], fill=CYAN)
    small(d, "PRESS ANY KEY TO PLAY", 202, YELLOW)
    small(d, "H FOR THE CONTROLS", 218, CYAN)

    img.save(os.path.join(OUT, "title.png"))
    img.save(os.path.join(OUT, "title.jpg"), quality=95, subsampling=0)
    print("wrote title.png and title.jpg in", OUT)


# A 5x7 stroke alphabet, enough for the two prompt lines.
FONT = {
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "11110", "10001", "10001", "10001", "11110"],
    "C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "11110", "10000", "10000", "10000", "11111"],
    "F": ["11111", "10000", "11110", "10000", "10000", "10000", "10000"],
    "G": ["01110", "10001", "10000", "10111", "10001", "10001", "01110"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "Y": ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
    " ": ["00000"] * 7,
}


def small(d, text, y, colour):
    cw = 6
    x = (W - len(text) * cw) // 2
    for ch in text:
        rows = FONT.get(ch)
        if rows:
            for ry, row in enumerate(rows):
                for rx, bit in enumerate(row):
                    if bit == "1":
                        d.point((x + rx, y + ry), fill=colour)
        x += cw


if __name__ == "__main__":
    main()
