"""appleimg.py - read Apple II hi-res image tables and turn them into RGB121.

The image-table format is documented in the released HIRES.S: a table of
two-byte little-endian pointers indexed from one, and at each pointer an image
stored as a width in bytes, a height in lines, then width*height packed bytes
read left to right, top to bottom.  Byte 0 of the file is not part of the
pointer table - SETIMAGE computes its index as image*2-1, so pointer n lives at
offset n*2-1 and offset 0 is skipped.

Each image byte holds seven pixels in bits 0..6, lowest bit leftmost.  Bit 7
selects the colour palette, and in this data set it is set on every byte of
every table, so the whole game is drawn in one palette group and a pixel is
simply on or off.  Colour then follows from position: an isolated lit pixel
takes one of two colours by its parity, and two or more adjacent lit pixels
read as white.

This module reads nothing but the files the user points it at.  It contains no
game data of its own.
"""

# A decoded image uses three colours plus a transparent black: one for an
# isolated lit pixel at an even position, one for an odd position, and one for
# a run of two or more, which is the bulk of any shape.
#
# These are slot NUMBERS, not colours.  MODE 2's sixteen entries are
# programmable, so the converter gives each image table its own group of three
# and the engine decides what they actually look like.  That is what lets the
# figures be told apart from the walls, which the original hardware could not
# do: it had one set of colours for everything.
BLACK = 0
# Unset pixels are not black: they are TRANSPARENT, a slot the display maps to
# black but the blitter skips.  Black that is part of a figure - the gaps
# between an arm and the body - is OPAQUE_BLACK, a second black slot the
# blitter does draw.  The original gets the same effect with a mask: every
# lit pixel also masks its neighbour on each side within its seven-pixel byte
# (MASKTAB in HRTABLES.S), so the figure carries a thin black edge and nothing
# shows through it.
TRANSPARENT = 11
OPAQUE_BLACK = 7
BLUE = 1
ORANGE = 12
WHITE = 15

# Every table in the released set is stored relative to this load address.
DEFAULT_BASE = 0x6000

# Sanity bounds for a decoded image header.  The widest thing the game draws is
# a fraction of the 40-byte screen and the tallest is under one screen.
MAX_W_BYTES = 64
MAX_H_LINES = 200


class Image:
    """One entry of an image table, still in packed Apple form."""

    __slots__ = ("index", "w", "h", "data")

    def __init__(self, index, w, h, data):
        self.index = index      # 1-based, as the game refers to it
        self.w = w              # width in bytes
        self.h = h              # height in lines
        self.data = data        # w*h packed bytes

    @property
    def pixel_width(self):
        return self.w * 7

    def __repr__(self):
        return "Image(%d, %dx%d px)" % (self.index, self.pixel_width, self.h)


def read_table(path, base=DEFAULT_BASE):
    """Parse one image-table file.  Returns a list of Image.

    Stops at the first entry that does not decode, which is normal: the pointer
    table is padded and its final entry can point past the data.
    """
    with open(path, "rb") as fh:
        blob = fh.read()
    if len(blob) < 4:
        raise ValueError("%s: too short to be an image table" % path)

    first = blob[1] | (blob[2] << 8)
    span = first - base - 1
    if span < 2 or span % 2:
        raise ValueError(
            "%s: pointer table does not resolve against base $%04X "
            "(first pointer $%04X)" % (path, base, first))

    count = span // 2
    images = []
    for n in range(1, count + 1):
        at = n * 2 - 1
        if at + 1 >= len(blob):
            break
        off = (blob[at] | (blob[at + 1] << 8)) - base
        if off < 0 or off + 2 > len(blob):
            break
        w = blob[off]
        h = blob[off + 1]
        if w == 0 or h == 0 or w > MAX_W_BYTES or h > MAX_H_LINES:
            break
        if off + 2 + w * h > len(blob):
            break
        images.append(Image(n, w, h, blob[off + 2:off + 2 + w * h]))
    if not images:
        raise ValueError("%s: no images decoded" % path)
    return images


