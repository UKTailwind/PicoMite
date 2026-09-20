"""run_scentest.py - play scenarios on the board and report what they proved.

    python run_scentest.py [scenfile ...] [--engine [path]] [--clean]
                           [--timeout n] [--out file]

The engine itself is the test rig: player.bas looks for `scen.txt` beside
itself, and if it is there it plays the scenarios in it with nothing drawn
instead of playing the game.  So a case is a few lines of text that go to the
board in under a second, rather than a two-minute upload of the program.

    python run_scentest.py scen/slicer.txt

puts that file on the board as scen.txt, runs the resident program and prints
what it said.  The exit status is 0 only if the board printed PASS.  Several
files are joined in the order given; with none named, every scenario in
scen/ is run, which is the regression pass.

--engine also puts player.min.bas (build it first with build.py) and LOADs it,
which is needed whenever the engine itself has changed.  --clean takes scen.txt
off the board afterwards, so the next RUN plays the game again.

Files go over the board's own TFTP server (SG_HOST, default 192.168.1.199),
which is much faster than XMODEM; set SG_HOST to an empty string to force
serial.  The board is the one SG_PORT names, COM16 if unset.

Why the board and not a host model: the engine is 4900 lines of MMBasic that
only MMBasic runs, and the point of a scenario is to pin what THIS program
does.  A transcription of it on the host would be a second thing to keep
right.  moverref.py exists because MOVER.S is small and self-contained; the
game is not.
"""
import argparse
import glob
import os
import re
import sys

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, '..', 'elite_tools'))
sys.path.insert(0, os.path.join(here, '..', 'exile_tools'))
os.environ.setdefault('PC3_PORT', os.environ.get('SG_PORT', 'COM16'))

from pc3 import PC3   # noqa: E402
import tftp           # noqa: E402

HOST = os.environ.get('SG_HOST', '192.168.1.199')
SCEN_ON_BOARD = 'scen.txt'
ENGINE_ON_BOARD = 'player.bas'
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]|\x1b[=>]|\r")


def put(board, name, data):
    """Put a file on the board's drive, by TFTP if it has an address."""
    if HOST:
        tftp.send(HOST, name, data)
    else:
        board.xmodem_send('A:/' + name, data)
    print("put %s, %d bytes" % (name, len(data)))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scenfile", nargs="*",
                    help="scenario files, joined in the order given; with none, "
                         "every scen/*.txt except the discover ones, which only print")
    ap.add_argument("--engine", nargs="?", const="player.min.bas", default=None,
                    help="put the engine as well and LOAD it; takes a path, so "
                         "the same scenarios can be run against an older build "
                         "to show that they notice the bug")
    ap.add_argument("--clean", action="store_true",
                    help="remove scen.txt from the board when the run is over")
    ap.add_argument("--timeout", type=float, default=600.0)
    ap.add_argument("--out", default=None, help="also write the output to a file")
    args = ap.parse_args(argv)

    names = args.scenfile
    if not names:
        names = sorted(glob.glob(os.path.join(here, "scen", "*.txt")))
        names = [n for n in names if not os.path.basename(n).startswith("discover")]
    scen = b""
    for n in names:
        scen += (b"\n' ---- " + os.path.basename(n).encode() + b"\n"
                 + open(n, "rb").read().replace(b"\r\n", b"\n") + b"\n")
    print("scenarios: " + ", ".join(os.path.basename(n) for n in names))
    board = PC3()
    print("board on %s, files by %s" % (board.port, ("tftp " + HOST) if HOST else "xmodem"))
    board.attention()

    if args.engine:
        engine = args.engine
        if not os.path.isabs(engine):
            engine = os.path.join(here, engine)
        # LOAD ,C - the file goes over by TFTP as before, and MMBasic strips
        # the comments, blank lines and the spaces inside lines as it reads it.
        # The engine is within a few hundred bytes of program memory and
        # build.py does not remove the inner spaces, so without the C it no
        # longer fits.  A LOAD that does not fit leaves the PREVIOUS program on
        # the board and says so in one line; every scenario then runs against
        # the old engine and passes, which is worse than useless.
        put(board, ENGINE_ON_BOARD, open(engine, "rb").read().replace(b"\r\n", b"\n"))
        reply = board.cmd('LOAD "A:/%s", C' % ENGINE_ON_BOARD, timeout=120)
        print(reply)
        if "Error" in reply:
            print("the engine did not load: the board is still running the "
                  "program it had, so nothing below would mean anything")
            return 2

    put(board, SCEN_ON_BOARD, scen)

    out = ANSI.sub("", board.run(args.timeout))
    print(out)
    if args.out:
        open(args.out, "w", encoding="utf-8").write(out)

    if args.clean:
        board.cmd('KILL "A:/%s"' % SCEN_ON_BOARD, timeout=20)
        print("scen.txt removed; the next RUN plays the game")

    tail = [l for l in out.splitlines() if l.strip() in ("PASS", "FAIL")]
    if not tail:
        print("no verdict: the program did not reach the end of the scenarios")
        return 2
    return 0 if tail[-1].strip() == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
