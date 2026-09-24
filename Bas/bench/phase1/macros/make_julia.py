"""make_julia.py - build the CSUB half of the Julia pair from the interpreted half.

    python make_julia.py

1. julia_csub.bas = julia_interp.bas with HALF$ = "csub" and REPS% = 20;
2. mmb2csub.py converts its plotjulia into a CSUB in the library file:
       mmb2csub.py julia_csub.bas plotjulia --library julialib.bas --lean
   which comments the SUB out of julia_csub.bas (/* ... */) and writes the
   CSUB to julialib.bas;
3. the blob is compared word for word with the one committed in
   mmb2csub/examples/julia.bas (same kernel, same tool), and julia_expect.py
   prints the CHECK value both halves must print.

Everything is written to this directory (the conversion runs in work/, with
the compiler's temporary files in work/tmp); nothing in the repository is
touched. Needs arm-none-eabi-gcc on PATH and pyelftools, as mmb2csub does.
"""
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HERE, "work")
TOOLS = r"D:/Dropbox/PicoMite/PicoMite/mmb2csub/user-tools"
EXAMPLE = r"D:/Dropbox/PicoMite/PicoMite/mmb2csub/examples/julia.bas"


def crlf(path):
    t = open(path, encoding="latin-1").read().replace("\r\n", "\n")
    open(path, "w", encoding="latin-1", newline="\r\n").write(t)


def blob_words(text, name):
    m = re.search(r"CSUB %s\b.*?\n(.*?)End CSUB" % name, text, re.S | re.I)
    if not m:
        return None
    words = []
    for ln in m.group(1).splitlines():
        ln = ln.strip()
        if not ln or ln.startswith("'"):
            continue
        words += ln.split()
    return words


def main():
    os.makedirs(os.path.join(WORK, "tmp"), exist_ok=True)
    src = open(os.path.join(HERE, "julia_interp.bas"), encoding="latin-1").read().replace("\r\n", "\n")
    out, n = re.subn(r'Const HALF\$ = "interp"', 'Const HALF$ = "csub"', src)
    assert n == 1
    out, n = re.subn(r"Const REPS% = 1 ", "Const REPS% = 20", out)
    assert n == 1
    wsrc = os.path.join(WORK, "julia_csub.bas")
    wlib = os.path.join(WORK, "julialib.bas")
    open(wsrc, "w", encoding="latin-1", newline="\r\n").write(out)
    if os.path.exists(wlib):
        os.remove(wlib)
    env = dict(os.environ, TMP=os.path.join(WORK, "tmp"), TEMP=os.path.join(WORK, "tmp"),
               PYTHONDONTWRITEBYTECODE="1")
    cmd = [sys.executable, os.path.join(TOOLS, "mmb2csub.py"), "julia_csub.bas", "plotjulia",
           "--library", "julialib.bas", "--lean", "--no-backup"]
    print(">", " ".join(cmd[1:]))
    r = subprocess.run(cmd, cwd=WORK, env=env, capture_output=True, text=True)
    print(r.stdout.strip())
    if r.returncode:
        print(r.stderr)
        sys.exit("mmb2csub failed")
    for f in ("julia_csub.bas", "julialib.bas"):
        shutil.copyfile(os.path.join(WORK, f), os.path.join(HERE, f))
        crlf(os.path.join(HERE, f))
    lib = open(os.path.join(HERE, "julialib.bas"), encoding="latin-1").read()
    prog = open(os.path.join(HERE, "julia_csub.bas"), encoding="latin-1").read()
    mine = blob_words(lib, "plotjulia")
    ref = blob_words(open(EXAMPLE, encoding="latin-1").read(), "PlotJulia")
    print("blob: %d words; committed example: %d words; identical: %s"
          % (len(mine), len(ref or []), mine == ref))
    print("julia_csub.bas: SUB plotjulia commented out:",
          "/*" in prog and re.search(r"^\s*sub plotjulia", prog, re.I | re.M) is not None)
    r = subprocess.run([sys.executable, os.path.join(HERE, "julia_expect.py")], capture_output=True,
                       text=True, env=env)
    print(r.stdout.strip())


if __name__ == "__main__":
    main()
