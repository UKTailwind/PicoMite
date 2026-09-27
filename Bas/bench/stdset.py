"""stdset.py PORT pc3|vga OUT.json [--skip-put] [--symbols-off] [--compile|--shadow] [--only TEST ...]

The standard set every Route A step is measured with: resets the board (so no
OPTION LOCAL VARIABLES or MODE left by an earlier program leaks in), uploads
the programs, runs each, and writes every figure to OUT.json:
  metrics  {"test:label": cycles or ms}   what stdcmp.py compares
  checks   {"test": "CHECK/GAMECHECK/golden lines"}   must never change
--compile runs everything with OPTION COMPILE ON (Route B) and keeps what
MM.INFO(COMPILE) said after each test in "compile", to show it ran compiled;
--shadow does the same with OPTION COMPILE SHADOW (a compiled result that
differs from the text evaluator's stops the program with SHADOW: ...).
Games (PC3 only) expect Bas/bench/phase1/games' programs already in A:/gb/
(gamebench.py put); nothing of the games' data is uploaded from here.
"""
import json, os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
P1 = os.path.join(HERE, "phase1")
PY = sys.executable
SE = os.path.join(HERE, "se_bench.py")
RUNMACRO = os.path.join(P1, "macros", "runmacro.py")
GAMEBENCH = os.path.join(P1, "games", "gamebench.py")

UPLOAD = [(os.path.join(HERE, "..", "solar_eclipse.bas"), "A:/se.bas"),
          (os.path.join(HERE, "bench_micro.bas"), "A:/bench_micro.bas"),
          (os.path.join(P1, "bench_micro2.bas"), "A:/bench_micro2.bas"),
          (os.path.join(P1, "bench_subs0.bas"), "A:/bench_subs0.bas"),
          (os.path.join(P1, "bench_subs200.bas"), "A:/bench_subs200.bas"),
          (os.path.join(P1, "bench_heap.bas"), "A:/bench_heap.bas"),
          (os.path.join(P1, "bench_gfx.bas"), "A:/bench_gfx.bas"),
          (os.path.join(P1, "macros", "knivd_fixed.bas"), "A:/knivd_fixed.bas"),
          (os.path.join(P1, "macros", "julia_interp.bas"), "A:/julia_interp.bas"),
          (os.path.join(P1, "macros", "gfx_frames.bas"), "A:/gfx_frames.bas")]

BAS = ["bench_micro", "bench_micro2", "bench_subs0", "bench_subs200", "bench_heap", "bench_gfx",
       "knivd_fixed", "julia_interp", "gfx_frames"]
GAMES = [("exile", "gbxphys.bas", []), ("elite", "gbelite.bas", []), ("pop", "gbpop.bas", ["--crunch"])]


def sh(args, timeout):
    p = subprocess.run([PY] + args, capture_output=True, text=True, timeout=timeout, encoding="utf-8", errors="replace")
    return p.stdout + p.stderr


def restart(port):
    try:
        sh([SE, port, "cmd", "CPU RESTART"], 20)
    except subprocess.TimeoutExpired:
        pass    # the board resets under the command; no prompt comes back
    time.sleep(8)
    for _ in range(20):
        try:
            out = sh([SE, port, "cmd", "Print MM.Info(CPUSPEED)"], 30)
            if re.search(r"\b\d{9}\b", out):
                return re.search(r"\b\d{9}\b", out).group(0)
        except Exception:
            pass
        time.sleep(3)
    sys.exit("board did not come back after CPU RESTART")


