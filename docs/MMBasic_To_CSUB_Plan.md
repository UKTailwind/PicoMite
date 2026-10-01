# mmb2csub: an implementation plan

*Turning an MMBasic SUB into a CSUB in one command, by adding a CSUB target to
the Fuzix `mmb2c` transpiler.*

September 2026. Companion to `MMBasic_To_CSUB_Feasibility.md`, which argues the
case; this document is the build plan. Written against mmb2c.py as it stands
(10,912 lines) and PicoMite 6.03.02b8.

---

## 1. What the survey found

Five facts about mmb2c.py shape everything below. They are better news than the
feasibility note assumed.

**1.1 There is no IR — it is a single-pass, direct-to-C emitter.** `class Conv`
(lines 844–10665) owns the whole job. All C text goes out through two methods,
`emit()` (line 1162) and `raw()` (1176), appending to `self.out`. There is no
tree to retarget, so a CSUB backend is not "swap the code generator"; it is
"change what a small number of emit sites produce".

**1.2 Expressions are `(code_string, type)` pairs.** Every expression method —
`e_logical`, `e_compare`, `e_shift`, `e_add`, `e_mul`, `e_unary`, `e_pow`,
`e_primary` — returns a tuple of a C fragment and one of `TY_I`/`TY_F`/`TY_S`.
So the type of every operand is known **at the point the operator is emitted**.
That is exactly what a CSUB target needs, because the choice between `a + b` and
`FAdd(a, b)` is a type decision.

**1.3 A runtime-call convention already exists.** MMBasic `\` already emits
`mm_idiv(a, b)`, `MOD` emits `mm_mod(a, b)`, `/` emits `mm_fdiv(a, b)` when
checks are on, string concat emits `mm_scat`, float→int emits `mm_toint`. 217
distinct `mm_*` names are referenced across the file. So the emitter is already
in the habit of calling a runtime rather than emitting operators, and a CSUB
target is partly a matter of **pointing existing calls at different functions**.

**1.4 Array parameters already carry their bounds.** `signature()` (line 9971)
emits, for an array parameter, both `MMINTEGER *p_a` **and** `const MMINTEGER
*__b_a`, because `BOUND()` inside the routine has to read the caller's bounds.
Scalar `byref` parameters are already `TYPE *`. **A SUB whose parameters are all
by-reference is already almost CSUB-shaped** — the interpreter passes exactly
that, one pointer per argument.

**1.5 There is precedent for a target flag, but no target abstraction.**
`self.fcc` (line 864) selects C89 output for the Fuzix compiler and has about six
use sites. So adding `self.target` is idiomatic for this codebase; expecting a
clean backend seam is not.

---

## 2. Architecture

**Decision: a target mode inside mmb2c, not a fork and not a separate tool.**

```
mmb2c.py        Conv(...)  with  self.target in ('fuzix', 'csub')
  └── csub emission differences: §4 (about 12 localised sites)
mmcsub.h        the freestanding runtime subset for the CSUB target   (new)
mmb2csub.py     the driver: pick a SUB, drive Conv, drive armcfgen,   (new)
                splice the result back into the .bas
```

`mmb2csub.py` is the user-facing command. It imports mmb2c rather than
duplicating it, so keyword coverage keeps being shared. The alternative —
copying the front end — forks 10,000 lines and is rejected.

Everything the CSUB target needs beyond mmb2c is new code in two files. The
changes *inside* mmb2c.py are small, additive and guarded by `self.target ==
'csub'`, so the Fuzix path is untouched.

### 2.1 The adapter SUB: the idea that makes it drop-in

The CSUB ABI cannot express everything a SUB signature can: no more than ten
arguments, no return value, no array bounds, no globals. Rather than restricting
the language to what the ABI allows, **the generator emits two things**:

1. the CSUB block, whose entry takes whatever the kernel actually needs; and
2. optionally, a thin **MMBasic adapter SUB carrying the original name**, which
   marshals and calls it.

```basic
' original
SUB Mix(a(), n, k)  ...  END SUB

' generated
SUB Mix(a(), n, k)                       ' same name, same call sites
  MixK a(), n, k, Bound(a(),1)           ' the adapter supplies what the ABI can't
End Sub
CSUB MixK INTEGER, INTEGER, FLOAT, INTEGER
  ...
