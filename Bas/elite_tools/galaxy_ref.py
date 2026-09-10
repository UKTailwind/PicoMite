"""Reference implementation of Elite's galaxy generator.

The port's MMBasic version has to agree with this bit for bit, over all
eight galaxies and all 256 systems in each, or the universe is not Elite's.
This file is the oracle: it writes tests/galaxy_ref.txt, which galaxy_test.bas
checks itself against on the board.

Everything here follows the original's arithmetic exactly - the seeds are
three 16-bit words, the twist is a 16-bit add with the carry discarded, and
every field is a slice of particular bits.  Nothing is "tidied up", because
a tidied version generates a different galaxy.

Usage:  python galaxy_ref.py            write the reference file
        python galaxy_ref.py check      just print the well known systems
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "elite", "tests", "galaxy_ref.txt"))

# Galaxy 1's starting seeds.
GAL1 = (0x5A4A, 0x0248, 0xB753)

# Two letter fragments, indexed by the five bit value taken from the seeds.
# Index 0 is never emitted - the original tests for it and prints nothing -
# and index 15 is the one that contributes a single "A".
PAIRS = ["AL", "LE", "XE", "GE", "ZA", "CE", "BI", "SO", "US", "ES", "AR", "MA",
         "IN", "DI", "RE", "A?", "ER", "AT", "EN", "BE", "RA", "LA", "VE", "TI",
         "ED", "OR", "QU", "AN", "TE", "IS", "RI", "ON"]

GOVERNMENT = ["Anarchy", "Feudal", "Multi-gov", "Dictatorship",
              "Communist", "Confederacy", "Democracy", "Corporate State"]
ECONOMY = ["Rich Ind", "Average Ind", "Poor Ind", "Mainly Ind",
           "Mainly Agri", "Rich Agri", "Average Agri", "Poor Agri"]


def lo(w):
    return w & 0xFF


def hi(w):
    return (w >> 8) & 0xFF


def twist(s):
    """One turn of the seed generator.  s2 becomes s0+s1+s2, discarding any
    carry out of 16 bits, and the other two shuffle down."""
    s0, s1, s2 = s
    tmp = (s0 + s1) & 0xFFFF
    s0, s1 = s1, s2
    s2 = (tmp + s1) & 0xFFFF
    return (s0, s1, s2)


def four(s):
    """A system is four turns on from the one before it."""
    for _ in range(4):
        s = twist(s)
    return s


def name(s):
    """Three or four fragments, taken from a copy of the seeds so the caller's
    are left where they were."""
    n = 4 if (lo(s[0]) & 64) else 3
    out = ""
    for _ in range(n):
        i = hi(s[2]) & 31
        if i:
            out += PAIRS[i]
        s = twist(s)
    return out.replace("?", "")


def system(s):
    """Every field the game shows, from the current seeds."""
    gov = (lo(s[1]) >> 3) & 7
    eco = hi(s[0]) & 7
    if gov <= 1:
        eco |= 2
    tech = (eco ^ 7) + (hi(s[1]) & 3) + ((gov + 1) >> 1)
    pop = tech * 4 + eco + gov + 1
    prod = ((eco ^ 7) + 3) * (gov + 4) * pop * 8
    return {
        "name": name(s),
        "x": hi(s[1]),
        "y": hi(s[0]) >> 1,
        "gov": gov,
        "eco": eco,
        "tech": tech,                       # the game displays tech + 1
        "pop": pop,                         # tenths of a billion
        "prod": prod,                       # millions of credits
        "radius": ((hi(s[2]) & 15) + 11) * 256 + hi(s[1]),
        "goatsoup": lo(s[2]),
    }


def galaxy(g):
    """All 256 systems of galaxy g, g counting from 1.  A galaxy is reached by
    rotating every one of the six seed bytes left by one bit, independently."""
    s = GAL1
    for _ in range(g - 1):
        b = [lo(s[0]), hi(s[0]), lo(s[1]), hi(s[1]), lo(s[2]), hi(s[2])]
        b = [((x << 1) | (x >> 7)) & 0xFF for x in b]
        s = (b[0] | (b[1] << 8), b[2] | (b[3] << 8), b[4] | (b[5] << 8))
    out = []
    for _ in range(256):
        out.append(system(s))
        s = four(s)
    return out


def describe(d):
    return ("%-9s (%3d,%3d) %-15s %-12s tech %2d  pop %4.1f bn  prod %6d MCr  radius %d km"
            % (d["name"], d["x"], d["y"], GOVERNMENT[d["gov"]], ECONOMY[d["eco"]],
               d["tech"] + 1, d["pop"] / 10.0, d["prod"], d["radius"]))


def checksum():
    """A single number over every field of every system in every galaxy.
    The board computes the same one, so one comparison proves all 2048."""
    h = 0
    for g in range(1, 9):
        for d in galaxy(g):
            for f in ("x", "y", "gov", "eco", "tech", "pop", "prod", "radius"):
                h = (h * 31 + d[f]) % 2147483647
            for ch in d["name"]:
                h = (h * 31 + ord(ch)) % 2147483647
    return h


def main():
    g1 = galaxy(1)
    if "checksum" in sys.argv:
        print("galaxy checksum over 8 x 256 systems: %d" % checksum())
        return
    if "check" in sys.argv:
        print("galaxy 1, system 0 and the four everyone knows:")
        for i in (0,):
            print("  %3d %s" % (i, describe(g1[i])))
        for want in ("LAVE", "ZAONCE", "DISO", "RIEDQUAT", "LEESTI", "TIBEDIED"):
            for i, d in enumerate(g1):
                if d["name"] == want:
                    print("  %3d %s" % (i, describe(d)))
                    break
            else:
                print("  ??? %s not found in galaxy 1" % want)
        return

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", newline="\n") as f:
        f.write("# Elite galaxy reference, generated by elite_tools/galaxy_ref.py\n")
        f.write("# galaxy system name x y gov eco tech pop prod radius\n")
        n = 0
        for g in range(1, 9):
            for i, d in enumerate(galaxy(g)):
                f.write("%d %d %s %d %d %d %d %d %d %d %d\n" % (
                    g, i, d["name"], d["x"], d["y"], d["gov"], d["eco"],
                    d["tech"], d["pop"], d["prod"], d["radius"]))
                n += 1
    print("wrote %s: %d systems" % (OUT, n))
    print()
    for i, d in enumerate(galaxy(1)):
        if d["name"] in ("LAVE", "ZAONCE", "DISO", "RIEDQUAT"):
            print("  %3d %s" % (i, describe(d)))


if __name__ == "__main__":
    main()
