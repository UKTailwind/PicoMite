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

## Micro suite

`bench_micro.bas` times one statement per loop against an empty loop and
prints `BENCH name microseconds cycles`. `bench_micro.py` is its
MicroPython twin, run at module level (like BASIC) and inside a `def`.

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
