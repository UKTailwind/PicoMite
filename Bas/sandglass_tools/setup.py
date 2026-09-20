"""setup.py - fetch the game's data, convert it, and leave a set ready for the board.

Called by `setup.bat` on Windows and `setup.sh` on Linux and macOS; run it
directly if you would rather.

    python setup.py [path to your copy of the source release]

With no argument it fetches the published source release itself, by `git clone`
if git is on the path and by downloading the archive if not, into `release/`
beside this script. Give it a path and it uses that copy and downloads nothing.

It then converts the data, strips the engine, and puts the result in `board/`.
Everything in `board/` goes on the board's drive; nothing else does.

What it fetches is Jordan Mechner's publication of the original Apple II
source, at

    https://github.com/jmechner/Prince-of-Persia-Apple-II

which is the same thing the instructions tell you to fetch by hand. `convert.py`
itself still downloads nothing and reads only the files it is pointed at.

**What comes out of this is not yours to pass on.** It is the game's own
artwork, levels, animation tables and sound in another format. Keep `board/`
to your own machine and your own board, and see "Keep what comes out to
yourself" in README.md.
"""
import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = "https://github.com/jmechner/Prince-of-Persia-Apple-II"
ARCHIVE = REPO + "/archive/refs/heads/master.zip"
RELEASE = os.path.join(HERE, "release")
BOARD = os.path.join(HERE, "board")

# What the engine opens.  art.idx and convert.log are written too, and are for
# reading rather than for the board, so they are left outside board/.
NOT_FOR_THE_BOARD = ("art.idx", "convert.log")


def say(msg=""):
    print(msg, flush=True)


def step(n, msg):
    say()
    say("== %d. %s" % (n, msg))


def looks_like_the_release(path):
    for candidate in (os.path.join(path, "01 POP Source"), path):
        if os.path.isdir(os.path.join(candidate, "Images")) and \
           os.path.isdir(os.path.join(candidate, "Levels")):
            return True
    return False


def have(program):
    return shutil.which(program) is not None


def fetch():
    """Get the published source release into RELEASE, and return its path."""
    if looks_like_the_release(RELEASE):
        say("   already here: %s" % RELEASE)
        return RELEASE

    say("   from %s" % REPO)
    say("   this is Jordan Mechner's publication of the original Apple II")
    say("   source.  Nothing of the game is in this zip; it comes from there.")
    say()

    if have("git"):
        say("   git clone --depth 1 ...")
        subprocess.check_call(["git", "clone", "--depth", "1", REPO + ".git",
                               RELEASE])
        return RELEASE

    say("   no git on the path, so downloading the archive instead")
    tmp = os.path.join(HERE, "_release.zip")
    with urllib.request.urlopen(ARCHIVE) as r, open(tmp, "wb") as fh:
        shutil.copyfileobj(r, fh)
    say("   %d bytes, unpacking" % os.path.getsize(tmp))
    staging = os.path.join(HERE, "_release")
    with zipfile.ZipFile(tmp) as z:
        z.extractall(staging)
    inner = [os.path.join(staging, d) for d in os.listdir(staging)]
    inner = [d for d in inner if os.path.isdir(d)]
    if len(inner) != 1:
        raise SystemExit("the archive did not unpack the way it was expected to")
    shutil.move(inner[0], RELEASE)
    shutil.rmtree(staging, ignore_errors=True)
    os.remove(tmp)
    return RELEASE


def run(*args):
    subprocess.check_call([sys.executable] + [str(a) for a in args])


def main(argv):
    say("Prince of Pico - setting up")
    say("---------------------------")

    if not os.path.isfile(os.path.join(HERE, "convert.py")):
        raise SystemExit("convert.py is not beside this script; unpack the whole zip")

    step(1, "The game's own data")
    if argv:
        source = os.path.abspath(argv[0])
        if not looks_like_the_release(source):
            raise SystemExit(
                "%s does not hold the source release.\n"
                "Looked for Images/ and Levels/, either directly in it or under\n"
                "'01 POP Source'.  Point this at your copy, or give no argument\n"
                "and it will fetch one." % source)
        say("   using the copy you named: %s" % source)
    else:
        source = fetch()
    if not looks_like_the_release(source):
        raise SystemExit("the release is not where it was expected: %s" % source)

    step(2, "Converting it")
    if os.path.isdir(BOARD):
        shutil.rmtree(BOARD)
    os.makedirs(BOARD)
    run(os.path.join(HERE, "convert.py"), "--source", source, "--out", BOARD)

    step(3, "The engine")
    # Stripped beside this script, not into board/: build.py writes a .map
    # next to its output so a line number in an error message can be traced
    # back to the commented source, and that is for you, not for the board.
    stripped = os.path.join(HERE, "player.min.bas")
    run(os.path.join(HERE, "build.py"), os.path.join(HERE, "player.bas"),
        "-o", stripped)
    shutil.copy(stripped, os.path.join(BOARD, "player.bas"))
    say("   player.bas  (%d bytes, and player.min.bas.map is beside this script)"
        % os.path.getsize(stripped))

    step(4, "The title picture")
    title = os.path.join(HERE, "title.jpg")
    if os.path.isfile(title):
        shutil.copy(title, os.path.join(BOARD, "title.jpg"))
        say("   title.jpg")
    else:
        say("   title.jpg is not here - the game will start straight into play")

    # Out of board/, so that what is left is exactly what the board wants.
    for name in NOT_FOR_THE_BOARD:
        src = os.path.join(BOARD, name)
        if os.path.isfile(src):
            shutil.move(src, os.path.join(HERE, name))

    files = sorted(os.listdir(BOARD))
    total = sum(os.path.getsize(os.path.join(BOARD, f)) for f in files)

    say()
    say("Done.  %d files, %d bytes, in" % (len(files), total))
    say("   %s" % BOARD)
    say()
    for f in files:
        say("   %-14s %8d" % (f, os.path.getsize(os.path.join(BOARD, f))))
    say()
    say("Copy the whole of that directory onto the board's drive, then")
    say()
    say('   LOAD "A:/player.bas", C')
    say("   RUN")
    say()
    say("The board needs firmware 6.03.02b12 or later and PSRAM.  README.md has")
    say("the rest, including what stops it starting if it does not.")
    say()
    say("What is in board/ is the game's own content in another format.  Keep it")
    say("to yourself: see \"Keep what comes out to yourself\" in README.md.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
