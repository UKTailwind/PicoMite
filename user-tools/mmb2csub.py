#!/usr/bin/env python3
"""mmb2csub.py - turn one MMBasic SUB into a PicoMite CSUB, in one command.

    python mmb2csub.py prog.bas --sub PlotJulia [-o out.txt] [--keep-c]

It drives the MMBasic->C translator (mmb2c.py) to get C for the chosen
routine, wraps it in a CSUB entry shim, and runs armcfgen.py to produce the
`CSUB name ... End CSUB` block.

Why it is this small
--------------------
Two things do most of the work, and neither is in this file:

  * mmb2c already emits a SUB as `void f_name(MMFLOAT *p_a, MMINTEGER *p_b)` -
    one pointer per by-reference parameter, which IS the CSUB calling
    convention.  The shim below only has to cast the interpreter's void*.

  * mmcsub.h defines every runtime helper the generator may name (mm_*) and
    every arithmetic helper the compiler may emit (__aeabi_*), each forwarding
    to a firmware CallTable vector.  So anything unsupported is a COMPILE
    ERROR that names itself, and the scope check needs no hand-maintained list
    of supported statements.  A clean compile is the proof.

Scope (phase 1)
---------------
SUBs and numeric FUNCTIONs whose parameters are numeric scalars or numeric
arrays.  Everything else is refused here with a reason; everything subtly
unsupported is refused by the compiler.  Both refusals are better than a wrong
CSUB, which on a board is a hard fault with no line number.

FUNCTIONs need no special machinery, because MMBasic passes every argument by
reference: the CSUB takes the result as an extra leading out-parameter, the
generated C function is used exactly as mmb2c emitted it, and a three-line
BASIC wrapper keeps the original name so call sites do not change.
"""

import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FIRMWARE = os.path.dirname(HERE)          # PicoCFunctions.h lives one up
ARMCFGEN = os.path.join(HERE, "armcfgen.py")

# Where mmb2c.py may be found, in order.  It is a tool in its own right and
# is kept in one place rather than copied into every project that uses it -
# this is a driver for it, not a fork of it.
MMB2C_CANDIDATES = [
    os.environ.get("MMB2C_PATH"),
    os.path.join(HERE, "mmb2c.py"),
    r"\\wsl.localhost\Ubuntu\home\peter\src\FUZIX\Applications\mmb2c\mmb2c.py",
    "/home/peter/src/FUZIX/Applications/mmb2c/mmb2c.py",
]


# armcfgen's processor for the target board (--target): none for the PicoMite's default
CPU_ARGS = []
# the comment that ends a CMM2 CSUB's first line, so a tool (or one day the firmware) can tell its blob from one
# built for the PicoMite's processors; the firmware's type-list parse stops at the '
CMM2_MARK = "   ' Cortex-M7 (CMM2)"


def load_mmb2c(explicit=None):
    """Import mmb2c.py from wherever it is and return the module."""
    import importlib.util
    for cand in ([explicit] if explicit else []) + MMB2C_CANDIDATES:
        if cand and os.path.exists(cand):
            spec = importlib.util.spec_from_file_location("mmb2c", cand)
            mod = importlib.util.module_from_spec(spec)
            sys.modules["mmb2c"] = mod
            spec.loader.exec_module(mod)
            mod.__mmb2csub_path = cand
            return mod
    sys.exit("error: cannot find mmb2c.py - pass --mmb2c PATH or set MMB2C_PATH\n"
             "  looked in: " + ", ".join(c for c in MMB2C_CANDIDATES if c))


# ---------------------------------------------------------------- convert

def convert_program(mmb2c, path):
    """Run mmb2c's pipeline over the whole program and return the Conv.

    The WHOLE program, not just the chosen SUB, so that CONSTs, DIMs and the
    types of globals resolve exactly as they do for the real interpreter.
    lenient=True means trouble elsewhere in the program is commented out and
    recorded rather than stopping us - only the chosen routine has to be clean.
    """
    lines = []
    with open(path, "r") as f:
        for ln in f:
            if "\0" in ln:
                ln = ln.split("\0", 1)[0]
                if ln:
                    lines.append(ln)
                break
            lines.append(ln)

    # mmb2c works on this very list, and pass_fonts blanks in it what it cannot
    # translate: CSUB and DefineFont blocks and '#' lines. The program is
    # written back from this copy instead, or converting one routine would erase
    # every CSUB already in it - an earlier routine's of this same command too.
    # Lines are blanked, never added or removed, so a span found in conv.lines
    # fits source_lines.
    source_lines = list(lines)
    conv = mmb2c.Conv(lines, path)
    conv.source_lines = source_lines
    conv.lenient = True
    conv.pass_fonts()
    conv.pass_routine_names()
    conv.pass_types()
    conv.pass_declarations()
    conv.walk("scan")
    consts = {}
    for nm in conv.globals:
        if conv.globals[nm].is_const:
            consts[nm] = conv.globals[nm].acc
    for nm in consts:
        conv.globals[nm].acc = mmb2c.cconst(nm)
    mmb2c._heap_fixup(conv)
    mmb2c._local_heap_fixup(conv)
    # A program that uses ON ERROR anywhere makes mmb2c wrap EVERY routine in
    # __mm_e[] checks, so a SUB gets error-trapping code it has nothing to do
    # with. None of it can work in a CSUB - the interpreter's error machinery
    # is not reachable from inside one - so it is suppressed here, and any
    # routine that uses ON ERROR itself is refused by check_scope instead.
    # Setting the flag is not enough: do_on_error sets it again as it emits
    # each ON ERROR statement, so every routine after the first one would be
    # wrapped anyway. Pin it false for the emit pass instead. (A data
    # descriptor on the class beats the instance attribute, and Conv has no
    # __slots__, so this is the least invasive way to say "not for this run".)
    # Three flags, not one: uses_onerror gates the routine entry/exit guards,
    # while checks_on() - which gates the "evaluate to a temporary, store only
    # if nothing raised" form and the checked mm_fdiv - reads onerror_global
    # and err_window instead.
    _off = property(lambda s: False, lambda s, v: None)
    _zero = property(lambda s: 0, lambda s, v: None)
    conv.__class__ = type("ConvNoOnError", (conv.__class__,),
                          {"uses_onerror": _off, "onerror_global": _off,
                           "err_window": _zero})
    conv.tmpn = 0
    conv.out_main = []
    conv.out_body = []
    conv.walk("emit")
    for nm in consts:
        conv.globals[nm].acc = consts[nm]
    return conv


def slice_function(conv, cname):
    """Pull one function's text out of conv.out_body, brace-balanced."""
    body = conv.out_body
    start = None
    for i, ln in enumerate(body):
        if re.match(r"^[A-Za-z_].*\b" + re.escape(cname) + r"\s*\(", ln) and ln.rstrip().endswith("{"):
            start = i
            break
    if start is None:
        return None
    depth = 0
    for i in range(start, len(body)):
        depth += body[i].count("{") - body[i].count("}")
        if depth == 0:
            return "\n".join(body[start:i + 1])
    return None


def cname_of(nm):
    """An MMBasic name as a C identifier: a dot is legal in BASIC, not in C."""
    return nm.replace(".", "__")


def called_routines(cbody, own_cname):
    """The f_ names this body calls, other than itself."""
    seen = re.findall(r"\bf_([A-Za-z_][A-Za-z_0-9]*)\s*\(", cbody)
    return [m for m in seen if ("f_" + m) != own_cname]


