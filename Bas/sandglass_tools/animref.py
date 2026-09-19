"""animref.py - host-side reference for the frame advance.

    python animref.py --data <output directory>

Phase 2 validated the sequence walk; this validates what the sequences *do* to a
character.  One frame advance runs the byte code until it reaches a frame
number, updating the character on the way, and that frame number becomes the
pose.  The engine on the board does the same thing, and the two are compared by
running every sequence for a fixed number of frames and checksumming the states
that come out.

Two things worth keeping straight, both taken from the dispatcher rather than
guessed:

  * `chx` moves the character and is adjusted for facing; `chy` is not.
  * The frame table's own displacements are *drawing* offsets, applied when the
    sprite is positioned.  They do not move the character.  Only `chx` and `chy`
    do that.  Conflating the two gives movement that is subtly wrong everywhere.

Facing is -1 for left, which is how the artwork is drawn, and 0 for right, which
is mirrored.  Moving forward while facing left decreases x.
"""

import argparse
import os
import sys

# Opcode, operand count, from the dispatcher in COLL.S.  jaru takes none:
# the walk in Phase 2 could not tell, since 'jaru,79' parses either way.
CHX, CHY, ABOUTFACE, GOTO = 0xFB, 0xFA, 0xFE, 0xFF
UP, DOWN, ACT, SETFALL = 0xFD, 0xFC, 0xF9, 0xF8
IFWTLESS, DIE, JARU, JARD = 0xF7, 0xF6, 0xF5, 0xF4
EFFECT, TAP, NEXTLEVEL = 0xF3, 0xF2, 0xF1
OP_LOW = 0xF1

FRAME_BYTES = 5
GUARD = 400          # a frame advance that takes this many steps is broken


def sgn8(v):
    return v - 256 if v > 127 else v


class Char:
    """The fifteen-byte character record, as the original lays it out."""

    __slots__ = ("posn", "x", "y", "face", "blockx", "blocky", "action",
                 "xvel", "yvel", "seq", "scrn", "repeat", "cid", "sword", "life")

    def __init__(self, seq, x=100, y=100, face=0xFF):
        self.posn = 0
        self.x = x
        self.y = y
        self.face = face          # 0xFF is left, 0 is right
        self.blockx = 0
        self.blocky = 1
        self.action = 0
        self.xvel = 0
        self.yvel = 0
        self.seq = seq
        self.scrn = 1
        self.repeat = 0
        self.cid = 0
        self.sword = 0
        self.life = 0

    def state(self):
        return (self.posn, self.x, self.y, self.face, self.blocky,
                self.action, self.xvel, self.yvel, self.seq)


def advance(seq, ch, weightless=0):
    """One frame advance.  True if it reached a frame, False if it ran away."""
    for _ in range(GUARD):
        if ch.seq < 0 or ch.seq >= len(seq):
            return False
        op = seq[ch.seq]
        ch.seq += 1

        if op < OP_LOW:                       # a frame number: the pose
            ch.posn = op
            return True

        if op == CHX:
            d = sgn8(seq[ch.seq]); ch.seq += 1
            ch.x = (ch.x + (-d if ch.face & 0x80 else d)) & 0xFF
        elif op == CHY:
            d = sgn8(seq[ch.seq]); ch.seq += 1
            ch.y = (ch.y + d) & 0xFF
        elif op == ABOUTFACE:
            ch.face ^= 0xFF
        elif op == GOTO:
            ch.seq = seq[ch.seq] | (seq[ch.seq + 1] << 8)
        elif op == UP:
            ch.blocky = (ch.blocky - 1) & 0xFF
        elif op == DOWN:
            ch.blocky = (ch.blocky + 1) & 0xFF
        elif op == ACT:
            ch.action = seq[ch.seq]; ch.seq += 1
        elif op == SETFALL:
            ch.xvel = seq[ch.seq]; ch.yvel = seq[ch.seq + 1]; ch.seq += 2
        elif op == IFWTLESS:
            if weightless:
                ch.seq = seq[ch.seq] | (seq[ch.seq + 1] << 8)
            else:
                ch.seq += 2
        elif op in (DIE, JARD, JARU, NEXTLEVEL):
            pass                      # jaru/jard set a flag and take no operand
        elif op in (EFFECT, TAP):
            ch.seq += 1
        else:
            return False
    return False


def draw_position(ch, frames, scrn_left=58, scrn_top=0):
    """Where the sprite goes for the current pose.

    The frame's own displacement is applied here and nowhere else, facing
    adjusted in x exactly as the character's own movement is.  The result is
    then doubled, which is how the 140-unit character space becomes 280 pixels.
    """
    # Row 0 of the table is frame 1: the original subtracts one, then x5.
    if ch.posn < 1:
        return None
    f = (ch.posn - 1) * FRAME_BYTES
    if f + FRAME_BYTES > len(frames):
        return None
    image = frames[f]
    dx = sgn8(frames[f + 2])
    dy = sgn8(frames[f + 3])
    x = (ch.x + (-dx if ch.face & 0x80 else dx)) & 0xFF
    return image, ((x - scrn_left) & 0xFF) * 2, (ch.y + dy - scrn_top) & 0xFF


