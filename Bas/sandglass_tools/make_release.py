"""make_release.py - build the zip that is handed to a player.

    python make_release.py [-o PrinceOfPico.zip]

What goes in is everything in this directory that is our own work, under a
single `PrinceOfPico/` folder so the zip does not empty itself into whatever
the player unpacks it in:

  * the instructions, `README.md`, which is where anyone should start;
  * the engine, both as the commented source and as the stripped
    `player.min.bas` that goes on the board;
  * the converter and the modules it imports, so the player can turn their own
    copy of the published source release into the data the engine needs;
  * `title.jpg`;
  * the test kit, because the instructions describe it and a zip whose README
    names files that are not in it is worse than no README.

What stays out is anything generated or private: `player.min.bas.map`,
`__pycache__`, and - this is the point of the whole arrangement - **any
converted game data**. The zip carries no artwork, no rooms, no animation
tables and no music, and the engine will not start until the player has run
the converter against a copy of the release they obtained themselves. See
"Keep what comes out to yourself" in the README.

`player.min.bas` is rebuilt from `player.bas` before it is packed, so the two
cannot drift apart in the zip.
"""
import argparse
import os
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
FOLDER = "PrinceOfPico"

# Not ours to give away, or not worth giving: converted data and everything
# the tools leave behind.  Matched against the name, not the path.
SKIP_NAMES = {"player.min.bas.map", "convert.log", "make_release.py"}
SKIP_DIRS = {"__pycache__", "out", ".git"}
SKIP_EXT = {".pyc", ".zip", ".log", ".bmp", ".dat", ".idx", ".bin", ".wav", ".map"}
# ...except these, which are ours and are wanted.
KEEP_ANYWAY = {"title.jpg"}


def wanted(rel):
    parts = rel.replace("\\", "/").split("/")
    if any(p in SKIP_DIRS for p in parts):
        return False
    name = parts[-1]
    if name in KEEP_ANYWAY:
        return True
    if name in SKIP_NAMES or name.startswith("."):
        return False
    return os.path.splitext(name)[1].lower() not in SKIP_EXT


def collect():
    out = []
    for base, dirs, files in os.walk(HERE):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for f in sorted(files):
            rel = os.path.relpath(os.path.join(base, f), HERE)
            if wanted(rel):
                out.append(rel)
    return sorted(out)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--out", default=os.path.join(HERE, FOLDER + ".zip"))
    args = ap.parse_args(argv)

    # Rebuild the stripped engine so the zip cannot carry a stale one.
    subprocess.check_call([sys.executable, os.path.join(HERE, "build.py"),
                           os.path.join(HERE, "player.bas"),
                           "-o", os.path.join(HERE, "player.min.bas")])

    names = collect()
    for must in ("README.md", "player.bas", "player.min.bas", "convert.py",
                 "title.jpg"):
        if must not in names:
            raise SystemExit("%s is missing and the zip would be no use without it"
                             % must)

    total = 0
    with zipfile.ZipFile(args.out, "w", zipfile.ZIP_DEFLATED) as z:
        for rel in names:
            path = os.path.join(HERE, rel)
            total += os.path.getsize(path)
            z.write(path, FOLDER + "/" + rel.replace("\\", "/"))

    print("%s" % args.out)
    print("%d files, %d bytes in, %d out" % (len(names), total,
                                             os.path.getsize(args.out)))
    for rel in names:
        print("   %s" % rel.replace("\\", "/"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
