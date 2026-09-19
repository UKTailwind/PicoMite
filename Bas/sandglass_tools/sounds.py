"""sounds.py - turn the original's sound effects into frequencies and lengths.

The effects are not recordings.  Each one is a short routine that drives the
Apple's one-bit speaker through a common tone helper: a pitch, which sets how
long the code dawdles between flipping the speaker, and a count of flips.  So
every effect reduces to a frequency and a duration, and those two numbers port
to any machine that can make a tone.

Turning the pitch into a frequency needs the machine it ran on.  The tone
helper's inner loop is three instructions, about eight cycles, and it runs
`pitch` times between flips; the processor runs at 1.023 MHz.  One flip is half
a cycle of the wave, so

    frequency = 1023000 / (2 * 8 * pitch)

and the duration is the flip count times half that period.

**The one deliberate difference.**  Those durations come out between half a
millisecond and twenty-five, because a click is all the hardware could really
manage.  Played literally on a machine with a proper sound chip most of them
would be inaudible, so a floor is put under the duration.  The pitches, and the
relative lengths above the floor, are the original's.

This reads only the file the caller supplies and contains no data of its own.
"""

import re

# The twenty effects, in the order the original numbers them.  The names are
# the labels its sound module uses.
EFFECTS = [
    "DoPlateDown", "DoPlateUp", "DoGateDown", "DoSpecialKey1", "DoSpecialKey2",
    "DoSplat", "DoMirrorCrack", "DoLooseCrash", "DoGotKey", "DoFootstep",
    "DoRaisingExit", "DoRaisingGate", "DoLowerGate", "DoSmackWall",
    "DoImpaled", "DoGateSlam", "DoFlashMsg", "DoSwordClash1", "DoSwordClash2",
    "DoJawsClash",
]

CPU_HZ = 1023000.0
CYCLES_PER_INNER = 8
MIN_MS = 30          # the floor described above
MAX_MS = 400

_EQUATE = re.compile(r"^(\]?[A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+?)\s*(?:;.*)?$")
_LABEL = re.compile(r"^(\]?[A-Za-z_][A-Za-z0-9_]*)\s*$")
_IMM = re.compile(r"^\s+(ld[axy])\s+#(>?)(\S+)", re.I)
_JMP = re.compile(r"^\s+(jmp|jsr)\s+(\S+)", re.I)


def _value(text, equates):
    text = text.strip()
    if text.startswith("$"):
        return int(text[1:], 16)
    if text.startswith("%"):
        return int(text[1:], 2)
    if text.lstrip("-").isdigit():
        return int(text)
    return equates.get(text, 0)


def read_effects(path):
    """Read the sound module and return, per effect, a (frequency, ms) pair.

    Returns a list of twenty pairs in the original's own numbering.
    """
    with open(path, "r", encoding="latin-1") as fh:
        lines = [l.rstrip("\r\n") for l in fh]

    # Two passes: the equates first, since a routine may use one declared after
    # it, then the routines themselves.
    equates = {}
    for line in lines:
        m = _EQUATE.match(line)
        if m and not line[0].isspace():
            equates[m.group(1)] = _value(m.group(2), equates)
        elif m:
            equates[m.group(1)] = _value(m.group(2), equates)

    # Walk the file collecting each label's tone parameters.  Labels that sit
    # on top of one another share the body that follows, and a routine that
    # jumps straight to another borrows its sound.
    bodies = {}          # label -> (pitch, count) or ("alias", other)
    pending = []
    lo = hi = dur = None
    for line in lines:
        if not line.strip() or line.lstrip().startswith(("*", ";")):
            continue
        m = _LABEL.match(line)
        if m and not line[0].isspace():
            pending.append(m.group(1))
            lo = hi = dur = None
            continue
        if _EQUATE.match(line):
            continue
        m = _IMM.match(line)
        if m:
            op, high, arg = m.group(1).lower(), m.group(2), m.group(3)
            v = _value(arg, equates)
            v = (v >> 8) if high else (v & 0xFF)
            if op == "ldy":
                lo = v
            elif op == "ldx":
                hi = v
            else:
                dur = v
            continue
        m = _JMP.match(line)
        if m:
            target = m.group(2)
            if target == "tone" and pending and lo is not None:
                pitch = lo + 256 * (hi or 0)
                for name in pending:
                    bodies.setdefault(name, (pitch, dur if dur else 1))
                pending = []
            elif target.startswith("Do") and pending:
                for name in pending:
                    bodies.setdefault(name, ("alias", target))
                pending = []
            continue

    out = []
    for name in EFFECTS:
        body = bodies.get(name)
        seen = set()
        while body and body[0] == "alias" and body[1] not in seen:
            seen.add(body[1])
            body = bodies.get(body[1])
        if not body or body[0] == "alias":
            out.append((0, 0))
            continue
        pitch, count = body
        if pitch < 1:
            pitch = 1
        freq = CPU_HZ / (2.0 * CYCLES_PER_INNER * pitch)
        half_ms = (CYCLES_PER_INNER * pitch / CPU_HZ) * 1000.0
        ms = count * half_ms
        if ms < MIN_MS:
            ms = MIN_MS
        if ms > MAX_MS:
            ms = MAX_MS
        out.append((int(round(freq)), int(round(ms))))
    return out


def pack(effects):
    """Two little-endian words an effect: frequency in hertz, length in ms."""
    import struct
    data = bytearray()
    for freq, ms in effects:
        data += struct.pack("<HH", min(freq, 20000), min(ms, 5000))
    return bytes(data)
