"""convert.py - turn your own copy of the published Apple II source release into
the data files the engine reads.

    python convert.py --source <path to your clone> --out <directory>

Nothing is downloaded and nothing is bundled.  You supply the path to a clone of
the published source repository that you obtained yourself; this script reads it
and writes data files onto your machine.  The engine ships with no game data in
it and does nothing until you have run this.

Outputs, all data files, never BASIC:

    sheet1.bmp ...   artwork, one file per image slot, RGB121 as a 4-bit BMP
    art.idx          where every image sits in which sheet
    levels.dat       the fifteen level blueprints
    convert.log      what was read and what was produced
"""

import argparse
import os
import sys

import appleimg
import blueprint

# The tables the release ships, and whether their contents are characters (which
# the game mirrors as it draws, so we store both facings) or scenery (which it
# never mirrors).
TABLES = [
    ("CHTAB1", "Images/IMG.CHTAB1", True),
    ("CHTAB2", "Images/IMG.CHTAB2", True),
    ("CHTAB3", "Images/IMG.CHTAB3", True),
    ("CHTAB4FAT", "Images/IMG.CHTAB4.FAT", True),
    ("CHTAB4GD", "Images/IMG.CHTAB4.GD", True),
    ("CHTAB4SHAD", "Images/IMG.CHTAB4.SHAD", True),
    ("CHTAB4SKEL", "Images/IMG.CHTAB4.SKEL", True),
    ("CHTAB4VIZ", "Images/IMG.CHTAB4.VIZ", True),
    ("CHTAB5", "Images/IMG.CHTAB5", True),
    ("CHTAB6A", "Images/IMG.CHTAB6.A", True),
    ("CHTAB6B", "Images/IMG.CHTAB6.B", True),
    ("CHTAB7", "Images/IMG.CHTAB7", True),
    ("BGTAB1DUN", "Images/IMG.BGTAB1.DUN", False),
    ("BGTAB1PAL", "Images/IMG.BGTAB1.PAL", False),
    ("BGTAB2DUN", "Images/IMG.BGTAB2.DUN", False),
    ("BGTAB2PAL", "Images/IMG.BGTAB2.PAL", False),
]

# Palette groups.  Each image table draws with three slots: an isolated lit
# pixel at an even position, one at an odd position, and a run of two or more,
# which is the bulk of any shape.  Giving the walls, the player and his
# opponents separate groups is what lets them be told apart - the original
# hardware had one set of colours for everything, which is why the figures are
# the same white as the stone they stand on.
#
# Slot 0 is black everywhere and is the transparent colour.
GROUP_SCENERY = (1, 2, 3)
GROUP_PLAYER = (4, 5, 6)
GROUP_OPPONENT = (8, 9, 10)
GROUP_CUTSCENE = (12, 13, 14)

# What those slots actually look like.  MODE 2's sixteen entries are
# programmable, so these are a choice rather than a constraint: warm stone for
# the architecture, a pale figure against it, and cool steel for the guards so
# an opponent reads at a glance.  Written as 0xRRGGBB.
PALETTE = {
    0:  0x000000,
    # Scenery.  The two "edge" slots are not edge shading in practice: back
    # walls are drawn as a dither of isolated pixels, so those slots carry the
    # wall texture.  Making them much darker than the solid stone sends every
    # dithered surface almost black, which is what happened first time.
    1:  0x8A6B45,   # stone, dithered darker
    2:  0xB08A5A,   # stone, dithered lighter
    3:  0xC4A276,   # stone, solid
    # The player.  Slot 6 is the bulk of the figure, so it is the tunic colour
    # and wants to be clearly not-white: the stone is warm, so he is cool.
    4:  0x24507F,   # player, deep
    5:  0x4E8CC4,   # player, mid
    6:  0x8FC9F0,   # player, tunic
    # Opponents, warm against his cool so the two never read alike.
    8:  0x7A2A20,   # opponent, deep
    9:  0xB5503A,   # opponent, mid
    10: 0xE08A5F,   # opponent
    12: 0x6B4A5A,   # cutscene, deep
    13: 0x9A7A8A,   # cutscene, mid
    14: 0xE0CDD6,   # cutscene
    15: 0xFFFFFF,   # kept white for anything that wants it
}

