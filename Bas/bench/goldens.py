"""goldens.py PORT OUTDIR [--corpus DIR] [--only NAME,...] [--put] [--dir B:/g]

Run the mmb2c test corpus on a board and keep what this firmware prints as
the golden for later builds, and compare it with each program's .expected.

The corpus lives in A:/g/ between runs (--dir elsewhere, e.g. B:/g when a small
A: cannot hold it): only files missing there, or of a
different size, are sent (--put sends them all).  For every program: CHDIR
to A:/g/, LOAD, RUN twice.  The
driver watches every byte and ends a run as soon as it is over: back at the
prompt, an Error, or an INPUT it has no answer for (Ctrl-C).  It answers
INPUT from <name>.in and PRESS ANY KEY with a space.  A 60 s cap only
catches a program that really does run away.

Writes OUTDIR/<name>.out and .out2, <name>.diff where it differs from
.expected, and OUTDIR/SUMMARY.md.
"""
import difflib, json, os, re, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

CORPUS = r"\\wsl.localhost\Ubuntu\home\peter\src\FUZIX\Applications\mmb2c\tests"
ANSI = pc3.ANSI
CAP = 60.0


def norm(text, inputs=()):
    """What the corpus Makefile compares: no CR, no 'Time taken' lines.  A
    board also echoes what is typed at an INPUT, which the translator's
    stdin does not, so the echoed answers (and the blank line after each)
    come out too; so do leading and trailing blank lines."""
    lines = [l.rstrip() for l in text.replace("\r", "").split("\n")]
    if inputs:
        out, want = [], list(inputs)
        i = 0
        while i < len(lines):
            if want and lines[i].strip() == want[0].strip():
                want.pop(0)
                i += 1
                if i < len(lines) and lines[i] == "":
                    i += 1
                continue
            out.append(lines[i])
            i += 1
        lines = out
    lines = [l for l in lines if "Time taken" not in l]
    while lines and not lines[-1]:
        lines.pop()
    while lines and not lines[0]:
        lines.pop(0)
    return "\n".join(lines)


def recompare(outdir, corpus):
    """Re-score saved outputs against .expected without running anything."""
    rows = json.load(open(os.path.join(outdir, "summary.json")))
    for r in rows:
        n = r["name"]
        outf = os.path.join(outdir, n + ".out")
        exp = os.path.join(corpus, n + ".expected")
        if r["class"] not in ("MATCH", "DIFF") or not os.path.exists(outf) or not os.path.exists(exp):
            continue
        inp = os.path.join(corpus, n + ".in")
        inputs = open(inp, encoding="latin-1").read().splitlines() if os.path.exists(inp) else []
        got = norm(open(outf, encoding="utf-8").read(), inputs)
        want = norm(open(exp, encoding="latin-1").read())
        dpath = os.path.join(outdir, n + ".diff")
        if got == want:
            r["class"] = "MATCH"
            if os.path.exists(dpath):
                os.remove(dpath)
        else:
            r["class"] = "DIFF"
            d = difflib.unified_diff(want.split("\n"), got.split("\n"), "expected", "board", lineterm="")
            open(dpath, "w", encoding="utf-8").write("\n".join(d))
    json.dump(rows, open(os.path.join(outdir, "summary.json"), "w"), indent=1)
    return rows


def run_program(b, inputs):
    """RUN the loaded program; return (class, output)."""
    b.drain(0.2)
    b.send_line("RUN")
    buf, t0, fed, last = "", time.time(), 0, time.time()
    status = None
    while True:
        t = b._read()
        now = time.time()
        if t:
            buf += t
            last = now
        clean = ANSI.sub("", buf)
        tail = clean[-200:]
        if re.search(r"PRESS ANY KEY", tail):
            time.sleep(0.1)
            b.s.write(b" ")
            buf = buf.replace("PRESS ANY KEY", "PRESS-ANY-KEY")
            continue
        quiet = now - last > 0.3
        if quiet and re.search(r"\n> ?$", clean):
            status = "ERROR" if re.search(r"\bError\b", clean) else "OK"
            break
        if quiet and clean.rstrip(" ").endswith("?"):
            if fed < len(inputs):
                b.send_line(inputs[fed])
                fed += 1
                buf += "\n"
                last = time.time()
                continue
            b.s.write(b"\x03")
            status = "NEEDS-INPUT"
            time.sleep(0.5)
            buf += b.drain(0.5)
            break
        if now - t0 > CAP:
            b.s.write(b"\x03")
            time.sleep(0.3)
            b.s.write(b"\x03")
            status = "TIMEOUT"
            time.sleep(0.5)
            buf += b.drain(0.5)
            break
        if not t:
            time.sleep(0.01)
    clean = ANSI.sub("", buf)
    # drop the echoed RUN and the final prompt
    clean = re.sub(r"^\s*RUN\s*\n", "", clean)
    clean = re.sub(r"\n> ?$", "\n", clean)
    return status, clean, round(time.time() - t0, 1)


