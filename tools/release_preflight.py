#!/usr/bin/env python3
"""Pre-flight checks for a PicoMite release.

Run from anywhere:  python tools/release_preflight.py
Exits non-zero on the first failure, so it can gate the publish:

    python tools/release_preflight.py && gh release create ...

NEVER gate binary freshness on source-file mtimes in this repo: Dropbox
rewrites mtimes on sync without a byte changing, which condemns good
binaries.  Freshness is measured against COMMIT times (a sync cannot forge
one), with a separate porcelain check to catch sources edited but not yet
committed - which a commit-time check alone would miss.
"""
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

VARIANTS = [
    "PicoMiteRP2040", "PicoMiteRP2040MIN", "PicoMiteRP2040USB",
    "PicoMiteRP2040VGA", "PicoMiteRP2040VGAUSB",
    "PicoMiteRP2350", "PicoMiteRP2350USB",
    "PicoMiteRP2350VGA", "PicoMiteRP2350VGAUSB",
    "PicoMiteRP2350BT", "PicoMiteRP2350BTH",
    "PicoMiteHDMI", "PicoMiteHDMIUSB", "PicoMiteHDMIWEB",
    "WebMiteRP2040", "WebMiteRP2350",
]

# Everything a firmware binary is built from.  Bas/, Testfiles/, docs/ and
# PDF/ are deliberately absent: changing a BASIC demo or the manual does not
# invalidate a uf2.
#
# The two top-level wildcards carry :(glob) magic so that "*" stops at a
# directory separator.  Without it a bare "*.c" pathspec matches at ANY depth,
# so a hand-compiled CSUB under Bas/ counted as firmware - condemning good
# binaries and blocking the release on work in progress, which is exactly what
# the comment above says must not happen.  Only root-level PicoMite.c and the
# root headers belong here.
FIRMWARE_PATHS = [
    ":(glob)*.c", ":(glob)*.h", "core", "graphics", "input", "io", "misc",
    "net", "video", "bluetooth", "usb_host_files", "third_party_mod", "fonts",
    "cmake", "linker_overrides", "CMakeLists.txt", "buildpicomite.bat",
]

# What the mmb2csub distribution zip is built from.  Bas/ is deliberately
# absent even though two examples are packaged from it: it carries Peter's
# in-flight BASIC work, and a half-finished program there must not condemn
# the zip.
PACKAGE_PATHS = [
    "user-tools", "PicoCFunctions.h", "docs/mmb2csub.md", "docs/armcfgen.md",
    "tools/make_mmb2csub_zip.py",
]

# Work in progress that never reaches a uf2 and so cannot block a release.
# Listed at the end of the run so it stays visible rather than silently
# dropped.  docs/Exile_* are the working notes for the Exile port.
WIP_PATHS = ["Bas", "Testfiles", "docs/Exile_*",
             # A port plan or review is working notes for a game, reaches no
             # uf2 and is shipped in nothing, so an unfinished one must not
             # block a release - which is exactly what an untracked Prince of
             # Persia review did to b11.  The Exile entry above is the same
             # thing named for one port; these two generalise it.
             "docs/*_Port_Plan.html", "docs/*_Port_Review.html"]

fails = []
def check(name, ok, detail=""):
    # detail is the REASON IT FAILED, so it is shown only on failure -
    # printed beside an "ok" it reads as an instruction already carried out
    print("  ok   " + name if ok
          else "  FAIL " + name + (" - " + detail if detail else ""))
    if not ok:
        fails.append(name)

def git(*args):
    return subprocess.run(["git"] + list(args), cwd=REPO, capture_output=True,
                          text=True).stdout.strip()

def commit_time(paths):
    out = git("log", "-1", "--format=%ct", "--", *paths)
    return int(out) if out else 0

# 1. the version the firmware actually declares
src = open(os.path.join(REPO, "Version.h"), encoding="utf-8", errors="replace").read()
m = re.search(r'#define\s+VERSION\s+"([^"]+)"', src)
if not m:
    sys.exit("Version.h has no #define VERSION")
version = m.group(1)
print("Version.h declares V%s\n" % version)
if len(sys.argv) > 1:
    check("requested version matches Version.h", sys.argv[1].lstrip("Vv") == version,
          "asked for %s" % sys.argv[1])

