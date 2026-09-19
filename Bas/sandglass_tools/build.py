"""build.py - strip the engine down to what the board has room for.

    python build.py [source.bas] [-o out.bas]

The engine is written to be read: nearly half of it is comments explaining what
the original does and why the port does what it does.  MMBasic keeps the whole
program text in memory, comments included, and the board has 144 KB for it, so
the commented source no longer fits.

This removes the comments and the blank lines and nothing else.  The file that
goes to the board is the stripped one; the file in the repository stays
readable.  A line map is written beside it so an error reported against the
stripped program can be traced back: MM.ERRLINE gives a line in the stripped
file, and the map says which source line that was.

Quoted text is respected, so an apostrophe inside a string is not a comment.
"""

import argparse
import os
import sys


def strip_line(line):
    """Return the line without its trailing comment, respecting quotes."""
    out = []
    in_string = False
    for ch in line:
        if ch == '"':
            in_string = not in_string
            out.append(ch)
            continue
        if ch == "'" and not in_string:
            break
        out.append(ch)
    return "".join(out).rstrip()


def strip_program(text):
    """Return (stripped text, map from stripped line number to source line)."""
    kept = []
    mapping = []
    for n, line in enumerate(text.split("\n"), 1):
        body = strip_line(line)
        if not body.strip():
            continue
        word = body.strip().split(None, 1)[0].upper()
        if word == "REM":
            continue
        kept.append(body)
        mapping.append(n)
    return "\n".join(kept) + "\n", mapping


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", nargs="?", default="player.bas")
    ap.add_argument("-o", "--out", default=None,
                    help="output file (default: the source with .min.bas)")
    args = ap.parse_args(argv)

    src = os.path.abspath(args.source)
    out = args.out or os.path.splitext(src)[0] + ".min.bas"
    with open(src, encoding="utf-8") as fh:
        text = fh.read()

    stripped, mapping = strip_program(text)
    with open(out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(stripped)
    with open(out + ".map", "w", encoding="utf-8", newline="\n") as fh:
        for i, n in enumerate(mapping, 1):
            fh.write("%d %d\n" % (i, n))

    before = len(text.split("\n"))
    after = len(mapping)
    print("%s: %d lines, %d bytes" % (os.path.basename(src), before, len(text)))
    print("%s: %d lines, %d bytes  (%d%% of the original)"
          % (os.path.basename(out), after, len(stripped),
             100 * len(stripped) // max(1, len(text))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