End CSUB
```

The adapter absorbs, in one mechanism, every ABI mismatch:

| Mismatch | How the adapter handles it |
|---|---|
| Array bounds not passed | adapter passes `Bound(a(),d)` as extra arguments |
| More than 10 arguments | adapter packs the tail into one INTEGER/FLOAT array |
| `FUNCTION` has no CSUB form | adapter is a FUNCTION; CSUB writes an out-parameter |
| Globals not visible | adapter passes the globals the kernel touches |
| Expression arguments | adapter's parameters are variables, so the CSUB always gets an lvalue |

Cost is one interpreter call per invocation. That is nothing for a kernel called
once a frame and fatal for one called per pixel, so `--no-adapter` emits the CSUB
alone with its real signature and leaves the call sites to the author.

This is the single most important design decision in the plan: it converts a
long list of "phase 1 cannot do X" restrictions into "phase 1 costs one
interpreter call for X".

---

## 3. The CSUB entry shim

`signature()` already produces the inner function. The generator wraps it:

```c
#define CSUB_MEM_SHIMS
#include "PicoCFunctions.h"
#include "mmcsub.h"

static void s_Mix(MMINTEGER *p_a, const MMINTEGER *__b_a,
                  MMINTEGER *p_n, MMFLOAT *p_k)
{
    MMINTEGER i;
    for (i = 0; i <= *p_n - 1; i++)
        p_a[i] = LDiv(FloatToInt(FAdd(IntToFloat(LMul(p_a[i], 3)), *p_k)), 2);
}

/* CSUB entry: the interpreter's ten void* unpacked. Entry function first in
   the file so armcfgen's -e finds it 4-byte aligned. */
