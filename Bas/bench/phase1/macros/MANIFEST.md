# Phase 1 macros: manifest

Placeholders used below:

- `M` = `C:/Users/peter/AppData/Local/Temp/claude/d--Dropbox-PicoMite-PicoMite/689993b7-d124-4d1c-ae8c-e849096cd007/scratchpad/p1/macros`
- `R` = `.../scratchpad/p1/results`
- `B` = `D:/Dropbox/PicoMite/PicoMite/Bas/bench`
- `PC3` = the PC3's HDMIUSB console port (COM16 in Phase 0)
- `VGA` = the RP2040 PicoMiteVGA's port (COM7)

The PC2 MicroPython board gets nothing from this set. None of these three is a
graphics pair.

NOTES.md has the design, the source facts behind it and the estimates. Its
last section, "Review", lists what the review changed and what still needs a
board to settle.

## Boards and settings

| board | build | settings (check with OPTION LIST) |
|---|---|---|
| PC3 | HDMIUSB | `OPTION RESOLUTION 640x480, 378000`. On HDMI builds the resolution option sets the clock, and a bare `640x480` gives 252 MHz (`MM_Misc.c:6796-6815`). MODE 2 is then 320x240. Console on the UART. No `OPTION SCREEN KEYBOARD`: it reserves screen rows and CLS redraws the keyboard into the picture. |
| RP2040 | PicoMiteVGA | `OPTION CPUSPEED 315000`, 640x480 VGA |

On both boards:

- Leave `GUI CURSOR` off. The two Julia programs and `gfx_frames.bas` read
  the screen back for their checksum.
- The first line of every run, `BENCHSTART <device> <version> <mhz> MHz`,
  must show 378 on the PC3 and 315 on the RP2040. If it does not, the
  option is wrong; stop.

julia and gfx set `MODE 2` before timing and put back the prompt's mode
after the read-back (MODE survives END, so without that the next program
would inherit MODE 2). knivd sets `MODE 1`, the default mode Phase 0 ran
in. `OPTION LIST` should show no `DEFAULT MODE` line (that means MODE 1).

## Files to upload (XMODEM to A:/, the same set on both boards)

| file | size | purpose |
|---|---|---|
| `knivd_fixed.bas` | 3.8 KB | timing run |
| `knivd_fixed_prof.bas` | 3.8 KB | sampled run (PROFILE% = 1) |
| `julia_interp.bas` | 4.6 KB | timing run, interpreted half |
| `julia_interp_prof.bas` | 4.6 KB | sampled run |
| `julia_csub.bas` | 4.7 KB | timing run, CSUB half (needs `julialib.bas` in the library) |
| `julia_csub_prof.bas` | 4.7 KB | sampled run (optional: nearly all samples land in the blob) |
| `julialib.bas` | 2.0 KB | CSUB plotjulia, for `LIBRARY LOAD`; one blob for both chips |
| `gfx_frames.bas` | 19.0 KB | timing run, FBUF% = 1 |
| `gfx_frames_prof.bas` | 19.0 KB | sampled run, FBUF% = 1 (8192 entries on the PC3; only 1024 on the RP2040, where half the samples drop) |
| `gfx_frames_nofb_prof.bas` | 19.0 KB | sampled run, FBUF% = 0, for the RP2040 shares (4096 entries; same CHECK). Not uploaded or run in the first runs |

```
python B/se_bench.py PORT put M/knivd_fixed.bas A:/knivd_fixed.bas M/knivd_fixed_prof.bas A:/knivd_fixed_prof.bas M/julia_interp.bas A:/julia_interp.bas M/julia_interp_prof.bas A:/julia_interp_prof.bas M/julia_csub.bas A:/julia_csub.bas M/julia_csub_prof.bas A:/julia_csub_prof.bas M/julialib.bas A:/julialib.bas M/gfx_frames.bas A:/gfx_frames.bas M/gfx_frames_prof.bas A:/gfx_frames_prof.bas M/gfx_frames_nofb_prof.bas A:/gfx_frames_nofb_prof.bas
```

## Run order on each board

Run each timing program 3 times. Take the median, and run 5 times if the
spread is over 2% (gate G0). Profile once.

