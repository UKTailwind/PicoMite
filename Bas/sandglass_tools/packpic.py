"""packpic.py - read the packed full-screen pictures.

The image tables hold the game's small pieces.  Its full-screen pictures, the
rooms the cut scenes play against, are kept in a different and much simpler
form: a run of identical bytes is written as an escape byte, a count and the
value, and anything else is written as itself.  One byte value is reserved as
the escape, so a literal of that value cannot occur.

The awkward part is not the compression but the order.  The bytes are not in
reading order: they follow the path the original's unpacker takes through the
Apple's screen memory, which is famously not linear.  It walks one column of
the screen at a time, and within a column it steps through three bands of
sixty-four rows, eight groups of eight rows inside each band, and eight rows
inside each group.  So the decoder here reproduces that walk exactly and uses
it to place each byte, rather than trying to describe the layout in a formula.

Every byte comes out with its top bit set, because the unpacker forces it.  On
the Apple that bit chooses which pair of colours the seven pixels in the byte
are drawn with, so a picture unpacked this way always uses the second pair.

This reads only the file the caller supplies and contains no data of its own.
"""

ESCAPE = 0xFE
SCREEN = 0x2000
COLUMNS = 40
ROWS = 192


def unpack(data):
    """Expand the run-length stream into the bytes it stands for."""
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        b = data[i]
        if b != ESCAPE:
            out.append(b | 0x80)
            i += 1
            continue
        if i + 2 >= n:
            break                        # a truncated run at the end
        count = data[i + 1]
        value = data[i + 2] | 0x80
        out.extend([value] * count)
        i += 3
    return bytes(out)


def write_order():
    """The addresses the unpacker visits, in the order it visits them.

    This is the original's own walk, followed literally: column outermost, then
    the three bands, the eight groups and the eight rows.
    """
    order = []
    for col in range(COLUMNS - 1, -1, -1):
        band = 0x2078
        while True:
            band -= 0x28
            group = band + 0x400
            while True:
                group -= 0x80
                row = group + 0x2000
                while True:
                    row -= 0x400
                    order.append(row + col)
                    if (row >> 8) == (group >> 8):
                        break
                if group == band:
                    break
            if (band & 0xFF) == 0:
                break
    return order


def _address_map():
    """Every screen address, and the column and row it belongs to."""
    where = {}
    for y in range(ROWS):
        base = (SCREEN + (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80
                + (y >> 6) * 0x28)
        for x in range(COLUMNS):
            where[base + x] = (x, y)
    return where


def to_page(data):
    """Expand a packed picture into the eight kilobyte screen page it fills."""
    stream = unpack(data)
    order = write_order()
    page = bytearray(0x2000)
    for i, addr in enumerate(order):
        if i >= len(stream):
            break
        page[addr - SCREEN] = stream[i]
    return bytes(page)


def to_rows(data, even, odd, solid, off):
    """Expand a packed picture into rows of colour indices, top row first.

    The colour rule is the one the hardware imposes and the rest of this
    converter already follows: a lit pixel on its own takes one of two colours
    depending on whether it sits at an even or an odd position, and two or more
    together read as white.
    """
    page = to_page(data)
    where = _address_map()
    bits = [[0] * (COLUMNS * 7) for _ in range(ROWS)]
    for addr, (x, y) in where.items():
        byte = page[addr - SCREEN]
        for bit in range(7):
            bits[y][x * 7 + bit] = (byte >> bit) & 1

    width = COLUMNS * 7
    rows = []
    for y in range(ROWS):
        lit = bits[y]
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
    return rows


def main(argv=None):
    """Decode one packed picture to a four-bit BMP.

        python packpic.py <packed file> -o <out.bmp>

    The picture is 280 by 192, which sits inside the display without scaling.
    Colours follow the same rule as the rest of the converter, so a decoded
    background sits beside the game's own artwork rather than clashing with it.
    """
    import argparse
    import os
    import sys
    import sheets

    ap = argparse.ArgumentParser(description=main.__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", help="a packed picture from your own copy")
    ap.add_argument("-o", "--out", default=None)
    args = ap.parse_args(argv)

    with open(args.source, "rb") as fh:
        data = fh.read()
    rows = to_rows(data, even=1, odd=2, solid=3, off=0)
    out = args.out or os.path.splitext(args.source)[0] + ".bmp"
    size = sheets.write_bmp4(out, rows)
    print("%s: %d packed bytes -> %d x %d, %d bytes"
          % (os.path.basename(args.source), len(data), len(rows[0]), len(rows), size))
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
