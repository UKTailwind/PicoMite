"""blueprint.py - read the level files.

The layout is the one the released EQ.S declares for the blueprint buffer, and
the files match it exactly with nothing left over:

    offset  size  field
         0   720  block type, 24 screens of 30 blocks (10 across, 3 down)
       720   720  block modifier, same shape
      1440   256  link locations
      1696   256  link map
      1952    96  screen map, 24 entries of 4
      2048   256  starting state

Within the starting state, again from EQ.S: 64 bytes reserved, then the start
screen, block and facing for the player, a spare, the sword's screen and block,
a spare, and then six arrays of 24 bytes giving each guard's block, facing, x
position, sequence low byte, behaviour program and sequence high byte.

Nothing here is game data.  It is a description of where the fields sit.
"""

LEVEL_BYTES = 2304
SCREENS = 24
BLOCKS_PER_SCREEN = 30
BLOCKS_ACROSS = 10
BLOCKS_DOWN = 3
MAX_GUARDS = 24

OFF_TYPE = 0
OFF_SPEC = 720
OFF_LINKLOC = 1440
OFF_LINKMAP = 1696
OFF_MAP = 1952
OFF_INFO = 2048

# Offsets inside the 256-byte starting-state block.
INFO_KID_SCREEN = 64
INFO_KID_BLOCK = 65
INFO_KID_FACE = 66
INFO_SWORD_SCREEN = 68
INFO_SWORD_BLOCK = 69
INFO_GUARD_BLOCK = 71          # six arrays of MAX_GUARDS from here
INFO_GUARD_FACE = 95
INFO_GUARD_X = 119
INFO_GUARD_SEQ_LO = 143
INFO_GUARD_PROG = 167
INFO_GUARD_SEQ_HI = 191

# A guard slot holding this block number is empty.
NO_GUARD = 30

TYPE_MASK = 0x1F               # the low five bits are the block type


class Level:
    __slots__ = ("number", "raw")

    def __init__(self, number, raw):
        self.number = number
        self.raw = raw

    def block_type(self, screen, block):
        return self.raw[OFF_TYPE + screen * BLOCKS_PER_SCREEN + block] & TYPE_MASK

    def block_flags(self, screen, block):
        return self.raw[OFF_TYPE + screen * BLOCKS_PER_SCREEN + block] & ~TYPE_MASK

    def block_spec(self, screen, block):
        return self.raw[OFF_SPEC + screen * BLOCKS_PER_SCREEN + block]

    @property
    def info(self):
        return self.raw[OFF_INFO:OFF_INFO + 256]

    def guards(self):
        """Occupied guard slots, as dicts.  Most levels use only a few."""
        info = self.info
        out = []
        for i in range(MAX_GUARDS):
            block = info[INFO_GUARD_BLOCK + i]
            if block == NO_GUARD:
                continue
            out.append({
                "slot": i,
                "block": block,
                "face": info[INFO_GUARD_FACE + i],
                "x": info[INFO_GUARD_X + i],
                "prog": info[INFO_GUARD_PROG + i],
                "seq": info[INFO_GUARD_SEQ_LO + i] | (info[INFO_GUARD_SEQ_HI + i] << 8),
            })
        return out

    def start(self):
        info = self.info
        return {
            "screen": info[INFO_KID_SCREEN],
            "block": info[INFO_KID_BLOCK],
            "face": info[INFO_KID_FACE],
            "sword_screen": info[INFO_SWORD_SCREEN],
            "sword_block": info[INFO_SWORD_BLOCK],
        }


def read_level(path, number):
    with open(path, "rb") as fh:
        raw = fh.read()
    if len(raw) != LEVEL_BYTES:
        raise ValueError("%s: expected %d bytes, found %d"
                         % (path, LEVEL_BYTES, len(raw)))
    return Level(number, raw)
