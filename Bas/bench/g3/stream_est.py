"""stream_est.py - G3 capacity check: estimate the size of a Route B statement
stream for a program and compare it with the program's tokenised image.

    python stream_est.py FILE.bas ...      (no files: the G3 corpus)

The stream is costed statement by statement in 16-bit words, with the
encoding the prototype VM uses (Bas/bench/g3/vm.c, kernels.vma):

  every compiled statement   STMT (1) + 2 bytes in the pc -> text table
  operand                    1 (LDL/LDG/LAD); CONST names fold to literals
  literal                    1 for -128..127, 3 (LCW) otherwise, 5 past 32 bits;
                             a float is 1 (LCF k) + 8 bytes in the constant pool
  operator, comparison       1
  array element              index expressions + 1 (ALD/AST) + 2 per extra dimension
  built-in function          arguments + 2 (op, function number)
  user FUNCTION / SUB call   arguments + 3 (CALL nargs, target, nlocals)
  assignment                 expression + 1 (STL/STG/STP)
  IF / ELSEIF / ELSE         condition + 2 (JZ); ELSE and each ELSEIF also 2 (JMP)
  FOR / NEXT                 start + 1, limit + 1, step + 1, entry test 5; NEXT 3
  DO / LOOP                  condition + 2; LOOP 2 (JMP)
  SELECT CASE                selector + 1; per CASE value 4 + value; CASE 2 (JMP)
  SUB / FUNCTION             4 (frame header); END/EXIT 1-2
  spliced command            arguments + 3 (op, command number, argument count)
  anything else              2: a fallback record pointing at the statement's text

A type conversion is not costed per operand; 5% is added for them.  The
tokenised image comes from measure.py (an emulation of tokenise(), good to
about 3%, written for the Phase 0 format study), as text and in its model of
the S5/S6 symbol format.
"""
import os, re, sys, importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
BAS = os.path.normpath(os.path.join(HERE, "..", ".."))
MEASURE = os.path.join(HERE, "measure.py")
if len(sys.argv) > 1 and sys.argv[1].startswith("--measure="):
    MEASURE = sys.argv.pop(1).split("=", 1)[1]
spec = importlib.util.spec_from_file_location("measure", MEASURE)
measure = importlib.util.module_from_spec(spec)
spec.loader.exec_module(measure)

FUNCS = set(n[:-1].upper() for n, t in measure.toks if n.endswith("("))
OPWORDS = {"AND", "OR", "XOR", "NOT", "MOD", "INV"}
NOARGFN = {"PI", "TIMER", "RND", "INKEY$", "DATE$", "TIME$"}
SPLICE = {"LINE", "BOX", "PIXEL", "CIRCLE", "TRIANGLE", "RBOX", "ARC", "POLYGON", "BLIT", "SPRITE", "CLS",
          "COLOUR", "COLOR", "FRAMEBUFFER", "DRAW3D", "POKE", "PAUSE", "MATH", "MEMORY", "TILE", "TILEMAP",
          "SETPIN", "PIN", "PWM", "RANDOMIZE", "READ", "RESTORE", "IRETURN", "SETTICK", "SERVO"}
CORPUS = [
    ("anchor", [os.path.join(BAS, "solar_eclipse.bas")]),
    ("Exile", [os.path.join(BAS, "exile", "exile.bas")]),
    ("Elite program", [os.path.join(BAS, "elite", "elite.bas")]),
    ("Elite library", [os.path.join(BAS, "elite", "elite_lib.bas")]),
    ("Prince of Pico", [os.path.join(BAS, "bench", "phase1", "games", "pop", "gbpop.bas")]),
    ("G3 kernels", [os.path.join(HERE, "g3_kernels.bas")]),
]

TOKRE = re.compile(r'''\s*(?:
    (?P<str>"[^"]*"?) |
    (?P<hex>&[HhOoBb][0-9A-Fa-f]+) |
    (?P<num>(?:\d+\.?\d*|\.\d+)(?:[Ee][+-]?\d+)?) |
    (?P<name>[A-Za-z_][A-Za-z0-9_.]*[$%!]?\s*\(?) |
    (?P<op><=|>=|<>|=<|=>|<<|>>|[-+*/\\^=<>]) |
    (?P<punct>[(),;]) |
    (?P<other>\S))''', re.X)