def palette_bit_always_set(images):
    """True when bit 7 is set on every byte, which is what lets us treat the
    artwork as one bit a pixel.  The converter checks this rather than assuming
    it, so a different data set fails loudly instead of quietly wrong."""
    for img in images:
        for byte in img.data:
            if not byte & 0x80:
                return False
    return True


def decode(img, even=BLUE, odd=ORANGE, solid=WHITE, off=TRANSPARENT):
    """Expand one image to a list of rows of RGB121 colour indices, top first.

    The stored rows run bottom to top.  The original's plotter is given the
    coordinate of an image's *bottom* line and counts the screen row downwards
    as it consumes rows in order, so the first row in the file is the lowest one
    on screen.  Everything here works top-down, so the rows are reversed on the
    way out.

    This is not a detail you can spot in the artwork: walls and floors look
    plausible either way up.  A person does not.
    """
    width = img.pixel_width
    rows = []
    for line in range(img.h):
        packed = img.data[line * img.w:(line + 1) * img.w]
        lit = []
        for byte in packed:
            for bit in range(7):
                lit.append((byte >> bit) & 1)
        row = [off] * width
        x = 0
        while x < width:
            if not lit[x]:
                x += 1
                continue
            end = x
            while end < width and lit[end]:
                end += 1
            if end - x >= 2:
                for t in range(x, end):
                    row[t] = solid
            else:
                row[x] = even if (x & 1) == 0 else odd
            x = end
        rows.append(row)
    rows.reverse()
    return rows


def fill_interior(rows, fill, off=TRANSPARENT):
    """Make a figure solid: every unset pixel enclosed by the figure takes the
    body colour, and only the unset pixels that reach the edge of the image
    stay transparent.

    The original has to leave those holes black.  The Apple II gives it four
    colours and one bit a pixel, so the gaps in a shaded limb cannot be
    anything but background, and the mask paints a black edge to stop the wall
    showing through them.  A sixteen-colour display has no such limit, so the
    holes are filled with the figure's own colour and the black goes away.

    A gap that opens to the outside - between his legs, or under an arm held
    away from the body - is genuinely see-through and is left alone.
    """
    h = len(rows)
    w = len(rows[0])
    out = [list(r) for r in rows]
    # Flood from the border through unset pixels; whatever is not reached is
    # inside the figure.
    seen = [[False] * w for _ in range(h)]
    stack = []
    for x in range(w):
        for y in (0, h - 1):
            if out[y][x] == off and not seen[y][x]:
                seen[y][x] = True
                stack.append((x, y))
    for y in range(h):
        for x in (0, w - 1):
            if out[y][x] == off and not seen[y][x]:
                seen[y][x] = True
                stack.append((x, y))
    while stack:
        x, y = stack.pop()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < w and 0 <= ny < h and not seen[ny][nx] and out[ny][nx] == off:
                seen[ny][nx] = True
                stack.append((nx, ny))
    for y in range(h):
        for x in range(w):
            if out[y][x] == off and not seen[y][x]:
                out[y][x] = fill
    return out


def apply_mask(rows, off=TRANSPARENT, black=OPAQUE_BLACK):
    """Give a character its mask: an unset pixel next to a lit one, within
    the same seven-pixel byte, becomes opaque black.  Mirrors MASKTAB."""
    out = []
    for row in rows:
        w = len(row)
        new = list(row)
        for x in range(w):
            if row[x] != off:
                continue
            byte = x // 7
            left = x - 1 >= 0 and (x - 1) // 7 == byte and row[x - 1] != off
            right = x + 1 < w and (x + 1) // 7 == byte and row[x + 1] != off
            if left or right:
                new[x] = black
        out.append(new)
    return out


def mirror(rows):
    """Left-right flip.  The image tables store every character facing one way
    and the game mirrors as it draws; we pre-mirror instead, because BLIT FLASH
    has no mirror of its own."""
    return [list(reversed(r)) for r in rows]