def closure(conv, routine):
    """The routine and everything it calls, transitively, in emission order.

    mmb2c emits an inter-routine call as f_other(...), so a blob holding only
    the entry would leave that undefined.  armcfgen's merge mode already packs
    several functions into one blob and resolves the calls between them, so the
    work is picking WHICH functions: the call graph from the entry, with
    recursion and mutual recursion handled by the visited set.
    """
    by_cname = {r.cname: r for r in conv.routines.values()}
    order, seen = [], set()

    def walk(r):
        if r.cname in seen:
            return
        seen.add(r.cname)
        body = slice_function(conv, r.cname)
        if body is None:
            return
        for nm in called_routines(body, r.cname):
            callee = by_cname.get("f_" + nm)
            if callee is not None:
                walk(callee)
        order.append(r)

    walk(routine)
    return order          # callees first, entry last


def local_structs_for(conv, routines):
    """The `struct mm_l_<routine>` definitions the blob needs.

    A routine with LOCAL arrays or strings keeps them in one block per
    invocation, whose struct mmb2c declares at file scope - in a part of its
    output that a slice does not carry.  Only the structs for routines in this
    blob are taken, so an unrelated routine's locals cost nothing.
    """
    want = set("struct mm_l_%s {" % r.cname for r in routines)
    out, keep = [], False
    for ln in conv.local_structs():
        if ln in want:
            keep = True
        if keep:
            out.append(ln)
        if keep and ln == "};":
            keep = False
    return out


def consts_used(conv, mmb2c, text):
    """The CONSTs the emitted text mentions, as (cname, value) pairs.

    mmb2c emits a global CONST as a #define in a section of its output that a
    slice does not carry, so a routine using one would not compile.  Only the
    ones actually named are emitted; a CONST whose value needs the runtime is
    skipped, being a hidden global rather than a #define.
    """
    out = []
    for nm in sorted(conv.globals):
        sym = conv.globals[nm]
        if not sym.is_const or getattr(sym, "const_runtime", False):
            continue
        cn = mmb2c.cconst(nm)
        if re.search(r"\b" + re.escape(cn) + r"\b", text):
            out.append((cn, sym.acc))
    return out


def globals_touched(conv, routines):
    """The globals the whole blob reaches, in a stable order.

    mmb2c records these itself while scanning (Routine.gtouch, which is what
    its report's "Globals reached from inside a SUB" section is built from),
    so this is reading its answer rather than parsing the generated C.  The
    union over the closure, because a callee's globals have to arrive too.
    CONSTs are excluded - they become #defines and need nothing passed.
    """
    names = set()
    for r in routines:
        names |= set(r.gtouch)
    out = []
    for nm in sorted(names):
        s = conv.globals.get(nm)
        if s is None or s.is_const:
            continue
        out.append((nm, s))
    return out


# ---------------------------------------------------------------- checks

def uses_on_error(conv, routine):
    """Does this routine's own source use ON ERROR?

    convert_program suppresses the program-wide error-trapping code, which is
    right when the routine merely lives in a program that traps elsewhere and
    wrong if the routine traps itself - so that case is refused rather than
    silently given different semantics.
    """
    depth = 0
    for ln in conv.lines[max(routine.line - 1, 0):]:
        u = ln.upper()
        if "ON ERROR" in u:
            return True
        if "SUB " in u or "FUNCTION " in u:
            depth += 1
        if "END SUB" in u or "END FUNCTION" in u:
            depth -= 1
            if depth <= 0:
                break
    return False


def check_scope(conv, routine, name, routines=None):
    """Refuse what this knowingly cannot do.  The compiler catches the rest.

    Checked over the whole closure: a callee that cannot be converted stops the
    entry just as surely, and saying so here names the real routine.
    """
    bad = []
    routines = routines or [routine]
    for r in routines:
        who = r.name if r is not routine else name
        if uses_on_error(conv, r):
            bad.append("%s uses ON ERROR; a CSUB cannot reach the "
                       "interpreter's error handling" % who)
        for s in getattr(r, "statics", ()):
            # A STATIC is carried by the wrapper - see statics_touched. The one
            # shape that cannot travel is a string ARRAY, for the same reason a
            # string array parameter cannot.
            if s.ty == "s" and s.is_array:
                bad.append("%s: STATIC '%s' is a string ARRAY; its element "
                           "stride follows a LENGTH the CSUB is not told"
                           % (who, s.name))
        for p in r.params:
            if p.stype is not None:
                bad.append("%s: parameter '%s' is a user TYPE" % (who, p.name))
            elif getattr(p, "ty", None) == "s" and p.is_array:
                # A string SCALAR is just a char* - what the ABI already passes.
                # A string ARRAY is not: its element stride follows a declared
                # LENGTH that the CSUB is never told, so indexing it would be
                # wrong, silently.
                bad.append("%s: parameter '%s' is a string ARRAY; its element "
                           "stride follows a LENGTH the CSUB is not told"
                           % (who, p.name))
    return bad


def arg_count(routine, bodytext, gl):
    """How many arguments the CSUB will take.

    Its own parameters, a bound for each array it indexes with BOUND(), one for
    a FUNCTION's result - and ONE PER GLOBAL.  That last is what makes this
    worth checking separately: solar_eclipse's sefunc has two parameters and
    twenty globals, so it wants twenty-two arguments and cannot be called at
    all.  MAX_CSUB_ARGS is 16 on RP2350 and 10 on RP2040.
    """
    n = len(gl) + (1 if routine.is_func else 0)
    for p in routine.params:
        n += 1
        if p.is_array and bounds_used(bodytext, p):
            n += 1
    return n


# ---------------------------------------------------------------- emit

# MMBasic type suffixes: the generated wrapper is real BASIC and must declare
# its own types, whatever OPTION DEFAULT is where it gets pasted.
class StaticSym(object):
    """A routine's STATIC, dressed as a global so it travels the same road.

    A STATIC and a global differ only in who can see the name: both are one
    variable that outlives the call. So rather than give statics their own
    plumbing, the wrapper declares an MMBasic STATIC of its own and passes its
    address, and from the CSUB's side it is indistinguishable from a global.

    That is the only way it CAN work. mmb2c emits a C `static`, which is
    writable data in .bss, and a CSUB has none - the blob is position-
    independent code with .text and .rodata only, so the variable would sit at
    an address that is not the CSUB's. The wrapper owns the storage instead,
    and MMBasic's own STATIC gives exactly the right lifetime: one instance,
    initialised on the first call, surviving every later one.
    """

    __slots__ = ("name", "acc", "ty", "is_array", "dims", "owner",
                 "is_const", "decl")

    def __init__(self, sym, owner, decl):
        self.name = sym.name
        self.acc = sym.acc          # the C name in mmb2c's body, e.g. v_hits
        self.ty = sym.ty
        self.is_array = sym.is_array
        self.dims = sym.dims
        self.owner = owner          # which routine's STATIC this is
        self.is_const = False
        self.decl = decl            # rest of the BASIC declarator: "(4)",
        #                             "= 1.5", or ""


def split_declarators(text):
    """Split a STATIC declaration list on its top-level commas."""
    out, depth, cur, quoted = [], 0, "", False
    for ch in text:
        if ch == '"':
            quoted = not quoted
        if not quoted:
            if ch in "([":
                depth += 1
            elif ch in ")]":
                depth -= 1
            elif ch == "," and depth == 0:
                out.append(cur)
                cur = ""
                continue
        cur += ch
    if cur.strip():
        out.append(cur)
    return out


