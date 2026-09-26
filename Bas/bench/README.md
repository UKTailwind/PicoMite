# Interpreter benchmark harness (Phase 0)

The measuring kit for `docs/Interpreter_Performance_Plan.html`. Everything
here runs from a PC against a board on a serial port; nothing is needed on
the board beyond MMBasic (or Peter's MicroPython PC3 port for the Python
twins).

## The anchor: `Bas/solar_eclipse.bas`

| file | what it is |
|---|---|
| `../solar_eclipse.bas` | the interpreted anchor, unchanged |
| `se_csub.bas` | the same program with `sefunc` commented out for the CSUB, plus `Dim decl, rasc, rb, rlsun, rmm` (the wrapper reaches those globals by `VARADDR`, so they must exist) |
| `sefunc.bas` | the CSUB library, from `mmb2csub.py solar_eclipse.bas sefunc --library sefunc.bas --lean` |

Reference inputs, in order: `12,1,2000` / `39,40,36` / `-104,57,12` /
`1644` / `30`. Golden output: JD 2451904.14541560, .19690359 and
.25420764. The drivers answer the inputs and check those digits.

## Drivers

| script | use |
|---|---|
| `se_bench.py PORT put LOCAL DEV ...` | XMODEM files onto the board |
| `se_bench.py PORT run DEV RUNS LABEL [--lib DEV] [--pre "LINE"]` | LOAD once, RUN n times, one JSON line per run; stops at the prompt, on an error or on an unexpected INPUT; answers PRESS ANY KEY |
| `runbas.py PORT DEV OUT` | LOAD, RUN, save everything printed (the micro suite) |
| `mpy_bench.py PORT put LOCAL REMOTE` / `run MODULE RUNS LABEL` | the MicroPython equivalents (relative paths on the board) |
| `flash.py PORT UF2` | `UPDATE FIRMWARE`, then copy the image to the boot drive |
| `pcs_report.py OUT ELF [--src FILE] [--top N]` | turn the `[PCS]` lines of a sampled run into a split by function and bucket |

Run the CSUB twin with `--lib A:/sefunc.bas`, and `LIBRARY DELETE` before
running the interpreted program again: the library survives a same-variant
flash, and its `sefunc` clashes with the program's own.

## The PC sampler

`OPTION PROFILING ON, SAMPLE [, entries]` samples the interrupted PC and
the current program line every 97 us on a spare hardware alarm, and END
prints them after the `[PERF]` report. Put it after `Timer =0` to sample
only the timed region. Use 8192 entries on an RP2350, 4096 on an RP2040.
`pcs_report.py` needs the ELF of the exact build that was flashed; rebuild
it from the commit if it is gone. The reported program line is one less
than the file line.

## Goldens

`goldens.py PORT OUTDIR` runs the mmb2c test corpus and keeps what the
firmware prints; `goldens/` holds the reference for the RP2040 VGA and
PICORP2350, taken on the builds carrying the five Phase 0 bug fixes
(main c994e92..b4ccc22). A later build must reproduce them, except where
a change is meant to alter a program's output. `--recompare DIR`
re-scores saved runs without a board.

Some corpus programs depend on files others leave on A:/g - `rtest`
reads `w1.bmp`, which `wtest` (later in the order) writes - so the first
run on a clean drive and later runs differ there. Compare like with like.

## Micro suite

`bench_micro.bas` times one statement per loop against an empty loop and
prints `BENCH name microseconds cycles`. `bench_micro.py` is its
MicroPython twin, run at module level (like BASIC) and inside a `def`.

## Measuring a Route A step

Every step is judged on both boards against a baseline taken on the build
before it:

- **`stdset.py PORT pc3|vga OUT.json [--skip-put]`** runs the standard set: the anchor, both micro suites, `bench_subs0/200`, the heap and graphics suites, knivd, julia, the 3D frame and, on the PC3, the three games. It resets the board before and after the set, because Elite and Prince of Pico set OPTION LOCAL VARIABLES 128 and the graphics programs change MODE, both of which last until a reset. Every CHECK, GAMECHECK and golden line is recorded.
- **`stdcmp.py BASE.json NEW.json`** lists every figure that moved by more than 1%, every CHECK that changed, and the median change per test.
- **`goldens.py`**, then **`goldcmp.py REFDIR NEWDIR`**, compares the corpus outputs with a reference. Run it on a freshly reset board.

