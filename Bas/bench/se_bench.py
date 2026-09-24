"""Phase 0 anchor driver for MMBasic boards.

usage:
  se_bench.py PORT put LOCAL DEVPATH [LOCAL DEVPATH ...]
  se_bench.py PORT cmd "LINE" ["LINE" ...]
  se_bench.py PORT run DEVPATH RUNS [LABEL] [--lib DEVPATH] [--pre "LINE"]...

'run' LOADs DEVPATH once (after an optional LIBRARY LOAD), then RUNs it RUNS
times, answering the five INPUT prompts with the reference inputs, and prints
one JSON line per run: label, seconds, the JD digits, and a pass flag against
the golden output.
"""
import json, re, sys, time
sys.path.insert(0, r"D:/Dropbox/PicoMite/PicoMite/Bas/elite_tools")
import pc3

INPUTS = ["12,1,2000", "39,40,36", "-104,57,12", "1644", "30"]
GOLDEN = ["2451904.14541560", "2451904.19690359", "2451904.25420764"]
ANSI = pc3.ANSI


def board(port):
    b = pc3.PC3(port)
    b.attention()
    return b


def cmd_paged(b, line, timeout=180.0):
    """Like pc3.cmd, but answers the console's 'press any key' paging, which a
    long listing (LIBRARY LOAD lists the library it loaded) stops at."""
    b.drain(0.1)
    b.send_line(line)
    buf, t0 = "", time.time()
    while time.time() - t0 < timeout:
        t = b._read()
        if not t:
            continue
        buf += t
        clean = ANSI.sub("", buf)
        if re.search(r"any key", clean[-200:], re.I):
            time.sleep(0.1)
            b.s.write(b" ")
            buf = buf.replace("ANY KEY", "any-key").replace("any key", "any-key")
            continue
        if re.search(r"\n> ?$", clean):
            return clean
    raise TimeoutError("no prompt after %s: %s" % (line, ANSI.sub("", buf)[-400:]))


def run_once(b, timeout=240.0):
    b.drain(0.1)
    b.send_line("RUN")
    buf, sent, t0 = "", 0, time.time()
    while time.time() - t0 < timeout:
        t = b._read()
        if not t:
            continue
        buf += t
        clean = ANSI.sub("", buf)
        if re.search(r"any key", clean[-200:], re.I):
            time.sleep(0.1)
            b.s.write(b" ")
            buf = buf.replace("ANY KEY", "any-key").replace("any key", "any-key")
            continue
        if clean.rstrip(" ").endswith("?"):
            time.sleep(0.05)
            if not b._read():
                if sent == len(INPUTS):  # asked for more than the reference inputs: give up now
                    b.s.write(b"\x03")
                    buf += "\n[driver: unexpected INPUT prompt - stopped]\n"
                    time.sleep(0.5)
                    buf += b.drain(0.5)
                    break
                b.send_line(INPUTS[sent])
                sent += 1
                buf += "\n"
            continue
        if re.search(r"\n> ?$", clean) and (sent == len(INPUTS) or "Error" in clean):
            break  # back at the prompt: finished, or stopped by an error - never wait out the timeout
    clean = ANSI.sub("", buf)
    m = re.search(r"Time taken\s*:\s*([0-9.]+)", clean)
    jds = re.findall(r"24519\d\d\.\d+", clean)
    err = re.search(r"Error[^\n]*", clean)
    return {
        "seconds": float(m.group(1)) if m else None,
        "jd": jds,
        "golden": all(g in clean for g in GOLDEN),
        "error": err.group(0) if err else None,
        "tail": clean[-300:] if not m else "",
        "raw": clean,
    }


def main():
    port, what = sys.argv[1], sys.argv[2]
    b = board(port)
    try:
        if what == "put":
            args = sys.argv[3:]
            for i in range(0, len(args), 2):
                data = open(args[i], "rb").read()
                b.xmodem_send(args[i + 1], data)
                print("put %s -> %s (%d bytes)" % (args[i], args[i + 1], len(data)))
        elif what == "cmd":
            for c in sys.argv[3:]:
                print("--- " + c)
                print(b.cmd(c, 120))
        elif what == "run":
            dev, runs = sys.argv[3], int(sys.argv[4])
            rest = sys.argv[5:]
            label = rest[0] if rest and not rest[0].startswith("--") else dev
            lib, pre = None, []
            i = 0
            while i < len(rest):
                if rest[i] == "--lib":
                    lib = rest[i + 1]; i += 2
                elif rest[i] == "--pre":
                    pre.append(rest[i + 1]); i += 2
                else:
                    i += 1
            if lib:
                out = cmd_paged(b, 'LIBRARY LOAD "%s", O' % lib, 180)
                print("--- library:", " ".join(out.split())[-160:])
            print("--- load:", b.cmd('LOAD "%s"' % dev, 120).strip()[-200:])
            for p in pre:
                print("--- pre:", p, b.cmd(p, 30).strip()[-200:])
            dev_info = b.cmd("PRINT MM.DEVICE$; \" \"; MM.VER; \" \"; MM.INFO(CPUSPEED)", 10).strip()
            for n in range(runs):
                r = run_once(b)
                r.update({"label": label, "run": n + 1, "device": dev_info})
                raw = r.pop("raw")
                if label.endswith("-prof"):
                    open(label + ".txt", "w", encoding="utf-8").write(raw)
                print(json.dumps(r), flush=True)
    finally:
        b.close()


if __name__ == "__main__":
    main()
