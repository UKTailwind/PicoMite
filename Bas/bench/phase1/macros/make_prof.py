"""make_prof.py - write the sampled-run copies of the macros.

    python make_prof.py

Each X.bas becomes X_prof.bas with "Const PROFILE% = 1", which starts the PC
sampler right after "Timer = 0"; nothing else changes, so the line numbers
the [PCSLINE] report gives are the same in both. gfx_frames also gets
gfx_frames_nofb_prof.bas (FBUF% = 0): without FRAMEBUFFER F the RP2040 has
38,400 more bytes of heap and the sampler gets 4096 entries instead of 2048.
Run the timing copies for times and the _prof copies only for shares.
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
PROGS = ["knivd_fixed", "julia_interp", "julia_csub", "gfx_frames"]


def write(name, text):
    open(os.path.join(HERE, name), "w", encoding="latin-1", newline="\r\n").write(text)
    print("wrote", name)


def main():
    for p in PROGS:
        src = open(os.path.join(HERE, p + ".bas"), encoding="latin-1").read().replace("\r\n", "\n")
        out, n = re.subn(r"^Const PROFILE% = 0", "Const PROFILE% = 1", src, flags=re.M)
        assert n == 1, p
        write(p + "_prof.bas", out)
        if p == "gfx_frames":
            out2, n = re.subn(r"^Const FBUF% = 1", "Const FBUF% = 0", out, flags=re.M)
            assert n == 1
            write(p + "_nofb_prof.bas", out2)


if __name__ == "__main__":
    main()
