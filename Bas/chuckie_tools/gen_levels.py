"""Trace the eight Chuckie Egg floors out of the BBC screenshots in Bas/ and
emit them as MMBasic DATA lines.

Vertical fit
------------
The BBC screen is 256 rows: 32 for the status panel and 224 for the floor.
Ours is 240.  The whole 16 row difference comes out of the panel - the BBC
prints its two text rows in a 32 row band and our 6 x 8 font fits the same
two rows in 16 - so the playfield keeps its full 224 rows and every girder
gap, ladder run and jump is the original's, pixel for pixel.

The shots are 320 x 256, the BBC's 160-pixel screen doubled across, so one of
our 16 pixel map cells is 16 pixels there too and the columns map 1:1.
Girders sit 40 pixels apart at image y 82, 122, 162, 202 and 242; dropping
the panel puts them on rows 7, 12, 17, 22 and 27 of our 8 pixel grid, so
image y = 8 * row + 26.

The shots are mid-game, so Harry, the hens and the ladders hide bits of the
scenery.  Anything resting on a girder proves the girder is there, and a
ladder always meets a girder at both ends, so those gaps are filled back in.

Characters:  . empty   # girder   H ladder   + ladder through a girder
             E egg     S grain
             L lift shaft, one lift      K lift shaft, two lifts
             P Harry starts              1-4 a hen starts
"""
import glob
import os
import sys
from collections import deque

from PIL import Image

COLS, ROWS = 20, 28
# The five main floors, 40 pixels apart.  Levels 6 and 7 also carry short
# ledges on the rows between them, so girders are looked for on every row
# and PLAT is only a starting point for the ladder search.
PLAT = [7, 12, 17, 22, 27]
HUD_BOTTOM = 32
SHOTS = r'D:/Dropbox/PicoMite/PicoMite/Bas'
# which screenshot is which floor, read off the LEVEL field in the panel
LEVEL_OF = {'27bcf608': 4, '2965b440': 3, '2bde57f4': 2, '2c57c4cc': 9,
            '2c88944e': 8, '2cb89cf2': 6, '2ce8940c': 7, '2d1794dc': 1,
            '2deb5538': 5}
SOLID = '#+'

# How many hens patrol each floor, and where any extra ones belong.
#   2  the fourth hen works the upper ledges
#   5  the three of them are kept to the left hand side
HEN_COUNT = {1: 3, 2: 4, 3: 4, 4: 4, 5: 3, 6: 3, 7: 3, 8: 3}
HEN_BIAS = {2: 'top', 5: 'left'}
MAXHEN = 5                      # cycles 4 and 5 field five on every floor


def img_y(row):
    return 8 * row + 26


def settle(bottom_y):
    """Row for something resting on a girder, given its bottom image row.
    A girder is drawn centred on its row, so anything sitting on one has
    its last pixel a few rows above that."""
    r = int(round((bottom_y - 26) / 8.0))
    if 1 <= r < ROWS and abs(img_y(r) - bottom_y) <= 5:
        return r - 1
    return None


def classify(f):
    im = Image.open(f).convert('RGB')
    px = im.load()
    w, h = im.size
    tab = {}
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            if r < 70 and g < 70 and b < 70:
                c = '.'
            elif r > 150 and g > 150 and b > 150:
                c = 'W'
            elif g > 100 and r < 110 and b < 110:
                c = 'G'
            elif r > 110 and b > 110 and g < 110:
                c = 'M'
            elif r > 110 and g > 110 and b < 110:
                c = 'Y'
            elif g > 110 and b > 110 and r < 110:
                c = 'C'
            else:
                c = '.'
            tab[(x, y)] = c
    return tab, w, h


def blobs(tab, w, h, want, y0=0):
    seen, out = set(), []
    for y in range(y0, h):
        for x in range(w):
            if tab[(x, y)] != want or (x, y) in seen:
                continue
            q = deque([(x, y)])
            seen.add((x, y))
            cell = []
            while q:
                cx, cy = q.popleft()
                cell.append((cx, cy))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        nx, ny = cx + dx, cy + dy
                        if (0 <= nx < w and y0 <= ny < h
                                and (nx, ny) not in seen
                                and tab[(nx, ny)] == want):
                            seen.add((nx, ny))
                            q.append((nx, ny))
            xs = [p[0] for p in cell]
            ys = [p[1] for p in cell]
            out.append((min(xs), min(ys), max(xs) - min(xs) + 1,
                        max(ys) - min(ys) + 1, len(cell)))
    return out