def main():
    if sys.argv[1] == "--recompare":
        outdir = sys.argv[2]
        corpus = sys.argv[sys.argv.index("--corpus") + 1] if "--corpus" in sys.argv else CORPUS
        device = open(os.path.join(outdir, "SUMMARY.md"), encoding="utf-8").readline().replace("# Goldens on", "").strip()
        rows = recompare(outdir, corpus)
        write_summary(outdir, rows, device)
        counts = {}
        for r in rows:
            counts[r["class"]] = counts.get(r["class"], 0) + 1
        print(counts)
        return
    port, outdir = sys.argv[1], sys.argv[2]
    corpus = sys.argv[sys.argv.index("--corpus") + 1] if "--corpus" in sys.argv else CORPUS
    only = sys.argv[sys.argv.index("--only") + 1].split(",") if "--only" in sys.argv else None
    gdir = sys.argv[sys.argv.index("--dir") + 1].rstrip("/") if "--dir" in sys.argv else "A:/g"
    os.makedirs(outdir, exist_ok=True)
    names = sorted(f[:-4] for f in os.listdir(corpus) if f.endswith(".bas"))
    if only:
        names = [n for n in names if n in only]
    b = pc3.PC3(port)
    b.attention()
    device = b.cmd('PRINT MM.DEVICE$; " "; MM.VER; " "; MM.INFO(CPUSPEED)', 10).strip()
    if gdir[1:2] == ":":
        b.cmd('DRIVE "%s"' % gdir[:2], 10)   # CHDIR does not change the drive, and LOAD takes plain names
    b.cmd('MKDIR "%s"' % gdir, 10)
    b.cmd('CHDIR "%s"' % gdir, 10)
    # The corpus stays on A:/g between runs. Send a file only if the board lacks
    # it or has a different size (a firmware update that moved the flash offset
    # wipes A:), or everything with --put.
    onboard = {}
    if "--put" not in sys.argv:
        listing = b.cmd('f$=Dir$("%s/*",FILE):Do While f$<>"":Print f$;"|";MM.INFO(FILESIZE "%s/"+f$):f$=Dir$():Loop' % (gdir, gdir), 60)
        for line in listing.splitlines():
            if "|" in line:
                name, size = line.rsplit("|", 1)
                if size.strip().isdigit():
                    onboard[name.strip().lower()] = int(size)
    files = [n + ".bas" for n in names]   # every file first: some programs run or read others
    files += [f for f in os.listdir(corpus) if f.endswith((".dat", ".txt", ".csv"))]
    sent = 0
    for f in files:
        data = open(os.path.join(corpus, f), "rb").read()
        if onboard.get(f.lower()) != (len(data) + 127) // 128 * 128:  # XMODEM pads to 128-byte blocks
            b.xmodem_send(gdir + "/" + f, data)
            sent += 1
    print("sent %d of %d files to %s" % (sent, len(files), gdir), flush=True)
    rows = []
    for n in names:
        inp = os.path.join(corpus, n + ".in")
        inputs = open(inp, encoding="latin-1").read().splitlines() if os.path.exists(inp) else []
        b.attention()
        if gdir[1:2] == ":":
            b.cmd('DRIVE "%s"' % gdir[:2], 10)
        b.cmd('CHDIR "%s"' % gdir, 10)
        load = b.cmd('LOAD "%s.bas"' % n, 60)
        if "rror" in load:
            rows.append({"name": n, "class": "LOAD-ERROR", "note": load.strip()[-120:]})
            print(json.dumps(rows[-1]), flush=True)
            continue
        s1, o1, t1 = run_program(b, inputs)
        b.attention()
        s2, o2, t2 = run_program(b, inputs)
        open(os.path.join(outdir, n + ".out"), "w", encoding="utf-8").write(o1)
        open(os.path.join(outdir, n + ".out2"), "w", encoding="utf-8").write(o2)
        det = norm(o1, inputs) == norm(o2, inputs)
        exp = os.path.join(corpus, n + ".expected")
        cls = s1
        note = ""
        if s1 == "OK" and os.path.exists(exp):
            want = norm(open(exp, encoding="latin-1").read())
            got = norm(o1, inputs)
            if want == got:
                cls = "MATCH"
            else:
                cls = "DIFF"
                d = difflib.unified_diff(want.split("\n"), got.split("\n"), "expected", "board", lineterm="")
                open(os.path.join(outdir, n + ".diff"), "w", encoding="utf-8").write("\n".join(d))
        elif s1 == "OK":
            cls = "NO-EXPECTED"
        if s1 in ("ERROR", "NEEDS-INPUT", "TIMEOUT"):
            m = re.findall(r"(\[\d+\][^\n]*\n)?[^\n]*Error[^\n]*", o1)
            note = (re.search(r"(\[\d+\][^\n]*\n[^\n]*Error[^\n]*)", o1) or re.search(r"[^\n]*Error[^\n]*", o1) or re.search(r".*$", o1.strip())).group(0)[-160:]
        rows.append({"name": n, "class": cls, "deterministic": det, "secs": t1, "note": note.replace("\n", " | ")})
        print(json.dumps(rows[-1]), flush=True)
    write_summary(outdir, rows, device)
    json.dump(rows, open(os.path.join(outdir, "summary.json"), "w"), indent=1)
    b.close()


def write_summary(outdir, rows, device):
    with open(os.path.join(outdir, "SUMMARY.md"), "w", encoding="utf-8") as f:
        f.write("# Goldens on %s\n\n" % device)
        counts = {}
        for r in rows:
            counts[r["class"]] = counts.get(r["class"], 0) + 1
        f.write(" ".join("%s %d" % kv for kv in sorted(counts.items())) + "\n\n")
        f.write("| program | class | deterministic | secs | note |\n|---|---|---|---|---|\n")
        for r in rows:
            f.write("| %s | %s | %s | %s | %s |\n" % (r["name"], r["class"], r.get("deterministic", ""), r.get("secs", ""), r.get("note", "").replace("|", "/")))


if __name__ == "__main__":
    main()