def static_decls(conv, routine):
    """{canonical name: the rest of its BASIC declarator} for one routine.

    Read out of the source rather than rebuilt from the symbol, so an array's
    bounds and an initialiser come across exactly as written. MMBasic's
    once-only STATIC initialiser is what replaces the __once_ guard mmb2c
    emits in C.
    """
    span = find_routine_text(conv, routine)
    out = {}
    if span is None:
        return out
    for ln in conv.lines[span[0]:span[1]]:
        m = re.match(r"\s*static\s+(.*)$", ln, re.I)
        if not m:
            continue
        for dec in split_declarators(m.group(1).rstrip()):
            d2 = re.match(r"\s*([A-Za-z_][A-Za-z_0-9.]*)[$%!]?\s*(.*)$", dec)
            if d2:
                out[d2.group(1).lower()] = d2.group(2).strip()
    return out


def statics_touched(conv, routines):
    """Every STATIC in the blob, with the name it travels under.

    Named per routine, because two routines in one blob may each have a STATIC
    called `count` and they are different variables. MAXVARLEN is 32, so the
    name is kept short rather than descriptive.
    """
    out = []
    for r in routines:
        decls = static_decls(conv, r)
        for s in r.statics:
            nm = ("__s%d_%s" % (len(out), cname_of(s.name)))[:28]
            out.append((nm, StaticSym(s, r, decls.get(s.name.lower(), ""))))
    return out


SFX = {"f": lambda nm, arr: nm + "!" + ("()" if arr else ""),
       "i": lambda nm, arr: nm + "%" + ("()" if arr else ""),
       "s": lambda nm, arr: nm + "$" + ("()" if arr else "")}

CT = {"f": "MMFLOAT", "i": "MMINTEGER", "s": "char"}


def frame_on_stack(body):
    """Turn the per-invocation LOCAL block into an ordinary C local.

    mmb2c emits `struct mm_l_X *__L = mm_lheap(sizeof *__L);` because its own
    runtime keeps that block on a heap. Here the size is a compile-time
    constant and the lifetime is exactly the C function's, so a plain stack
    variable is the right thing: each call gets its own - which is what makes
    recursion work - and it costs nothing to release.

    mm_lheap would otherwise have to be alloca to get that lifetime, and
    alloca forces a frame pointer and a dynamic stack adjustment into every
    routine that has any LOCAL array or string. On a blob of nineteen such
    routines that measured 20% larger than this.
    """
    return re.sub(
        r"(?m)^(\s*)struct (mm_l_\w+) \*__L = mm_lheap\(sizeof \*__L\);$",
        r"\1struct \2 __Lv; struct \2 *__L = &__Lv;\n"
        r"\1mm_zeroed(__L, sizeof *__L);",
        body)


def strip_statics(body):
    """Remove mmb2c's C `static` declarations and its once-only init blocks.

    The variables now arrive as pointers - bind_globals rewrites the names -
    and the wrapper's own STATIC does the initialising, so both are dead
    weight, and both would put writable data in a blob that cannot have any.
    """
    out, lines, i = [], body.split("\n"), 0
    while i < len(lines):
        ln = lines[i]
        if re.match(r"\s*static\s+[A-Za-z_].*;\s*$", ln):
            i += 1
            continue
        if re.match(r"\s*if \(!__once_[A-Za-z_0-9]+\) \{", ln):
            depth = ln.count("{") - ln.count("}")
            i += 1
            while i < len(lines) and depth > 0:
                depth += lines[i].count("{") - lines[i].count("}")
                i += 1
            continue
        out.append(ln)
        i += 1
    return "\n".join(out)


def bind_globals(body, routine, flat, heap):
    """Point this routine's global references at the pointers in CFuncRam.

    Done by rewriting the body rather than with #define, because a macro has no
    scope: a program with a global `i` and a local `i` in some other routine
    gives both the C name v_i, and a file-scope #define would rewrite the
    LOCAL's declaration into a syntax error.

    Substituting per routine is sound precisely because that ambiguity cannot
    arise inside one routine - mmb2c does not record a global as touched by a
    routine that has a local of the same name (note_touch skips it), so within
    a single body v_i means one thing or the other, never both.
    """
    decls = []
    for k, (nm, s) in enumerate(flat):
        owner = getattr(s, "owner", None)
        if owner is not None:
            # a STATIC: bind only the owning routine's, because another
            # routine's STATIC can share mmb2c's C name for it
            if owner is not routine:
                continue
        elif nm not in routine.gtouch:
            continue
        ptr = "__gp_" + cname_of(nm)
        # An array or a string already IS the pointer; a scalar is dereferenced.
        if s.is_array or s.ty == "s":
            body = re.sub(r"\b" + re.escape(s.acc) + r"\b", ptr, body)
        else:
            body = re.sub(r"\b" + re.escape(s.acc) + r"\b", "(*%s)" % ptr, body)
        # Read the pointer ONCE into a local rather than substituting the
        # CFuncRam expression at every use: the compiler then keeps it in a
        # register. Inlining it cost about 6% on a globals-heavy inner loop.
        decls.append("    %s *const %s = (%s *)MMCSUB_G[%d];"
                     % (CT[s.ty], ptr, CT[s.ty], len(heap) + k))
    if decls:
        head, rest = body.split("\n", 1)
        body = head + "\n" + "\n".join(decls) + "\n" + rest
    return body


def add_polls(body):
    """Put mm_poll() at the top of every loop body.

    The interpreter tests the break key between statements.  A CSUB is ONE
    statement, so a converted loop that runs for thirty seconds cannot be
    interrupted at all - the board simply stops answering, which is exactly
    what the KnivD benchmark did the first time it was converted.  mm_poll()
    calls CheckAbort once in every 1024 times round, far below the cost of any
    loop worth converting.
    """
    out = []
    for ln in body.split("\n"):
        out.append(ln)
        st = ln.strip()
        if ln.rstrip().endswith("{") and (st.startswith("for (")
                                          or st.startswith("while (")
                                          or st == "do {"):
            out.append(" " * (len(ln) - len(ln.lstrip()) + 4) + "mm_poll();")
    return "\n".join(out)


def heap_member(s):
    """One member of our struct __gv, for a global mmb2c reaches as H->name.

    mmb2c lays every global array out inside one block and indexes it with its
    real shape, so a 2-D array is written H->v_x[i][j].  The interpreter
    allocates each array separately, so the member here is a POINTER to the
    caller's storage - and for more than one dimension that has to be a pointer
    to an array, or the second subscript has nothing to apply to.  mmb2c
    declares the dimensions reversed; the outermost one is what the pointer
    replaces.
    """
    ct = CT[s.ty]
    nm = s.acc[3:]
    dims = getattr(s, "dims", None) or []
    if s.is_array and len(dims) > 1:
        rest = "".join("[%s]" % d for d in reversed(dims[:-1]))
        return "%s (*%s)%s;" % (ct, nm, rest)
    if s.ty == "s" and s.is_array:
        return "char (*%s)[%s];" % (nm, s.slen + 1 if s.slen else "STRINGSIZE")
    return "%s *%s;" % (ct, nm)


def bounds_used(text, p):
    """Does the blob actually read this array parameter's bounds?

    mmb2c always gives an array parameter a companion __b_ argument because
    BOUND() inside the routine would need it.  Most kernels never call BOUND(),
    and every argument not passed is one more the caller can spend - which
    matters, because MAX_CSUB_ARGS is 10 on the RP2040.  So pass NULL when the
    generated code never mentions it.
    """
    # every function's FIRST line is its signature, which always names the
    # __b_ argument - only a mention in a body counts
    bodies_only = "\n".join(
        ln for b in text.split("\n}\n") for ln in b.split("\n")[1:])
    return ("__b_" + p.name.replace(".", "__")) in bodies_only