LEVEL_COUNT = 15
SOURCE_SUBDIR = "01 POP Source"

# Tables only the intro and ending use.  Leaving them out drops the artwork from
# five sheets to four, which matters: there are five RAM image slots and the last
# of them is where a RAM library lives, so five sheets and a library cannot both
# be resident.
CUTSCENE_TABLES = {"CHTAB6A", "CHTAB6B", "CHTAB7"}

SETS = {
    "game": lambda name: name not in CUTSCENE_TABLES,
    "full": lambda name: True,
}


def group_for(name):
    """Which palette group a table draws with."""
    if name.startswith("BGTAB"):
        return GROUP_SCENERY
    if name in CUTSCENE_TABLES:
        return GROUP_CUTSCENE
    if name.startswith("CHTAB4"):
        return GROUP_OPPONENT
    return GROUP_PLAYER


def find_source(root):
    """Accept either the top of a clone or the source directory inside it."""
    for candidate in (os.path.join(root, SOURCE_SUBDIR), root):
        if os.path.isdir(os.path.join(candidate, "Images")) and \
           os.path.isdir(os.path.join(candidate, "Levels")):
            return candidate
    raise SystemExit(
        "Could not find Images/ and Levels/ under %r.\n"
        "Point --source at your clone of the published source release." % root)


def masked_front_images(src):
    """Image numbers that are drawn as a FRONT piece through the original's
    mask-then-OR path, and are not used anywhere else.

    A front piece is not simply OR'd over what is already there.  drawfrnt
    writes posts and the arch tops with STA, which is opaque, and sends
    everything else through maddfore: the image masks itself first (the same
    one-pixel dilation MASKTAB applies to a character) and is then OR'd.  Skip
    that and the dither holes in a pillar let the character show through it.

    Only images used SOLELY as a front piece are masked here, so a piece that
    also appears in a wall or a floor is left exactly as it was.
    """
    import merlin
    ba = merlin.Assembler()
    bg = ba.assemble(os.path.join(src, "Source", "BGDATA.S"))
    STA_TYPES = {3, 27, 28, 29}          # posts, archtop2..archtop3 (>= archtop2)
    at = ba.symbols["fronti"]
    front = set()
    for tnum in range(BLOCK_TYPES):
        v = bg[at + tnum]
        if v and tnum not in STA_TYPES:
            front.add(v)
    # blockfr is the solid block's own front-piece variants and slicerfrnt is
    # the slicer's, so both are front pieces too.  Leaving blockfr in the
    # "used elsewhere" set is what let the character show through the dither
    # of the block he was standing against.
    at = ba.symbols["blockfr"]
    front.update(v for v in bg[at:at + 2] if v)
    at = ba.symbols["slicerfrnt"]
    front.update(v for v in bg[at:at + 5] if v)
    other = set()
    for name in PIECE_TABLES:
        if name == "fronti":
            continue
        at = ba.symbols[name]
        other.update(v for v in bg[at:at + BLOCK_TYPES] if v)
    for name, count in VARIANT_TABLES:
        if name == "blockfr":
            continue
        at = ba.symbols[name]
        other.update(v for v in bg[at:at + count] if v)
    return front - other