# 2. all 16 uf2 present, correctly named, and no leftovers from another version
uf2dir = os.path.join(REPO, "uf2")
present = sorted(f for f in os.listdir(uf2dir) if f.endswith(".uf2"))
wanted = sorted("%sV%s.uf2" % (v, version) for v in VARIANTS)
missing = [f for f in wanted if f not in present]
stale = [f for f in present if f not in wanted]
check("all 16 uf2 present for V%s" % version, not missing, "missing " + ", ".join(missing))
check("no uf2 left over from another version", not stale, "stale " + ", ".join(stale))

uf2_times = {f: os.path.getmtime(os.path.join(uf2dir, f)) for f in wanted
             if os.path.exists(os.path.join(uf2dir, f))}

# 3. every binary newer than the last change to the fit limits
t_cfg = commit_time(["configuration.h"])
older = [f for f, t in uf2_times.items() if t < t_cfg]
check("every uf2 newer than the last configuration.h commit", not older,
      ", ".join(older))

# 4. every binary newer than the last firmware-affecting commit.  Version.h
#    is excluded: the version bump is committed AFTER the build by
#    definition, and the uf2 FILENAMES already prove which VERSION string
#    was compiled in - buildpicomite.bat reads it from Version.h.
fw_paths = FIRMWARE_PATHS + [":(exclude)Version.h"]
t_fw = commit_time(fw_paths)
older = [f for f, t in uf2_times.items() if t < t_fw]
check("every uf2 newer than the last firmware commit (%s)" % git("log", "-1",
      "--format=%h", "--", *fw_paths), not older, ", ".join(older))

# 5. no firmware source edited but not committed (mtimes cannot see this)
dirty = git("status", "--porcelain", "--", *FIRMWARE_PATHS)
check("no uncommitted firmware source", not dirty, dirty.replace("\n", "; "))

# 6. the release manual exists and is the master, byte for byte
master = os.path.join(REPO, "PDF", "PicoMite_User_Manual.pdf")
copy = os.path.join(REPO, "PDF", "PicoMite_User_ManualV%s.pdf" % version)
if not os.path.exists(copy):
    check("versioned manual PDF present", False, os.path.basename(copy))
else:
    a, b = open(master, "rb").read(), open(copy, "rb").read()
    check("versioned manual PDF present", True)
    check("versioned manual PDF identical to the master", a == b,
          "%d vs %d bytes" % (len(a), len(b)))

# 7. the manual PDF is not older than the docx it is made from - by commit
#    time, again, not mtime
t_docx = commit_time(["docs/PicoMite_User_Manual.docx"])
t_pdf = commit_time(["PDF/PicoMite_User_Manual.pdf"])
docx_dirty = git("status", "--porcelain", "--", "docs/PicoMite_User_Manual.docx",
                 "PDF/PicoMite_User_Manual.pdf")
check("manual PDF regenerated since the docx last changed",
      t_pdf >= t_docx or bool(docx_dirty),
      "docx committed after the PDF")

# 8. the mmb2csub distribution zip, named from Version.h by its builder.
#    Gitignored like the uf2s, so freshness is its mtime against the COMMIT
#    time of the tooling it packages - the same asymmetry the uf2 checks use,
#    and for the same reason (Dropbox rewrites mtimes, not commit times).
zipname = "mmb2csub-%s.zip" % version
zippath = os.path.join(REPO, zipname)
if not os.path.exists(zippath):
    check("mmb2csub zip present", False,
          "%s - run tools/make_mmb2csub_zip.py" % zipname)
else:
    check("mmb2csub zip present", True, zipname)
    t_pkg = commit_time(PACKAGE_PATHS)
    check("mmb2csub zip newer than the tooling it packages (%s)"
          % git("log", "-1", "--format=%h", "--", *PACKAGE_PATHS),
          os.path.getmtime(zippath) >= t_pkg,
          "rebuild it - the zip predates the tools inside it")
    pkg_dirty = git("status", "--porcelain", "--", *PACKAGE_PATHS)
    check("no uncommitted change to what the zip packages", not pkg_dirty,
          pkg_dirty.replace(chr(10), "; "))

