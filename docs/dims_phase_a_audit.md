# Array dimensions — Phase A audit

Phase A of allowing single-element array dimensions in MMBasic. It changes **no
behaviour at all**: it renames the variable-table dimension member so the compiler
is forced to enumerate every site that reads it, routes all of those sites through
an accessor layer, and records what each one actually means. Phase B is a separate,
much smaller change that flips one switch in that layer.

## Why the restriction exists

`dimtbl[i]` holds the **declared upper bound** of dimension `i`. The element count of
a dimension is `dimtbl[i] + 1 - g_OptionBase`. The value `0` is overloaded four ways:

1. **Scalar marker** — `dimtbl[0] == 0` means "not an array".
2. **Rank terminator** — the number of dimensions is found by scanning until the
   first `0`; there is no separate rank field.
3. **Heap-ownership classifier** — `ERASE`/`ClearVars` free `val.s` only when
   `dimtbl[0] != 0` (or the type is string). Misclassify and you leak or double-free.
4. **Gate for inline string storage** — a scalar string short enough to fit is stored
   *inside* `dimtbl[1..]` with `val.s = &dimtbl[1]` (`MMBasic.c` findvar, mirrored in
   `cmd_const`, accounted for in `Memory.c`). Only `dimtbl[0] == 0` keeps that safe.

`-1` is already reserved for an unbound empty-array parameter (`SUB s(a())`).

An upper bound of `0` therefore has no encoding. Under `OPTION BASE 1` a single-element
dimension is `DIM a(1)` — upper bound 1, perfectly representable. Under `OPTION BASE 0`
it would be `DIM a(0)` — upper bound 0, which collides with all four meanings above.
The restriction is enforced in exactly one place, `MMBasic.c` findvar:

```c
if (dim[i] <= g_OptionBase)
    error("Dimensions");
```

so today every dimension has at least two elements, and everything downstream relies
on it.

## The accessor layer (core/MMBasic.h)

The struct member is now `dimtbl` in both `s_vartbl` and `s_structmember`. Nothing
outside the header touches it directly.

| Accessor | Meaning | Phase A expansion |
|---|---|---|
| `RAW_DIM(v,i)` | the slot exactly as stored | `(v).dimtbl[i]` |
| `DIM_TABLE(v)` | the whole table, for copies | `(v).dimtbl` |
| `DimUpper(d)` | effective upper bound | `(d)` |
| `DimElements(d)` | element count | `DimUpper(d) + 1 - g_OptionBase` |
| `DimIsScalar(d)` | `dimtbl[0]`: not an array | `(d) == 0` |
| `DimIsEnd(d)` | `dimtbl[i]`: no further dimension | `(d) == 0` |
| `DimIsAllocated(d)` | `dimtbl[0]`: array or array parameter | `(d) != 0` |
| `DimIsRealArray(d)` | a real dimension is present | `(d) > 0` |
| `DimIsEmptyParam(d)` | unbound empty-array parameter | `(d) == -1` |

`DimIsScalar` and `DimIsEnd` expand identically on purpose — they are different
*questions* about the same stored value, and separating them is the point of the audit.

`DIM_DECODE_ENABLED` is `0`. While it is 0 every accessor expands to the expression it
replaced and `DIM_ONE` is never stored, so the interpreter is unchanged.

## Verification

Phase A is inert by construction, and that was checked end to end rather than argued.
Nine variants were built from the pristine tree and again with the refactor applied,
in the same build directories, and the firmware images compared:

```
IDENTICAL  PICORP2350   WEBRP2350   HDMIUSB   HDMIWEB   PICOBTHRP2350
IDENTICAL  PICO         PICOMIN     WEB       VGAUSB
9/9 variants byte-identical
```

That covers both chips (`int dimtbl[5]` on RP2350, `short dimtbl[6]` on RP2040), both
`STRUCTENABLED` states (PICOMIN is the only variant without it), and every source file
touched. A byte-identical `.uf2` means the compiler emitted the same program, so no
behavioural drift is possible. `__LINE__`/`__FILE__` appear only in `third_party_mod`
files, none of which were edited, so nothing line-number dependent is baked into the
image and the comparison is a sound oracle.

## What Phase B has to do

1. Set `DIM_DECODE_ENABLED` to `1`.
2. Relax the two creation-path guards from `<=` to `<`: findvar in `core/MMBasic.c`,
   and the struct TYPE member-array check in `core/Commands.c`.
3. Store the escape on creation: an upper bound of `0` is written as `DIM_ONE`.
4. Have the trace JIT (`core/MMtrace.c`) refuse to compile any array containing
   `DIM_ONE` and deopt to the interpreter; it already bails safely when the observed
   rank disagrees.

Sites classified `element-count`, `bounds-check` and `is-array-test` then start
decoding automatically, because they go through `DimElements`/`DimUpper`/
`DimIsRealArray`. The classes that need individual review are listed below.

## The audit's blind spot — read this before Phase B

**The compiler could only flag accesses to the struct member.** Several consumers copy
the dimension table into a *local* `dims[MAXDIM]` array first (via `parse*array()` or
`memcpy`) and then do their arithmetic on the copy. Those sites kept compiling and were
**not** enumerated by this pass. They are the same code shapes that are already fragile:

- `core/MATHS.c` — `M_INVERSE`, `M_TRANSPOSE` and the `parse*array` shape checks use
  `dims[i] - g_OptionBase` (count minus one) on the local copy.