def convert_images(src, outdir, log, wanted):
    entries = []
    stats = []
    front_masked = masked_front_images(src)
    log("  front pieces needing the mask: %s"
        % ",".join(str(v) for v in sorted(front_masked)))
    for name, relpath, is_char in TABLES:
        if not wanted(name):
            continue
        path = os.path.join(src, relpath.replace("/", os.sep))
        if not os.path.exists(path):
            raise SystemExit("missing %s" % path)
        group = group_for(name)
        images = appleimg.read_table(path)
        if not appleimg.palette_bit_always_set(images):
            raise SystemExit(
                "%s uses both colour palettes; this converter assumes one. "
                "Refusing to guess." % name)
        pixels = 0
        for img in images:
            rows = appleimg.decode(img, even=group[0], odd=group[1],
                                   solid=group[2])
            if is_char:
                # No black in a figure: fill the enclosed holes with its own
                # colour instead of masking them out.  See fill_interior.
                rows = appleimg.fill_interior(rows, group[2])
            elif name.startswith("BGTAB1") and img.index in front_masked:
                rows = appleimg.apply_mask(rows)
            elif name.startswith("BGTAB2") and (img.index + 0x80) in front_masked:
                rows = appleimg.apply_mask(rows)
            entries.append(((name, img.index, 0), rows))
            pixels += img.pixel_width * img.h
            if is_char:
                entries.append(((name, img.index, 1), appleimg.mirror(rows)))
                pixels += img.pixel_width * img.h
        stats.append((name, len(images), pixels, is_char))
        log("  %-11s %4d images %8d px %s, slots %s"
            % (name, len(images), pixels,
               "both facings" if is_char else "as drawn",
               ",".join(str(g) for g in group)))

    import sheets
    grids, placements = sheets.pack(entries)
    log("")
    log("  packed %d images into %d sheets" % (len(entries), len(grids)))

    for i, grid in enumerate(grids):
        w = len(grid[0])
        h = len(grid)
        if not sheets.slot_capacity_ok(w, h):
            raise SystemExit("sheet %d is %dx%d, too big for an image slot" % (i + 1, w, h))
        path = os.path.join(outdir, "sheet%d.bmp" % (i + 1))
        size = sheets.write_bmp4(path, grid)
        log("  sheet%d.bmp  %4d x %4d  %7d bytes  (slot needs %d)"
            % (i + 1, w, h, size, 8 + w * h // 2))

    index_path = os.path.join(outdir, "art.idx")
    placements.sort(key=lambda p: (p.key[0], p.key[1], p.key[2]))
    with open(index_path, "w", newline="\n") as fh:
        fh.write("# table facing image sheet x y w h\n")
        for p in placements:
            fh.write("%s %d %d %d %d %d %d %d\n"
                     % (p.key[0], p.key[2], p.key[1], p.sheet + 1,
                        p.x, p.y, p.w, p.h))
    log("  art.idx     %d entries" % len(placements))

    meta = _write_art_bin(os.path.join(outdir, "art.bin"), stats, placements, log)
    return stats, grids, placements, meta


ART_RECORD = 9          # sheet u8, x u16, y u16, w u16, h u16


def _write_art_bin(path, stats, placements, log):
    """The same index as a flat binary, for the engine to MEMORY INPUT.

    Layout, all little endian:
        u16 table_count
        per table: u16 images, u8 facings, u32 first record
        then the records, nine bytes each, table by table, image 1..n
        ascending, facing 0 then 1.
    """
    import struct

    by_key = {p.key: p for p in placements}
    order = [(name, count, 2 if is_char else 1)
             for name, count, _px, is_char in stats]

    header = struct.pack("<H", len(order))
    directory = b""
    records = b""
    first = 0
    for name, count, facings in order:
        directory += struct.pack("<HBI", count, facings, first)
        for image in range(1, count + 1):
            for facing in range(facings):
                p = by_key.get((name, image, facing))
                if p is None:
                    raise SystemExit("art index: %s image %d facing %d missing"
                                     % (name, image, facing))
                records += struct.pack("<BHHHH", p.sheet + 1, p.x, p.y, p.w, p.h)
        first += count * facings

    with open(path, "wb") as fh:
        fh.write(header)
        fh.write(directory)
        fh.write(records)
    total = len(header) + len(directory) + len(records)
    log("  art.bin     %d tables, %d records of %d bytes, %d bytes total"
        % (len(order), first, ART_RECORD, total))
    meta = {"art_len": total, "art_tables": len(order), "art_records": first,
            "art_record_bytes": ART_RECORD}
    # Publish each table's number by name.  Which tables are present depends on
    # --set, so the numbering shifts; an engine that hard-coded them would break
    # silently and draw from the wrong table.
    for i, (name, count, _facings) in enumerate(order):
        meta["tab_" + name] = i + 1
        meta["img_" + name] = count
    return meta


def convert_tables(src, outdir, log):
    """Assemble the two data modules that are source, not binary, and write them
    out with every address turned into an offset from the start of its file."""
    import merlin

    layout = {}

    # --- the frame table -----------------------------------------------------
    fa = merlin.Assembler()
    frames = fa.assemble(os.path.join(src, "Source", "FRAMEDEF.S"))
    base = fa.symbols["org"]
    for name in ("altset1", "altset2", "swordtab"):
        if name not in fa.symbols:
            raise SystemExit("FRAMEDEF.S: expected a %s label" % name)
        layout["frames_" + name] = fa.symbols[name] - base
    layout["frames_len"] = len(frames)
    layout["frames_entry"] = 5
    main_frames = layout["frames_altset1"] // 5
    layout["frames_count"] = main_frames
    with open(os.path.join(outdir, "frames.dat"), "wb") as fh:
        fh.write(frames)
    log("  frames.dat  %d bytes, %d frames of 5, alternates at %d and %d"
        % (len(frames), main_frames,
           layout["frames_altset1"], layout["frames_altset2"]))

    # --- the animation sequences --------------------------------------------
    sa = merlin.Assembler()
    seq = bytearray(sa.assemble(os.path.join(src, "Source", "SEQTABLE.S")))
    sbase = sa.origin
    # The module opens with a run of addresses, one per sequence.  Count the
    # leading addresses that sit back to back from offset zero; the first gap is
    # where the table ends and the sequences begin.  Deriving the size from the
    # first entry instead would be wrong, because entry one does not point at
    # the byte after the table: another sequence is assembled in between.
    count = 0
    for i, site in enumerate(sa.word_sites):
        if site != i * 2:
            break
        count = i + 1
    table_bytes = count * 2
    if count == 0:
        raise SystemExit("SEQTABLE.S: sequence pointer table did not resolve")

    # Every address in the module becomes an offset into this file, so the
    # engine never has to know what address the original was assembled at.
    rebased = 0
    for site in sa.word_sites:
        value = seq[site] | (seq[site + 1] << 8)
        if not (sbase <= value < sbase + len(seq)):
            raise SystemExit("SEQTABLE.S: address $%04X at %d points outside "
                             "the module" % (value, site))
        off = value - sbase
        seq[site] = off & 0xFF
        seq[site + 1] = off >> 8
        rebased += 1

    layout["seq_len"] = len(seq)
    layout["seq_count"] = count
    layout["seq_first"] = table_bytes
    with open(os.path.join(outdir, "seq.dat"), "wb") as fh:
        fh.write(seq)
    log("  seq.dat     %d bytes, %d sequences, %d addresses rebased to offsets"
        % (len(seq), count, rebased))

    return layout


BLOCK_TYPES = 30

# The twelve per-block-type tables, in the order they are written out.  A block
# is not one picture: it is a back section, a middle section, a front-of-middle
# section, a floor section and a foreground piece that draws over the character,
# each with its own image number and vertical offset.
PIECE_TABLES = ["maska", "piecea", "pieceay", "maskb", "pieceb", "pieceby",
                "bstripe", "piecec", "pieced", "fronti", "fronty", "frontx"]

# Screen geometry.  BlockEdge is twenty entries, the rest are five.
GEOMETRY = [("BlockEdge", 20), ("BlockTop", 5), ("BlockBot", 5),
            ("FloorY", 5), ("BlockAy", 5)]

GEOM_CONSTANTS = ["BlockHeight", "ScrnBot", "VertDist", "DHeight"]

# Variant tables.  A block does not just have a type: it also carries a state
# byte, and for scenery that state picks which VARIANT of a piece is drawn -
# which back-wall panel pattern, which of two solid-block faces, which floor.
# Ignoring the state makes every wall render as variant zero, which is why a
# level looks flatter than it should.
VARIANT_TABLES = [
    ("panelb", 3), ("panelc", 3),
    ("spaceb", 4), ("spaceby", 4),
    ("floorb", 4), ("floorby", 4),
    ("blockb", 2), ("blockc", 2), ("blockd", 2), ("blockfr", 2),
]

VARIANT_CONSTANTS = ["numpans", "numblox", "numbpans",
                     "panelb0", "panelc0", "archpanel"]

# The moving parts.  A gate, a spike pit, a loose floor, a slicer and a torch
# each animate by their state byte, and these tables turn a state into the
# image drawn for it.  They sit in the background-data module.
ANIM_TABLES = [
    ("spikea", 10), ("spikeb", 10),
    ("loosea", 11), ("looseby", 11), ("loosed", 11),
    ("gate8c", 8), ("gate8b", 8),
    ("slicerseq", 7), ("slicertop", 5), ("slicerbot", 5), ("slicerbot2", 5),
    ("slicergap", 5), ("slicerfrnt", 5),
]
ANIM_CONSTANTS = ["looseb", "gatebotSTA", "gatebotORA", "gateB1", "gatecmask",
                  "spikeExt", "spikeRet", "slicerExt", "slicerRet", "Ffalling",
                  "CUmask", "CUpiece"]

# The mover module is code, so it is read leniently: only its equates and its
# one table come out.  Its equates refer to the data modules, so those are
# assembled first and their symbols handed over.
MOVER_TABLES = [("MOVER.S", "gateinc", 3), ("MOVER.S", "gatevel", 9),
                ("GAMEBG.S", "torchflame", 18),
                # which background tiles and which enemy each level uses
                ("MISC.S", "bgset1", 15), ("MISC.S", "chset", 15),
                # where the shadow appears, and the thief's recorded moves
                ("AUTO.S", "shadpos6a", 8), ("AUTO.S", "shadpos5", 8),
                ("AUTO.S", "shadpos12", 8), ("AUTO.S", "ShadProg5", 16),
                # the guards' temperaments, one column per skill level
                ("AUTO.S", "strikeprob", 12), ("AUTO.S", "restrikeprob", 12),
                ("AUTO.S", "blockprob", 12), ("AUTO.S", "impblockprob", 12),
                ("AUTO.S", "advprob", 12), ("AUTO.S", "refractimer", 12),
                ("AUTO.S", "specialcolor", 12), ("AUTO.S", "extrastrength", 12),
                ("AUTO.S", "basicstrength", 14), ("AUTO.S", "basiccolor", 14)]
MOVER_CONSTANTS = [
    ("MOVER.S", ["pptimer", "spiketimer", "slicetimer", "gatetimer",
                 "loosetimer", "FFaccel", "FFtermvel", "crumbletime",
                 "crumbletime2", "disappeartime", "CrushDist", "maxgatevel",
                 "wiggletime"]),
    ("SUBS.S", ["slicersync"]),
    ("CTRL.S", ["stairthres"]),
    ("TOPCTRL.S", ["initmaxstr", "deadenough"]),
    ("CTRLSUBS.S", ["maxmaxstr"]),
    ("MISC.S", ["wtlesstimer"]),
    ("MOVEDATA.S", ["gmaxval", "gminval", "torchLast"]),
    ("COLL.S", ["gatemargin"]),
    ("AUTO.S", ["flaskscrn", "flaskx", "flasky", "swordscrn", "swordx",
               "swordy", "shadstrength", "mirscrn", "mirx", "miry"]),
]


def convert_blocks(src, outdir, log):
    """Extract what turns a block type into pictures, and the screen geometry."""
    import merlin

    ba = merlin.Assembler()
    bg = ba.assemble(os.path.join(src, "Source", "BGDATA.S"))
    ta = merlin.Assembler()
    tb = ta.assemble(os.path.join(src, "Source", "TABLES.S"))
    tbase = ta.origin

    layout = {}
    out = bytearray()
    for name in PIECE_TABLES:
        if name not in ba.symbols:
            raise SystemExit("BGDATA.S: expected a %s table" % name)
        at = ba.symbols[name]
        if at + BLOCK_TYPES > len(bg):
            raise SystemExit("BGDATA.S: %s runs past the end" % name)
        layout["block_" + name] = len(out)
        out += bg[at:at + BLOCK_TYPES]
    layout["block_types"] = BLOCK_TYPES

    for name, count in GEOMETRY:
        if name not in ta.symbols:
            raise SystemExit("TABLES.S: expected a %s table" % name)
        at = ta.symbols[name] - tbase
        if at < 0 or at + count > len(tb):
            raise SystemExit("TABLES.S: %s runs past the end" % name)
        layout["geom_" + name] = len(out)
        out += tb[at:at + count]

    for name, count in VARIANT_TABLES:
        if name not in ba.symbols:
            raise SystemExit("BGDATA.S: expected a %s table" % name)
        at = ba.symbols[name]
        if at + count > len(bg):
            raise SystemExit("BGDATA.S: %s runs past the end" % name)
        layout["var_" + name] = len(out)
        layout["varn_" + name] = count
        out += bg[at:at + count]

    for name in VARIANT_CONSTANTS:
        if name not in ba.symbols:
            raise SystemExit("BGDATA.S: expected a %s constant" % name)
        layout["const_" + name] = ba.symbols[name]

    for name in GEOM_CONSTANTS:
        if name not in ta.symbols:
            raise SystemExit("TABLES.S: expected a %s constant" % name)
        layout["const_" + name] = ta.symbols[name]

    for name, count in ANIM_TABLES:
        if name not in ba.symbols:
            raise SystemExit("BGDATA.S: expected a %s table" % name)
        at = ba.symbols[name]
        if at + count > len(bg):
            raise SystemExit("BGDATA.S: %s runs past the end" % name)
        layout["anim_" + name] = len(out)
        layout["animn_" + name] = count
        out += bg[at:at + count]
    for name in ANIM_CONSTANTS:
        if name not in ba.symbols:
            raise SystemExit("BGDATA.S: expected a %s constant" % name)
        layout["const_" + name] = ba.symbols[name]

    md = merlin.Assembler()
    md.assemble(os.path.join(src, "Source", "MOVEDATA.S"))
    seed = dict(md.symbols)
    seed.update(ba.symbols)
    # the sequence numbers, so a table that names a sequence resolves
    sd = merlin.Assembler()
    sd.assemble(os.path.join(src, "Source", "SEQDATA.S"))
    seed.update(sd.symbols)
    modules = {}
    for fname, names in MOVER_CONSTANTS:
        if fname == "MOVEDATA.S":
            asm, data = md, None
        else:
            asm = merlin.Assembler(lenient=True)
            asm.symbols.update(seed)
            data = asm.assemble(os.path.join(src, "Source", fname))
        modules[fname] = (asm, data)
        for name in names:
            if name not in asm.symbols:
                raise SystemExit("%s: expected a %s constant" % (fname, name))
            layout["const_" + name] = asm.symbols[name]
    for fname, name, count in MOVER_TABLES:
        if fname not in modules:
            asm = merlin.Assembler(lenient=True)
            asm.symbols.update(seed)
            modules[fname] = (asm, asm.assemble(os.path.join(src, "Source", fname)))
        asm, data = modules[fname]
        if name not in asm.symbols:
            raise SystemExit("%s: expected a %s table" % (fname, name))
        at = asm.symbols[name] - asm.origin
        if at < 0 or at + count > len(data):
            raise SystemExit("%s: %s runs past the end" % (fname, name))
        layout["anim_" + name] = len(out)
        layout["animn_" + name] = count
        out += data[at:at + count]

    layout["blocks_len"] = len(out)
    with open(os.path.join(outdir, "blocks.dat"), "wb") as fh:
        fh.write(out)
    log("  blocks.dat  %d bytes, %d piece tables of %d, %d geometry, %d variant, %d animation"
        % (len(out), len(PIECE_TABLES), BLOCK_TYPES, len(GEOMETRY),
           len(VARIANT_TABLES), len(ANIM_TABLES) + len(MOVER_TABLES)))
    log("              block %d px tall, floor %d, screen bottom %d"
        % (ta.symbols["BlockHeight"], ta.symbols["DHeight"], ta.symbols["ScrnBot"]))
    return layout


def convert_cutroom(root, outdir, log):
    """The room the cut scenes play in, if this copy carries it.

    Every scene between levels, and both endings, happen in the same place, so
    one picture covers all of them.  It is kept in the packed full-screen form
    rather than in the image tables, and lives with the tools rather than with
    the game's own artwork, so it may not be present in every copy.  If it is
    not, the scenes are skipped and nothing else changes.
    """
    import packpic
    found = None
    for base, dirs, files in os.walk(root):
        if "PAC.PROOM" in files:
            found = os.path.join(base, "PAC.PROOM")
            break
    if not found:
        log("  cutroom     not in this copy - the scenes will be skipped")
        return {}
    with open(found, "rb") as fh:
        data = fh.read()
    rows = packpic.to_rows(data, even=1, odd=2, solid=3, off=0)
    import sheets
    path = os.path.join(outdir, "cutroom.bmp")
    # Written with the default palette, like the sheets.  The board matches a
    # loaded image's colours against that palette to recover the pixel values,
    # so a file that carries the game's own colours instead comes back with
    # every value changed.  The values are what matter: the game's palette is
    # live when the picture is shown, so value 1 is stone whatever the file
    # says it looks like.
    size = sheets.write_bmp4(path, rows)
    log("  cutroom.bmp %d x %d, %d bytes (from %d packed)"
        % (len(rows[0]), len(rows), size, len(data)))
    return {}


def convert_sounds(src, outdir, log):
    """The twenty sound effects, as a frequency and a length each."""
    import sounds
    fx = sounds.read_effects(os.path.join(src, "Source", "SOUND.S"))
    with open(os.path.join(outdir, "sounds.dat"), "wb") as fh:
        fh.write(sounds.pack(fx))
    named = list(zip(sounds.EFFECTS, fx))
    log("  sounds.dat  %d effects, %d bytes" % (len(fx), len(fx) * 4))
    for name, (f, ms) in named:
        log("              %-16s %5d Hz %4d ms" % (name[2:], f, ms))
    return {"sounds_count": len(fx)}


def convert_levels(src, outdir, log):
    out = bytearray()
    guards = 0
    for n in range(LEVEL_COUNT):
        path = os.path.join(src, "Levels", "LEVEL%d" % n)
        if not os.path.exists(path):
            raise SystemExit("missing %s" % path)
        lvl = blueprint.read_level(path, n)
        out += lvl.raw
        guards += len(lvl.guards())
    path = os.path.join(outdir, "levels.dat")
    with open(path, "wb") as fh:
        fh.write(out)
    log("  levels.dat  %d levels, %d bytes, %d guards placed"
        % (LEVEL_COUNT, len(out), guards))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--source", required=True,
                    help="path to your own clone of the published source release")
    ap.add_argument("--out", required=True, help="directory to write the data files into")
    ap.add_argument("--set", choices=sorted(SETS), default="game", dest="which",
                    help="'game' (default) leaves out the intro and ending artwork "
                         "and fits four image slots; 'full' converts every table")
    args = ap.parse_args(argv)

    src = find_source(os.path.abspath(args.source))
    outdir = os.path.abspath(args.out)
    os.makedirs(outdir, exist_ok=True)

    lines = []

    def log(msg):
        lines.append(msg)
        print(msg)

    log("source : %s" % src)
    log("output : %s" % outdir)
    log("set    : %s" % args.which)
    log("")
    log("artwork")
    _stats, _grids, _places, artmeta = convert_images(src, outdir, log,
                                                      SETS[args.which])
    log("")
    log("animation tables")
    layout = convert_tables(src, outdir, log)
    layout.update(artmeta)
    log("")
    log("block data")
    layout.update(convert_blocks(src, outdir, log))
    layout.update(convert_sounds(src, outdir, log))
    layout.update(convert_cutroom(os.path.abspath(args.source), outdir, log))
    log("")
    log("levels")
    convert_levels(src, outdir, log)

    # Publish the palette alongside the layout: the engine sets these
    # sixteen entries at startup, and they are what let the walls, the
    # player and his opponents be told apart.
    for slot in range(16):
        layout["pal_%d" % slot] = PALETTE.get(slot, 0)

    with open(os.path.join(outdir, "tables.idx"), "w", newline="\n") as fh:
        fh.write("# layout of frames.dat, seq.dat and blocks.dat, "
                 "all offsets in bytes\n")
        for key in sorted(layout):
            fh.write("%s %d\n" % (key, layout[key]))
    log("")
    log("  tables.idx  %d values" % len(layout))

    with open(os.path.join(outdir, "convert.log"), "w", newline="\n") as fh:
        fh.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