long long MixK(void *a0, void *a1, void *a2, void *a3)
{
    MMINTEGER b_a[1];
    b_a[0] = *(MMINTEGER *)a3;          /* the bound the adapter passed */
    s_Mix((MMINTEGER *)a0, b_a, (MMINTEGER *)a1, (MMFLOAT *)a2);
    return 0;
}
```

Note what this shows: the body is ordinary mmb2c output with the arithmetic
retargeted, and the shim is mechanical. Nothing here requires new analysis.

---

## 4. The emission changes inside mmb2c.py

This is the exhaustive list. Every raw C arithmetic operator the emitter can
produce, and what the CSUB target must produce instead. The "inline" rows are
measured on `-mcpu=cortex-m0plus`: they compile to inline code with no libgcc
helper and need **no change**.

### 4.1 Integers (`TY_I`, `MMINTEGER` = `long long`)

| Site | Operator | Fuzix output | CSUB output |
|---|---|---|---|
| `e_add` 1530 | `+` `-` | `((a) + (b))` | unchanged — inline |
| `e_unary` 1587 | unary `-` | `(-(a))` | unchanged — inline |
| `e_compare` 1490 | `<` `>` `=` … | `((a) < (b))` | unchanged — inline |
| `e_logical` 1476 | `AND` `OR` `XOR` `NOT` | `((a) & (b))` … | unchanged — inline |
| `e_mul` 1547 | `*` | `((a) * (b))` | **`LMul(a, b)`** |
| `e_mul` 1547 | `\` | `mm_idiv(a, b)` | **`LDiv(a, b)`** (retarget the call) |
| `e_mul` 1547 | `MOD` | `mm_mod(a, b)` | **`LMod(a, b)`** (retarget the call) |
| `e_shift` 1522 | `<<` | `((a) << (b))` | **`LShl(a, b)`** |
| `e_shift` 1522 | `>>` | `((a) >> (b))` | **`LLsr(a, b)`** — MMBasic's `>>` is logical |

Shifts by a literal count could stay inline as an optimisation; not worth it in
phase 1.

### 4.2 Floats (`TY_F`, `MMFLOAT` = `double`)

| Site | Operator | CSUB output |
|---|---|---|
| `e_add` | `+` `-` | **`FAdd(a,b)` / `FSub(a,b)`** |
| `e_mul` | `*` | **`FMul(a,b)`** |
| `e_mul` | `/` (and `mm_fdiv`) | **`FDiv(a,b)`** |
| `e_unary` | unary `-` | unchanged — inline (a sign-bit flip, measured) |
| `e_compare` | `<` `>` `=` … | **`(FCmp(a,b) < 0)`** etc. — `__aeabi_dcmplt` otherwise |
| `e_pow` 1597 | `^` | **`Power(a,b)`** |
| `as_flt` 1318 | `(MMFLOAT)(int)` cast | **`IntToFloat(a)`** |
| `as_int` 1308 | `mm_toint(...)` | **`FloatToInt(a)`** — note it rounds, matching MMBasic |

Float literals are fine: `armcfgen` places `.rodata` inside `.text`, hardware
verified.

### 4.3 Mechanism

Do **not** scatter `if self.target == 'csub'` through the expression methods.
Add one helper pair on `Conv` and route the sites above through it:

```python
def ibin(self, op, a, b):      # integer binary op -> code string
def fbin(self, op, a, b):      # float binary op
```

For the Fuzix target they return today's strings; for the CSUB target they
return the vector calls. That is two new methods plus about a dozen one-line
call-site edits, and it keeps the operator policy in one readable place.

### 4.4 Other target-conditional points

- **`checks_on()`** — ON ERROR does not exist inside a CSUB. Force it false for
  the CSUB target, which removes the `store()` temporary-and-commit dance
  (line 1185) and the `mm_fdiv` divisor check. Simpler *and* faster.
- **`emit_local_decl` 9917 / `emit_dim_alloc` 4671** — a `LOCAL` array must not
  become a C local. Phase 1: reject with a diagnostic. Phase 2: emit
  `GetTempMemory`.
- **`global_decls` 10033** — must emit nothing; there is no `.bss`. Any global
  reaching this point in the CSUB target is a bug in the scope check (§5.2).
- **`open_routine` 9866 / `close_routine` 10004** — emit `static` on the inner
  function and append the entry shim.
- **`write` 10210** — the CSUB target writes one translation unit containing the
  entry function first, so `armcfgen -e` finds it 4-byte aligned.

---

## 5. Phase 1 — the deliverable

**Goal:** `mmb2csub.py prog.bas --sub Mix` rewrites `prog.bas` in place, replacing
`SUB Mix` with an adapter SUB plus a `CSUB MixK … End CSUB` block, and the
program runs unchanged but faster.

### 5.1 Scope

In: `INTEGER` and `FLOAT` scalars and arrays as parameters; `LOCAL` scalars;
`IF`/`ELSEIF`/`ELSE`, `FOR`/`NEXT`, `DO`/`LOOP`, `WHILE`/`WEND`,
`SELECT CASE`, `EXIT`; all arithmetic, comparison and logical operators;
`CONST`; the maths functions with CallTable slots (`SIN` `COS` `SQR` `ATAN2`
`POW` and the single-precision set); calls to other SUBs being transpiled into
the same blob.

Out, each with a diagnostic naming the reason and the workaround: strings;
`LOCAL` arrays; globals; `PRINT` and all I/O; graphics; `ON ERROR`; `GOTO`
out of the routine; `RND`; maths functions with no slot (`TAN` `LOG` `EXP`
`ASIN` `ACOS`).

### 5.2 The scope check

A new pre-pass, run before emission, walking the chosen SUB and refusing
anything out of scope. This is the most important piece of phase 1 to get right,
because **a silently wrong CSUB is a hard fault with no line number**. The rule
is: if the checker is not certain a construct is supported, it refuses. Every
refusal prints the source line and what to do instead.

The check is also what makes the tool safe to point at an arbitrary program: it
either produces a CSUB that is faithful, or it produces nothing.

### 5.3 Work items

| # | Item | Where |
|---|---|---|
| 1 | `self.target` on `Conv`, defaulted to `'fuzix'` | mmb2c.py `__init__` |
| 2 | `ibin`/`fbin` helpers; route the ~12 operator sites | mmb2c.py §4.1–4.2 |
| 3 | `checks_on()` false for the CSUB target | mmb2c.py 1029 |
| 4 | `static` inner function + entry shim + file ordering | mmb2c.py 9866/10004/10210 |
| 5 | `mmcsub.h` — the freestanding runtime subset | new |
| 6 | Scope checker with per-construct diagnostics | mmb2csub.py |
| 7 | Adapter-SUB generator | mmb2csub.py |
| 8 | `.bas` splicer: find the SUB, replace it, keep the rest byte-identical | mmb2csub.py |
| 9 | armcfgen invocation (`-O s`, `-I`, entry first) | mmb2csub.py |
| 10 | Differential test harness (§7) | new |

### 5.4 `mmcsub.h`

The CSUB target's runtime. Phase 1 needs very little, because §4 turns most of
the work into CallTable calls:

```c
typedef long long MMINTEGER;
typedef double    MMFLOAT;
/* the CallTable wrappers come from PicoCFunctions.h */
/* MMBasic semantics that are not single vectors: */
static MMINTEGER mm_pow_i(MMINTEGER b, MMINTEGER e);   /* integer ^ */
static MMINTEGER mm_abs_i(MMINTEGER v);
static MMFLOAT   mm_abs_f(MMFLOAT v);
static MMINTEGER mm_sgn(...);  /* etc. */
```

Everything in it is `static` so the linker drops what a given kernel does not
use. Estimated size in phase 1: under 200 lines.

---

## 6. Later phases

**Phase 2 — locals, arrays, multiple SUBs.**
`LOCAL` arrays onto the MMBasic heap via `GetTempMemory`/`FreeMemory`; array
bounds threaded properly; several SUBs transpiled into one blob (armcfgen's
`merge` mode already resolves intra-blob `bl`, and mmb2c already emits
inter-routine calls through `emit_call`/`pass_arg`, so this is mostly driver
work); MMBasic strings, which need a length-prefixed string runtime in
`mmcsub.h` using `GetTempMemory` — the largest single item in this phase.

**Phase 3 — globals and completeness.**
Globals resolved at runtime by walking `g_slotrec`, the record of each variable
number (CallTable `0x60`/`0x64`, `struct s_vartbl` published in
`PicoCFunctions.h`), with the record's pointer cached once per CSUB call: a
global's record stays where it is while the variable exists. A libm subset compiled into the blob, or more CallTable slots for
`TAN`/`LOG`/`EXP`/`ASIN`/`ACOS` — the slot route is cheaper in blob space and
costs one table word each. `PRINT` via `MMPrintString`/`FloatToStr`/`IntToStr`.

**Phase 4 — the fallback.**
Anything still unsupported becomes `CallExecuteProgram("…")`, so the tool never
refuses outright. Correct, no speed benefit, and limited to statements that only
touch globals — worth having as a last resort, not as a goal.

---

## 7. Testing

Three gates, all automatic. Given how a wrong CSUB fails — a hard fault with no
line number — this is not the part to economise on.

**7.1 Host differential (fast, every commit).** Compile the generated C as a
host DLL with the `EXPORT`/`_MSC_VER` trick already used by
`Bas/exile_tools/csub/exile.c`, stub the CallTable vectors with their native C
equivalents, and run it against the same inputs as the MMBasic original executed
by mmb2c's existing Python-side interpreter. Compare every output argument after
every call. This catches semantic drift — the `>>`-is-logical class of bug —
in seconds, without hardware.

**7.2 Board differential (per milestone).** The generated program runs on a
board with both paths present: the original SUB renamed, the CSUB alongside, and
a driver calling both over a table of inputs and comparing. This is what
`Bas/lltest.bas` does by hand for the nine new slots, and it generalises.

**7.3 Corpus.** mmb2c already ships `.bas`/`.expected` pairs under
`mmb2c/tests/`. Phase 1 adds a `csub/` corpus in the same shape, so
`fcctests.sh`-style gating works unchanged.

A fourth, cheap check worth wiring in from day one: assert that the armcfgen
link produced **no** undefined symbol. An `undefined reference to __aeabi_lmul`
is the tool telling you §4.1 has a gap, and it should fail the build loudly
rather than be worked around.

---

## 8. Risks

**The scope checker is the whole safety story.** If it lets something through
that the emitter mistranslates, the failure mode is a hard fault with no line
number. Mitigation: refuse by default, and make 7.1 mandatory in the driver —
the tool should not emit a CSUB it has not diff-tested on the host.

**Program memory, not speed, is the ceiling.** A CSUB costs its hex text as well
as its binary, about 3.5x the code size; a 44 KB kernel already overflowed a
PC3's 144 KB. A tool that makes CSUBs easy will hit this sooner than a human
writing them one at a time. Phase 1 emits `-O s` by default and reports the blob
size; phase 2 should know about `LIBRARY SAVE`, which stores the binary alone.

**Every-call overhead.** `CallCFunction` re-parses the argument list and the type
list on every call, and the adapter SUB adds an interpreter call on top. Both are
irrelevant for a kernel called once a frame and dominant for one called in an
inner loop. The driver should report the estimated per-call overhead so the
choice of what to transpile is informed.

**mmb2c divergence.** Changes land in a shared file. Keeping them additive and
behind `self.target` keeps the Fuzix tests green, and those tests are the
regression gate.

---

## 9. Decisions needed before coding

1. **Where does the tool live** — in the FUZIX tree beside mmb2c, or vendored
   into the PicoMite tree? It needs the PicoMite firmware headers and armcfgen,
   which live here; mmb2c lives there.
2. **Does mmb2c stay MicroPython-clean?** It avoids `re`, f-strings, `typing`
   and `enum` so it can run on the Pico. A CSUB generator must run the ARM
   toolchain, so it is host-only regardless — but changes to the shared file
   should probably keep the restriction, and the new files need not.
3. **Adapter SUB by default, or CSUB-only by default?** Recommendation: adapter
   by default, because it makes the tool drop-in, with `--no-adapter` for hot
   paths.
4. **Which real SUB is the first target?** Phase 1 should be judged on something
   that matters. The Exile viewport/scroll model or a Thrust inner loop would
   both exercise integer arithmetic and array parameters without needing strings.
