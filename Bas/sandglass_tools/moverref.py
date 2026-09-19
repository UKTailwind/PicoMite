"""moverref.py - host-side reference for the moving parts (MOVER.S).

    python moverref.py --data <output directory> [--level N] [--frames N]

The pressure plates, gates, spike pits, loose floors, slicers and torches all
animate through one mechanism: a block's STATE byte in the blueprint, advanced
once a frame for every object on a "transition list", plus a list of floors in
free fall.  This is a transcription of that mechanism, routine for routine, so
the engine on the board can be checked against it exactly.

The test scripts a set of events - a plate pressed on some frame, a floor
stepped on, a spike pit walked over - runs the movers for a fixed number of
frames and checksums the two blueprint planes, the transition list and the
falling floors after every frame.  The engine runs the same script, and the
two numbers must agree.

Where the original reads outside the level - a torch picks its flame at random -
the same tiny generator is used on both sides with the same seed, so even that
is reproducible.  Sounds are counted, not played.
"""

import argparse
import os
import sys

# Block types that move, from BGDATA.S.
SPACE, FLOOR, SPIKES, GATE, DPLATE, PLATE = 0, 1, 2, 4, 5, 6
FLASK, LOOSE, RUBBLE, UPLATE, EXIT, SLICER, TORCH, BLOCK = 10, 11, 14, 15, 16, 18, 19, 20
IDMASK, REQMASK = 0x1F, 0x20

# Sound numbers from SOUNDNAMES.S.  Counted only.
PLATE_DOWN, PLATE_UP, GATE_DOWN, LOOSE_CRASH, RAISING_GATE, JAWS_CLASH = 0, 1, 2, 7, 11, 19
GATE_SLAM, LOWER_GATE = 20, 21          # placeholders: only counted

OFF_SPEC, OFF_LINKLOC, OFF_LINKMAP, OFF_MAP, OFF_INFO = 720, 1440, 1696, 1952, 2048
LEVEL_BYTES = 2304
MAXTR = 31                 # trobspace - 1
MAXMOB = 15                # mobspace - 1


def read_layout(path):
    values = {}
    with open(path) as fh:
        for line in fh:
            line = line.strip()
            if line and not line.startswith("#"):
                k, v = line.split()
                values[k] = int(v)
    return values


