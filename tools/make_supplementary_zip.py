"""make_supplementary_zip.py - build the release's supplementary-manuals zip.

    python tools/make_supplementary_zip.py

Writes `PicoMite_Supplementary_Manuals-<version>.zip` at the repo root, where
<version> comes from Version.h, exactly as tools/make_mmb2csub_zip.py does.

The zip is every PDF in PDF/ EXCEPT the two copies of the user manual - the
live `PicoMite_User_Manual.pdf` and the versioned snapshot - because the manual
ships as an asset in its own right and is much the largest file.

This existed as a six-line throwaway at the repo root for V6.04.00RC0, which is
why it is here now: the asset list is part of the release and its builder
belongs in the repo with the others.  The zip itself is gitignored (`*.zip`).
"""
import os
import re
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PDFDIR = os.path.join(ROOT, "PDF")


def version():
    """The version string, from the one place that owns it."""
    path = os.path.join(ROOT, "Version.h")
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = re.match(r'\s*#define\s+VERSION\s+"([^"]+)"', line)
            if m:
                return m.group(1)
    raise SystemExit("no #define VERSION in %s" % path)


def main():
    ver = version()
    out = os.path.join(ROOT, "PicoMite_Supplementary_Manuals-%s.zip" % ver)

    names = sorted(f for f in os.listdir(PDFDIR)
                   if f.lower().endswith(".pdf")
                   and not f.startswith("PicoMite_User_Manual"))
    if not names:
        raise SystemExit("no supplementary PDFs found in %s" % PDFDIR)

    total = 0
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for name in names:
            path = os.path.join(PDFDIR, name)
            total += os.path.getsize(path)
            z.write(path, name)

    print("%s" % out)
    print("%d files, %d bytes in, %d out"
          % (len(names), total, os.path.getsize(out)))
    for name in names:
        print("   %s" % name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