# 8b. the supplementary-manuals zip: every PDF/ manual except the two copies
#     of the user manual, which ships as an asset of its own.  Added as a
#     22nd asset at Peter's request during V6.04.00RC0 and built by a
#     throwaway script that release; tools/make_supplementary_zip.py now.
#     Freshness is its mtime against the COMMIT time of the PDFs it packs,
#     the same asymmetry the uf2 and mmb2csub checks use.
supname = "PicoMite_Supplementary_Manuals-%s.zip" % version
suppath = os.path.join(REPO, supname)
# The zip excludes both copies of the user manual, so they must not count
# towards its freshness either - cutting the versioned snapshot at release
# time would otherwise condemn a zip that is perfectly current.
SUPP_PATHS = [':(glob)PDF/*.pdf',
              ':(exclude,glob)PDF/PicoMite_User_Manual*.pdf']
if not os.path.exists(suppath):
    check("supplementary manuals zip present", False,
          "%s - run tools/make_supplementary_zip.py" % supname)
else:
    check("supplementary manuals zip present", True, supname)
    t_sup = commit_time(SUPP_PATHS)
    check("supplementary zip newer than the manuals it packages",
          os.path.getmtime(suppath) >= t_sup,
          "rebuild it - the zip predates a PDF inside it")
    import zipfile as _zf
    n_in = len([f for f in os.listdir(os.path.join(REPO, 'PDF'))
                if f.lower().endswith('.pdf')
                and not f.startswith('PicoMite_User_Manual')])
    with _zf.ZipFile(suppath) as _z:
        n_zip = len(_z.namelist())
    check("supplementary zip holds every supplementary manual (%d)" % n_in,
          n_in == n_zip,
          "zip has %d, PDF/ has %d" % (n_zip, n_in))

# 9. the two HELP files, which are generated FROM the manual - so they go
#    stale exactly when the docx changes, and nothing else notices.
#
#    Checked by REGENERATING and comparing, not by date. A date check cannot
#    tell "never regenerated" from "regenerated, came out the same", and the
#    second happens routinely: helpmin.txt carries syntax and summary only, so
#    most manual edits leave it byte-identical and a date check cries wolf.
for h, extra in (("help.txt", []), ("helpmin.txt", ["--short"]),
                 ("helptiny.txt", ["--tiny"])):
    hp = os.path.join(REPO, "docs", h)
    if not os.path.exists(hp):
        check("docs/%s present" % h, False, "run tools/gen_help.py")
        continue
    check("docs/%s present" % h, True)
    tmp = os.path.join(tempfile.gettempdir(), "preflight_" + h)
    r = subprocess.run([sys.executable, os.path.join(REPO, "tools", "gen_help.py"),
                        "-o", tmp] + extra,
                       cwd=REPO, capture_output=True, text=True)
    if r.returncode != 0:
        check("docs/%s matches the manual" % h, False,
              "could not run gen_help.py: " + (r.stderr.strip().splitlines() or [""])[-1])
    else:
        same = open(hp, "rb").read() == open(tmp, "rb").read()
        check("docs/%s matches the manual" % h, same,
              "stale - run: python tools/gen_help.py %s" % " ".join(extra))
        os.remove(tmp)

# 10. everything the release ships is committed and pushed.  Work in progress
#    under Bas/ and Testfiles/ is NOT a release blocker - a half-finished
#    BASIC program does not reach a uf2 - and gh release --target main tags
#    the REMOTE head, so anything unpushed is simply excluded.  An unpushed
#    COMMIT is a blocker, because it would be excluded silently.
release_dirty = git("status", "--porcelain", "--",
                    *[":(exclude)" + p for p in WIP_PATHS])
check("everything the release ships is committed", not release_dirty,
      release_dirty.replace(chr(10), "; "))
ahead = git("rev-list", "--count", "origin/main..HEAD")
check("HEAD pushed to origin/main", ahead == "0", "%s commit(s) unpushed" % ahead)

other = git("status", "--porcelain", "--", *WIP_PATHS)
if other:
    print("")
    print("  note: work in progress left out of the release:")
    for line in other.splitlines():
        print("        " + line.strip())

print()
if fails:
    print("PRE-FLIGHT FAILED: " + ", ".join(fails))
    sys.exit(1)
print("PRE-FLIGHT PASSED for V%s - 22 assets ready" % version)
print("  16 uf2 + the manual PDF + %s" % zipname)
print("  + docs/help.txt, docs/helpmin.txt, docs/helptiny.txt")
print("  + %s" % supname)
