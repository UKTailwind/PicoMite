"""make_doppler_mod.py [OUT.mod]: doppler.mod for doppler.bas, the PLAY MODSAMPLE demo. Everything in it is made here:
a short tune in A minor (Am F C G) on four channels from synthesised samples, kept quieter than the effects, and two
looped effect samples for PLAY MODSAMPLE ..., LOOP and PLAY MODSAMPLE CHANGE.

  1 bass   a band-limited sawtooth, one 64-byte cycle, looped     (C-3 plays it at about 259 Hz)
  2 lead   a band-limited pulse wave, one 64-byte cycle, looped
  3 kick   a sine swept from 150 to 45 Hz, decaying               (made at 16574 Hz: play it at C-3)
  4 snare  noise and a 180 Hz tone, decaying
  5 hihat  high-passed noise, a short decay
 10 doppler engine  a four-cylinder engine at 16000 (the default rate): firing pulses at 200 Hz in a 50 Hz cycle
 11 doppler horn    a two-note car horn at 16000: 420 Hz and 500 Hz with six harmonics each

The music's samples peak at about 70% and play at volumes of 10 to 30, so that an effect at volume 64 stands out.
The song is patterns 0 (bass and drums), 1 and 2 (the tune, two ways), at speed 5: 6.4 s a pattern."""
import math, random, struct, sys

DRUM_RATE = 16574                               # C-3's rate: a drum made at it sounds as made when played at C-3
FX_RATE = 16000                                 # doppler.bas's rate for the effects
rnd = random.Random(1924)


def s8(xs, peak):
    m = max(abs(x) for x in xs) or 1.0
    return bytes((int(round(x / m * peak)) & 0xFF) for x in xs)


def cycle(harm, n=64):                          # one cycle of sum(harm[k] * sin(k...)), k from 1
    return [sum(a * math.sin(2 * math.pi * (k + 1) * i / n) for k, a in enumerate(harm)) for i in range(n)]


def bass():
    return s8(cycle([1.0 / k * (0.92 ** k) for k in range(1, 13)]), 90)


def lead():
    # a 25% pulse: harmonic k has amplitude sin(pi k / 4) / k
    return s8(cycle([math.sin(math.pi * k * 0.25) / k for k in range(1, 11)]), 80)


def kick():
    n, out, ph = 4000, [], 0.0
    for i in range(n):
        t = i / DRUM_RATE
        f = 45 + 105 * math.exp(-t / 0.04)
        ph += 2 * math.pi * f / DRUM_RATE
        out.append(math.sin(ph) * math.exp(-t / 0.09))
    return s8(out, 100)


def snare():
    n, out, y = 3000, [], 0.0
    for i in range(n):
        t = i / DRUM_RATE
        y += 0.55 * (rnd.uniform(-1, 1) - y)
        out.append((0.8 * y + 0.5 * math.sin(2 * math.pi * 180 * t)) * math.exp(-t / 0.05))
    return s8(out, 90)


def hihat():
    n, out, prev = 800, [], 0.0
    for i in range(n):
        t = i / DRUM_RATE
        w = rnd.uniform(-1, 1)
        out.append((w - prev) * math.exp(-t / 0.012))
        prev = w
    return s8(out, 70)


def engine():
    cyc, cycles = 320, 12
    n = cyc * cycles
    amps = [1.0, 0.82, 0.93, 0.78]
    out = [0.0] * n
    for c in range(cycles):
        for k in range(4):
            start = c * cyc + k * 80
            a = amps[k] * (1 + rnd.uniform(-0.1, 0.1))
            fres = 520 * (1 + rnd.uniform(-0.04, 0.04))
            for t in range(80):
                out[start + t] += a * math.exp(-t / 11.0) * math.sin(2 * math.pi * fres * t / FX_RATE)
    for i in range(n):
        out[i] += 0.30 * math.sin(2 * math.pi * 50 * i / FX_RATE) + 0.22 * math.sin(2 * math.pi * 100 * i / FX_RATE + 0.7)
    noise = [rnd.uniform(-1, 1) for _ in range(n)]
    y, hiss = 0.0, [0.0] * n
    for _ in range(2):                          # a circular low-pass: the second pass starts where the first ended
        for i in range(n):
            y += 0.18 * (noise[i] - y)
            hiss[i] = y
    return s8([out[i] + 0.35 * hiss[i] for i in range(n)], 118)


