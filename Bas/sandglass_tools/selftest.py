"""selftest.py - write a synthetic sheet for checking the pipeline on a board.

The sheet carries a deterministic pattern rather than any game artwork, so the
board test proves the BMP writer, the image-slot load and the blit path without
needing converted data present.  The colour of a pixel is

    ((x >> 3) + (y >> 3)) AND 15

so every eight-by-eight cell is one flat colour and any pixel's expected value
can be worked out on the board and compared.

    python selftest.py --out <directory> [--width 512] [--height 574]
"""

import argparse
import os
import sys

import sheets


def build(width, height):
    rows = []
    for y in range(height):
        band = y >> 3
        rows.append([((x >> 3) + band) & 15 for x in range(width)])
    return rows


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True)
    ap.add_argument("--width", type=int, default=sheets.SHEET_W)
    ap.add_argument("--height", type=int, default=sheets.SHEET_H_MAX)
    args = ap.parse_args(argv)

    if not sheets.slot_capacity_ok(args.width, args.height):
        raise SystemExit("%dx%d does not fit an image slot" % (args.width, args.height))

    os.makedirs(args.out, exist_ok=True)
    path = os.path.join(args.out, "testshet.bmp")
    size = sheets.write_bmp4(path, build(args.width, args.height))
    print("%s  %d x %d  %d bytes  (slot needs %d)"
          % (path, args.width, args.height, size,
             8 + args.width * args.height // 2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