class Prog:
    def __init__(self, text):
        self.subs = set()
        self.consts = set()
        self.floats = set()
        for l in text.splitlines():
            m = re.match(r"(?i)\s*(?:SUB|FUNCTION|CSUB|CFUNCTION)\s+([A-Za-z_][A-Za-z0-9_.]*)", l)
            if m:
                self.subs.add(m.group(1).upper())
            for m in re.finditer(r"(?i)\bCONST\s+([A-Za-z_][A-Za-z0-9_.]*[%!]?)", l):
                self.consts.add(m.group(1).upper().rstrip("%!"))

    def literal(self, v):
        if isinstance(v, float) or v != int(v):
            self.floats.add(v)
            return 1
        v = int(v)
        if -128 <= v <= 127:
            return 1
        return 3 if -(1 << 31) <= v < (1 << 31) else 5

    def expr(self, s):
        """Words for an expression, or None if it involves strings."""
        w = 0
        for m in TOKRE.finditer(s):
            k = m.lastgroup
            t = m.group(k)
            if k == "str":
                return None
            if k == "hex":
                w += self.literal(int(t[2:], {"H": 16, "O": 8, "B": 2}[t[1].upper()]))
            elif k == "num":
                w += self.literal(float(t) if ("." in t or "e" in t.lower()) else int(t))
            elif k == "name":
                call = t.endswith("(")
                n = t.rstrip("(").strip().upper()
                if n.endswith("$"):
                    return None
                if n in OPWORDS:
                    w += 1
                elif call and n in FUNCS:
                    w += 2
                elif n in self.subs:
                    w += 3
                elif call:
                    w += 1  # array element: ALD (extra dimensions are added at the commas below)
                elif n in self.consts:
                    w += 1
                elif n in NOARGFN or n.startswith("MM."):
                    w += 2
                else:
                    w += 1
            elif k == "op":
                w += 1
            elif k == "punct":
                if t == ",":
                    w += 0.5  # an extra dimension or an argument: averaged
            elif k == "other":
                return None
        return w

    def stmt(self, s):
        """(words, kind) for one statement; kind is 'code', 'splice' or 'fallback'."""
        s = s.strip()
        if not s or s.startswith("'"):
            return 0, "none"
        u = s.upper()
        kw = re.match(r"[A-Z_][A-Z0-9_.]*\$?", u)
        kw = kw.group(0) if kw else ""
        rest = s[len(kw):].strip()

        def code(w):
            return (w + 2, "code") if w is not None else (2, "fallback")  # + STMT and the line table

        if kw == "REM" or kw == "DATA":
            return 0, "none"
        if kw in ("CONST",):
            return 0, "none"
        if kw in ("SUB", "FUNCTION"):
            return 4, "code"
        if kw == "END":
            if re.match(r"(?i)END\s*(SUB|FUNCTION)", s):
                return 1, "code"
            if re.match(r"(?i)END\s*(IF|SELECT)", s):
                return 0, "none"
            return code(1) if not rest else (2, "fallback")
        if kw in ("ENDIF",):
            return 0, "none"
        if kw == "EXIT":
            return code(2)
        if kw == "CONTINUE":
            return code(2)
        if kw == "ELSE":
            return 2, "code"
        if kw in ("ELSEIF",):
            m = re.match(r"(?i)ELSEIF\s+(.*?)\s*(THEN)?$", s)
            e = self.expr(m.group(1))
            return code(None if e is None else e + 4)
        if kw == "FOR":
            m = re.match(r"(?i)FOR\s+([A-Za-z_][A-Za-z0-9_.]*[%!]?)\s*=\s*(.*?)\s+TO\s+(.*?)(?:\s+STEP\s+(.*))?$", s)
            if not m:
                return 2, "fallback"
            parts = [self.expr(m.group(2)), self.expr(m.group(3))] + ([self.expr(m.group(4))] if m.group(4) else [])
            if None in parts:
                return 2, "fallback"
            return code(sum(parts) + len(parts) + 5)
        if kw == "NEXT":
            return 3, "code"
        if kw == "DO":
            m = re.match(r"(?i)DO\s+(WHILE|UNTIL)\s+(.*)$", s)
            if not m:
                return 0, "none"
            e = self.expr(m.group(2))
            return code(None if e is None else e + 2)
        if kw == "LOOP":
            m = re.match(r"(?i)LOOP\s+(WHILE|UNTIL)\s+(.*)$", s)
            if not m:
                return 2, "code"
            e = self.expr(m.group(2))
            return code(None if e is None else e + 2)
        if kw == "SELECT":
            e = self.expr(re.sub(r"(?i)^SELECT\s+CASE\s+", "", s))
            return code(None if e is None else e + 1)
        if kw == "CASE":
            if re.match(r"(?i)CASE\s+ELSE", s):
                return 2, "code"
            items = [x for x in re.split(r",", rest)]
            w = 2
            for it in items:
                it = re.sub(r"(?i)^IS\s*", "", it.strip())
                parts = re.split(r"(?i)\s+TO\s+", it)
                for p in parts:
                    e = self.expr(p.lstrip("<>="))
                    if e is None:
                        return 2, "fallback"
                    w += e + 4
            return code(w)
        if kw in ("GOTO", "GOSUB"):
            return code(2)
        if kw == "RETURN":
            return code(1)
        if kw in ("LOCAL", "DIM", "STATIC"):
            if "$" in s or re.search(r"(?i)\bSTRING\b", s):
                return 2, "fallback"
            w = 0
            body = re.sub(r"(?i)^(LOCAL|DIM|STATIC)\s+(INTEGER|FLOAT)?", "", s)
            depth, cur, items = 0, "", []
            for ch in body:
                if ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
                if ch == "," and depth == 0:
                    items.append(cur)
                    cur = ""
                else:
                    cur += ch
            items.append(cur)
            for it in items:
                it = re.sub(r"(?i)\s+AS\s+\w+", "", it).strip()
                if "=" in it:
                    e = self.expr(it.split("=", 1)[1])
                    if e is None:
                        return 2, "fallback"
                    w += e + 1
                if "(" in it:  # an array: its dimensions and a DIM op
                    e = self.expr(it[it.index("(") + 1:it.rindex(")")] if ")" in it else "0")
                    w += (e or 1) + 2
            return code(w) if w else (0, "none")
        if kw == "INC":
            e = self.expr(rest.split(",", 1)[1]) if "," in rest else 1
            return code(None if e is None else e + 3)
        if kw == "LET":
            s = rest
            u = s.upper()
            kw = ""
        # assignment?
        m = re.match(r"^([A-Za-z_][A-Za-z0-9_.]*[%!$]?)\s*(\((.*)\))?\s*=(.*)$", s)
        if m and kw not in ("IF", "PRINT"):
            if m.group(1).endswith("$"):
                return 2, "fallback"
            e = self.expr(m.group(4))
            ix = self.expr(m.group(3)) if m.group(3) is not None else 0
            if e is None or ix is None:
                return 2, "fallback"
            return code(e + ix + 1)
        if kw == "CALL":
            e = self.expr(rest)
            return code(None if e is None else e + 3)
        if kw in self.subs:
            e = self.expr(rest.strip("()")) if rest else 0
            return code(None if e is None else e + 3)
        if kw in SPLICE:
            e = self.expr(rest) if rest else 0
            if e is None:
                return 2, "fallback"
            return e + 5, "splice"
        return 2, "fallback"

    def line(self, l):
        """Words for a source line (statements split at ':' and single-line IFs)."""
        out, cur, q = [], "", False
        for ch in l:
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
        total, kinds = 0, []
        for s in out:
            s = s.strip()
            if not s:
                continue
            if re.match(r"^[A-Za-z_][A-Za-z0-9_.]*$", s) and s.upper() not in ("ELSE", "ENDIF", "LOOP", "NEXT", "DO", "END", "RETURN") and s.upper() not in self.subs:
                continue  # a label
            m = re.match(r"(?i)^(ELSE)?IF\s+(.*?)\s+THEN\s*(.*)$", s)
            if m:
                e = self.expr(m.group(2))
                if e is None:
                    total += 2
                    kinds.append("fallback")
                    continue
                total += e + 2 + 2 + (2 if m.group(1) else 0)
                kinds.append("code")
                tail = m.group(3).strip()
                if tail:
                    parts = re.split(r"(?i)\s+ELSE\s+", tail)
                    if len(parts) > 1:
                        total += 2
                    for p in parts:
                        if re.match(r"^\d+$", p.strip()):
                            total += 2  # THEN linenumber
                            continue
                        w, k = self.stmt(p)
                        total += w
                        kinds.append(k)
                continue
            w, k = self.stmt(s)
            total += w
            kinds.append(k)
        return total, kinds