**Judging the numbers.** Any code change moves flash-resident functions by a few bytes. That reshuffles QMI cache conflicts between code and the program text, which is also read through the cache. Loops that scan program text then move by several percent whatever the change does: FOR/DO entry over a long body, and a float literal inside a SUB. The RP2040, which runs most of the interpreter from flash, shows this more. So judge a step on the workloads and on its own targeted tests, and treat a single micro outlier as placement unless a profile puts the extra time in the changed code.

## Phase 1 (`phase1/`, `results/phase1/`)

The characterisation behind `docs/Interpreter_Phase1_Results.html`:

- **Micro tests:** `gen_micro2.py` writes `bench_micro2.bas` and `bench_subs0`/`bench_subs200.bas`. They cover calls, LOCALs, lookups at depth, SETTICK, REM/DATA, FOR/DO/SELECT scans and labels.
- **Heap test:** `bench_heap.bas` times allocation with the heap empty, top-filled and bottom-filled.
- **Graphics pairs:** `bench_gfx.bas` with its MicroPython twin `bench_gfx.py`, run on the PC2 with `runmpy.py`. `bppline.py` compares framebuf lines at 4 and 8 bits a pixel.
- **Call profiles:** `calls_pcs_*.bas`. Use 4096 sampler entries on the RP2040.
- **Macros (`macros/`):** fixed-work knivd, the julia interpreted/CSUB pair and a 3D frame, run with `runmacro.py`. `MANIFEST.md` has the order.
- **Games (`games/`):** `make_games.py` builds headless, fixed-length copies of the Exile kernel, Elite and Prince of Pico from the repository, and `gamebench.py` runs them. No game data is kept here; the Prince of Pico data comes from a local conversion.
- **Sampler size on the PC3:** use `SAMPLE, 16384`. The tables then go to PSRAM and do not distort allocation.

## Symbols and interpreter regression tests

Each drives one board on its serial port and prints PASS or FAIL.

| script | checks |
|---|---|
| `symab.py`, `symtime.py`, `rtlist.py`, `symsizes.py` | OPTION SYMBOLS A/Bs on the same firmware: output, timing, LIST round trip, saved size |
| `symchain.py PORT` | CHAIN and SAVE/LOAD CONTEXT keep each name bound to its own variable (fixed in 8d3ee25) |
| `cmtloop.py PORT` | FOR, DO and GOSUB lines ending in `: ' comment` (fixed in 8740953) |
| `inttests/inttests.py` | the S1c interrupt sources |

## Gate G3: the Route B prototype (`g3/`, `results/g3/`)

- **`vm.c`** is a 40-opcode wordcode VM built as a CSUB; **`native.c`** has the same kernels in C, as the floor.
- **Building the CSUBs:**
  - M0+: `armcfgen.py vm.c --compile -n VMRUN -e main -O 2 -I <repo> -o vm_m0.txt`.
  - M33: the same command with `armcfgen33.py`, which is armcfgen with `-mcpu=cortex-m33`.
  - CSUB C code cannot use `/` or `%` on integers (there is no libgcc).
- **`kernels.vma`** holds the hand-compiled wordcode: an integer loop, insertion sort, the Julia loop and findleap.
- **`make_g3.py`** assembles it and writes `g3_m0.bas` / `g3_m33.bas`.
- **`g3run.py PORT m0|m33`** uploads the harness and runs it. The harness times each kernel three ways and checks that the results agree.
- **`census.py`** gives the compilable share of the busiest lines and SUBs from saved profiles. The mapping from PCSLINE to file line is set per workload.
- **`stream_est.py`** (with `measure.py`) estimates stream size against the tokenised image.
- **Results** (2026-09-25): the VM is 8.8-22x faster than the interpreter on the PC3 and the RP2040 VGA; the census is 84-100%; the stream is 0.04-0.65x of the image. The Route B design is `docs/Interpreter_RouteB_Design.html`.

## Results so far (`results/`)

Anchor at 378 MHz on the PC3 unless stated, three runs each, spread under
0.1% (MicroPython about 3%):

| build | interpreted | NOCHECK | CSUB |
|---|---|---|---|
| HDMIWEB, WiFi live | 14.672 s | 12.804 s | 2.515 s |
| HDMIWEB, WiFi off | 14.293 s | | |
| HDMIUSB | 13.227 s | 12.731 s | 2.514 s |
| PICORP2350 | 12.122 s | 11.826 s | 1.534 s |
| RP2040 VGA at 315 MHz | 32.04 s | | 5.726 s |
| MicroPython (PC2, double precision) | 9.07 s | | |

Sampled split of the anchor's time on PICORP2350: evaluator 27.6%, name
lookup 26.1% (findvar 25.1%), ExecuteProgram 12.4%, text walking 11.7%,
housekeeping about 5%, the maths itself about 5%.