def entry_shim(conv, routine, entry, bodytext, gl, gtab=False):
    """The CSUB entry: cast the interpreter's void*, publish the globals, call.

    The routine's own parameters go straight through as arguments.  The globals
    go into CFuncRam, where every function in the blob can reach them - a
    callee's signature is fixed by mmb2c and cannot be given them.
    """
    args, decl, passed, slot = [], [], [], 0
    sfx = lambda nm, arr, ty: nm + {"f": "!", "i": "%", "s": "$"}[ty] + ("()" if arr else "")

    retslot = None
    if routine.is_func:
        # MMBasic passes every argument by reference, so a FUNCTION's result is
        # just one more out-parameter and the generated C is used as emitted.
        # A STRING function already works that way in mmb2c's output - it takes
        # its destination as its first parameter, char *__ret - so there the
        # slot is handed straight through rather than assigned to.
        decl.append("void *a0")
        passed.append(sfx("__r", False, routine.ty))
        slot = 1
        if routine.ty == "s":
            args.append("(char *)a0")
        else:
            retslot = CT[routine.ty]
    for p in routine.params:
        args.append("(%s *)a%d" % (CT[p.ty], slot))
        decl.append("void *a%d" % slot)
        passed.append(sfx(p.name, p.is_array, p.ty))
        slot += 1
        if p.is_array:
            if bounds_used(bodytext, p):
                args.append("(const MMINTEGER *)a%d" % slot)
                decl.append("void *a%d" % slot)
                passed.append("Bound(%s,1)" % sfx(p.name, True, p.ty))
                slot += 1
            else:
                args.append("0")      # never read

    # globals: the heap-resident ones first, so they lie over struct __gv
    heap = [(nm, s) for nm, s in gl if s.acc.startswith("H->")]
    flat = [(nm, s) for nm, s in gl if not s.acc.startswith("H->")]
    sets = []
    if gl and gtab:
        # ONE argument for all of them: an INTEGER array of addresses that the
        # wrapper fills with PEEK(VARADDR x). That is not an approximation of
        # the pointer the ABI would have passed - PEEK(VARADDR) calls findvar
        # with the same flags CallCFunction does, so it IS that pointer. The
        # argument ceiling then stops mattering however many globals there are.
        decl.append("void *a%d" % slot)
        passed.append("__gt()")
        sets.append("    {")
        sets.append("        const MMINTEGER *__t = (const MMINTEGER *)a%d;" % slot)
        for k, (nm, s) in enumerate(heap):
            sets.append("        H->%s = (%s *)(unsigned)__t[%d];"
                        % (s.acc[3:], CT[s.ty], k))
        for k, (nm, s) in enumerate(flat):
            sets.append("        MMCSUB_G[%d] = (void *)(unsigned)__t[%d];"
                        % (len(heap) + k, len(heap) + k))
        sets.append("    }")
        slot += 1
    else:
        for k, (nm, s) in enumerate(heap):
            sets.append("    H->%s = (%s *)a%d;" % (s.acc[3:], CT[s.ty], slot))
            decl.append("void *a%d" % slot)
            passed.append(sfx(nm, s.is_array, s.ty))
            slot += 1
        for k, (nm, s) in enumerate(flat):
            sets.append("    MMCSUB_G[%d] = a%d;" % (len(heap) + k, slot))
            decl.append("void *a%d" % slot)
            passed.append(sfx(nm, s.is_array, s.ty))
            slot += 1

    lines = ["/* CSUB entry - the interpreter hands us one pointer per argument. */",
             "long long %s(%s)" % (entry, ", ".join(decl) if decl else "void"),
             "{",
             "    mm_scratch_reset();"]
    lines += sets
    call = "%s(%s)" % (routine.cname, ", ".join(args))
    lines.append("    *(%s *)a0 = %s;" % (retslot, call) if retslot
                 else "    %s;" % call)
    lines.append("    return 0;")
    lines.append("}")
    return "\n".join(lines), slot, passed


def type_list(routine, bodytext, gl, gtab=False):
    """The `CSUB name INTEGER, FLOAT, ...` type list, matching entry_shim.

    It has to cover the globals too: a routine with no parameters can still
    take five arguments, and without a list CallCFunction takes the untyped
    path and the interpreter stops checking them.
    """
    T = {"f": "FLOAT", "i": "INTEGER", "s": "STRING"}
    out = []
    if routine.is_func:
        out.append(T[routine.ty])
    for p in routine.params:
        out.append(T[p.ty])
        if p.is_array and bounds_used(bodytext, p):
            out.append("INTEGER")      # the bound
    if gl and gtab:
        out.append("INTEGER")      # the single array of addresses
    else:
        for nm, sym in sorted(gl, key=lambda g: not g[1].acc.startswith("H->")):
            out.append(T[sym.ty])
    return ", ".join(out)


def repack(block):
    """Strip the CSUB's comments and refill its lines to eight words.

    armcfgen names each function in the blob with a comment line of its own and
    starts a fresh line at each function, so a blob of many small routines comes
    out as ragged five- and six-word lines with a comment between each.  Both
    cost program memory: the interpreter stores the text.

    The words themselves are untouched and stay in order - only how they are
    laid out changes - so the blob the firmware ends up with is identical.
    The entry-offset word keeps a line of its own, being a different thing from
    the code that follows it.

    Repacking goes with --lean because it makes the markers WRONG rather than
    merely absent: a marker names the words that follow it, and after refilling
    the lines it would point at the wrong place.
    """
    head, words = None, []
    for ln in block.split("\n"):
        st = ln.strip()
        if not st or st.startswith("'"):
            continue
        if st.upper().startswith("CSUB "):
            head = ln.rstrip()
            continue
        if st.upper().startswith("END CSUB"):
            continue
        words.extend(st.split())
    if head is None or not words:
        return block
    out = [head, "\t" + words[0]]            # the entry offset
    code = words[1:]
    for i in range(0, len(code), 8):
        out.append("\t" + " ".join(code[i:i + 8]))
    out.append("End CSUB")
    return "\n".join(out) + "\n"