# Block types that are not a floor you can stand on.  A solid block is in the
# list because it is handled separately: you cannot stand on it, you are pushed
# out to one side of it.
NO_FLOOR = {0, 9, 12, 20, 26, 27, 28, 29}

# Action classes, from the floor test in the original.
ACT_ONGROUND = (0, 1, 7)
ACT_HANG = (2, 6)
ACT_FALLING = 4
ACT_BUMPED = 5
ACT_THREE = 3

BLOCKS_ACROSS = 10
ROWS = 3


def block_at(level, scrn, bx, by):
    """The block type at a cell, or empty off the edge.

    Reaching into the neighbouring screen is screen linking and is not done
    yet, so off-screen reads are empty here as they are in the renderer."""
    if bx < 0 or bx >= BLOCKS_ACROSS or by < 0 or by >= ROWS:
        return 0
    return level[scrn * 30 + by * BLOCKS_ACROSS + bx] & 0x1F


def check_floor(ch, level, floory):
    """Decide whether the character is standing, falling, or going through a
    floor.  Returns a short word for what happened, so a test can compare.

    Y grows downwards, so having reached the floor plane means Y is at or past
    the floor line for the row below."""
    if ch.action in ACT_HANG:
        return "hang"
    if ch.action == ACT_BUMPED:
        if ch.posn in (109, 185):
            return "ground"
        return "bumped"
    if ch.action == ACT_THREE:
        if 102 <= ch.posn <= 105:
            return "fallon"
        return "other"
    if ch.action == ACT_FALLING:
        idx = ch.blocky + 1
        if idx < 0 or idx >= len(floory):
            return "offmap"
        if ch.y < floory[idx]:
            return "fallon"                    # not down to the floor yet
        t = block_at(level, ch.scrn, ch.blockx, ch.blocky)
        if t == 20:
            return "inblock"                   # pushed out to one side
        if t not in NO_FLOOR:
            ch.y = floory[idx]                 # land, aligned to the floor
            return "land"
        ch.blocky = (ch.blocky + 1) & 0xFF     # through the floor plane
        return "through"
    return "ground"


def run_floor(data):
    """Run the floor test over every cell of every screen of a real level, at a
    spread of heights, and checksum what comes out."""
    with open(os.path.join(data, "levels.dat"), "rb") as fh:
        levels = fh.read()
    with open(os.path.join(data, "blocks.dat"), "rb") as fh:
        blocks = fh.read()
    layout = {}
    with open(os.path.join(data, "tables.idx")) as fh:
        for line in fh:
            line = line.strip()
            if line and not line.startswith("#"):
                k, v = line.split()
                layout[k] = int(v)

    base = layout["geom_FloorY"]
    floory = [blocks[base + i] for i in range(5)]

    names = ["hang", "bumped", "other", "fallon", "land", "through",
             "inblock", "ground", "offmap"]
    counts = dict((n, 0) for n in names)
    total = 0
    level = levels[0:2304]                     # level 0
    for scrn in range(24):
        for by in range(ROWS):
            for bx in range(BLOCKS_ACROSS):
                for dy in (-20, -1, 0, 1, 40):
                    ch = Char(0)
                    ch.scrn = scrn
                    ch.blockx = bx
                    ch.blocky = by
                    ch.action = ACT_FALLING
                    ch.y = (floory[by + 1] + dy) & 0xFF
                    what = check_floor(ch, level, floory)
                    counts[what] += 1
                    for v in (ch.y, ch.blocky, names.index(what)):
                        total = (total * 31 + v) & 0xFFFFFF
    return counts, total, floory


# How hard he hits.  Below the first threshold it costs nothing, below the
# second it hurts, at or above it kills.
OOF_VELOCITY = 22
DEATH_VELOCITY = 33
SPIKES = 2

LAND_SOFT, LAND_MED, LAND_HARD, LAND_IMPALE = 0, 1, 2, 3


def landing(yvel, underfoot, alive, spikes_lethal):
    """What a fall ends in.

    Order matters: being dead already, and landing on live spikes, both decide
    the outcome before the velocity is looked at."""
    if underfoot == SPIKES and spikes_lethal:
        return LAND_IMPALE
    if not alive:
        return LAND_HARD              # dead before he hit the ground
    if yvel < OOF_VELOCITY:
        return LAND_SOFT
    if yvel < DEATH_VELOCITY:
        return LAND_MED
    return LAND_HARD


# Reaching for a ledge on the way down.
GRAB_SPEED = 32      # too fast above this to catch anything
GRAB_LEAD = 25       # how far ahead of the floor line the reach begins
GRAB_REACH = -8      # how far forward he stretches, facing adjusted

GRAB_NO_BUTTON, GRAB_DEAD, GRAB_TOO_FAST, GRAB_TOO_HIGH, GRAB_REACHES = 0, 1, 2, 3, 4