`M/runmacro.py PORT DEVPATH OUTFILE [--lib DEVPATH] [--libdelete]` loads,
runs, saves the output, and presses a key at the profiling report's
"PRESS ANY KEY". Use it rather than `runbas.py` for the `_prof` runs.

| # | command | typed at the prompt instead |
|---|---|---|
| 1 | `python M/runmacro.py PORT A:/knivd_fixed.bas R/<board>_knivd_1.txt --libdelete`, then `_2`, `_3` without `--libdelete` | `LIBRARY DELETE`, `LOAD "A:/knivd_fixed.bas"`, `RUN` |
| 2 | `python M/runmacro.py PORT A:/gfx_frames.bas R/<board>_gfx_1.txt` (x3) | `LOAD "A:/gfx_frames.bas"`, `RUN` |
| 3 | `python M/runmacro.py PORT A:/julia_interp.bas R/<board>_julia_interp_1.txt` (x3) | `LOAD "A:/julia_interp.bas"`, `RUN` |
| 4 | `python M/runmacro.py PORT A:/julia_csub.bas R/<board>_julia_csub_1.txt --lib A:/julialib.bas` (x3; runmacro sends `NEW` first, and the load is a no-op after the first) | `NEW`, `LIBRARY LOAD "A:/julialib.bas", O`, `LOAD "A:/julia_csub.bas"`, `RUN` |
| 5 | `python B/se_bench.py PORT cmd "LIBRARY DELETE"` | `LIBRARY DELETE` |
| 6 | `python M/runmacro.py PORT A:/knivd_fixed_prof.bas R/<board>_knivd_prof.txt` | as above, `_prof` file |
| 7 | `python M/runmacro.py PORT A:/gfx_frames_prof.bas R/<board>_gfx_prof.txt`; on the RP2040 also `gfx_frames_nofb_prof.bas`, output `R/vga_gfx_nofb_prof.txt` | |
| 8 | `python M/runmacro.py PORT A:/julia_interp_prof.bas R/<board>_julia_interp_prof.txt` (the library must be absent: step 5) | |
| 9 | optional: `python M/runmacro.py PORT A:/julia_csub_prof.bas R/<board>_julia_csub_prof.txt --lib A:/julialib.bas`, then `LIBRARY DELETE` again | |
| 10 | if a run was stopped part-way (Ctrl-C, an error), type `MODE 1` before running anything from Phase 0: only a run that reaches the end puts the mode back | |

**Library rules**

- `NEW` must come before `LIBRARY LOAD` whenever the program in memory has
  a SUB of the same name as one in the library. After `julia_interp.bas`
  it does: the first run of step 4 stopped on both boards with
  `[81] Sub plotjulia ... Error: Duplicate name` (the library was written
  anyway, so the second run worked). `runmacro.py --lib` now does this.
- `LIBRARY DELETE` must come before `julia_interp.bas` or its `_prof` copy
  whenever `julialib.bas` is in the library. Its `SUB plotjulia` clashes
  with the library's `CSUB plotjulia`.
- `LIBRARY LOAD` replaces any library already there, including Phase 0's
  sefunc. Reload that with `--lib A:/sefunc.bas` before the next anchor
  CSUB run.
- `LIBRARY LOAD` stops with "Flash Slot n already in use" if the last flash
  slot holds a saved program. Free that slot first.
- Leave the board with no library: step 5 and the end of step 9.

**Profile analysis** (needs the ELF of the build that was flashed; for the
builds at 0096659 they are `.../scratchpad/p1/uf2/HDMIUSB_0096659.elf` and
`VGA_0096659.elf`):

```
python M/pcs_gfx.py R/<board>_gfx_prof.txt <build>.elf --src M/gfx_frames_prof.bas
```