def adapter_sub(name, routine, entry, passed, gl=(), gtab=False, base=0):
    """A thin MMBasic SUB or FUNCTION with the ORIGINAL name.

    It supplies what the CSUB ABI cannot carry - a FUNCTION's result, an
    array's BOUND(), the globals - so that call sites do not change.  When the
    CSUB takes exactly the routine's own arguments there is nothing to add and
    the CSUB carries the original name itself.
    """
    ps = [SFX[p.ty](p.name, p.is_array) for p in routine.params]
    pre = []
    # The wrapper owns every STATIC in the blob. MMBasic's STATIC gives the
    # lifetime the C `static` would have had - one instance, initialised once -
    # and the declarator is copied from the original so bounds and initialiser
    # come across exactly as written.
    for nm, s in gl:
        if getattr(s, "owner", None) is None:
            continue
        base = SFX[s.ty](nm, False)
        # "(4)" joins on; "= 1.5" needs the space
        if s.decl.startswith("("):
            pre.append("  Static %s%s" % (base, s.decl))
        elif s.decl:
            pre.append("  Static %s %s" % (base, s.decl))
        else:
            pre.append("  Static %s" % base)
    if gl and gtab:
        # One argument for every global: an array of their addresses.
        # PEEK(VARADDR x) is findvar with the same flags CallCFunction uses to
        # build an argument pointer, so these ARE the pointers the ABI would
        # have passed - and it costs one findvar each, exactly as passing them
        # separately would have. Built fresh every call rather than cached,
        # because a REDIM between calls would strand a cached address.
        heap = [(nm, s) for nm, s in gl if s.acc.startswith("H->")]
        flat = [(nm, s) for nm, s in gl if not s.acc.startswith("H->")]
        order = heap + flat
        # The subscripts have to start at the program's OPTION BASE, and the
        # C side reads from the data pointer, which is the FIRST element
        # whichever base that is - so the two agree without knowing it.
        pre.append("  Local Integer __gt(%d)" % (base + len(order) - 1))
        for k, (nm, s) in enumerate(order):
            pre.append("  __gt(%d) = Peek(VARADDR %s)"
                       % (base + k, SFX[s.ty](nm, s.is_array)))
    body = "\n".join(pre) + ("\n" if pre else "")
    if routine.is_func:
        # `name` is already spelled as the source declares it, suffix or not,
        # and is used verbatim: adding a suffix the original did not have
        # breaks every existing call site with "Inconsistent type suffix".
        fn = name
        rn = SFX[routine.ty]("__r", False)     # the local DOES need its type
        return ("Function %s(%s)\n  Local %s\n%s  %s %s\n  %s = %s\nEnd Function\n"
                % (fn, ", ".join(ps), rn, body, entry, ", ".join(passed), fn, rn))
    if not pre and list(ps) == list(passed):
        return None
    return ("Sub %s%s\n%s  %s %s\nEnd Sub\n"
            % (name, (" " + ", ".join(ps)) if ps else "", body,
               entry, ", ".join(passed)))


def write_c(cpath, source, subname, routines, bodies, shim, gl, consts,
            lstructs=()):
    """Write the translation unit: globals, every function in the blob, the shim.

    The bodies are mmb2c's output character for character.  Only names are
    redirected around them, by #defines that stay in force for the whole blob
    because a callee needs them just as much as the entry does.
    """
    heap = [(nm, s) for nm, s in gl if s.acc.startswith("H->")]
    flat = [(nm, s) for nm, s in gl if not s.acc.startswith("H->")]
    with open(cpath, "w") as f:
        f.write("/* Generated by mmb2csub.py from %s, %s.\n"
                " * Do not edit - regenerate instead. */\n" % (source, subname))
        f.write('#include "mmcsub.h"\n\n')
        for cn, val in consts:
            f.write("#define %s %s\n" % (cn, val))
        if consts:
            f.write("\n")
        if gl:
            f.write("/* Globals the blob reaches. A CSUB has no writable static data,\n"
                    "   so each one arrives as a pointer in CFuncRam and the names the\n"
                    "   generated bodies use are pointed at it. */\n")
        if heap:
            f.write("struct __gv {\n")
            for nm, s in heap:
                f.write("    %s\n" % heap_member(s))
            f.write("};\n#define H ((struct __gv *)MMCSUB_G)\n")
        f.write("\n")
        if lstructs:
            f.write("\n".join(lstructs) + "\n\n")
        if len(routines) > 1:
            f.write("/* every function in the blob, so the calls between them resolve */\n")
            for r in routines:
                f.write("static %s;\n" % bodies[r.cname].split("\n", 1)[0].rstrip(" {"))
            f.write("\n")
        for r in routines:
            f.write("static "
                    + add_polls(bind_globals(
                        frame_on_stack(strip_statics(bodies[r.cname])),
                        r, flat, heap))
                    + "\n\n")
        f.write(shim + "\n")


def try_build(conv, mmb2c, routine, name, opt="s"):
    """Can this routine be converted, and at what cost?

    Returns (ok, detail, blob_bytes, closure_names).  Actually compiles and
    links, because the compiler is the scope check - anything else would be a
    guess that goes stale.
    """
    import tempfile
    try:
        rs = closure(conv, routine)
        bodies = {}
        for r in rs:
            b = slice_function(conv, r.cname)
            if b is None:
                return False, "%s did not translate" % r.name, 0, [], 0
            bodies[r.cname] = b
        bodytext = "\n".join(bodies[r.cname] for r in rs)
        probs = check_scope(conv, routine, name, rs)
        if probs:
            return False, probs[0], 0, [r.name for r in rs], 0
        gl = globals_touched(conv, rs) + statics_touched(conv, rs)
        if len(gl) > 62:
            return False, "%d globals; CFuncRam holds 62" % len(gl), 0, [], 0
        shim, nargs, passed = entry_shim(conv, routine, name + "K", bodytext, gl)
    except Exception as e:
        return False, "%s: %s" % (type(e).__name__, e), 0, [], 0

    tmp = tempfile.mkdtemp()
    cpath = os.path.join(tmp, "%s.c" % name)
    write_c(cpath, "(list)", name, rs, bodies, shim, gl,
            consts_used(conv, mmb2c, bodytext), local_structs_for(conv, rs))
    out = os.path.join(tmp, "%s.txt" % name)
    r = subprocess.run([sys.executable, ARMCFGEN, cpath, "--compile",
                        "-n", name + "K", "-e", name + "K", "-O", opt,
                        "-I", FIRMWARE, "-I", HERE, "-o", out] + CPU_ARGS,
                       capture_output=True, text=True)
    if r.returncode != 0:
        why = "did not build"
        syms = sorted({l.split("`")[1].rstrip("'")
                       for l in r.stderr.splitlines() if "undefined reference to" in l})
        if syms:
            why = "needs " + ", ".join(syms[:3])
        else:
            errs = [l for l in r.stderr.splitlines() if "error:" in l]
            if errs:
                why = errs[0].split("error:")[-1].strip()[:44]
        return False, why, 0, [r_.name for r_ in rs], 0
    blob = open(out).read()
    words = sum(len(l.split()) for l in blob.splitlines()
                if l.startswith("\t") and not l.lstrip().startswith("'"))
    return True, "", (words - 1) * 4, [x.name for x in rs],         arg_count(routine, bodytext, gl)


def warn_overlap(conv, names):
    """Say so when two chosen routines would each carry the same callees.

    Each becomes a separate CSUB with its own copy of its closure, so picking
    two routines from the same call tree pays for the shared part twice. The
    usual answer is to convert only the one higher up.
    """
    cl = {}
    for nm in names:
        r = conv.routines.get(nm) or conv.routines.get(nm.lower())
        if r is not None:
            cl[nm] = set(x.name for x in closure(conv, r))
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            if a not in cl or b not in cl:
                continue
            if b in cl[a]:
                print("note: %s already carries %s - converting both duplicates it"
                      % (a, b))
            elif a in cl[b]:
                print("note: %s already carries %s - converting both duplicates it"
                      % (b, a))
            else:
                both = cl[a] & cl[b]
                if both:
                    print("note: %s and %s share %d routine%s (%s) - each blob "
                          "gets its own copy"
                          % (a, b, len(both), "" if len(both) == 1 else "s",
                             ", ".join(sorted(both)[:4])))