- `core/Commands.c` — `MATH INSERT` / `MATH SLICE` (around the `dimcount` loops) treat
  `dims[i] - g_OptionBase > 0` as "this dimension exists", which erases a single-element
  dimension entirely. This is the failure reproduced on hardware: a fabricated 3×1 array
  makes `MATH SLICE` fail with "Argument count".
- `core/MMBasic.c` — `ClearVars` zeroes a whole slot with
  `memset(&g_vartbl[i], 0, sizeof(struct s_vartbl))`, which writes the reserved
  "scalar" value without going through any accessor. Correct today; worth re-checking.

Note this failure class is **base-symmetric**: it breaks a single-element dimension
under `OPTION BASE 1` (natural encoding, no sentinel involved) just as it does under
base 0. Natural-1 is therefore *not* a safety net for a missed site — completeness of
the audit is the only real protection.

## Site inventory

214 sites across 13 files, classified by what the site actually asks of the slot.

| Class | Count | Phase B action |
|---|---|---|
| `is-array-test` | 58 | automatic via `DimIsRealArray` / `DimIsAllocated` |
| `rank-scan` | 44 | automatic — terminates on raw `0`, which `DIM_ONE` is not |
| `element-count` | 41 | automatic via `DimElements` |
| `verbatim-copy` | 17 | automatic — the encoded value is copied unchanged |
| `scalar-test` | 16 | automatic via `DimIsScalar` |
| `bounds-check` | 14 | automatic via `DimUpper` |
| `assignment` | 10 | **review** — anything that *writes* a bound must apply the escape |
| `inline-string-storage` | 5 | **review** — must stay raw; see findvar's save/restore |
| `other` | 3 | **review** |
| `fragile-minus-base` | 3 | **review** — `- g_OptionBase` with no `+ 1` |
| `empty-param-test` | 2 | automatic via `DimIsEmptyParam` |
| `raw-access` | 1 | **review** |

The three `fragile-minus-base` sites, which compute a count as upper bound minus base
and so are one short of the element count:

- `net/WiFi.c:552` — `(dimtbl[0] - g_OptionBase) * 8`, WEB SCAN buffer size; element 0
  is deliberately reserved for the length word.
- `misc/Custom.c:3074` — `(dimtbl[0] - g_OptionBase) * 8`, JSON$ LongString payload cap.
- `net/MMtcpserver.c:871` — inside commented-out dead code; no runtime effect.

Per-file counts: `Commands.c` 76, `MMBasic.c` 44, `MMtrace.c` 30, `MATHS.c` 24,
`Custom.c` 9, `DrawInternal.h` 9, `Memory.c` 5, `FileIO.c` 5, `MMtcpserver.c` 4,
`WiFi.c` 3, `Audio.c` 2, `MMTCPclient.c` 2, `Functions.c` 1.

## Observations recorded during the audit

These were found while classifying and were deliberately **not** changed — Phase A
alters no behaviour. Each should be judged on its own merits.

- **`cmd_redim` and the empty-array parameter** (`Commands.c` ~7680 vs ~7683): the
  "not an array" guard uses `!dimtbl[0]`, which an unbound parameter (`-1`) passes,
  while the dimension capture just below uses `> 0`, which `-1` fails. Such a variable
  slips past the error and is REDIMmed with its old memory and dimensions never
  captured.
- **STRUCT commands and the empty-array parameter** (`STRUCT SORT`/`EXTRACT`/`INSERT`/
  `FIND`): these test `dimtbl[0] == 0` for "not an array", so a `-1` parameter passes
  and then flows into an element count of `-1 + 1 - g_OptionBase`, i.e. 0 elements
  under base 0 and −1 under base 1.
- **Inconsistent "is it an array" predicate**: the LIST printers and the DIM/STATIC
  paths ask `> 0`, while the STRUCT and CONST paths ask `!= 0`. Both are identical for
  real arrays and differ only for the `-1` parameter case. The classification records
  which is which.
- **`cmd_read` cardinality** (`Commands.c` ~7819 / ~7840): the loop runs all `MAXDIM`
  slots and relies on arithmetic rather than a rank scan to terminate. An absent
  dimension yields 1 under base 0 (multiplied in harmlessly) and 0 under base 1
  (skipped by an `if`). Two different mechanisms happen to give the right answer.
- **BYVAL array trap** (`MMBasic.c` ~2277) tests `> 0`, so a forwarded unbound
  parameter is not trapped.
- **`cmd_restore` comment** (`Commands.c` ~8142/~8151) reads "Not an array" but the
  guarded error fires when the variable *is* an array.
- **CONST inline-string capacity** (`Commands.c` ~8763) is `(MAXDIM - 1) * sizeof(slot)`,
  coupling that limit to both `MAXDIM` and the slot type, which differs per chip.
- **`fun_bound`** normalises the `-1` marker on the result (`if (iret == -1) iret = 0`)
  rather than at the read. Still correct once decoding is enabled, because `DimUpper`
  is applied when the slot is read.
- **MMtrace replay strides** (~2586/2608/2992/3014) write `hi0 + 1 - g_OptionBase`
  longhand on a local, not on the struct member. Correct once decoding is on *only*
  because `DimUpper` is applied where `hi0` is loaded.

A machine-readable table of all 214 sites (file, line, class, before, after, note)
accompanies this document as `dims_phase_a_audit_sites.csv`.
