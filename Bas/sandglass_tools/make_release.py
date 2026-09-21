"""make_release.py - build the zip that is handed to a player.

    python make_release.py [-o PrinceOfPico.zip]

Make a directory, unpack the zip into it, and run `setup.bat` or `setup.sh`.
The zip has no folder of its own inside it, so it lands where it is put.

**What goes in is the minimum needed to create the resources and play the
game, and nothing else.** It is a named list, not a filter. A filter lets
anything new in this directory into the next release by default, and that is
how the scenario harness and twenty developer probes came to be in a player's
zip. That is not merely untidy: a player who runs the harness gets a `scen.txt`
on their board, and the engine then plays test scenarios instead of the game -
silently, because the engine sets `Option Console Serial` and every word of the
explanation goes to a port they are not watching.

Everything else stays in this directory and is described in `DEVELOPING.md`,
which does not go in the zip either.

`player.min.bas` is deliberately not listed. `setup.py` builds it from
`player.bas` with `build.py` as part of the install, so a second copy in the
zip could only ever go stale.

What stays out on principle is anything generated or private - and, this being
the point of the whole arrangement, **any converted game data**. `release/`
matters as much as `board/` does: run the setup here and the published source
release is sitting in this directory, and it is no more ours to hand on than
the data converted out of it. A named list makes that automatic rather than
something to remember.

So the zip carries no artwork, no rooms, no animation tables and no music, and
the engine will not start until the player has run the converter against a copy
of the release they fetched themselves. See "Keep what comes out to yourself"
in the README.
"""
import argparse
import os
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))

# The release is this list, and only this list.  Putting a file in this
# directory does not put it in the next zip; adding it here does.
CONTENTS = [
    # The instructions, which are where anyone should start.
    "README.md",
    # The installer: one command does the whole job.
    "setup.bat", "setup.sh", "setup.py",
    # The engine as source; setup.py strips it with build.py.
    "player.bas", "build.py",
    # The converter and every module it imports, so a player can turn their own
    # copy of the published source release into the data the engine needs.
    "convert.py", "appleimg.py", "blueprint.py", "merlin.py", "packpic.py",
    "sheets.py", "sounds.py",
    # Ours - drawn by a user of thebackshed, not taken from any official source.
    "title.jpg",
]


def collect():
    """CONTENTS, checked. A name that is not in it is not in the release."""
    missing = [n for n in CONTENTS if not os.path.isfile(os.path.join(HERE, n))]
    if missing:
        raise SystemExit("these are not in %s and the zip would be no use "
                         "without them:%s" % (HERE, "".join(
                             "\n   " + m for m in missing)))
    return list(CONTENTS)


def unlisted():
    """Local modules the listed scripts import that CONTENTS does not carry.

    A list is the right way to build the zip and is also the one thing that
    goes quietly wrong when the converter grows a module: the zip still builds,
    and dies on someone else's machine with an ImportError for a file we never
    sent. So read the imports back and refuse instead.
    """
    here = {os.path.splitext(f)[0] for f in os.listdir(HERE)
            if f.endswith(".py")}
    listed = {os.path.splitext(n)[0] for n in CONTENTS if n.endswith(".py")}
    needed = set()
    for name in [n for n in CONTENTS if n.endswith(".py")]:
        with open(os.path.join(HERE, name), encoding="utf-8") as fh:
            for line in fh:
                line = line.strip()
                if line.startswith("import "):
                    mod = line[7:].split()[0].split(".")[0].rstrip(",")
                    if mod in here:
                        needed.add(mod)
    return sorted(needed - listed)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--out", default=os.path.join(HERE, "PrinceOfPico.zip"))
    args = ap.parse_args(argv)

    names = collect()
    short = unlisted()
    if short:
        raise SystemExit("the listed scripts import these and CONTENTS does "
                         "not carry them:%s" % "".join(
                             "\n   " + m + ".py" for m in short))

    total = 0
    with zipfile.ZipFile(args.out, "w", zipfile.ZIP_DEFLATED) as z:
        for rel in names:
            path = os.path.join(HERE, rel)
            total += os.path.getsize(path)
            arc = rel.replace("\\", "/")
            info = zipfile.ZipInfo.from_file(path, arc)
            info.compress_type = zipfile.ZIP_DEFLATED
            # A shell script that arrives without its execute bit is a puzzle
            # for whoever unpacks it, and Windows will not have set one.
            info.external_attr = ((0o755 if arc.endswith(".sh") else 0o644) << 16)
            with open(path, "rb") as fh:
                z.writestr(info, fh.read())

    print("%s" % args.out)
    print("%d files, %d bytes in, %d out" % (len(names), total,
                                             os.path.getsize(args.out)))
    for rel in names:
        print("   %-16s %8d" % (rel, os.path.getsize(os.path.join(HERE, rel))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
