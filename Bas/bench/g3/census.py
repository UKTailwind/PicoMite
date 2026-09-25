"""census.py - G3 coverage census: what share of the sampled time is spent on
program lines a Route B compiler could compile.

For each workload it reads the [PCSLINE] histogram of a profiled run (the
busiest program lines and their PC samples), finds each line in the source
that ran, and classifies every statement on it:

  core    numeric LET/IF/ELSE/FOR/NEXT/DO/LOOP/EXIT/LOCAL/DIM/SELECT/CASE,
          SUB/FUNCTION calls and returns - compiled outright (Route B P2-P4)
  splice  a firmware command whose arguments are all numeric expressions
          (LINE, BOX, PIXEL, BLIT ...): the arguments are compiled and the
          handler takes them through the value splice (Route B P5)
  text    anything touching strings, I/O or an unlisted command: falls back
          to the interpreter

A line counts as compilable when every statement on it is core or splice.
The histogram only lists the busiest lines, so the result is a share of the
listed samples, with the listed share of all samples beside it.

    python census.py
"""
import os, re, sys

BAS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
R = os.path.join(BAS, "bench", "results")
G = os.path.join(BAS, "bench", "phase1", "games")
WORKLOADS = [
    # (name, profiled run, sources (program first, then its library), PCSLINE -> 0-based file line offset)
    # The offsets were fitted (census.py --fit): the profiled twins of the macros
    # were made from these files with a few header lines changed.
    ("anchor (PICORP2350)", os.path.join(R, "picorp2350-pcs-prof.txt"), [os.path.join(BAS, "solar_eclipse.bas")], -1),
    ("anchor (VGA RP2040)", os.path.join(R, "vga315-pcs3-prof.txt"), [os.path.join(BAS, "solar_eclipse.bas")], -1),
    ("Exile kernel", os.path.join(R, "phase1", "v2_exile_phys_prof_1.txt"), [os.path.join(G, "exile", "gbxphysp.bas")], 0),
    ("Elite", os.path.join(R, "phase1", "v2_elite_prof_1.txt"), [os.path.join(G, "elite", "gbelitep.bas"), os.path.join(G, "elite", "gbelite_lib.bas")], 0),
    ("Prince of Pico", os.path.join(R, "phase1", "v2_pop_prof_1.txt"), [os.path.join(G, "pop", "gbpopp.bas")], 0),
    ("julia (PC3)", os.path.join(R, "phase1", "pc3_julia_interp_prof.txt"), [os.path.join(BAS, "bench", "phase1", "macros", "julia_interp.bas")], 3),
    ("knivd (PC3)", os.path.join(R, "phase1", "pc3_knivd_prof.txt"), [os.path.join(BAS, "bench", "phase1", "macros", "knivd_fixed.bas")], 10),
]

CORE = {"LET", "IF", "ELSEIF", "ELSE", "ENDIF", "END", "FOR", "NEXT", "DO", "LOOP", "EXIT", "LOCAL", "DIM", "STATIC",
        "SELECT", "CASE", "CONST", "GOTO", "GOSUB", "RETURN", "SUB", "FUNCTION", "INC", "CALL", "THEN", "CONTINUE"}
SPLICE = {"LINE", "BOX", "PIXEL", "CIRCLE", "TRIANGLE", "RBOX", "ARC", "POLYGON", "BLIT", "SPRITE", "CLS", "COLOUR", "COLOR",
          "FRAMEBUFFER", "DRAW3D", "POKE", "PAUSE", "MATH", "MEMORY", "TILE", "TILEMAP", "PLAY", "SETPIN", "PIN", "PWM",
          "IRETURN", "RANDOMIZE", "READ", "RESTORE", "FASTGFX", "WEBMITE"}
TEXT = {"PRINT", "INPUT", "OPEN", "CLOSE", "LINE INPUT", "TEXT", "FONT", "LOAD", "SAVE", "FILES", "ON", "ERROR", "DATA",
        "TRACE", "REM", "OPTION", "CHDIR", "KILL", "MKDIR", "EXECUTE", "RUN", "PLAY", "GUI"}
NUMFN = set("""ABS ACOS ASIN ATN ATAN2 COS SIN TAN SQR EXP LOG INT FIX CINT SGN MOD MIN MAX RND PI DEG RAD TIMER
AND OR XOR NOT BOUND PEEK POKE MATH SHL SHR INV DEVICE KEYDOWN MM.INFO""".split())


def split_statements(line):
    """Split a line at ':' outside strings, and after THEN/ELSE on a single-line IF."""
    out, cur, q = [], "", False
    for ch in line:
        if ch == '"':
            q = not q
        if ch == "'" and not q:
            break
        if ch == ":" and not q:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    out.append(cur)
    res = []
    for s in out:
        s = s.strip()
        if not s:
            continue
        m = re.match(r"(?i)^(IF\b.*?\bTHEN)\s+(\S.*)$", s)
        if m:
            res.append(m.group(1))
            rest = m.group(2)
            parts = re.split(r"(?i)\s+ELSE\s+", rest)
            res += [p for p in parts if p.strip()]
        else:
            res.append(s)
    return res