def estimate(path):
    text = open(path, encoding="latin-1").read()
    p = Prog(text)
    words, kinds = 0, {"code": 0, "splice": 0, "fallback": 0, "none": 0}
    for l in text.splitlines():
        w, ks = p.line(l)
        words += w
        for k in ks:
            kinds[k] += 1
    words *= 1.05  # type conversions
    stream = int(words * 2 + 8 * len(p.floats) + 4 * len(p.subs) + 16)
    st = measure.Stats()
    measure.tokenise_program(text, st)
    c = st.c
    image = sum(v for k, v in c.items() if k not in ("comment_trailing", "name_label_def"))
    return stream, image, kinds, len(p.floats), text


def symbol_image(text):
    """The image in the S5/S6 symbol format (measure.py's final model, without printing)."""
    import io, contextlib
    f = io.StringIO()
    tmp = os.path.join(HERE, "_sym_tmp.bas")
    open(tmp, "w", encoding="latin-1", newline="").write(text)
    with contextlib.redirect_stdout(f):
        measure.final(tmp)
    os.remove(tmp)
    m = re.search(r"-> +([\d.]+)K", f.getvalue())
    return float(m.group(1)) * 1024 if m else None


def main():
    files = [(os.path.basename(a), [a]) for a in sys.argv[1:]] or CORPUS
    print("%-16s %9s %9s %9s %7s %7s   %s" % ("program", "image", "symbols", "stream", "s/img", "s/sym", "statements: code / splice / fallback"))
    for name, paths in files:
        if not os.path.exists(paths[0]):
            print("%-16s (missing %s)" % (name, paths[0]))
            continue
        stream, image, kinds, nf, text = estimate(paths[0])
        sym = symbol_image(text)
        print("%-16s %9d %9d %9d %6.2fx %6.2fx   %d / %d / %d  (%d float constants)" % (
            name, image, sym or 0, stream, stream / image, stream / sym if sym else 0,
            kinds["code"], kinds["splice"], kinds["fallback"], nf))


if __name__ == "__main__":
    main()