class Mover:
    """One level's moving parts."""

    def __init__(self, raw, blocks, layout, seed=0, visscrn=1):
        self.lv = bytearray(raw)
        self.c = {k[6:]: v for k, v in layout.items() if k.startswith("const_")}
        g = layout["geom_BlockAy"]
        self.block_ay = list(blocks[g:g + 5])
        g = layout["geom_BlockBot"]
        self.block_bot = list(blocks[g:g + 5])
        g = layout["anim_gateinc"]
        self.gateinc = [v - 256 if v > 127 else v for v in blocks[g:g + 3]]
        g = layout["anim_gatevel"]
        self.gatevel = list(blocks[g:g + 9])
        self.seed = seed
        self.visscrn = visscrn
        self.level = 1
        self.trans = []          # [loc, scrn, direc] in list order
        self.mobs = []           # dicts x, y, scrn, vel, type, level
        self.sounds = 0
        self.alertguard = 0
        self.exitopen = 0
        self.jarabove = 0
        self.kid = {"scrn": 0, "blockx": 0, "blocky": 0, "y": 0, "life": 0xFF}
        self.crushes = 0
        # scratch, as the original's zero-page temporaries
        self.tempscrn = 0
        self.tempblockx = 0
        self.tempblocky = 0
        self.trloc = 0
        self.trscrn = 0
        self.trdirec = 0
        self.pptype = 0
        self.linkindex = 0
        self.state = 0

    # ---- the blueprint ---------------------------------------------------

    def tidx(self, scrn, loc):
        return (scrn - 1) * 30 + loc

    def btype(self, scrn, loc):
        return self.lv[self.tidx(scrn, loc)] & IDMASK

    def bspec(self, scrn, loc):
        return self.lv[OFF_SPEC + self.tidx(scrn, loc)]

    def set_spec(self, scrn, loc, v):
        self.lv[OFF_SPEC + self.tidx(scrn, loc)] = v & 0xFF

    def set_type(self, scrn, loc, v):
        i = self.tidx(scrn, loc)
        self.lv[i] = (self.lv[i] & ~IDMASK) | v

    def neighbour(self, scrn, which):
        if scrn == 0:
            return 0
        return self.lv[OFF_MAP + (scrn - 1) * 4 + which]

    def rdblock(self, scrn, bx, by):
        """RDBLOCK: resolve an off-screen reference, then read the type.

        Leaves tempscrn/tempblockx/tempblocky at the resolved position, which
        the callers rely on.  A null screen reads as solid block."""
        while True:
            if bx < 0:
                bx += 10
                scrn = self.neighbour(scrn, 0)
            elif bx >= 10:
                bx -= 10
                scrn = self.neighbour(scrn, 1)
            elif by < 0:
                by += 3
                scrn = self.neighbour(scrn, 2)
            elif by >= 3:
                by -= 3
                scrn = self.neighbour(scrn, 3)
            else:
                break
        self.tempscrn, self.tempblockx, self.tempblocky = scrn, bx, by
        if scrn == 0:
            return BLOCK
        return self.btype(scrn, by * 10 + bx)

    def rdblock1(self):
        return self.rdblock(self.tempscrn, self.tempblockx, self.tempblocky)

    def loc(self):
        return self.tempblocky * 10 + self.tempblockx

    # ---- the random generator (GRAFIX.S RND) -----------------------------

    def rnd(self):
        self.seed = (self.seed * 5 + 23) & 0xFF
        return self.seed

    # ---- link table ------------------------------------------------------

    def gettimer(self, x):
        return self.lv[OFF_LINKMAP + x] & 0x1F

    def chgtimer(self, x, v):
        self.lv[OFF_LINKMAP + x] = (self.lv[OFF_LINKMAP + x] & 0xE0) | (v & 0x1F)

    def getloc(self, x):
        return self.lv[OFF_LINKLOC + x] & 0x1F

    def getlastflag(self, x):
        return self.lv[OFF_LINKLOC + x] & 0x80

    def getscrn(self, x):
        lo = (self.lv[OFF_LINKLOC + x] & 0x60) >> 2
        hi = self.lv[OFF_LINKMAP + x] & 0xE0
        return ((lo + hi) & 0xFF) >> 3

    # ---- the transition list ---------------------------------------------

    def stopobj(self):
        self.trdirec = -1

    def addtrob(self):
        for e in self.trans:
            if e[0] == self.trloc and e[1] == self.trscrn:
                e[2] = self.trdirec
                return
        if len(self.trans) >= MAXTR:
            return
        self.trans.append([self.trloc, self.trscrn, self.trdirec])

    def addsound(self, n):
        self.sounds += 1

    # ---- triggers from the character side ---------------------------------

    def pushpp(self, scrn, loc):
        """PUSHPP: a plate at (scrn, loc) is stepped on."""
        self.pptype = self.btype(scrn, loc)
        self._pushpp1(scrn, loc)

    def jampp(self, scrn, loc):
        """JAMPP: dead weight lands on a plate."""
        self.pptype = self.btype(scrn, loc)
        if self.pptype == PLATE:
            self.set_type(scrn, loc, DPLATE)
        else:
            self.set_type(scrn, loc, FLOOR)
            self.set_spec(scrn, loc, 0)
            self.pptype = RUBBLE
        self._pushpp1(scrn, loc)

    def _pushpp1(self, scrn, loc):
        x = self.bspec(scrn, loc)
        self.linkindex = x
        t = self.gettimer(x)
        if t == 31:
            return                              # permanently down
        if t >= 2:
            self.chgtimer(x, self.c["pptimer"])  # just restart the count
            self.trigger()
            return
        self.chgtimer(x, self.c["pptimer"])
        self.trloc, self.trscrn, self.trdirec = loc, scrn, 1
        self.addtrob()
        self.alertguard = 1
        self.addsound(PLATE_DOWN)
        self.trigger()

    def trigger(self):
        while True:
            x = self.linkindex
            if self.lv[OFF_LINKLOC + x] == 0xFF:
                return
            self.trloc = self.getloc(x)
            self.trscrn = self.getscrn(x)
            if self.trscrn == 0:
                t = BLOCK
            else:
                t = self.btype(self.trscrn, self.trloc)
            self.trigobj(t)
            if self.trdirec >= 0:
                self.addtrob()
            self.linkindex = (x + 1) & 0xFF
            if self.getlastflag(x):
                return

    def trigobj(self, t):
        if t == GATE:
            self.triggate()
        elif t == EXIT:
            if self.bspec(self.trscrn, self.trloc) != 0:
                self.trdirec = -1               # can only open
            else:
                self.trdirec = 1

    def triggate(self):
        gmax = self.c["gmaxval"]
        st = self.bspec(self.trscrn, self.trloc)
        if self.pptype == UPLATE:               # raise
            self.trdirec = 1
            if st == 0xFF:
                self.stopobj()
                return
            if st < gmax:
                return
            self.set_spec(self.trscrn, self.trloc, self.c["gatetimer"])
            self.stopobj()
        elif self.pptype == RUBBLE:             # open and jam
            self.trdirec = 2
            if st < gmax:
                return
            self.set_spec(self.trscrn, self.trloc, 0xFF)
            self.stopobj()
        else:                                   # lower
            if st == self.c["gminval"]:
                self.stopobj()
            else:
                self.trdirec = 3

    def trigspikes(self, scrn, loc):
        st = self.bspec(scrn, loc)
        if st == 0:
            self.trloc, self.trscrn, self.trdirec = loc, scrn, 1
            self.addtrob()
            self.addsound(GATE_DOWN)
        elif st & 0x80:
            if st != 0xFF:
                self.set_spec(scrn, loc, self.c["spiketimer"])

    def jamspikes(self, scrn, loc):
        self.set_spec(scrn, loc, 0xFF)
        self.trloc, self.trscrn, self.trdirec = loc, scrn, -1
        self.addtrob()
        self.addsound(GATE_DOWN)

    def getspikes(self, scrn, loc):
        """0 safe, 1 lethal, 2 springing."""
        st = self.bspec(scrn, loc)
        if st & 0x80:
            return 0 if st == 0xFF else 1
        if st == 0:
            return 0
        if st < self.c["spikeExt"]:
            return 2
        return 0

    def breakloose(self, scrn, loc, initial=1):
        i = self.tidx(scrn, loc)
        if self.lv[i] & REQMASK:
            return
        st = self.bspec(scrn, loc)
        if not (st & 0x80) and st != 0:
            return
        self.set_spec(scrn, loc, initial)
        self.trloc, self.trscrn, self.trdirec = loc, scrn, 0
        self.addtrob()

    def shakeit(self, scrn, loc):
        st = self.bspec(scrn, loc)
        if st != 0:
            return
        self.set_spec(scrn, loc, 0x80)
        self.trloc, self.trscrn, self.trdirec = loc, scrn, 1
        self.addtrob()

    def shakem1(self, scrn, row):
        for bx in range(9, -1, -1):
            if self.rdblock(scrn, bx, row) == LOOSE:
                self.shakeit(self.tempscrn, self.loc())

    def shakem(self, row):
        if self.level == 13:
            return
        self.shakem1(self.visscrn, row)

    def shakeloose(self, kid_blocky):
        if self.jarabove < 0:
            self.jarabove = 0
            self.shakem(kid_blocky)
        elif self.jarabove > 0:
            self.jarabove = 0
            self.shakem(kid_blocky - 1)

    def trigtorch(self, scrn, loc):
        self.trloc, self.trscrn, self.trdirec = loc, scrn, 1
        self.set_spec(scrn, loc, self.rnd() & 0x0F)
        self.addtrob()

    def trigslicer(self, scrn, loc, newstate):
        st = self.bspec(scrn, loc)
        if st != 0 and st < self.c["slicerRet"]:
            return
        self.trloc = loc
        self.set_spec(scrn, loc, newstate)
        self.trscrn, self.trdirec = scrn, 1
        self.addtrob()

    def addslicers(self, scrn, row):
        """ADDSLICERS: start the slicers on the character's row, phased apart.
        Called on entering a screen and whenever he changes row."""
        if row < 0 or row >= 3:
            return
        tempstate = self.c["slicetimer"]
        for loc in range(row * 10, row * 10 + 10):
            if self.btype(scrn, loc) != SLICER:
                continue
            st = self.bspec(scrn, loc)
            if (st & 0x7F) != 0 and (st & 0x7F) < self.c["slicerRet"]:
                continue                        # mid-slice: leave it alone
            self.trigslicer(scrn, loc, (st & 0x80) | tempstate)
            tempstate = (tempstate - self.c["slicersync"]) & 0xFF
            if tempstate < self.c["slicerRet"]:
                tempstate += self.c["slicetimer"] + 1 - self.c["slicerRet"]

    def enter_screen(self, scrn, row):
        """Arriving at a screen: light its torches, start its slicers."""
        self.visscrn = scrn
        for loc in range(30):
            if self.btype(scrn, loc) == TORCH:
                self.trigtorch(scrn, loc)
        self.addslicers(scrn, row)

    # ---- ANIMTRANS ----------------------------------------------------------

    def animtrans(self):
        if not self.trans:
            return
        clean = False
        for i in range(len(self.trans) - 1, -1, -1):
            self.animobj(i)
            if self.trdirec < 0:
                clean = True
            self.trans[i][2] = self.trdirec
        if clean:
            self.trans = [e for e in self.trans if e[2] != -1]

    def animobj(self, i):
        self.trloc, self.trscrn, self.trdirec = self.trans[i]
        if self.trscrn == 0:
            self.state = 0
            t = BLOCK
        else:
            self.state = self.bspec(self.trscrn, self.trloc)
            t = self.btype(self.trscrn, self.trloc)
        if t == TORCH:
            self.animtorch()
        elif t in (UPLATE, PLATE):
            self.animplate()
        elif t == SPIKES:
            self.animspikes()
        elif t == LOOSE:
            self.animfloor()
        elif t == SPACE:
            self.stopobj()                      # a loose floor that has gone
        elif t == SLICER:
            self.animslicer()
        elif t == GATE:
            self.animgate()
        elif t == EXIT:
            self.animexit()
        else:
            self.stopobj()
        if self.trscrn != 0:
            self.set_spec(self.trscrn, self.trloc, self.state)

    def animtorch(self):
        if self.trdirec < 0:
            return
        if self.trscrn != self.visscrn:
            self.stopobj()
            return
        self.state = self.getflameframe(self.state)

    def getflameframe(self, st):
        last = self.c["torchLast"]
        r = self.rnd()
        if r != st and r < last + 1:
            return r
        st = st + 1
        return 0 if st >= last + 1 else st

    def animplate(self):
        if self.trdirec < 0:
            return
        x = self.state
        t = (self.gettimer(x) - 1) & 0xFF
        self.chgtimer(x, t)
        if t >= 2:
            return
        self.addsound(PLATE_UP)
        self.stopobj()

    def animspikes(self):
        if self.trdirec < 0:
            return
        old = self.state
        if old & 0x80:
            self.state = (self.state - 1) & 0xFF
            if self.state & 0x7F:
                return
            self.state = self.c["spikeExt"] + 1     # first retracting frame
            return
        self.state = (self.state + 1) & 0xFF
        if old == self.c["spikeExt"]:
            self.state = self.c["spiketimer"]
        elif old == self.c["spikeRet"]:
            self.state = 0
            self.stopobj()

    def animfloor(self):
        if self.trdirec < 0:
            return
        self.state = (self.state + 1) & 0xFF
        if self.state & 0x80:
            if self.level == 13:
                return
            if self.state < self.c["wiggletime"] + 0x80:
                return
            self.state = 0
            self.stopobj()
            return
        if self.state < self.c["loosetimer"]:
            return
        self.set_type(self.trscrn, self.trloc, SPACE)
        self.state = 0                              # makespace: 0 for the dungeon set
        self.stopobj()
        lvl = self.trloc // 10
        mob = {"x": (self.trloc % 10) * 4, "y": self.block_bot[lvl + 1],
               "scrn": self.trscrn, "vel": 0, "type": 0, "level": lvl}
        self.addamob(mob)

    def animslicer(self):
        if self.trdirec < 0:
            return
        hi = self.state & 0x80
        n = (self.state & 0x7F) + 1
        if n >= self.c["slicetimer"] + 1:
            n = 1
        self.state = hi | n
        if n == self.c["slicerExt"]:
            self.addsound(JAWS_CLASH)
        on_screen = (self.trscrn == self.visscrn
                     and self.trloc // 10 == self.kid["blocky"])
        if on_screen:
            if self.kid["life"] & 0x80:
                return
            if hi:
                return
        if n >= self.c["slicerRet"]:
            self.stopobj()

    def animgate(self):
        gmax, gmin = self.c["gmaxval"], self.c["gminval"]
        x = self.trdirec
        if x < 0:
            return
        if x >= 3:                              # coming down fast
            if x < self.c["maxgatevel"]:
                x += 1
                self.trdirec = x
            old = self.state
            self.state = (old - self.gatevel[x]) & 0xFF
            if old < self.gatevel[x]:           # the subtraction borrowed
                self.stopobj()
                self.state = 0
                self.addsound(GATE_SLAM)
            return
        if self.state == 0xFF:
            self.stopobj()
            self.addsound(GATE_DOWN)
            return
        self.state = (self.state + self.gateinc[x]) & 0xFF
        if x == 0:
            if self.state <= gmin:
                self.stopobj()
                self.addsound(GATE_DOWN)
                return
            if self.state >= gmax:
                return
            self.addsound(LOWER_GATE)
            return
        if self.state >= gmax:
            if x >= 2:
                self.state = 0xFF
                self.stopobj()
                self.addsound(GATE_DOWN)
                return
            self.state = self.c["gatetimer"]
            self.trdirec = 0
            return
        self.addsound(RAISING_GATE)

    def animexit(self):
        if self.trdirec < 0:
            return
        if self.trdirec >= 3:
            return
        self.addsound(RAISING_GATE)
        self.state = (self.state + 4) & 0xFF
        if self.state >= 43 * 4:
            self.stopobj()
            self.addsound(GATE_DOWN)
            self.exitopen = 1

    # ---- ANIMMOBS ------------------------------------------------------------

    def addamob(self, mob):
        if len(self.mobs) >= MAXMOB:
            return
        self.mobs.append(dict(mob))

    def animmobs(self):
        if not self.mobs:
            return
        for i in range(len(self.mobs) - 1, -1, -1):
            m = dict(self.mobs[i])
            self.tempnt = i
            if m["type"] == 0:
                self.mobfloor(m)
            if m["vel"] & 0x80:
                m["vel"] = (m["vel"] + 1) & 0xFF
            self.checkcrush(m)
            self.mobs[i] = m
        self.mobs = [m for m in self.mobs if m["vel"] != 0xFF]

    def mobfloor(self, m):
        if m["vel"] & 0x80:
            return
        if m["vel"] < self.c["FFtermvel"]:
            m["vel"] += self.c["FFaccel"]
        m["y"] = (m["y"] + m["vel"]) & 0xFF
        if m["scrn"] == 0:
            if m["y"] >= 192 + 17:
                m["vel"] = (-self.c["disappeartime"]) & 0xFF
            return
        if m["y"] >= 0x100 - 30:
            return
        if m["y"] < self.block_ay[m["level"] + 1]:
            return
        self.tempblocky = m["level"]
        self.tempblockx = m["x"] >> 2
        self.tempscrn = m["scrn"]
        t = self.rdblock1()
        if t == SPACE:
            self.passthru(m)
        elif t == LOOSE:
            self.knockloose(m)
            self.passthru(m)
        else:
            self.addsound(LOOSE_CRASH)
            self.shakem1(m["scrn"], m["level"])
            m["y"] = self.block_ay[m["level"] + 1]
            m["vel"] = (-self.c["crumbletime"]) & 0xFF
            self.makerubble(m)

    def passthru(self, m):
        m["level"] += 1
        if m["level"] < 3:
            return
        m["y"] = (m["y"] - 192) & 0xFF
        m["level"] = 0
        m["scrn"] = self.neighbour(m["scrn"], 3)

    def knockloose(self, m):
        scrn, loc = self.tempscrn, self.loc()
        self.set_type(scrn, loc, SPACE)
        self.set_spec(scrn, loc, 0)
        m["vel"] >>= 1
        self.mobs[self.tempnt] = dict(m)
        m["y"] = (m["y"] + 6) & 0xFF
        self.passthru(m)
        self.addamob(m)

    def makerubble(self, m):
        self.tempblocky = m["level"]
        self.tempblockx = m["x"] >> 2
        self.tempscrn = m["scrn"]
        t = self.rdblock1()
        scrn, loc = self.tempscrn, self.loc()
        if scrn == 0:
            return
        if t == PLATE:
            self.pushpp(scrn, loc)
            self.rdblock1()
            self.set_type(scrn, loc, RUBBLE)
        elif t == UPLATE:
            self.set_type(scrn, loc, RUBBLE)
            self.pushpp(scrn, loc)
            self.rdblock1()
            self.set_type(scrn, loc, RUBBLE)
        elif t in (FLOOR, SPIKES, FLASK, TORCH):
            self.set_type(scrn, loc, RUBBLE)

    def checkcrush(self, m):
        k = self.kid
        if m["scrn"] != k["scrn"] or (m["x"] >> 2) != k["blockx"]:
            return
        if m["y"] >= k["y"]:
            return
        if ((k["y"] - self.c["CrushDist"]) & 0xFF) >= m["y"]:
            return
        self.crushes += 1

    # ---- one frame, and the checksum --------------------------------------

    def frame(self):
        self.animmobs()
        self.animtrans()

    def checksum(self, total):
        for v in self.lv[:1440]:
            total = (total * 31 + v) & 0xFFFFFF
        for e in self.trans:
            for v in e:
                total = (total * 31 + (v & 0xFF)) & 0xFFFFFF
        for m in self.mobs:
            for k in ("x", "y", "scrn", "vel", "level"):
                total = (total * 31 + m[k]) & 0xFFFFFF
        total = (total * 31 + self.sounds) & 0xFFFFFF
        return total


# ---- the scripted run ---------------------------------------------------------
#
# Level 1 has every kind of gadget but a slicer, and its plates, gates and loose
# floors are wired together, so one script exercises the lot: an up-plate that
# raises two gates, a plate that lowers one while it is still rising, spikes
# sprung and re-sprung, a loose floor that falls through an empty row and
# shatters on the one below, a plate crushed to rubble by dead weight, and a
# falling floor that lands on the character.  The board runs the same list.

SCRIPT_LEVEL1 = [
    (1, "enter", 5, 0),          # arrive on screen 5, top row: torches start
    (2, "pushpp", 5, 4),         # up-plate: raises gates (5,9) and (5,5)
    (3, "trigspikes", 6, 23),
    (5, "breakloose", 7, 5),     # loose floor: falls after ten frames
    (6, "pushpp", 5, 2),         # plate: lowers gate (5,9) while it is rising
    (8, "shakem1", 1, 2),        # jar the bottom row of screen 1
    (20, "jampp", 6, 2),         # dead weight on an up-plate: rubble, gate jams
    (40, "pushpp", 12, 3),       # off-screen: a gate on screen 12
    (60, "trigspikes", 6, 23),   # already out: the timer restarts
    (61, "jamspikes", 6, 24),
    (100, "kid", 8, 3, 2, 181),  # he stands on screen 8, block 3 of the bottom row
    (101, "breakloose", 8, 13),  # the floor above him lets go
    (150, "pushpp", 5, 4),       # raise again: (5,9) closed by now, (5,5) still open
    (200, "trigspikes", 10, 21),
    (230, "enter", 9, 1),
    (231, "pushpp", 9, 0),       # opens the exit
]
FRAMES_LEVEL1 = 300

# Level 3, screen 16: three slicers on the bottom row and two torches.  They
# start in step on arrival, stop once he leaves the row, and restart when the
# row is entered again.
SCRIPT_LEVEL3 = [
    (1, "enter", 16, 2),
    (1, "kid", 16, 4, 2, 181),
    (60, "kid", 16, 4, 1, 118),
    (90, "addslicers", 16, 2),
    (91, "kid", 16, 4, 2, 181),
]
FRAMES_LEVEL3 = 120


def run_script(mover, script, frames, trace=None):
    total = 0
    for f in range(1, frames + 1):
        for ev in script:
            if ev[0] != f:
                continue
            kind = ev[1]
            if kind == "enter":
                mover.enter_screen(ev[2], ev[3])
            elif kind == "addslicers":
                mover.addslicers(ev[2], ev[3])
            elif kind == "pushpp":
                mover.pushpp(ev[2], ev[3])
            elif kind == "jampp":
                mover.jampp(ev[2], ev[3])
            elif kind == "trigspikes":
                mover.trigspikes(ev[2], ev[3])
            elif kind == "jamspikes":
                mover.jamspikes(ev[2], ev[3])
            elif kind == "breakloose":
                mover.breakloose(ev[2], ev[3])
            elif kind == "shakem1":
                mover.shakem1(ev[2], ev[3])
            elif kind == "kid":
                mover.kid.update(scrn=ev[2], blockx=ev[3], blocky=ev[4], y=ev[5])
        mover.frame()
        total = mover.checksum(total)
        if trace:
            trace(f, mover, total)
    return total


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data", required=True)
    ap.add_argument("--level", type=int, default=1, choices=(1, 3))
    ap.add_argument("--trace", action="store_true", help="print every frame")
    args = ap.parse_args(argv)
    data = os.path.abspath(args.data)
    layout = read_layout(os.path.join(data, "tables.idx"))
    with open(os.path.join(data, "blocks.dat"), "rb") as fh:
        blocks = fh.read()
    with open(os.path.join(data, "levels.dat"), "rb") as fh:
        levels = fh.read()
    raw = levels[args.level * LEVEL_BYTES:(args.level + 1) * LEVEL_BYTES]
    m = Mover(raw, blocks, layout, seed=0)
    m.level = args.level
    script = SCRIPT_LEVEL1 if args.level == 1 else SCRIPT_LEVEL3
    frames = FRAMES_LEVEL1 if args.level == 1 else FRAMES_LEVEL3

    def trace(f, mv, total):
        print("f%-4d trans %-40s mobs %d sounds %d sum %d"
              % (f, " ".join("%d:%d:%d" % tuple(e) for e in mv.trans), len(mv.mobs),
                 mv.sounds, total))

    total = run_script(m, script, frames, trace if args.trace else None)
    print("level %d, %d frames" % (args.level, frames))
    print("in transition  %d" % len(m.trans))
    print("falling floors %d" % len(m.mobs))
    print("sounds         %d" % m.sounds)
    print("crushes        %d" % m.crushes)
    print("exit open      %d" % m.exitopen)
    print("checksum       %d" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