def main():
    a = sys.argv[1:]
    if len(a) < 3:
        sys.exit(__doc__)
    port, board, outf = a[:3]
    skip_put = "--skip-put" in a
    only = a[a.index("--only") + 1:] if "--only" in a else None
    res = {"board": board, "port": port, "started": time.strftime("%Y-%m-%d %H:%M:%S"),
           "metrics": {}, "checks": {}, "errors": []}
    res["cpuspeed"] = restart(port)
    print("restarted, cpuspeed", res["cpuspeed"], flush=True)
    if "--symbols-off" in a:
        # save every program as text for an A/B of symbols on the same firmware (not kept over a reset)
        sh([SE, port, "cmd", "OPTION SYMBOLS OFF"], 30)
        res["symbols"] = "off"
    compiled = "--compile" in a or "--shadow" in a
    if compiled:
        # Route B: RAM-only, so it is set after the reset and lasts until the next
        sh([SE, port, "cmd", "OPTION COMPILE " + ("SHADOW" if "--shadow" in a else "ON")], 30)
        res["compile"] = {}

    def compile_status(t):
        if compiled:
            m = re.search(r"(COMPILED|NONE|TEXT|OFF)[^\r\n]*", sh([SE, port, "cmd", "Print MM.Info(COMPILE)"], 30))
            res["compile"][t] = m.group(0).strip() if m else "?"
            print("  compile:", res["compile"][t], flush=True)
    sh([SE, port, "cmd", "LIBRARY DELETE"], 60)
    if not skip_put:
        args = [SE, port, "put"]
        for loc, dev in UPLOAD:
            args += [loc, dev]
        print(sh(args, 900).strip().splitlines()[-1], flush=True)

    def want(t):
        return only is None or t in only

    if want("anchor"):
        out = sh([SE, port, "run", "A:/se.bas", "2", "stdset_anchor"], 900)
        secs, gold = [], []
        for ln in out.splitlines():
            if ln.startswith("{"):
                j = json.loads(ln)
                secs.append(j["seconds"]); gold.append(bool(j.get("golden")))
        if secs:
            res["metrics"]["anchor:seconds"] = min(secs)
            res["checks"]["anchor"] = "golden" if all(gold) and len(gold) == 2 else "NOT GOLDEN"
        else:
            res["errors"].append("anchor: " + out[-300:])
        print("anchor", secs, gold, flush=True)
        compile_status("anchor")

    for t in BAS:
        if not want(t):
            continue
        tmp = outf + "." + t + ".txt"
        out = sh([RUNMACRO, port, "A:/%s.bas" % t, tmp, "--timeout", "800"], 900)
        text = open(tmp, encoding="utf-8", errors="replace").read() if os.path.exists(tmp) else ""
        n = 0
        for ln in text.splitlines():
            m = re.match(r"BENCH (.*?) (-?[\d.]+) (-?\d+)\s*$", ln.strip())
            if m:
                res["metrics"]["%s:%s" % (t, m.group(1))] = int(m.group(3)); n += 1
        chk = [ln.strip() for ln in text.splitlines() if ln.startswith("CHECK")]
        if chk:
            res["checks"][t] = " | ".join(chk)
        if re.search(r"\bError\b", text) or "BENCHEND" not in text:
            res["errors"].append("%s: %s" % (t, " ".join(text.split())[-300:]))
        print(t, n, "figures", "ERROR" if res["errors"] and res["errors"][-1].startswith(t) else "", flush=True)
        compile_status(t)

    if board == "pc3":
        for name, prog, extra in GAMES:
            if not want(name):
                continue
            out = sh([GAMEBENCH, port, "run", prog, "--runs", "1", "--label", os.path.abspath(outf) + "." + name, "--timeout", "900"] + extra, 1000)
            for ln in out.splitlines():
                if ln.startswith("{"):
                    j = json.loads(ln)
                    for k in ("gamebench", "gamekernel", "gameframe"):
                        if j.get(k):
                            res["metrics"]["%s:%s_ms_per_frame" % (name, k)] = j[k]["ms_per_frame"]
                    res["checks"][name] = " | ".join(j.get("check") or [])
                    if j.get("error") or j.get("stopped"):
                        res["errors"].append("%s: %s %s" % (name, j.get("error"), j.get("stopped")))
            if name not in res["checks"]:  # gamebench failed before a run: keep what it said
                res["errors"].append("%s: %s" % (name, " ".join(out.split())[-300:]))
            print(name, res["checks"].get(name, "NO RESULT"), flush=True)
            compile_status(name)

    res["finished"] = time.strftime("%Y-%m-%d %H:%M:%S")
    json.dump(res, open(outf, "w"), indent=1)
    # leave the board as a reset left it: Elite and Prince of Pico set OPTION
    # LOCAL VARIABLES 128 and the graphics programs change MODE, both of which
    # last until a reset and broke a goldens run that followed
    restart(port)
    print("wrote", outf, len(res["metrics"]), "metrics,", len(res["errors"]), "errors")
    for e in res["errors"]:
        print("ERROR", e)


if __name__ == "__main__":
    main()
