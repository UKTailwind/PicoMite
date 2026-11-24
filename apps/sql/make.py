"""make.py: builds apps/sql/out/sql.app, SQLite run from MMBasic's RUN, and apps/sql/out/mod/sql.mod, the module the
SQL command loads (misc/SQL.c), for the PicoMitePC3 and the PicoMiteHDMIUSB (the Colour Maximite 2's docs/SQL_Plan.md).

The sources are the Colour Maximite 2's apps/sql (CMM2V1.5 v8-dev 5e9721f): sqlite_cmm2.h (SQLite's settings,
usqlite's, MIT), sqlite_unit.c (the amalgamation compiled as usqlite compiles it), sqlcore.c (its OS layer on the
services, here the PicoMite's, with databases on B: or C: only) and sql_app.c (the program, MIT). SQLite 3.47.0, the
version the MicroPython port builds, is fetched the first time from sqlite.org into apps/sql/sqlite (git ignores it),
checked against the zip's SHA-256, and given the CMM2's one patch (below). SQLite is in the public domain.

    python apps/sql/make.py
then copy out/sql.app (by a transfer that keeps its size: not XMODEM) to any drive and, at the prompt:
    RUN "sql.app", "B:/club.db SELECT * FROM member"
"""
import hashlib, io, os, subprocess, sys, urllib.request, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(HERE, 'sqlite')
URL = 'https://www.sqlite.org/2024/sqlite-amalgamation-3470000.zip'
SHA256 = '2842fddbb1cc33f66c7da998a57535f14a6bfee159676a07bb4bf3e59375d93e'
FILES = ('sqlite3.c', 'sqlite3.h')
STACK = 0x10000                  # sql.app's stack, from the SRAM heap: the CMM2's 64 KB (-stack reports what is used)
MOD_STACK = 0x4000               # sql.mod's own stack, from the SRAM heap while it is loaded: sql.app -stack measured
                                 # 8.6 KB at most (24 Nov 2026: 50,000 rows, a sort to temporary files, an index,
                                 # triggers, json, integrity_check), so 16 KB


def fetch():
    """sqlite3.c and sqlite3.h from the amalgamation's zip, once"""
    if all(os.path.exists(os.path.join(SRC, f)) for f in FILES):
        return
    data = urllib.request.urlopen(URL, timeout=120).read()
    got = hashlib.sha256(data).hexdigest()
    if got != SHA256:
        sys.exit('%s: SHA-256 %s, not %s' % (URL, got, SHA256))
    os.makedirs(SRC, exist_ok=True)
    z = zipfile.ZipFile(io.BytesIO(data))
    for f in FILES:
        open(os.path.join(SRC, f), 'wb').write(z.read('sqlite-amalgamation-3470000/' + f))


# The CMM2's one change to SQLite (13 Nov 2026): its sorter keeps rows in memory up to the cache's size before it writes
# them out, capped by SQLITE_MAX_PMASZ, which sqlite3.c defines whatever is set before it. Guarded, sqlite_cmm2.h's
# 256 KB holds, and a big page cache no longer makes a big sorter.
PATCH_OLD = b'#define SQLITE_MAX_PMASZ    (1<<29)\n'
PATCH_NEW = b'#ifndef SQLITE_MAX_PMASZ            /* (CMM2: apps/sql/make.py) */\n# define SQLITE_MAX_PMASZ    (1<<29)\n#endif\n'


def patch():
    p = os.path.join(SRC, 'sqlite3.c')
    s = open(p, 'rb').read()
    if PATCH_NEW in s:
        return
    if s.count(PATCH_OLD) != 1:
        sys.exit('%s: SQLITE_MAX_PMASZ is not as make.py expects' % p)
    open(p, 'wb').write(s.replace(PATCH_OLD, PATCH_NEW))


def build(out, srcs, extra):
    cmd = [sys.executable, '-I', os.path.join(REPO, 'tools', 'sdk', 'build.py'), out] + srcs + extra
    cmd += ['-I', SRC, '-I', HERE, '--cflag=-include' + os.path.join(HERE, 'sqlite_cmm2.h')]
    cmd += ['--cflag=' + a for a in sys.argv[1:] if a.startswith('-')]
    if subprocess.run(cmd).returncode:
        sys.exit(1)


def main():
    fetch()
    patch()
    out = os.path.join(HERE, 'out')
    os.makedirs(out, exist_ok=True)
    core = [os.path.join(HERE, 'sqlite_unit.c'), os.path.join(HERE, 'sqlcore.c')]
    # sql.app: the PicoMite's services version 1 (file_truncate and dir_current are in it); PSRAM; 1 MB of heap at least
    build(os.path.join(out, 'sql.app'), core + [os.path.join(HERE, 'sql_app.c')],
          ['--api', '1', '--psram', '--heap', str(1 << 20), '--stack', str(STACK), '--cflag=-DSQL_STACK=%d' % STACK])
    # sql.mod, the SQL command's module, in a folder of its own (build.py's objects and map are named after it): its
    # heap is what SQL OPEN gives it, its stack MOD_STACK of the SRAM heap (the header's stack_size: misc/Loader.c)
    os.makedirs(os.path.join(out, 'mod'), exist_ok=True)
    build(os.path.join(out, 'mod', 'sql.mod'), core + [os.path.join(HERE, 'sql_mod.c')],
          ['--api', '1', '--psram', '--module', '--stack', str(MOD_STACK)])


if __name__ == '__main__':
    main()
