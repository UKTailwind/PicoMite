# Firmware findings from the Elite port

## 1. `DRAW3D(...)` function family always fails: "Internal fault 2(sorry)"

**Status: diagnosed, NOT fixed (the port does not need it).**

Every one of the eight `DRAW3D()` functions fails on PicoMiteHDMIWEB V6.03.02b3, whatever the
caller does with the result:

```
fv = DRAW3D(XMIN 1)        -> Internal fault 2(sorry)
iv = DRAW3D(XMIN 1)        -> Internal fault 2(sorry)
PRINT DRAW3D(XMIN 1)       -> Internal fault 2(sorry)
fv = DRAW3D(DISTANCE 1)    -> Internal fault 2(sorry)
fv = DRAW3D(X 1)           -> Internal fault 2(sorry)
```
Repro: create any object, `Draw3D SHOW` it, then call any `DRAW3D()` function.
The command set (`CREATE`/`ROTATE`/`SHOW`/`WRITE`/`DIAGNOSE`/`CLOSE`) is unaffected.

**Cause.** Two mismatches around the function's return value.

* `AllCommands.h:853` registers the token as `T_FUN | T_INT`, so `getvalue()` in `core/MMBasic.c`
  sets `tmp = targ = T_INT` before the call and afterwards reads the INTEGER return slot `iret`.
* `fun_3D()` in `graphics/Draw3D.c:1285-1346` writes its result to the FLOAT slot `fret` in all
  eight branches and **never assigns `targ`** - every other function in the firmware sets it.
  `targ` is therefore left holding whatever the argument evaluation inside `getint()` put there,
  and the guard at `core/MMBasic.c:3218`, `if ((tmp & targ) == 0) error("Internal fault 2(sorry)")`,
  fires.

Even with the type check satisfied the value would be wrong, because the caller would read `iret`
while the function wrote `fret`.

**Proposed fix** (two lines, not applied):
1. `AllCommands.h:853`: declare the function as returning a float - `T_FUN | T_NBR` instead of
   `T_FUN | T_INT`. All eight members are float-valued (`xmin`..`ymax` are shorts widened to float;
   `x`, `y`, `z`, `distance` are genuinely fractional).
2. `graphics/Draw3D.c`, `fun_3D()`: assign `targ = T_NBR;` on every branch, as the rest of the
   firmware does - simplest as a single assignment before the `if` chain returns.

Worth a regression test, because nothing in the test suite calls these.

## 2. `Draw3D CLOSE ALL` skipped the last object

Fixed 2026-09-09 in `graphics/Draw3D.c` (loop was `0..MAX3D-1`, now `1..MAX3D`). See the plan.

## 3. `LIGHT`, `SET FLAGS` and `RESET` dereferenced a missing object

Fixed 2026-09-09: they now raise the same "object does not exist" error as `SHOW`, and `SET FLAGS`
rejects a face index outside `0..nf-1`.
