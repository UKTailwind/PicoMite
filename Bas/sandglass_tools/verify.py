"""verify.py - walk the converted data and check it holds together.

    python verify.py --data <output directory>

The useful one is the sequence walk.  Every animation in the game is a short
byte-code program, and the engine will run these exact bytes, so walking all of
them here proves three things at once before any BASIC is written: that the
assembler read the module correctly, that every address was rebased to a valid
offset, and that the opcode operand counts are right.  If any of those were
wrong the walk would step into the middle of an instruction and run off the end.

Operand counts come from the dispatcher in COLL.S, not from how a sequence
looks: "jaru,79" is a flag followed by frame 79, not an opcode with an operand.
Anything that is not an opcode is a frame number and takes no operands.
"""

import argparse
import os
import sys

# Opcodes are small negative numbers, so they arrive as high byte values.
OPCODES = {
    0xFF: ("goto", 2),        # target address
    0xFE: ("aboutface", 0),
    0xFD: ("up", 0),
    0xFC: ("down", 0),
    0xFB: ("chx", 1),
    0xFA: ("chy", 1),
    0xF9: ("act", 1),
    0xF8: ("setfall", 2),
    0xF7: ("ifwtless", 2),    # conditional, target address
    0xF6: ("die", 0),
    0xF5: ("jaru", 0),        # sets a flag; the byte after it is a frame
    0xF4: ("jard", 0),
    0xF3: ("effect", 1),
    0xF2: ("tap", 1),
    0xF1: ("nextlevel", 0),
}

JUMPS = {0xFF, 0xF7}
STOPS = {0xF6, 0xF1}          # die, nextlevel


def read_layout(path):
    values = {}
    with open(path) as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            key, val = line.split()
            values[key] = int(val)
    return values


def walk(seq, start, limit):
    """Follow one sequence until it loops, stops or leaves the file.

    Returns (steps, frames, error or None).
    """
    pc = start
    seen = set()
    steps = 0
    frames = 0
    while True:
        if pc < 0 or pc >= len(seq):
            return steps, frames, "ran off the end at offset %d" % pc
        if pc in seen:
            return steps, frames, None          # a cycle: normal for an idle loop
        seen.add(pc)
        op = seq[pc]
        steps += 1
        if op not in OPCODES:
            frames += 1
            pc += 1
            continue
        name, operands = OPCODES[op]
        if pc + 1 + operands > len(seq):
            return steps, frames, "%s at %d needs %d operands past the end" % (
                name, pc, operands)
        if op in JUMPS:
            target = seq[pc + 1] | (seq[pc + 2] << 8)
            if target >= limit:
                return steps, frames, "%s at %d targets %d, outside the file" % (
                    name, pc, target)
            if op == 0xFF:
                pc = target
                continue
            pc += 3                              # conditional: fall through too
            continue
        if op in STOPS:
            return steps, frames, None
        pc += 1 + operands


PIECE_SECTIONS = ["piecea", "pieceb", "piecec", "pieced", "fronti"]
BG_SPLIT = 0x80


def check_blocks(d, layout):
    """Check every block type resolves to real images, and work out how much
    drawing a screen costs.  The busiest screen sets the frame budget, so it is
    worth knowing before the renderer is tuned."""
    path = os.path.join(d, "blocks.dat")
    if not os.path.exists(path):
        return 0
    with open(path, "rb") as fh:
        blocks = fh.read()
    with open(os.path.join(d, "levels.dat"), "rb") as fh:
        levels = fh.read()

    types = layout["block_types"]
    bg1 = layout.get("img_BGTAB1DUN", 0)
    bg2 = layout.get("img_BGTAB2DUN", 0)

    problems = 0
    sections = {}
    for name in PIECE_SECTIONS:
        base = layout["block_" + name]
        table = blocks[base:base + types]
        sections[name] = table
        if bg1 and bg2:
            for t, v in enumerate(table):
                if v == 0:
                    continue
                if v < BG_SPLIT and v > bg1:
                    print("block %d %s names image %d, past the %d in table 1"
                          % (t, name, v, bg1))
                    problems += 1
                elif v >= BG_SPLIT and (v - BG_SPLIT) > bg2:
                    print("block %d %s names image %d in table 2, which has %d"
                          % (t, name, v - BG_SPLIT, bg2))
                    problems += 1

    # How many sections a block type draws, then how many a screen draws.
    per_type = [sum(1 for name in PIECE_SECTIONS if sections[name][t]) for t in range(types)]

    counts = []
    used_types = set()
    for lv in range(len(levels) // 2304):
        base = lv * 2304
        for scr in range(24):
            total = 0
            for b in range(30):
                t = levels[base + scr * 30 + b] & 0x1F
                if t < types:
                    used_types.add(t)
                    total += per_type[t]
            counts.append(total)

    counts.sort()
    print("blocks      %d types, %d ever used; a type draws %d sections at most"
          % (types, len(used_types), max(per_type)))
    print("screens     %d across all levels: busiest %d sections, median %d, mean %d"
          % (len(counts), counts[-1], counts[len(counts) // 2],
             sum(counts) / len(counts)))
    return problems


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data", required=True, help="the converter's output directory")
    args = ap.parse_args(argv)
    d = os.path.abspath(args.data)

    layout = read_layout(os.path.join(d, "tables.idx"))
    with open(os.path.join(d, "seq.dat"), "rb") as fh:
        seq = fh.read()
    with open(os.path.join(d, "frames.dat"), "rb") as fh:
        frames = fh.read()

    problems = 0

    # --- sizes ---------------------------------------------------------------
    if len(seq) != layout["seq_len"]:
        print("seq.dat is %d bytes, tables.idx says %d" % (len(seq), layout["seq_len"]))
        problems += 1
    if len(frames) != layout["frames_len"]:
        print("frames.dat is %d bytes, tables.idx says %d"
              % (len(frames), layout["frames_len"]))
        problems += 1

    # --- the sequence walk ---------------------------------------------------
    count = layout["seq_count"]
    table = [seq[i * 2] | (seq[i * 2 + 1] << 8) for i in range(count)]
    bad_entry = [i + 1 for i, p in enumerate(table)
                 if not (count * 2 <= p < len(seq))]
    if bad_entry:
        print("sequence entries pointing outside the data: %s" % bad_entry[:10])
        problems += len(bad_entry)

    walked = 0
    total_steps = 0
    total_frames = 0
    for i, start in enumerate(table):
        if i + 1 in bad_entry:
            continue
        steps, frames_seen, err = walk(seq, start, len(seq))
        total_steps += steps
        total_frames += frames_seen
        walked += 1
        if err:
            print("sequence %d (offset %d): %s" % (i + 1, start, err))
            problems += 1

    print("sequences   %d of %d walked, %d instructions, %d frame references"
          % (walked, count, total_steps, total_frames))

    # --- the frame table -----------------------------------------------------
    entry = layout["frames_entry"]
    main_count = layout["frames_count"]
    used = set()
    for n in range(main_count):            # row n is frame n+1
        used.add(frames[n * entry])
    print("frame table %d entries of %d bytes, %d distinct images referenced"
          % (main_count, entry, len(used)))

    # --- blocks and the drawing load ----------------------------------------
    problems += check_blocks(d, layout)

    print()
    if problems:
        print("FAILED: %d problem(s)" % problems)
        return 1
    print("OK: every sequence walks cleanly and every address is in range")
    return 0


if __name__ == "__main__":
    sys.exit(main())