def horn():
    n = 3200                                    # 0.2 s: whole cycles of 420 and 500 Hz, so the loop joins
    out = [sum(math.sin(2 * math.pi * f * h * i / FX_RATE) / h for f in (420, 500) for h in range(1, 7))
           for i in range(n)]
    loop = s8(out, 110)
    return loop[-48:] + loop, 48, len(loop)     # (a lead-in of the loop's end: the file is whole 128-byte blocks)


# ---- the tune
NOTES = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
PERIODS = [856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
           428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
           214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113]


def period(note):                               # "A-2" -> ProTracker's period, octaves 1 to 3
    name, octave = note[:-2].rstrip("-"), int(note[-1])
    return PERIODS[(octave - 1) * 12 + NOTES[name]]


def cell(note=None, sample=0, effect=0, param=0):
    p = period(note) if note else 0
    return bytes([(sample & 0xF0) | (p >> 8), p & 0xFF, ((sample & 0x0F) << 4) | effect, param])


SET_VOL, SET_SPEED = 0xC, 0xF
ROOTS = [("A", "A"), ("F", "F"), ("C", "C"), ("G", "G")]           # Am F C G, a bar (16 rows) each
BASS_OCT = {"A": (1, 2), "F": (1, 2), "C": (2, 3), "G": (1, 2)}


def pattern(melody, with_lead, first=False):
    rows = [[cell() for _ in range(4)] for _ in range(64)]
    for bar, (root, _) in enumerate(ROOTS):
        lo, hi = BASS_OCT[root]
        for r in range(0, 16, 2):               # the bass: eighths, root and octave
            note = "%s-%d" % (root, lo if r % 4 == 0 else hi)
            rows[bar * 16 + r][0] = cell(note, 1, SET_VOL, 30 if r % 4 == 0 else 22)
        for r in (0, 8, 10) if bar % 2 else (0, 8):                 # the kick, with a push in alternate bars
            rows[bar * 16 + r][2] = cell("C-3", 3, SET_VOL, 30)
        for r in (4, 12):                       # the snare on the backbeat
            rows[bar * 16 + r][2] = cell("C-3", 4, SET_VOL, 26)
        for r in range(0, 16, 2):               # the hi-hat: eighths, accented on the beat
            rows[bar * 16 + r][3] = cell("C-3", 5, SET_VOL, 16 if r % 4 == 0 else 10)
    if with_lead:
        for row, note, length in melody:
            rows[row][1] = cell(note, 2, SET_VOL, 22)
            end = row + length
            if end < 64 and not any(m[0] == end for m in melody):
                rows[end][1] = cell(None, 0, SET_VOL, 0)            # a rest: the looped note silenced
    if first:
        rows[0][3] = cell("C-3", 5, SET_VOL, 16)[:2] + bytes([(5 << 4) | SET_SPEED, 5])   # speed 5 (keeps the hat)
    return b"".join(b"".join(r) for r in rows)


def bars(*b):                                   # [(row in bar, note, length)] per bar -> rows in the pattern
    return [(i * 16 + r, n, l) for i, bar in enumerate(b) for r, n, l in bar]


MELODY_A = bars([(0, "A-2", 4), (4, "C-3", 2), (6, "E-3", 2), (8, "D-3", 4), (12, "C-3", 2), (14, "B-2", 2)],
                [(0, "A-2", 6), (6, "G-2", 2), (8, "F-2", 4), (12, "A-2", 2), (14, "C-3", 2)],
                [(0, "E-3", 6), (6, "D-3", 2), (8, "C-3", 2), (10, "D-3", 2), (12, "E-3", 2), (14, "G-3", 2)],
                [(0, "D-3", 8), (8, "B-2", 4), (12, "G-2", 3)])
