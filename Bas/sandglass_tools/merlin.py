"""merlin.py - just enough of the Merlin assembler to read two data files.

The frame table and the animation sequences are not binaries in the release;
they are assembler source.  To get at them we have to assemble them, so this is
a two-pass assembler covering only the directives those files use:

    name = expr        equate
    label              a label in column one; :name is local to the last label
     org expr          set the location counter
     dum expr / dend   a dummy section: defines labels, emits nothing
     db / dfb a,b,c    bytes, signed values allowed
     dw label          two bytes, little endian
     ds n              n zero bytes; "ds expr-*" pads up to expr
     hex 0a,0b / hex 0a0b   raw bytes
     do expr / else / fin   conditional assembly, honoured
     lst / tr / lstdo / xc / put   listing and assembly control

Expressions are symbols, decimal, $hex, %binary, "c" characters and the current
location counter *, combined with + and - and evaluated strictly left to right,
which is what Merlin does.

A lenient assembler skips anything else, which is how a constant or a small
table is read out of a module that is otherwise code.  In that mode addresses
are only meaningful relative to a nearby label.

This reads only files the caller supplies.  It contains no data of its own.
"""

import re

_COMMENT = re.compile(r"\s;.*$")


class AsmError(Exception):
    pass


class Assembler:
    def __init__(self, lenient=False):
        # Lenient: a code module.  Instructions are not assembled, only skipped,
        # so labels are only consistent relative to one another within a run of
        # data.  That is enough to read an equate or a table out of a module
        # that is mostly code, which is what the mover constants require.
        self.lenient = lenient
        self.symbols = {}
        self.output = bytearray()
        self.origin = None       # address the first emitted byte sits at
        self.pc = 0
        self.in_dummy = False
        self.last_global = ""
        # Where every two-byte address landed in the output, so a caller can
        # rebase absolute addresses to offsets without having to find them.
        self.word_sites = []

    # ---- expressions -------------------------------------------------------

    def _term(self, text):
        text = text.strip()
        if not text:
            raise AsmError("empty term")
        if text == "*":
            return self.pc
        if text.startswith("$"):
            return int(text[1:], 16)
        if text.startswith("%"):
            return int(text[1:], 2)
        if text.startswith('"') and len(text) >= 2:
            return ord(text[1])
        if text.startswith("'") and len(text) >= 2:
            return ord(text[1])
        if re.fullmatch(r"-?\d+", text):
            return int(text, 10)
        name = text
        if name.startswith(":"):
            name = self.last_global + name
        if name in self.symbols:
            return self.symbols[name]
        raise AsmError("undefined symbol %r" % text)

    @staticmethod
    def _tokenise(text):
        """Split into alternating terms and operators.

        A '*' is the location counter when a term is expected and multiplication
        otherwise, which is how Merlin reads it.
        """
        tokens = []
        cur = ""
        want_term = True
        for ch in text:
            if ch in "+-*/" and not want_term:
                tokens.append(cur.strip())
                tokens.append(ch)
                cur = ""
                want_term = True
                continue
            if ch == "-" and want_term and not cur:
                cur = "-"                  # a sign, part of the term
                continue
            cur += ch
            if not ch.isspace():
                want_term = False
        tokens.append(cur.strip())
        return tokens

    def evaluate(self, text, tolerant=False):
        """Left to right, no precedence, as Merlin evaluates."""
        text = text.strip()
        if not text:
            raise AsmError("empty expression")
        tokens = self._tokenise(text)

        def term(t):
            try:
                return self._term(t)
            except AsmError:
                if tolerant:
                    return 0
                raise

        value = term(tokens[0])
        i = 1
        while i + 1 < len(tokens):
            op = tokens[i]
            rhs = term(tokens[i + 1])
            if op == "+":
                value += rhs
            elif op == "-":
                value -= rhs
            elif op == "*":
                value *= rhs
            elif op == "/":
                value = value // rhs if rhs else 0
            else:
                raise AsmError("unknown operator %r in %r" % (op, text))
            i += 2
        return value

    # ---- emitting ----------------------------------------------------------

    def _emit(self, data):
        if self.in_dummy:
            self.pc += len(data)
            return
        if self.origin is None:
            self.origin = self.pc
        gap = self.pc - self.origin - len(self.output)
        if gap > 0:
            self.output += bytes(gap)
        self.output += bytes(data)
        self.pc += len(data)

    # ---- one pass ----------------------------------------------------------

    def _pass(self, lines, final):
        self.output = bytearray()
        self.origin = None
        self.pc = 0
        self.in_dummy = False
        self.last_global = ""
        dummy_resume = 0
        cond = []                   # the stack of open 'do' conditions
        # A lenient run never sees the equate files a code module includes, so
        # an undefined symbol has to read as zero there on both passes.
        tol = (not final) or self.lenient

        for raw in lines:
            line = raw.rstrip("\r\n")
            stripped = line.strip()
            # A whole-line comment: '*' in column one, or ';' anywhere leading.
            if not stripped or line.startswith("*") or stripped.startswith(";"):
                continue
            line = _COMMENT.sub("", line)
            if not line.strip():
                continue

            if not all(cond) and not line.strip().lower().split()[0] in ("do", "else", "fin") and not (line.split(None, 1)[1:] and line.split(None, 1)[1].strip().lower().split()[0] in ("do", "else", "fin")):
                continue
            label = ""
            if not line[0].isspace():
                bits = line.split(None, 1)
                label = bits[0]
                line = bits[1] if len(bits) > 1 else ""
            body = line.strip()

            # An equate defines its symbol and emits nothing.
            if label and body.startswith("="):
                self.symbols[label] = self.evaluate(body[1:], tolerant=tol)
                continue

            if label:
                name = self.last_global + label if label.startswith(":") else label
                if not label.startswith(":"):
                    self.last_global = label
                self.symbols[name] = self.pc

            if not body:
                continue

            bits = body.split(None, 1)
            op = bits[0].lower()
            arg = bits[1].strip() if len(bits) > 1 else ""

            # Listing control, conditional assembly and the build system's own
            # "write this module out" directive, which emits nothing.  Every
            # module in the release ends with one usr line carrying the disk
            # location and the module's length.
            # Conditional assembly: "do expr" opens a block that is assembled
            # only when expr is non-zero, "else" flips it, "fin" closes it.
            # The stairs sequence keeps a disabled variant this way, and
            # emitting it sent the character back to standing instead of on
            # to the next level.
            if op == "do":
                cond.append(self.evaluate(arg, tolerant=tol) != 0)
                continue
            if op == "else":
                if cond:
                    cond[-1] = not cond[-1]
                continue
            if op == "fin":
                if cond:
                    cond.pop()
                continue
            if not all(cond):
                continue
            if op in ("lst", "tr", "lstdo", "xc", "put",
                      "sav", "asc", "dsk", "usr", "typ", "end"):
                continue
            if op == "dum":
                dummy_resume = self.pc
                self.in_dummy = True
                self.pc = self.evaluate(arg, tolerant=tol)
                continue
            if op == "dend":
                self.in_dummy = False
                self.pc = dummy_resume
                continue
            if op == "org":
                self.pc = self.evaluate(arg, tolerant=tol)
                continue
            if op in ("db", "dfb"):
                out = []
                for piece in _split_args(arg):
                    out.append(self.evaluate(piece, tolerant=tol) & 0xFF)
                self._emit(out)
                continue
            if op == "dw":
                for piece in _split_args(arg):
                    v = self.evaluate(piece, tolerant=tol) & 0xFFFF
                    self._emit([v & 0xFF, v >> 8])
                    # Taken after the emit: _emit can insert padding first, so
                    # the byte position is only known once the bytes are down.
                    if final and not self.in_dummy:
                        self.word_sites.append(len(self.output) - 2)
                continue
            if op == "ds":
                if "*" in arg:                      # pad up to an address
                    target = self.evaluate(arg.replace("-*", ""), tolerant=tol)
                    count = target - self.pc
                else:
                    count = self.evaluate(arg, tolerant=tol)
                if count < 0:
                    if final:
                        raise AsmError("ds moves backwards by %d" % -count)
                    count = 0
                self._emit(bytes(count))
                continue
            if op == "hex":
                text = arg.replace(",", "").strip()
                if len(text) % 2:
                    raise AsmError("odd hex run %r" % arg)
                self._emit(bytes.fromhex(text))
                continue
            if final and not self.lenient:
                raise AsmError("unhandled directive %r in %r" % (op, raw.strip()))

    def assemble(self, path):
        with open(path, "r", encoding="latin-1") as fh:
            lines = fh.readlines()
        lines = _expand_lup(lines)
        self._pass(lines, final=False)      # collect symbols
        self._pass(lines, final=True)       # emit
        return bytes(self.output)


