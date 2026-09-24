"""gamebench.py - put the Phase 1 game workloads on the PC3 and run them.

usage:
  gamebench.py PORT put exile|elite|pop [--popdata DIR] [--only NAME ...]
  gamebench.py PORT run PROG [--crunch] [--runs N] [--label L] [--libdel] [--nocheck] [--timeout S]
  gamebench.py PORT cmd "LINE" ["LINE" ...]

'put' makes A:/gb/ and XMODEMs the workload's files into it (MANIFEST.md
lists them).  The Prince of Pico data is NOT in this directory - it is the
game's own content, converted locally - so 'put pop' reads it from --popdata
(default C:/Users/peter/AppData/Local/Temp/claude/popdata).

'run' sends OPTION NOCHECK OFF (ON with --nocheck: the flag outlives programs
and cannot be read back), LOADs A:/gb/PROG once (",C" with --crunch, which
gbpop needs), then RUNs it --runs times.  Every run is watched until the prompt comes back: the
[PERF]/[PCS] report pages ("PRESS ANY KEY"), which this answers; an error or
an unexpected INPUT prompt stops the run at once.  One JSON line per run with
the GAMEBENCH / GAMENULL / GAMEKERNEL / GAMECHECK figures; the raw output of
every run is saved as <label>_<n>.txt (the [PCS] lines are what
Bas/bench/pcs_report.py wants, with the ELF of the build that was flashed).
"""
import json
import os
import re
import sys
import time

sys.dont_write_bytecode = True      # pc3.py lives in the repository: leave no cache there
sys.path.insert(0, r"D:/Dropbox/PicoMite/PicoMite/Bas/elite_tools")
import pc3  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
DEV = "A:/gb/"
POPDATA = r"C:/Users/peter/AppData/Local/Temp/claude/popdata"
ANSI = pc3.ANSI

FILES = {
    "exile": [("exile", f) for f in ("gbx_world.bin", "gbx_tables.bin", "gbx_feed.txt",
                                     "gbxphys.bas", "gbxphysp.bas", "gbxcsub.bas", "gbxcsubp.bas")],
    "elite": [("elite", f) for f in ("gbelite_lib.bas", "gbelite.bas", "gbelitep.bas")],
    "pop": [("pop", f) for f in ("gbscen.txt", "gbpop.bas", "gbpopp.bas")]
           + [(None, f) for f in ("tables.idx", "blocks.dat", "seq.dat", "frames.dat", "art.bin",
                                  "sounds.dat", "levels.dat", "sheet1.bmp", "sheet2.bmp",
                                  "sheet3.bmp", "sheet4.bmp")],
}


def board(port):
    b = pc3.PC3(port)
    b.attention()
    return b


def watch(b, timeout):
    """Collect output until the prompt returns; answer paging; stop on an error or INPUT."""
    buf, t0, stop = "", time.time(), None
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
            buf = buf.replace("ANY KEY", "any-key").replace("any key", "any-key")
            continue
        if re.search(r"\n> ?$", clean):
            return clean, stop
        if clean.rstrip(" ").endswith("?"):
            time.sleep(0.2)
            if not b._read():
                b.s.write(b"\x03")
                stop = "unexpected INPUT prompt - stopped"
                time.sleep(0.5)
                return ANSI.sub("", buf + b.drain(0.5)), stop
    return ANSI.sub("", buf), "no prompt after %.0f s - still running? (not interrupted)" % timeout