def do_list(conv, mmb2c, opt):
    """Report what can be converted, what each costs, and what covers what."""
    names = sorted(conv.routines)
    res = {}
    sys.stderr.write("checking %d routines" % len(names))
    for nm in names:
        res[nm] = try_build(conv, mmb2c, conv.routines[nm], nm, opt)
        sys.stderr.write(".")
        sys.stderr.flush()
    sys.stderr.write("\n")

    okset = set(n for n in names if res[n][0])
    # a routine is a ROOT if no other convertible routine's closure contains it
    covered_by = {}
    for n in okset:
        for other in res[n][3]:
            if other != n:
                covered_by.setdefault(other, []).append(n)
    roots = [n for n in okset if n not in covered_by]

    print("\nConvertible, and not already inside another one's blob:")
    print("  %-14s %6s %8s %7s %7s  %s"
          % ("routine", "args", "blob", "prog", "closure", "also brings in"))
    for n in sorted(roots, key=lambda x: -res[x][2]):
        ok, why, size, cl, na = res[n]
        others = sorted(c for c in cl if c != n)
        joined = ", ".join(others)
        # What has to fit in PROGRAM memory, which is the blob TWICE over:
        # the hex text of the CSUB block (about 2.4x) plus the binary the
        # interpreter builds from it (1x). The board shows the two on
        # separate MEMORY lines. An argument count over the ceiling means
        # it cannot be called at all, however well it compiled.
        print("  %-14s %6s %8d %6dk %7d  %s"
              % (n, ("%d !" % na) if na > 16 else str(na), size,
                 int(size * 3.4) // 1024, len(cl),
                 (joined[:58] + "...") if len(joined) > 58 else (joined or "-")))
    sub = sorted(okset - set(roots), key=lambda x: -res[x][2])
    if sub:
        # Shown with their own numbers, because when a root is out of reach -
        # too many arguments, or too much program memory - the next thing to
        # try is the largest routine BELOW it that is not.
        print("\nAlso convertible, and worth having if a root above is out of reach:")
        print("  %-14s %6s %8s %7s %7s  %s"
              % ("routine", "args", "blob", "prog", "closure", "carried by"))
        for n in sub:
            ok, why, size, cl, na = res[n]
            print("  %-14s %6s %8d %6dk %7d  %s"
                  % (n, ("%d !" % na) if na > 16 else str(na), size,
                     int(size * 3.4) // 1024, len(cl),
                     ", ".join(sorted(covered_by[n])[:3])))
    bad = [n for n in names if not res[n][0]]
    if bad:
        print("\nNot convertible:")
        for n in bad:
            print("  %-14s %s" % (n, res[n][1]))
    print("\nprog is what has to fit in PROGRAM memory: the hex TEXT of the CSUB")
    print("plus the binary built from it, about 3.4x the blob. Compare it with the")
    print("Free figure from MEMORY on the board - a missing 'Embedded C Routine'")
    print("line there means the binary did not fit. --library moves it out of the")
    print("program entirely, for LIBRARY SAVE.")


# ---------------------------------------------------------------- main

def find_routine_text(conv, routine):
    """The source lines of the routine, as [start, end] indices into conv.lines.

    MMBasic routines cannot nest, so the first END SUB / END FUNCTION after the
    definition line is the end of this one.
    """
    start = routine.line - 1
    while start < len(conv.lines) and not re.match(
            r"\s*(sub|function)\b", conv.lines[start], re.I):
        start += 1
    if start >= len(conv.lines):
        return None
    for j in range(start + 1, len(conv.lines)):
        if re.match(r"\s*end\s*(sub|function)\b", conv.lines[j], re.I):
            return [start, j]
    return None


def declared_name(conv, routine, fallback):
    """The routine's name exactly as its own declaration spells it.

    MMBasic lets a FUNCTION be declared with or without a type suffix - both
    `FUNCTION Deep(n%)` and `FUNCTION Deep!(n%)` return a float - and the two
    are the SAME routine, so mmb2c stores one canonical name for both. The
    wrapper cannot: it has to be declared the way the call sites already spell
    it, or MMBasic rejects the program with "Inconsistent type suffix".
    """
    span = find_routine_text(conv, routine)
    if span is not None:
        m = re.match(r"\s*(?:sub|function)\s+([A-Za-z_][A-Za-z_0-9.]*[$%!]?)",
                     conv.lines[span[0]], re.I)
        if m:
            return m.group(1)
    # No declaration to read: spell it explicitly rather than let the
    # wrapper inherit whatever OPTION DEFAULT happens to be.
    return SFX[routine.ty](fallback, False) if routine.is_func else fallback


def calls_other_routines(routine, cbody):
    """Names of other BASIC routines this one calls.

    For now a converted routine has to be self-contained: only its own function
    goes into the blob, so a call to another one would be an undefined symbol at
    link time.  Catching it here says so in the user's terms instead.
    """
    seen = re.findall(r"\bf_([A-Za-z_][A-Za-z_0-9]*)\s*\(", cbody)
    return sorted(set(m for m in seen if ("f_" + m) != routine.cname))


MARKER = "' --- mmb2csub: "


def write_library(path, source, subname, wrapper, block, ctext, include_c,
                  fresh):
    """The CSUB and its wrapper, in a file of their own.

    For LIBRARY SAVE, which stores the binary WITHOUT the hex text - the
    difference between about 107 KB and 32 KB for a large blob, and the only
    way a big conversion fits alongside the program that uses it.  The wrapper
    goes in too: the library holds it, so call sites in the main program still
    find the original name.

    Appended rather than replaced when several routines are converted in one
    command, so they end up in one library together.
    """
    out = []
    if fresh:
        out.append("' CSUBs generated by mmb2csub.py from %s.\n"
                   % os.path.basename(source))
        out.append("'\n")
        out.append("' These are meant for the library, not to be run:\n")
        out.append("'     LOAD \"%s\"\n" % os.path.basename(path))
        out.append("'     LIBRARY SAVE\n")
        out.append("' then load %s and run it as usual.\n"
                   % os.path.basename(source))
    out.append("\n")
    out.append(MARKER + "%s\n" % subname)
    if wrapper:
        out.append("' The wrapper keeps the original name, so call sites are unchanged.\n")
        out.append(wrapper if wrapper.endswith("\n") else wrapper + "\n")
        out.append("\n")
    out.append(block.rstrip("\n") + "\n")
    if include_c:
        out.append("\n' The C this was compiled from, for reference. Regenerate with:\n")
        out.append("'     python mmb2csub.py %s %s\n'\n"
                   % (os.path.basename(source), subname))
        for ln in ctext.splitlines():
            out.append("'" + ln + "\n")
    with open(path, "w" if fresh else "a") as f:
        f.writelines(out)