def classify(stmt, subs):
    s = stmt.strip()
    u = s.upper()
    if u.startswith("REM"):
        return "core"
    kw = re.match(r"[A-Z_][A-Z0-9_.]*", u)
    kw = kw.group(0) if kw else ""
    strish = "$" in s or '"' in s
    if re.match(r"^[A-Za-z_][A-Za-z0-9_.]*[%!]?\s*(\(.*?\))?\s*=", s) and kw not in ("IF", "FOR", "ELSEIF", "CASE", "DO", "LOOP", "CONST"):
        return "text" if strish else "core"          # assignment
    if kw in ("END", "EXIT", "ELSE", "ENDIF", "LOOP", "NEXT", "RETURN", "IRETURN", "CONTINUE"):
        return "text" if strish else "core"
    if kw in CORE:
        return "text" if strish else "core"
    if kw in subs:
        return "text" if strish else "core"          # user SUB call
    if kw in SPLICE:
        return "text" if strish else "splice"
    return "text"


def subs_of(sources):
    names = set()
    for f in sources:
        for l in open(f, encoding="latin-1"):
            m = re.match(r"(?i)\s*(?:SUB|FUNCTION|CSUB|CFUNCTION)\s+([A-Za-z_][A-Za-z0-9_.]*)", l)
            if m:
                names.add(m.group(1).upper())
    return names


def line_kind(line, subs):
    kinds = [classify(st, subs) for st in split_statements(line)]
    return "text" if "text" in kinds else ("splice" if "splice" in kinds else "core")


def sub_bodies(sources):
    """{NAME: [body lines]} for every SUB/FUNCTION in the sources."""
    bodies, cur = {}, None
    for f in sources:
        for l in open(f, encoding="latin-1"):
            m = re.match(r"(?i)\s*(?:SUB|FUNCTION)\s+([A-Za-z_][A-Za-z0-9_.]*)", l)
            if m:
                cur = m.group(1).upper()
                bodies[cur] = []
                continue
            if re.match(r"(?i)\s*END\s+(SUB|FUNCTION)", l):
                cur = None
                continue
            if cur is not None:
                bodies[cur].append(l.rstrip("\r\n"))
    return bodies


def line_view(text, srcs, off, subs, verbose):
    total = int(re.search(r"\[PCS\] samples=(\d+)", text).group(1))
    hist = [(int(l), int(n)) for l, n in re.findall(r"\[PCSLINE\] (-?\d+) (\d+)", text)]
    src = open(srcs[0], encoding="latin-1").read().splitlines()
    agg = {"core": 0, "splice": 0, "text": 0}
    rows = []
    for l, n in hist:
        if not (0 <= l + off < len(src)) or l < 0:
            rows.append((n, "?", "(line %d: library, not placed)" % l))
            continue
        line = src[l + off]
        k = line_kind(line, subs)
        agg[k] += n
        rows.append((n, k, line.strip()[:100]))
    listed = sum(agg.values())
    if verbose:
        for n, k, t in rows:
            print("      %5.2f%% %-6s %s" % (100.0 * n / total, k, t))
    return 100.0 * listed / total, {k: 100.0 * v / max(listed, 1) for k, v in agg.items()}


def sub_view(text, srcs, subs, verbose):
    """Each SUB's [PERF] self time, split over its body lines equally (a static weighting)."""
    el = int(re.search(r"\[PERF\] elapsed=(\d+)", text).group(1))
    blk = text.split("top SUBs by exclusive (self) time:")[1].split("[PERF]")[0]
    rows = re.findall(r"^\D*?(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+([A-Za-z_][A-Za-z0-9_.]*)\s*$", blk, re.M)
    bodies = sub_bodies(srcs)
    agg = {"core": 0.0, "splice": 0.0, "text": 0.0}
    covered = 0
    for self_us, incl, calls, per, name in rows:
        body = [l for l in bodies.get(name.upper(), []) if l.strip() and not l.strip().startswith("'")]
        if not body or int(self_us) > el:  # (a SUB still open when profiling ended reports nonsense)
            continue
        covered += int(self_us)
        kinds = [line_kind(l, subs) for l in body]
        for k in kinds:
            agg[k] += int(self_us) / len(kinds)
        if verbose:
            print("      %5.1f%% %-14s core %d splice %d text %d" % (100.0 * int(self_us) / el, name, kinds.count("core"), kinds.count("splice"), kinds.count("text")))
    return 100.0 * covered / el, {k: 100.0 * v / max(covered, 1) for k, v in agg.items()}


def main():
    verbose = "-v" in sys.argv
    print("%-22s | %-38s | %-38s" % ("", "line histogram (top 40 lines)", "SUB self time (top 20 SUBs)"))
    print("%-22s | %6s %6s %6s %6s %6s | %6s %6s %6s %6s %6s" % ("workload", "listed", "core", "splice", "text", "compil", "listed", "core", "splice", "text", "compil"))
    for name, prof, srcs, off in WORKLOADS:
        text = open(prof, encoding="utf-8", errors="replace").read()
        subs = subs_of(srcs)
        if verbose:
            print(name)
        a, la = line_view(text, srcs, off, subs, verbose)
        b, sa = sub_view(text, srcs, subs, verbose)
        print("%-22s | %5.1f%% %5.1f%% %5.1f%% %5.1f%% %5.1f%% | %5.1f%% %5.1f%% %5.1f%% %5.1f%% %5.1f%%" % (
            name, a, la["core"], la["splice"], la["text"], la["core"] + la["splice"],
            b, sa["core"], sa["splice"], sa["text"], sa["core"] + sa["splice"]))


if __name__ == "__main__":
    main()
