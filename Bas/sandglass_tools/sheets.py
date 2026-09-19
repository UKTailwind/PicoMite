"""sheets.py - pack decoded images into image-slot sheets and write them as BMPs.

An image slot on the RP2350 holds MAX_PROG_SIZE bytes, which on the PicoComputer
3 is 144 KB.  The firmware stores a slot as an eight-byte header followed by the
picture at two pixels a byte, so a slot holds a little under 295,000 pixels.

Sheets are written as four-bit BMPs whose palette is exactly the sixteen MODE 2
colours, so the firmware's decoder maps every entry back to the index it came
from with no approximation.
"""

import struct

TRANSPARENT = 11               # the slot the blitter skips; see appleimg

# graphics/Draw.c colours[16], as 0xRRGGBB.
PALETTE = [
    0x000000, 0x0000FF, 0x004000, 0x0040FF,
    0x008000, 0x0080FF, 0x00FF00, 0x00FFFF,
    0xFF0000, 0xFF00FF, 0xFF4000, 0xFF40FF,
    0xFF8000, 0xFF80FF, 0xFFFF00, 0xFFFFFF,
]

SHEET_W = 512          # a multiple of 8, so four-bit BMP rows need no padding
SHEET_H_MAX = 574      # 512*574 = 293,888 pixels, inside a 144 KB slot
SLOT_BYTES = 144 * 1024
PAD = 1                # blank pixel between packed images, to catch off-by-ones


class Placement:
    __slots__ = ("key", "sheet", "x", "y", "w", "h")

    def __init__(self, key, sheet, x, y, w, h):
        self.key = key
        self.sheet = sheet
        self.x = x
        self.y = y
        self.w = w
        self.h = h


def slot_capacity_ok(w, h):
    return 8 + (w * h + 1) // 2 <= SLOT_BYTES


def pack(entries, sheet_w=SHEET_W, sheet_h_max=SHEET_H_MAX):
    """Shelf-pack (key, rows) entries into sheets.

    Returns (sheets, placements) where a sheet is a list of rows of colour
    indices and placements says where every key landed.
    """
    order = sorted(entries, key=lambda e: (-len(e[1]), -len(e[1][0])))

    sheets = []
    placements = []
    # Per sheet: current shelf y, shelf height, and x cursor.
    shelf_y = 0
    shelf_h = 0
    cursor = 0
    grid = None

    def new_sheet():
        nonlocal grid, shelf_y, shelf_h, cursor
        grid = [[TRANSPARENT] * sheet_w for _ in range(sheet_h_max)]
        sheets.append(grid)
        shelf_y = 0
        shelf_h = 0
        cursor = 0

    new_sheet()

    for key, rows in order:
        h = len(rows)
        w = len(rows[0])
        if w > sheet_w:
            raise ValueError("image %r is %d px wide, wider than a sheet" % (key, w))
        if h > sheet_h_max:
            raise ValueError("image %r is %d px tall, taller than a sheet" % (key, h))

        if cursor + w > sheet_w:                 # close the shelf
            shelf_y += shelf_h + PAD
            shelf_h = 0
            cursor = 0
        if shelf_y + h > sheet_h_max:            # close the sheet
            new_sheet()

        for dy, row in enumerate(rows):
            line = grid[shelf_y + dy]
            line[cursor:cursor + w] = row

        placements.append(Placement(key, len(sheets) - 1, cursor, shelf_y, w, h))
        cursor += w + PAD
        if h > shelf_h:
            shelf_h = h

    # Trim each sheet to the rows actually used, rounded up to keep the height
    # even so the four-bit row packing stays simple.
    used = [0] * len(sheets)
    for p in placements:
        bottom = p.y + p.h
        if bottom > used[p.sheet]:
            used[p.sheet] = bottom
    trimmed = []
    for i, grid in enumerate(sheets):
        height = used[i] + (used[i] & 1)
        trimmed.append(grid[:height])
    return trimmed, placements


def write_bmp4(path, rows, colours=None):
    """Write a four-bit BMP.  Rows are top to bottom; BMP stores bottom to top.

    The colours written into the file decide how the board reads it back.  A
    sheet goes into an image slot, which keeps the pixel values as they are, so
    the default palette is written and the numbers survive.  A picture drawn
    straight to the screen is matched against the palette that is live at the
    time, colour by colour, so it has to carry the colours the game actually
    uses or every pixel lands in the wrong slot - which is how the stone room
    came out blue.
    """
    h = len(rows)
    w = len(rows[0])
    if w % 8:
        raise ValueError("sheet width %d must be a multiple of 8" % w)
    row_bytes = w // 2                       # already 4-byte aligned when w%8==0

    pixel_data = bytearray()
    for row in reversed(rows):
        packed = bytearray(row_bytes)
        for x in range(0, w, 2):
            packed[x // 2] = ((row[x] & 15) << 4) | (row[x + 1] & 15)
        pixel_data += packed

    palette = bytearray()
    for rgb in (colours or PALETTE):
        palette += bytes(((rgb) & 0xFF, (rgb >> 8) & 0xFF, (rgb >> 16) & 0xFF, 0))

    offset = 14 + 40 + len(palette)
    filesize = offset + len(pixel_data)

    header = b"BM" + struct.pack("<IHHI", filesize, 0, 0, offset)
    info = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 4, 0, len(pixel_data),
                       2835, 2835, 16, 16)
    with open(path, "wb") as fh:
        fh.write(header)
        fh.write(info)
        fh.write(palette)
        fh.write(pixel_data)
    return filesize