MELODY_B = bars([(0, "E-3", 4), (4, "A-3", 4), (8, "G-3", 2), (10, "E-3", 2), (12, "D-3", 2), (14, "C-3", 2)],
                [(0, "C-3", 6), (6, "A-2", 2), (8, "C-3", 2), (10, "F-3", 6)],
                [(0, "E-3", 2), (2, "G-3", 2), (4, "E-3", 2), (6, "C-3", 2), (8, "D-3", 4), (12, "E-3", 4)],
                [(0, "D-3", 4), (4, "B-2", 4), (8, "D-3", 4), (12, "G-2", 3)])


def header(name, data, vol, rep=0, replen=None):
    nm = name.encode("ascii")[:22].ljust(22, b"\0")
    if replen is None:                          # one-shot: ProTracker's no-loop is a repeat of one word
        rp, rl = 0, 1
    else:
        rp, rl = rep // 2, replen // 2
    return nm + struct.pack(">HBBHH", len(data) // 2, 0, vol, rp, rl)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "doppler.mod"
    hrn, hrep, hlen = horn()
    smp = {1: ("bass", bass(), 40, 0, 64), 2: ("lead", lead(), 30, 0, 64), 3: ("kick", kick(), 40, 0, None),
           4: ("snare", snare(), 34, 0, None), 5: ("hihat", hihat(), 20, 0, None),
           10: ("doppler engine", engine(), 64, 0, 3840), 11: ("doppler horn", hrn, 64, hrep, hlen)}
    hdr = b"MODSAMPLE demo".ljust(20, b"\0")
    for slot in range(1, 32):
        if slot in smp:
            name, data, vol, rep, rl = smp[slot]
            hdr += header(name, data, vol, rep, rl)
        elif slot == 31:
            hdr += b"made for MMBasic".ljust(22, b"\0") + struct.pack(">HBBHH", 0, 0, 0, 0, 1)
        else:
            hdr += bytes(22) + struct.pack(">HBBHH", 0, 0, 0, 0, 1)
    order = [0, 1, 2]
    hdr += bytes([len(order), 127]) + bytes(order).ljust(128, b"\0") + b"M.K."
    pats = pattern([], False, first=True) + pattern(MELODY_A, True) + pattern(MELODY_B, True)
    data = b"".join(smp[s][1] for s in sorted(smp))
    mod = hdr + pats + data
    pad = (-len(mod)) % 128                     # whole 128-byte blocks (XMODEM pads): longer hi-hat tail of silence
    if pad:
        assert pad % 2 == 0
        hh = smp[5][1] + bytes(pad)
        smp[5] = ("hihat", hh, 20, 0, None)
        return main_write(out, smp, order)
    open(out, "wb").write(mod)
    print(out, len(mod), "bytes")


def main_write(out, smp, order):
    hdr = b"MODSAMPLE demo".ljust(20, b"\0")
    for slot in range(1, 32):
        if slot in smp:
            name, data, vol, rep, rl = smp[slot]
            hdr += header(name, data, vol, rep, rl)
        elif slot == 31:
            hdr += b"made for MMBasic".ljust(22, b"\0") + struct.pack(">HBBHH", 0, 0, 0, 0, 1)
        else:
            hdr += bytes(22) + struct.pack(">HBBHH", 0, 0, 0, 0, 1)
    hdr += bytes([len(order), 127]) + bytes(order).ljust(128, b"\0") + b"M.K."
    pats = pattern([], False, first=True) + pattern(MELODY_A, True) + pattern(MELODY_B, True)
    mod = hdr + pats + b"".join(smp[s][1] for s in sorted(smp))
    assert len(mod) % 128 == 0, len(mod)
    open(out, "wb").write(mod)
    print(out, len(mod), "bytes")


main()