def _expand_lup(lines):
    """Expand Merlin's 'lup n' ... '--^' repeat blocks in place.

    The count is a literal in the modules we read, which keeps this a plain text
    expansion that runs before either assembly pass.
    """
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        head = stripped.split(None, 1)
        if head and head[0].lower() == "lup" and not line[0].isspace():
            pass                                     # a label called lup: leave it
        elif head and head[0].lower() == "lup" and len(head) > 1:
            count_text = head[1].split(";")[0].strip()
            try:
                count = int(count_text[1:], 16) if count_text.startswith("$") \
                    else int(count_text, 10)
            except ValueError:
                raise AsmError("lup count %r is not a literal" % count_text)
            depth = 1
            body = []
            j = i + 1
            while j < len(lines):
                s = lines[j].strip()
                token = s.split(None, 1)[0].lower() if s.split() else ""
                if token == "lup":
                    depth += 1
                elif s.startswith("--^"):
                    depth -= 1
                    if depth == 0:
                        break
                body.append(lines[j])
                j += 1
            if depth:
                raise AsmError("lup without a matching --^")
            out.extend(_expand_lup(body) * count)
            i = j + 1
            continue
        out.append(line)
        i += 1
    return out


def _split_args(text):
    """Split on commas that are not inside a quoted character."""
    out = []
    cur = ""
    quote = None
    for ch in text:
        if quote:
            cur += ch
            if ch == quote:
                quote = None
            continue
        if ch in "\"'":
            quote = ch
            cur += ch
            continue
        if ch == ",":
            out.append(cur)
            cur = ""
            continue
        cur += ch
    if cur.strip():
        out.append(cur)
    return [p for p in out if p.strip()]