def grab_gates(button, alive, yvel, y, floor_line):
    """The three gates before a ledge is even looked for.

    Order matters here too: a dead character never reaches, and speed is
    checked before position, so a fast fall is refused whatever the height."""
    if not button:
        return GRAB_NO_BUTTON
    if not alive:
        return GRAB_DEAD
    if yvel >= GRAB_SPEED:
        return GRAB_TOO_FAST
    if y + GRAB_LEAD < floor_line:
        return GRAB_TOO_HIGH
    return GRAB_REACHES


def run_grab():
    names = ["nobutton", "dead", "toofast", "toohigh", "reaches"]
    counts = dict((n, 0) for n in names)
    total = 0
    floor_line = 118
    for button in (1, 0):
        for alive in (1, 0):
            for yvel in range(0, 48, 2):
                for y in range(60, 140, 2):
                    r = grab_gates(button, alive, yvel, y, floor_line)
                    counts[names[r]] += 1
                    total = (total * 31 + r) & 0xFFFFFF
    return counts, total


# Reaching for a ledge: what makes one catchable.
#
# Clear air above him, and a solid floor above and in front - the exposed edge
# he catches.  Three exceptions make it fiddlier than that sounds: a panel
# without a floor counts as clear in general but not when he reaches the way its
# floorpiece faces, a panel with a floor can only be caught from one side, and a
# floor that has already sprung loose cannot be caught at all.
SOLID_BLOCK = 20
PANEL_NO_FLOOR = 12
PANEL_WITH_FLOOR = 7


def can_grab(above, aboveinf, facing_left):
    if above == SOLID_BLOCK:
        return False
    if above == PANEL_NO_FLOOR and not facing_left:
        return False
    if above not in NO_FLOOR:
        return False
    if aboveinf in NO_FLOOR:
        return False
    if aboveinf == PANEL_WITH_FLOOR and facing_left:
        return False
    return True


def run_grabrule():
    """Every combination of the two blocks and both facings."""
    caught = refused = 0
    total = 0
    for above in range(30):
        for aboveinf in range(30):
            for facing_left in (True, False):
                r = can_grab(above, aboveinf, facing_left)
                if r:
                    caught += 1
                else:
                    refused += 1
                total = (total * 31 + (1 if r else 0)) & 0xFFFFFF
    return caught, refused, total


def run_landing():
    names = ["soft", "med", "hard", "impale"]
    counts = dict((n, 0) for n in names)
    total = 0
    for underfoot in range(30):
        for yvel in range(41):
            for alive in (1, 0):
                for lethal in (1, 0):
                    r = landing(yvel, underfoot, alive, lethal)
                    counts[names[r]] += 1
                    total = (total * 31 + r) & 0xFFFFFF
    return counts, total


def run(data, frames_per_seq=24):
    with open(os.path.join(data, "seq.dat"), "rb") as fh:
        seq = fh.read()
    with open(os.path.join(data, "frames.dat"), "rb") as fh:
        frames = fh.read()
    layout = {}
    with open(os.path.join(data, "tables.idx")) as fh:
        for line in fh:
            line = line.strip()
            if line and not line.startswith("#"):
                k, v = line.split()
                layout[k] = int(v)

    count = layout["seq_count"]
    table = [seq[i * 2] | (seq[i * 2 + 1] << 8) for i in range(count)]

    total = 0
    advances = 0
    stalled = 0
    for i, start in enumerate(table):
        ch = Char(start)
        for _ in range(frames_per_seq):
            if not advance(seq, ch):
                stalled += 1
                break
            advances += 1
            for v in ch.state():
                total = (total * 31 + v) & 0xFFFFFF
    return advances, stalled, total


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data", required=True)
    ap.add_argument("--frames", type=int, default=24)
    ap.add_argument("--mode", choices=("advance", "floor", "land", "grab", "grabrule"),
                    default="advance")
    args = ap.parse_args(argv)
    data = os.path.abspath(args.data)

    if args.mode == "grabrule":
        caught, refused, total = run_grabrule()
        print("  catchable  %6d" % caught)
        print("  refused    %6d" % refused)
        print("checksum       %d" % total)
        return 0

    if args.mode == "grab":
        counts, total = run_grab()
        for name in ("nobutton", "dead", "toofast", "toohigh", "reaches"):
            print("  %-9s %6d" % (name, counts[name]))
        print("checksum       %d" % total)
        return 0

    if args.mode == "land":
        counts, total = run_landing()
        for name in ("soft", "med", "hard", "impale"):
            print("  %-7s %6d" % (name, counts[name]))
        print("checksum       %d" % total)
        return 0

    if args.mode == "floor":
        counts, total, floory = run_floor(data)
        print("floor lines    %s" % " ".join(str(v) for v in floory))
        for name in sorted(counts):
            if counts[name]:
                print("  %-9s %6d" % (name, counts[name]))
        print("checksum       %d" % total)
        return 0

    advances, stalled, total = run(data, args.frames)
    print("frame advances %d" % advances)
    print("stalled        %d" % stalled)
    print("checksum       %d" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