def rewrite_source(path, conv, routine, subname, wrapper, block, ctext,
                   include_c, keep_original, backup, allow_converted=False,
                   library=None):
    """Replace the original routine with the CSUB, and append it.

    Both the commented-out original and the generated C are kept by default,
    because between them they are the whole record of where the hex came from.
    Both can be dropped, because a comment costs PROGRAM MEMORY on the board -
    the interpreter stores the text - and on a tight machine that is the
    difference between a program that loads and one that does not.  Neither is
    lost by dropping it: the .bak has the original, and the C regenerates.
    """
    span = find_routine_text(conv, routine)
    if span is None:
        sys.exit("error: could not locate '%s' in the source text" % subname)
    lines = [ln if ln.endswith("\n") else ln + "\n" for ln in conv.source_lines]
    if not allow_converted and any(MARKER in ln for ln in lines):
        sys.exit("error: %s already holds a mmb2csub conversion. Restore it from "
                 "the .bak before converting again." % path)

    if backup:
        with open(path + ".bak", "w") as f:
            f.writelines(lines)

    if keep_original:
        original = lines[span[0]:span[1] + 1]
        if CPU_ARGS:
            # the CMM2 has no block comment (yet): an apostrophe on each line
            lines[span[0]:span[1] + 1] = (
                [MARKER + "%s replaced by a CSUB; original follows\n" % subname]
                + ["'" + ln for ln in original])
        else:
            # MMBasic's block comment; the interpreter wants /* and */ on lines of
            # their own, which is also what makes it easy to restore by hand
            lines[span[0]:span[1] + 1] = (
                ["/*\n", MARKER + "%s replaced by a CSUB; original follows\n" % subname]
                + original + ["*/\n"])
    else:
        lines[span[0]:span[1] + 1] = [
            MARKER + "%s is now the CSUB below (original in %s.bak)\n"
            % (subname, os.path.basename(path))]

    if library is not None:
        # The CSUB lives in its own file; the program keeps only the record of
        # what was taken out of it.
        with open(path, "w") as f:
            f.writelines(lines)
        return

    out = ["\n", MARKER + "generated code for %s\n" % subname]
    if wrapper:
        out.append("' The wrapper keeps the original name, so call sites are unchanged.\n")
        out.append(wrapper if wrapper.endswith("\n") else wrapper + "\n")
        out.append("\n")
    out.append(block.rstrip("\n") + "\n")
    if include_c:
        out.append("\n' The C this was compiled from, for reference. Regenerate with:\n")
        out.append("'     python mmb2csub.py %s %s\n'\n"
                   % (os.path.basename(path), subname))
        # one apostrophe per line rather than /* */, because the C carries its
        # own /* */ comments and MMBasic's block comment does not nest
        for ln in ctext.splitlines():
            out.append("'" + ln + "\n")

    with open(path, "w") as f:
        f.writelines(lines)
        f.writelines(out)