def rip(f, lvl):
    tab, w, h = classify(f)
    g = [['.'] * COLS for _ in range(ROWS)]
    notes = []

    # --- girders, on every row ---------------------------------------
    # A girder is drawn as three green courses centred on its row, so look
    # in a five pixel band; rows are eight apart, so bands cannot collide.
    gird = [[False] * COLS for _ in range(ROWS)]
    for r in range(ROWS):
        y0 = img_y(r)
        if y0 + 3 < HUD_BOTTOM or y0 - 2 >= h:
            continue
        for c in range(COLS):
            if any(tab[(x, y)] == 'G'
                   for x in range(c * 16, c * 16 + 16)
                   for y in range(max(0, y0 - 2), min(h, y0 + 3))):
                gird[r][c] = True

    # --- ladders -------------------------------------------------------
    lad = [[False] * COLS for _ in range(ROWS)]
    for r in range(ROWS):
        y0 = img_y(r)
        if y0 < HUD_BOTTOM or y0 + 8 > h:
            continue
        for c in range(COLS):
            rails = sum(
                1 for x in range(c * 16, c * 16 + 16)
                if sum(1 for y in range(y0, y0 + 8) if tab[(x, y)] == 'M') >= 6)
            if rails >= 3:
                lad[r][c] = True

    # Harry or a hen on the rungs hides a few rows of a ladder, so close
    # any short break in a column.
    for c in range(COLS):
        rs = [r for r in range(ROWS) if lad[r][c]]
        for a, b in zip(rs, rs[1:]):
            if 1 < b - a <= 4:
                for r in range(a + 1, b):
                    lad[r][c] = True

    def beside_girder(r, c):
        return ((c > 0 and gird[r][c - 1]) or
                (c < COLS - 1 and gird[r][c + 1]))

    # A ladder is drawn over the girder it meets, hiding the green, so put
    # the girder back at both ends of every run.
    for c in range(COLS):
        r = 0
        while r < ROWS:
            if not lad[r][c]:
                r += 1
                continue
            start = r
            while r < ROWS and lad[r][c]:
                r += 1
            end = r - 1
            if not gird[start][c]:
                for t in (start - 1, start - 2):
                    if t >= 0 and gird[t][c]:
                        for k in range(t + 1, start):
                            lad[k][c] = True
                        start = t
                        break
            if not gird[start][c] and beside_girder(start, c):
                gird[start][c] = True
            if end + 1 < ROWS and not gird[end + 1][c]:
                landed = False
                for t in (end + 2, end + 3):
                    if t < ROWS and gird[t][c]:
                        for k in range(end + 1, t):
                            lad[k][c] = True
                        landed = True
                        break
                if not landed:
                    gird[end + 1][c] = True

    # Wherever a ladder crosses a girder the green is hidden underneath it,
    # not just at the two ends of the run, so restore every crossing.  Test
    # against a snapshot so one ladder cannot seed the next column along.
    seen_gird = [row[:] for row in gird]
    for r in range(ROWS):
        for c in range(COLS):
            if lad[r][c] and not gird[r][c]:
                if ((c > 0 and seen_gird[r][c - 1]) or
                        (c < COLS - 1 and seen_gird[r][c + 1])):
                    gird[r][c] = True

    for r in range(ROWS):
        for c in range(COLS):
            if lad[r][c]:
                g[r][c] = '+' if gird[r][c] else 'H'
            elif gird[r][c]:
                g[r][c] = '#'

    # --- lifts ---------------------------------------------------------
    # An egg measures 7 to 12 wide by 5 or 6 deep; a lift is a flat bar,
    # 14 to 20 wide and always 4 deep.  That splits them cleanly, and it
    # matters: read as an egg, a lift would have a girder invented under
    # it and block its own shaft.
    bars = []
    for col in ('Y', 'W'):
        for (x, y, bw, bh, n) in blobs(tab, w, h, col, HUD_BOTTOM):
            if bw >= 14 and bh <= 5 and n >= 25 and x > 52:
                bars.append((x, bw))
    if bars:
        # The shaft is the column the floors are cut away for, which is a
        # surer guide than where the lift happened to be at the time.
        lo = min(x for x, bw in bars)
        hi = max(x + bw for x, bw in bars)
        cands = range(max(0, lo // 16 - 1), min(COLS, (hi - 1) // 16 + 2))
        shaft = max(cands, key=lambda c: sum(1 for r in PLAT if not gird[r][c]))
        notes.append('lift shaft column %d, %d lift(s)' % (shaft, len(bars)))
        ch = 'L' if len(bars) == 1 else 'K'
        for r in range(PLAT[0], 27):
            if g[r][shaft] == '.':
                g[r][shaft] = ch

    # --- eggs, grain and hens ------------------------------------------
    def rest_on(r, c, mark):
        """Place something on a girder, putting the girder back if the
        screenshot had it hidden."""
        if not (0 <= r < ROWS - 1 and 0 <= c < COLS) or g[r][c] != '.':
            return False
        if g[r + 1][c] not in SOLID:
            if g[r + 1][c] != '.':
                return False
            g[r + 1][c] = '#'
        g[r][c] = mark
        return True

    eggs = 0
    for (x, y, bw, bh, n) in blobs(tab, w, h, 'Y' if lvl <= 6 else 'W',
                                   HUD_BOTTOM):
        if not (4 <= bh <= 9 and bw <= 13 and n >= 25):
            continue                                # 14 wide and up is a lift
        if x < 52 and y < 84:                       # the cage
            continue
        r = settle(y + bh)
        if r is None:
            continue
        for k in range(max(1, (bw + 4) // 14)):
            if rest_on(r, (x + 6 + k * 13) // 16, 'E'):
                eggs += 1

    grain = 0
    for (x, y, bw, bh, n) in blobs(tab, w, h, 'M', HUD_BOTTOM):
        if not (8 <= bw <= 18 and bh <= 7 and n >= 8):
            continue
        r = settle(y + bh)
        if r is not None and rest_on(r, (x + bw // 2) // 16, 'S'):
            grain += 1

    hens = []
    for (x, y, bw, bh, n) in blobs(tab, w, h, 'C', HUD_BOTTOM):
        if n < 30:
            continue
        c = (x + bw // 2) // 16
        r = (y + bh - 1 - 26) // 8
        for p in range(max(0, r), ROWS):   # settle onto the girder below
            if 0 <= c < COLS and g[p][c] in SOLID:
                if p > 0 and g[p - 1][c] == '.':
                    hens.append((p - 1, c))
                break
    return g, eggs, grain, hens, notes


def stands(g):
    """Every empty square that has a girder directly under it."""
    return [(r, c) for r in range(ROWS - 1) for c in range(COLS)
            if g[r][c] == '.' and g[r + 1][c] in SOLID
            and not (1 <= r <= 6 and c <= 2)]


def top_up_eggs(g, want=12):
    """The shots are mid-game, so some eggs have already been taken.  Put
    the shortfall back on empty girder squares, spread over the floors -
    or drop any spare the trace picked up by mistake."""
    have = sum(row.count('E') for row in g)
    while have > want:
        for r in range(ROWS - 1, -1, -1):
            for c in range(COLS - 1, -1, -1):
                if g[r][c] == 'E':
                    g[r][c] = '.'
                    have -= 1
                    break
            else:
                continue
            break
    if have >= want:
        return have
    slots = stands(g)
    # take them evenly across the whole floor rather than in a clump
    slots.sort(key=lambda rc: (rc[0], (rc[1] * 7) % 20))
    step = max(1, len(slots) // max(1, want - have))
    for i in range(0, len(slots), step):
        if have >= want:
            break
        r, c = slots[i]
        if g[r][c] == '.':
            g[r][c] = 'E'
            have += 1
    return have


def ladder_up_dest(g, r, c):
    """Girder a hen standing at (r, c) could climb up to, mirroring the
    game's LadderUp: a run of ladder squares ending in one that is also a
    girder.  A ladder stands proud of the floor it serves, so the rungs
    just above a hen's head are usually only that overhang."""
    rr = r
    while rr >= 0:
        if g[rr][c] not in 'H+':
            return None
        if g[rr][c] == '+':
            return rr
        rr -= 1
    return None


def ladder_down_dest(g, r, c):
    """Girder it could climb down to, mirroring the game's LadderDown: it
    has to be standing on a crossing square, and the run has to reach
    something solid."""
    if r + 1 >= ROWS or g[r + 1][c] != '+':
        return None
    rr = r + 2
    while rr < ROWS:
        if g[rr][c] in SOLID:
            return rr
        if g[rr][c] not in 'H+':
            return None
        rr += 1
    return None


def hen_run(g, rc):
    """The stretch of girder a hen at rc would patrol, as (lo, hi)."""
    r, c = rc
    if r + 1 >= ROWS or g[r + 1][c] not in SOLID:
        return None
    lo = c
    while lo > 0 and g[r + 1][lo - 1] in SOLID:
        lo -= 1
    hi = c
    while hi < COLS - 1 and g[r + 1][hi + 1] in SOLID:
        hi += 1
    return lo, hi


def hen_ok(g, rc):
    """A hen needs somewhere to patrol and a way off it.  Three cells is
    the minimum walk - on one cell it just turns on the spot - and the
    stretch must carry a ladder that actually leads to another floor, or
    the hen is stuck there for the whole level."""
    run = hen_run(g, rc)
    if run is None:
        return False
    r = rc[0]
    lo, hi = run
    if hi - lo + 1 < 3:
        return False
    for cc in range(lo, hi + 1):
        if ladder_up_dest(g, r, cc) is not None:
            return True
        if ladder_down_dest(g, r, cc) is not None:
            return True
    return False


def hen_score(rc, bias, taken):
    """Higher is better.  Keep them apart, and honour the floor's character."""
    r, c = rc
    spread = min([abs(r - hr) * 2 + abs(c - hc) for (hr, hc) in taken] or [40])
    if bias == 'left':
        return (19 - c) * 4 + spread
    if bias == 'top':
        return (28 - r) * 4 + spread
    return spread * 4 + (c % 7)


def choose_hens(g, cand, want, bias, keep=None):
    """Trim or top up the hen squares to the wanted number, keeping any
    already chosen at the front of the list."""
    keep = list(keep or [])
    free = [rc for rc in cand
            if g[rc[0]][rc[1]] == '.' and rc not in keep and hen_ok(g, rc)]
    while len(keep) < want and free:
        best = max(free, key=lambda rc: hen_score(rc, bias, keep))
        free.remove(best)
        keep.append(best)
    if len(keep) < want:
        slots = [rc for rc in stands(g) if rc not in keep and hen_ok(g, rc)]
        while len(keep) < want and slots:
            best = max(slots, key=lambda rc: hen_score(rc, bias, keep))
            slots.remove(best)
            keep.append(best)
    return keep[:want]


def place_player(g, hens):
    """Harry starts on the lowest floor, as far from the hens as it allows.
    A hen sharing that floor with him counts for much more than one two
    storeys up, so he is not dropped in beside one."""
    row = max(r for (r, c) in stands(g))
    ground = [c for (r, c) in hens if r == row]
    best, bestd = None, -1
    for c in range(COLS):
        if (row, c) not in stands(g):
            continue
        near = min([abs(c - hc) for hc in ground] or [99])
        anyh = min([abs(c - hc) for (hr, hc) in hens] or [99])
        d = min(near, 12) * 4 + min(anyh, 12)
        if d > bestd:
            best, bestd = c, d
    if best is None:
        raise SystemExit('nowhere for Harry to stand')
    g[row][best] = 'P'
    return best


def validate(g):
    errs = []
    solid = [[g[r][c] in SOLID for c in range(COLS)] for r in range(ROWS)]
    ladder = [[g[r][c] in 'H+' for c in range(COLS)] for r in range(ROWS)]

    for tag in 'ESP12345':
        for r in range(ROWS):
            for c in range(COLS):
                if g[r][c] == tag and not (r + 1 < ROWS and solid[r + 1][c]):
                    errs.append('%s at r%d c%d has no girder under it'
                                % (tag, r, c))
    n = sum(row.count('E') for row in g)
    if n != 12:
        errs.append('%d eggs (want 12)' % n)

    for c in range(COLS):
        r = 0
        while r < ROWS:
            if ladder[r][c]:
                s = r
                while r < ROWS and ladder[r][c]:
                    r += 1
                if r >= ROWS or not solid[r][c]:
                    errs.append('ladder c%d foot r%d is not on a girder' % (c, r))
            else:
                r += 1

    for r in range(1, 7):
        for c in range(0, 3):
            if g[r][c] not in '.LK':
                errs.append('%s at r%d c%d is behind the cage' % (g[r][c], r, c))

    # reachability: walk, jump a one or two cell gap, take a ladder, or run
    # off an edge and steer the fall a couple of columns sideways
    nodes = {(r, c) for r in range(ROWS) for c in range(COLS) if solid[r][c]}
    adj = {k: set() for k in nodes}
    for (r, c) in nodes:
        if (r, c + 1) in nodes:
            adj[(r, c)].add((r, c + 1))
            adj[(r, c + 1)].add((r, c))
        for gap in (2, 3):
            if (r, c + gap) in nodes and all((r, c + k) not in nodes
                                             for k in range(1, gap)):
                adj[(r, c)].add((r, c + gap))
                adj[(r, c + gap)].add((r, c))
    for c in range(COLS):
        r = 0
        while r < ROWS:
            if ladder[r][c]:
                s = r
                while r < ROWS and ladder[r][c]:
                    r += 1
                stops = [(rr, c) for rr in range(s, min(r + 1, ROWS))
                         if (rr, c) in nodes]
                # A ladder often stands proud of the girder it serves, and
                # Harry can jump off the top of the overhang on to a ledge
                # beside it - that is the only way on to some of them.
                top = (s, c)
                if top not in adj:
                    adj[top] = set()
                for st in stops:
                    adj[top].add(st)
                    adj[st].add(top)
                for rr in range(max(0, s - 3), s + 2):
                    for cc in range(max(0, c - 2), min(COLS, c + 3)):
                        if (rr, cc) in nodes:
                            adj[top].add((rr, cc))
                            adj[(rr, cc)].add(top)
                for i in range(len(stops)):
                    for j in range(i + 1, len(stops)):
                        adj[stops[i]].add(stops[j])
                        adj[stops[j]].add(stops[i])
            else:
                r += 1
    # Harry clears about 30 pixels, so he can hop up on to a ledge up to
    # three rows above and a couple of columns either side - which is how
    # the staircases on levels 6 and 7 are climbed.
    for (r, c) in nodes:
        for dr in (1, 2, 3):
            for dc in range(-2, 3):
                if dr == 3 and abs(dc) > 1:
                    continue
                t = (r - dr, c + dc)
                if t in nodes:
                    adj[(r, c)].add(t)

    for (r, c) in nodes:
        for c2 in range(max(0, c - 2), min(COLS, c + 3)):
            for rr in range(r + 1, ROWS):
                if (rr, c2) in nodes:
                    adj[(r, c)].add((rr, c2))
                    break
    # a lift carries you between every girder end that borders its shaft
    shaft = [c for c in range(COLS)
             if any(g[r][c] in 'LK' for r in range(ROWS))]
    if shaft:
        sc = shaft[0]
        landings = [n2 for n2 in nodes if sc - 2 <= n2[1] <= sc + 2]
        for i in range(len(landings)):
            for j in range(i + 1, len(landings)):
                adj[landings[i]].add(landings[j])
                adj[landings[j]].add(landings[i])

    start = None
    for r in range(ROWS):
        for c in range(COLS):
            if g[r][c] == 'P':
                start = (r + 1, c)
    if start is None:
        return errs + ['no start square']
    seen, q = {start}, deque([start])
    while q:
        k = q.popleft()
        for m in adj.get(k, ()):
            if m not in seen:
                seen.add(m)
                q.append(m)
    for r in range(ROWS):
        for c in range(COLS):
            if g[r][c] in 'ES' and (r + 1, c) not in seen:
                errs.append('%s on r%d c%d is unreachable' % (g[r][c], r, c))
    return errs


def build_all():
    out = {}
    for f in sorted(glob.glob(os.path.join(SHOTS, '*.webp'))):
        key = os.path.basename(f)[:8]
        if key not in LEVEL_OF:
            continue
        lvl = LEVEL_OF[key]
        if lvl > 8:
            continue                      # level 9 repeats level 1
        g, eggs, grain, cand, notes = rip(f, lvl)
        eggs = top_up_eggs(g)
        bias = HEN_BIAS.get(lvl)
        # the floor's own hens first, then the spares for cycles 4 and 5
        hens = choose_hens(g, cand, HEN_COUNT[lvl], bias)
        hens = choose_hens(g, cand, MAXHEN, bias, hens)
        for i, (r, c) in enumerate(hens):
            g[r][c] = str(i + 1)
        place_player(g, hens[:HEN_COUNT[lvl]])
        out[lvl] = (g, eggs, grain, HEN_COUNT[lvl], notes)
    return out


if __name__ == '__main__':
    levels = build_all()
    bad = 0
    for lvl in sorted(levels):
        g, eggs, grain, nhen, notes = levels[lvl]
        errs = validate(g)
        print('level %d: %d eggs, %d grain, %d hens %s'
              % (lvl, eggs, grain, nhen, '; '.join(notes)), file=sys.stderr)
        for e in errs:
            bad += 1
            print('    ', e, file=sys.stderr)
    if '-p' in sys.argv:
        for lvl in sorted(levels):
            print('--- level', lvl)
            for r in range(ROWS):
                print('%2d %s' % (r, ''.join(levels[lvl][0][r])))
    else:
        for lvl in sorted(levels):
            print("' ---- level %d" % lvl)
            for r in range(ROWS):
                print('DATA "%s"' % ''.join(levels[lvl][0][r]))
    print('%d problem(s)' % bad, file=sys.stderr)