Use the same form for the other `_prof` outputs, each with its own `.bas` as
`--src`. Take the shares from these runs and the times from the timing runs.
Check the `dropped` figure on the first line: over about 5% means the
sampler table was too small for that run (expected for `gfx_frames_prof.bas`
on the RP2040; use the nofb run's shares there). Every `_prof` output must
contain a `[PCS] samples=` line; runmacro.py warns if it does not (two
RP2040 profiles of the first runs had none: reset and run again).

The first board runs used the pre-review files. `--src` must be the file
that ran, or the `[PCSLINE]` lines map to the wrong text: byte-exact copies
of those are in `M/work/prereview/` (sizes match the upload log).

## Expected output

| program | BENCH labels | CHECK line | PC3 378 MHz | RP2040 315 MHz |
|---|---|---|---|---|
| knivd_fixed | `knivd_fixed_us_per_outer_pass`, `_us_per_inner_pass` | PC3: `CHECK knivd_fixed i=3000 sumx=-29.008128472 f=-0.000041588 len=13 shash=4502` (equals the host model). RP2040: the same but `sumx=-29.008128473` (SIN/LOG/TAN differ in the last bits) | 28.2 s | 65.7 s |
| julia_interp | `julia240x180_interp_us_per_render`, `_us_per_pixel`, `_us_per_escape_iter` | `CHECK julia240x180 1188747455186` on both chips | 30.2 s + 1.4 s read-back | 70.9 s + 4 s |
| julia_csub | `julia240x180_csub_...` (per render, over 20 renders) | `CHECK julia240x180 1188747455186` on both chips (equals the interp half) | 18.7 s + 1.4 s | 66.5 s + 4 s |
| gfx_frames | `gfx_elite_frame_us_per_frame`, `_move_cls_`, `_stardust_`, `_planet_`, `_ships_`, `_dash_`, `_fbcopy_` | `CHECK gfx_elite 37966927747` on both chips (FBUF% = 1); FBUF% = 0 must agree | 25.9 s | 68.7 s |

The times and CHECK values are from the first board runs (pre-review
files; repeat runs agreed to 0.01%). The review did not change any timed
region. knivd now sets MODE 1 before timing; the first knivd runs came
before any program of this set changed the mode, and the graphics suite
that ran before them puts MODE 1 back, so they should repeat.

Each program prints `BENCHSTART <device> <version> <mhz> MHz`, its BENCH
lines (microseconds to 3 dp, then cycles), `CHECK`, `ELAPSED ... ms`, and
`BENCHEND`, then reaches END. The CHECK value is printed with `Str$`, so
there is exactly one space before it.

`_us_per_escape_iter` is the render time divided by the 470,875 escape
iterations, so it includes the per-pixel work (about 2,200 of its cycles
on the PC3).

## Sampler table sizes (the `_prof` runs)

| program | RP2040 VGA | PC3 |
|---|---|---|
| knivd, julia | 4096 (knivd: 5.5% dropped) | 8192 (0 dropped) |
| gfx, FBUF% = 1 | 1024 (49.6% dropped: do not use for shares) | 8192 (0.06% dropped) |
| gfx, FBUF% = 0 | 4096 (tight; expect drops, the frame has about 5,900 distinct PCs) | 8192 |

The dropped figures are from the first board runs. On the PC3 the second
8192-entry table lands in PSRAM for knivd and gfx (MM.INFO(HEAP) counts
PSRAM); the profiled runs cost the same with and without that (+1.5 to
+2.3% over the timing runs), and 4096 would drop samples, so 8192 stays.

## Wall-clock budget

Including LOAD, attention and report printing:

| board | timing runs | profiled runs | whole set |
|---|---|---|---|
| PC3 | about 8 min | about 3 min | about 11 min |
| RP2040 | about 17 min | about 6 min | about 23 min |

## Regenerating

These scripts write only into `M`; none of them touches the repo.

- `python M/make_julia.py` rebuilds `julia_csub.bas` and `julialib.bas`
  from `julia_interp.bas`, checks the blob against
  `mmb2csub/examples/julia.bas`, and prints the expected CHECK. It needs
  arm-none-eabi-gcc on PATH (`C:/Program Files (x86)/Arm GNU Toolchain
  arm-none-eabi/13.3 rel1/bin`) and pyelftools.
- `python M/gen_gfx.py` rewrites gfx_frames' mesh DATA from
  `Bas/3ddemo.bas`.
- `python M/make_prof.py` rewrites every `_prof` copy. Run it after any
  edit to a program.
- `python M/work/namecheck.py FILE...` and `python M/work/declcheck.py FILE`
  are the review's static checks: identifiers against the firmware's command
  and function tables, suffix collisions, and undeclared names under
  OPTION EXPLICIT.