def main():
    ap = argparse.ArgumentParser(
        description="Convert one MMBasic SUB or FUNCTION into a CSUB, in place.")
    ap.add_argument("source", help="the .bas program")
    ap.add_argument("sub", nargs="*",
                    help="the SUB(s) or FUNCTION(s) to convert. Each becomes a "
                         "SEPARATE CSUB with its own copy of everything it "
                         "calls, so prefer one routine high in the call graph "
                         "over several below it")
    ap.add_argument("--gtab", action="store_true",
                    help="always pass the globals as one array of addresses")
    ap.add_argument("--no-gtab", action="store_true",
                    help="never do that; fail instead if there are too many")
    ap.add_argument("--list", action="store_true",
                    help="report what can be converted and what it costs, "
                         "and change nothing")
    ap.add_argument("--name", help="CSUB name (default <sub>K)")
    ap.add_argument("-O", "--opt", default="s", help="armcfgen -O level (default s)")
    ap.add_argument("--target", choices=["picomite", "cmm2"], default="picomite",
                    help="the board the CSUB is for: picomite (Cortex-M0+ code, the default, which runs on "
                         "every PicoMite) or cmm2 (the Colour Maximite 2's Cortex-M7, hard-float). A CSUB "
                         "runs only on the board it was built for")
    ap.add_argument("--mmb2c", help="path to mmb2c.py")
    ap.add_argument("--keep-c", action="store_true",
                    help="also leave the generated .c on disk")
    ap.add_argument("--library", metavar="FILE",
                    help="write the CSUBs and their wrappers to FILE instead "
                         "of appending them to the program, for LIBRARY SAVE. "
                         "The program keeps the originals commented out. "
                         "--lean then applies to FILE only.")
    ap.add_argument("--no-c", action="store_true",
                    help="do not keep the generated C as comments")
    ap.add_argument("--no-original", action="store_true",
                    help="delete the original BASIC routine instead of "
                         "commenting it out")
    ap.add_argument("--lean", action="store_true",
                    help="both of the above - keep nothing but the CSUB. "
                         "Comments cost program memory on the board, and the "
                         ".bak still holds the original either way")
    ap.add_argument("--already-converted", action="store_true",
                    help=argparse.SUPPRESS)  # set on the per-routine re-runs
    ap.add_argument("--no-backup", action="store_true",
                    help="do not write <source>.bak")
    ap.add_argument("--dry-run", action="store_true",
                    help="build and report, but leave the source alone")
    args = ap.parse_args()
    if args.target == "cmm2":
        CPU_ARGS[:] = ["--cpu", "cortex-m7"]

    # A bare filename for --library goes beside the PROGRAM, not into whatever
    # directory the tool happens to be run from - which is where mmb2csub.py
    # lives, so `--library sefunc.bas` used to drop it in user-tools/. The
    # generated .c and .txt already resolve against the source this way.
    # A path given with any directory part at all is taken as meant.
    if args.library and not os.path.dirname(args.library):
        args.library = os.path.join(
            os.path.dirname(os.path.abspath(args.source)), args.library)

    mmb2c = load_mmb2c(args.mmb2c)
    conv = convert_program(mmb2c, args.source)

    if args.list:
        do_list(conv, mmb2c, args.opt)
        return
    if not args.sub:
        sys.exit("error: name a SUB or FUNCTION to convert, or use --list to "
                 "see what can be")

    if len(args.sub) > 1:
        # Each routine becomes its own CSUB with its own copy of everything it
        # calls, so converting two that share callees duplicates them. Say so,
        # then do them one at a time - each conversion rewrites the source, so
        # the next has to re-read it.
        warn_overlap(conv, args.sub)
        rest = [a for a in sys.argv[1:] if a not in args.sub and a != args.source]
        # ONE .bak for the whole command, written here. It has to hold the
        # program as it was before any of the conversions, so the children
        # must not each write their own - the last would otherwise preserve
        # the second-to-last conversion rather than the original.
        if not args.no_backup:
            with open(args.source + ".bak", "w") as f:
                f.writelines(ln if ln.endswith("\n") else ln + "\n"
                             for ln in conv.source_lines)
            print("original saved as %s.bak" % args.source)
        # Each conversion leaves its marker behind, and the next would read
        # that as "already converted" - which is the right answer for a second
        # run of the same command, but not for the second routine of this one.
        rest = [a for a in rest if a != "--no-backup"]
        rest += ["--no-backup", "--already-converted"]
        # The children append to the library so they share one file, which
        # means the first of them must not find a stale one.
        if args.library and os.path.exists(args.library):
            os.unlink(args.library)
        for nm in args.sub:
            rc = subprocess.run([sys.executable, __file__, args.source, nm] + rest)
            if rc.returncode:
                sys.exit(rc.returncode)
        return

    canon = args.sub[0].upper()
    routine = None
    for k, r in conv.routines.items():
        if k.upper() == canon:
            routine = r
            break
    if routine is None:
        sys.exit("error: no SUB or FUNCTION named '%s' in %s\n  found: %s"
                 % (args.sub[0], args.source, ", ".join(sorted(conv.routines))))

    # Everything the routine calls comes with it: mmb2c emits an inter-routine
    # call as f_other(...), and armcfgen's merge mode packs them into one blob
    # and resolves the calls between them.
    routines = closure(conv, routine)
    bodies = {}
    for r in routines:
        b = slice_function(conv, r.cname)
        if b is None:
            sys.exit("error: could not find '%s' in the generated C. %s may have "
                     "failed to translate - run mmb2c.py on the file to see why."
                     % (r.cname, r.name))
        bodies[r.cname] = b
    bodytext = "\n".join(bodies[r.cname] for r in routines)

    problems = check_scope(conv, routine, args.sub[0], routines)
    if problems:
        print("error: %s cannot be a CSUB yet:" % args.sub[0])
        for p in problems:
            print("  - " + p)
        sys.exit(1)

    # A STATIC rides with the globals: same pointer-in, same wrapper.
    gl = globals_touched(conv, routines) + statics_touched(conv, routines)
    if len(gl) > 62:
        sys.exit("error: %s and what it calls reach %d globals; CFuncRam holds 62"
                 % (args.sub[0], len(gl)))

    # Name it first, because the name depends on whether a wrapper is needed.
    # When the CSUB takes exactly the routine's own arguments there is nothing
    # for a wrapper to add, so the CSUB takes the ORIGINAL name and the call
    # sites go straight to it - MMBasic calls a CSUB exactly like a SUB. Only
    # when a wrapper has to sit in front (a FUNCTION's result, an array bound,
    # globals) does the CSUB need a name of its own for the wrapper to call.
    # One argument per global reaches the ceiling quickly - MAX_CSUB_ARGS is 10
    # on the RP2040 - so past that they travel as a single array of addresses
    # the wrapper builds, which has no practical limit. Below it they are
    # passed directly, which needs no wrapper at all when there are none.
    _, direct_n, _ = entry_shim(conv, routine, "probe", bodytext, gl)
    gtab = args.gtab or (direct_n > 10 and not args.no_gtab)
    _, _, probe = entry_shim(conv, routine, "probe", bodytext, gl, gtab)
    # The wrapper wears the name the program already calls, suffix and all;
    # the CSUB itself takes the suffix-free one with K on the end.
    dispname = declared_name(conv, routine, args.sub[0])
    needs_wrapper = adapter_sub(dispname, routine, "probe", probe,
                                gl, gtab, conv.opt_base) is not None
    entry = args.name or ((args.sub[0] + "K") if needs_wrapper else args.sub[0])
    shim, nargs, passed = entry_shim(conv, routine, entry, bodytext, gl, gtab)
    if nargs > 16:
        sys.exit("error: %s would need %d arguments (%d of them globals), and "
                 "MAX_CSUB_ARGS is 16 on RP2350, 10 on RP2040.\n"
                 "  A routine reaching that many globals is usually the wrong "
                 "one to convert - try one further down the call graph, or "
                 "gather the globals into an array."
                 % (args.sub[0], nargs, len(gl)))

    stem = os.path.splitext(os.path.basename(args.source))[0]
    srcdir = os.path.dirname(os.path.abspath(args.source))
    cpath = os.path.join(srcdir, "%s_%s.c" % (stem, args.sub[0].lower()))
    write_c(cpath, args.source, args.sub[0], routines, bodies, shim, gl,
            consts_used(conv, mmb2c, bodytext),
            local_structs_for(conv, routines))

    out = os.path.join(srcdir, "%s_%s.txt" % (stem, args.sub[0].lower()))
    cmd = [sys.executable, ARMCFGEN, cpath, "--compile",
           "-n", entry, "-e", entry, "-O", args.opt,
           "-I", FIRMWARE, "-I", HERE, "-o", out] + CPU_ARGS
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stderr)
        sys.exit("\nerror: the CSUB did not compile. 'undefined reference to "
                 "mm_xxx' means that MMBasic feature has no CSUB runtime yet;\n"
                 "'undefined reference to __aeabi_xxx' means an arithmetic helper is "
                 "missing from mmcsub.h. Either way the tool is telling you the\n"
                 "truth - do not work around it.")

    block = open(out).read()
    block = block.replace("CSUB " + entry.upper(),
                          "CSUB %s %s" % (entry, type_list(routine, bodytext, gl, gtab)) +
                          (CMM2_MARK if CPU_ARGS else ""), 1)
    if args.lean:
        block = repack(block)
    open(out, "w").write(block)
    ctext = open(cpath).read()

    wrapper = adapter_sub(dispname, routine, entry, passed, gl, gtab,
                          conv.opt_base)
    nwords = sum(len(l.split()) for l in block.splitlines()
                 if l.startswith("\t") and not l.lstrip().startswith("'"))

    print("%s: %s %s -> CSUB %s"
          % (os.path.basename(args.source),
             "FUNCTION" if routine.is_func else "SUB", args.sub[0], entry))
    if gl:
        print("  globals passed by reference: " + ", ".join(nm for nm, _ in gl))
    print("  %d argument%s: %s"
          % (nargs, "" if nargs == 1 else "s", ", ".join(passed)))
    print("  blob: %d bytes of code" % (nwords * 4))
    if len(routines) > 1:
        print("  also in the blob: "
              + ", ".join(r.name for r in routines if r is not routine))

    if args.dry_run:
        print("\n(dry run - %s not modified)" % args.source)
        if wrapper:
            print("\nwrapper:\n" + wrapper)
        return

    # From the file, not from conv.lines: those have been re-joined with
    # newlines, so on a CRLF source they undercount by a byte a line.
    before = os.path.getsize(args.source)
    # With --library the CSUB is not in the program at all, so --lean has
    # nothing to strip THERE: it applies to the library file, and the program
    # keeps its commented-out original unless --no-original says otherwise.
    keep_orig = not (args.no_original or (args.lean and not args.library))
    include_c = not (args.no_c or args.lean)
    rewrite_source(args.source, conv, routine, args.sub[0], wrapper, block, ctext,
                   include_c, keep_orig,
                   not args.no_backup, args.already_converted, args.library)
    if args.library:
        # Fresh unless one of this command's earlier routines has already
        # started the file - the parent deletes any stale one first, so the
        # first routine in writes the header and the rest append to it.
        write_library(args.library, args.source, args.sub[0], wrapper, block,
                      ctext, include_c,
                      not args.already_converted
                      or not os.path.exists(args.library))
    after = os.path.getsize(args.source)
    os.unlink(out)
    if not args.keep_c:
        os.unlink(cpath)
    kept = []
    if keep_orig:
        kept.append("the original, commented out")
    if include_c:
        kept.append("the generated C")
    if args.library:
        # the PATH as resolved, not the basename: where it went is the thing
        # worth reporting, and a bare --library name now means beside the
        # program rather than the current directory
        print("  CSUB%s written to %s%s"
              % (" and wrapper" if wrapper else "", args.library,
                 " (--lean)" if args.lean else ""))
    else:
        print("  CSUB%s appended%s"
              % (" and wrapper" if wrapper else "",
                 ("; kept " + " and ".join(kept)) if kept else "; nothing else kept"))
    # The program TEXT is what the board stores, so this - not the blob size -
    # is what has to fit in program memory.
    print("  program text: %d -> %d bytes%s"
          % (before, after,
             "" if kept else " (--lean)"))
    if args.library:
        # LIBRARY SAVE reads the CSUB out of PROGRAM memory, so the library
        # file has to load first - and loading it costs the hex text AND the
        # binary the interpreter builds from it, both at once. If the pair
        # does not fit, the load stops quietly, the binary is never built,
        # and LIBRARY SAVE then writes the declaration without its code:
        # the CSUB is listed by LIBRARY LIST and faults when called. So give
        # the user the number to check against MEMORY before they try.
        libtext = os.path.getsize(args.library)
        need = libtext + nwords * 4
        print("  library file: %d bytes" % libtext)
        print("  LOAD it, then LIBRARY SAVE. That needs %d bytes of PROGRAM"
              % need)
        print("  memory free (the text plus the binary built from it) - check")
        print("  MEMORY first; a program too big to load fails silently.")
    elif kept:
        print("       --lean would keep only the CSUB")
    if not args.no_backup:
        print("  previous version saved as %s.bak" % os.path.basename(args.source))


if __name__ == "__main__":
    main()
