"""runmacro.py PORT DEVPATH OUTFILE [--lib DEVPATH] [--libdelete] [--timeout S]

LOAD a Phase 1 macro from the board's drive, RUN it, and save everything it
prints until the prompt comes back. It is runbas.py (Bas/bench) with three
additions:

  --libdelete      LIBRARY DELETE before loading - needed before an interpreted
                   program whose SUB is also a CSUB in the library
                   (julia_interp.bas after julia_csub.bas)
  --lib DEVPATH    NEW, then LIBRARY LOAD "DEVPATH", O, before loading
                   (julia_csub.bas). The NEW stops a SUB of the program still
                   in memory clashing with the library (Duplicate name).
  paging           the [PERF]/[PCS] report that END prints when PROFILE% = 1
                   stops at "PRESS ANY KEY ..." once a screen of lines is out
                   (core/Commands.c perf_print); the driver presses a space.
                   The timing runs (PROFILE% = 0) never page.

It stops as soon as the prompt returns, on an error message, or on an
unexpected INPUT prompt (Ctrl-C), and only otherwise on --timeout (600 s).
The BENCH/CHECK/ELAPSED lines are printed at the end for a quick look.
"""
import re
import sys
import time

sys.path.insert(0, r"D:/Dropbox/PicoMite/PicoMite/Bas/elite_tools")
import pc3  # noqa: E402

ANSI = pc3.ANSI


def paged(b, line, timeout):
    """Send a command line and collect its output up to the prompt, pressing a
    key at every 'PRESS ANY KEY'. Returns (text, how it ended)."""
    b.drain(0.1)
    b.send_line(line)
    buf, t0 = "", time.time()
    while time.time() - t0 < timeout:
        t = b._read()
        if not t:
            continue
        buf += t
        clean = ANSI.sub("", buf)
        tail = clean[-200:]
        if re.search(r"any key", tail, re.I):
            time.sleep(0.1)
            b.s.write(b" ")
            buf = re.sub(r"(?i)any key", "any-key", buf)
            continue
        if re.search(r"(\n|any-key \.\.\. *)> ?$", clean):
            time.sleep(0.05)
            more = b._read()
            if not more:
                return clean, "error" if re.search(r"\bError\b", clean) else "prompt"
            buf += more
            continue
        if clean.rstrip(" ").endswith("?"):
            time.sleep(0.5)
            if not b._read():
                b.s.write(b"\x03")
                time.sleep(0.5)
                return ANSI.sub("", buf + b.drain(0.5)), "input prompt - stopped with Ctrl-C"
    return ANSI.sub("", buf), "timeout after %.0f s" % timeout


def main():
    args = sys.argv[1:]
    if len(args) < 3:
        sys.exit(__doc__)
    port, dev, outf = args[:3]
    lib, delete, timeout = None, False, 600.0
    i = 3
    while i < len(args):
        if args[i] == "--lib":
            lib = args[i + 1]
            i += 2
        elif args[i] == "--libdelete":
            delete = True
            i += 1
        elif args[i] == "--timeout":
            timeout = float(args[i + 1])
            i += 2
        else:
            sys.exit("unknown option " + args[i])
    b = pc3.PC3(port)
    try:
        b.attention()
        if delete:
            print("--- LIBRARY DELETE:", b.cmd("LIBRARY DELETE", 60).strip()[-200:])
        if lib:
            # NEW first. When the library changes, LIBRARY LOAD re-runs
            # PrepareProgram over the library AND the program still in memory
            # (core/MM_Misc.c, LIBRARY LOAD). After julia_interp.bas that
            # program has SUB plotjulia, which clashes with the library's CSUB:
            # "Error: Duplicate name" on the first --lib run on both boards.
            # NEW leaves the library alone (ClearProgram), and LOAD follows.
            print("--- NEW:", b.cmd("NEW", 60).strip()[-200:])
            out, how = paged(b, 'LIBRARY LOAD "%s", O' % lib, 180)
            print("--- LIBRARY LOAD (%s):" % how, " ".join(out.split())[-200:])
            if how != "prompt":
                sys.exit("library load did not finish cleanly")
        print("--- LOAD:", b.cmd('LOAD "%s"' % dev, 120).strip()[-200:])
        out, how = paged(b, "RUN", timeout)
        open(outf, "w", encoding="utf-8").write(out)
        print("--- RUN ended:", how, "- saved", outf)
        for ln in out.splitlines():
            if re.match(r"^(BENCHSTART|BENCH |BENCHEND|CHECK |ELAPSED |\[PCS\] samples)", ln) \
                    or re.search(r"\bError\b", ln):
                print(ln)
        if "[PERF]" in out and "[PCS] samples" not in out:
            # seen twice on the RP2040 in the first runs (gfx_frames_prof and
            # julia_interp_prof): [PERF] printed, the sampler section did not
            print("WARNING: [PERF] but no [PCS] section - the sampler was not "
                  "running at END. Reset the board and run this profile again.")
    finally:
        b.close()


if __name__ == "__main__":
    main()