def parse(out):
    r = {}
    for key in ("GAMEBENCH", "GAMENULL", "GAMEKERNEL", "GAMEFRAME"):
        m = re.search(r"^%s (\S+) (\d+) ([0-9.]+) ([0-9.]+)" % key, out, re.M)
        if m:
            r[key.lower()] = {"name": m.group(1), "frames": int(m.group(2)),
                              "ms": float(m.group(3)), "ms_per_frame": float(m.group(4))}
    r["start"] = (re.findall(r"^GAMESTART .*$", out, re.M) or [None])[0]
    r["check"] = re.findall(r"^GAMECHECK .*$", out, re.M)
    r["diff"] = re.findall(r"^GAMEDIFF .*$", out, re.M)
    m = re.search(r"\[PERF\] elapsed=(\d+) us\s+statements=(\d+)\s+findvar=(\d+).*user_subs=(\d+)", out)
    if m:
        r["perf"] = {"elapsed_us": int(m.group(1)), "statements": int(m.group(2)),
                     "findvar": int(m.group(3)), "user_subs": int(m.group(4))}
    m = re.search(r"\[PCS\] samples=(\d+) dropped=(\d+)", out)
    if m:
        r["pcs"] = {"samples": int(m.group(1)), "dropped": int(m.group(2))}
    err = re.search(r"^.*Error.*$", out, re.M)
    r["error"] = err.group(0).strip() if err else None
    return r


def main():
    port, what = sys.argv[1], sys.argv[2]
    args = sys.argv[3:]
    b = board(port)
    try:
        if what == "cmd":
            for c in args:
                print("--- " + c)
                print(b.cmd(c, 60))
        elif what == "put":
            wl = args[0]
            popdata = args[args.index("--popdata") + 1] if "--popdata" in args else POPDATA
            only = args[args.index("--only") + 1:] if "--only" in args else None
            print(b.cmd('MKDIR "%s"' % DEV.rstrip("/"), 20).strip() or "A:/gb made")
            total, t00 = 0, time.time()
            for sub, f in FILES[wl]:
                if only and f not in only:
                    continue
                src = os.path.join(popdata if sub is None else os.path.join(HERE, sub), f)
                data = open(src, "rb").read()
                t0 = time.time()
                b.xmodem_send(DEV + f, data)
                total += len(data)
                print("put %-18s %7d bytes %6.1f s" % (f, len(data), time.time() - t0), flush=True)
            print("%d bytes in %.0f s" % (total, time.time() - t00))
        elif what == "run":
            prog = args[0]
            runs = int(args[args.index("--runs") + 1]) if "--runs" in args else 1
            label = args[args.index("--label") + 1] if "--label" in args else os.path.splitext(prog)[0]
            timeout = float(args[args.index("--timeout") + 1]) if "--timeout" in args else 900.0
            if "--libdel" in args:
                print("--- LIBRARY DELETE:", " ".join(b.cmd("LIBRARY DELETE", 60).split())[-160:])
            # OPTION NOCHECK is a runtime flag that nothing resets but OFF or a
            # reboot (core/MM_Misc.c:5227-5238), and it cannot be read back, so
            # a NOCHECK A/B left on the board would silently change every
            # timing.  Set it explicitly on every run; --nocheck asks for ON.
            nocheck = "ON" if "--nocheck" in args else "OFF"
            b.cmd("OPTION NOCHECK " + nocheck, 10)
            load = 'LOAD "%s%s"%s' % (DEV, prog, ", C" if "--crunch" in args else "")
            reply = b.cmd(load, 180)
            print("--- %s: %s" % (load, " ".join(reply.split())[-200:]))
            if "rror" in reply:
                print("the program did not load - the board still holds the previous one; stopping")
                return 2
            info = b.cmd('PRINT MM.DEVICE$; " "; MM.VER; " "; MM.INFO(CPUSPEED)', 10).strip()
            for n in range(1, runs + 1):
                b.drain(0.1)
                b.send_line("RUN")
                t0 = time.time()
                out, stop = watch(b, timeout)
                wall = time.time() - t0
                fn = os.path.join(HERE, "%s_%d.txt" % (label, n))
                open(fn, "w", encoding="utf-8").write(out)
                r = parse(out)
                r.update({"label": label, "run": n, "device": info, "nocheck": nocheck,
                          "wall_s": round(wall, 1), "stopped": stop, "raw": fn})
                print(json.dumps(r), flush=True)
                if stop or r["error"]:
                    return 1
        else:
            print(__doc__)
            return 2
    finally:
        b.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
