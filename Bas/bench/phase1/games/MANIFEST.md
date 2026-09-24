# Game workloads for H3 (and H4): manifest

Three SUB-heavy game workloads, prepared as bench copies for the **PC3 on the
HDMIUSB build at 378 MHz**. None of the originals was edited. Everything here
is built by `make_games.py` from the repository and can be rebuilt with
`python make_games.py`. The board is driven by `gamebench.py`.

All files go into one directory on the board, **`A:/gb/`**. Each program finds
its files with `MM.INFO(PATH)`, so the directory could be anywhere, but the
driver uses `A:/gb/`.

Each program has a profiling twin whose name ends in `p`. The twin differs in
one line: `Const GBPROF = 1` instead of `0`. That flag starts the PC sampler
(`OPTION PROFILING ON, SAMPLE, 16384`) straight after the `TIMER = 0` that
opens the timed region. Timing runs use the plain file. Profiled runs are used
only for shares.

The sampler size is 16,384, not the 8,192 `Bas/bench/README.md` suggests. Each
of its two tables is entries x 8 bytes. At 8,192 they are 64 KB each, under
half the 152 KB heap, so `GetMemory` takes them from SRAM
(`core/Memory.c:2146-2148`). That would leave the profiled run 128 KB less SRAM
than the timing run, and Elite's in-flight mesh allocations and Prince of
Pico's temporaries would then spill to PSRAM in the profiled run only. At
16,384 each table is 128 KB, over half the heap, so both go straight to PSRAM.
The anchor alone produced 4,099 distinct PCs in Phase 0, so the larger table
also keeps a game's PCs from being dropped.

(Review, 2026-09-24: see NOTES.md, "Review", for what was changed and why.)

## Ranking

| rank | workload | why this rank |
|---|---|---|
| **1** | **Exile player kernel, interpreted and as a CSUB** (`gbxphys` / `gbxcsub`) | The only workload with an existing interpreted/CSUB pair, so it gives an overhead fraction directly. The kernel is a routine-for-routine 6502 transcription: 56 routines, 17 of them tiny helper FUNCTIONs called inside expressions. It is headless and uses no random numbers. It checks itself against the real game's end state for all 18 scenarios. The assets are small. |
| **2** | **Elite, DEMOSCENE 1 flight and combat** (`gbelite`) | A real game frame with drawing: 260 routines, `Draw3D` meshes, `LINE`, `BOX`, `TEXT`, `CLS` and `FRAMEBUFFER COPY`. It covers H3 and gives the one drawn H4 game profile. It is self-contained (a program plus its library) and small. It is deterministic because of a fixed tick and seeded random numbers. |
| 3 | Prince of Pico, the scenario harness (`gbpop`) | The most SUB-heavy real engine (252 routines), headless, and it checks itself (274 checks, PASS/FAIL). It ranks third because it needs about 0.9 MB of upload, including game data that has to be converted locally and must never be distributed, and because it needs `LOAD ,C`. |

**Run 1 and 2 first.** Together they give H3 on a call-dense interpreted
kernel with its native floor, and H3 plus H4 on a drawn game frame. Prince of
Pico is the confirmation run: another engine, with no drawing in the timed
region.

## Before the first run (once per session)

| what | why |
|---|---|
| PC3 running the development HDMIUSB build, `MM.INFO(CPUSPEED)` = 378000000 (`OPTION RESOLUTION 640x480, 378000`) | Same clock as the Phase 0 baseline. The ELF of the flashed build is needed for `pcs_report.py`: `scratchpad/p1/uf2/HDMIUSB_0096659.elf` if the board still carries 0096659. |
| PSRAM enabled (`OPTION LIST` shows `OPTION PSRAM PIN GP47`; the PC3 configures this itself) | Elite's `LIBRARY LOAD ..., RAM`, Prince of Pico's RAM image slots 4-7, and the Exile feed array (326 KB) all need PSRAM. |
| **No flash library**: `LIBRARY DELETE`. If `FLASH LIST` still shows slot 3 in use, `FLASH ERASE 3`. | Prince of Pico's `Option Local Variables 128` fails with "Variables already declared" if a library declares anything. Exile's `Option BASE 0` fails after a library `DIM`. The Phase 0 `sefunc` library may still be there. |
| No `OPTION AUDIO` needed | The Elite copy is silent (`SOUNDON = 0`). Prince of Pico probes for audio and runs silent without it. Exile has no sound. |
| No fonts and no flash image slots | Only the built-in fonts are used. |
| `OPTION NOCHECK OFF` | The flag survives every program and can only be cleared by `OPTION NOCHECK OFF` or a reboot (`core/MM_Misc.c:5227-5238`). It cannot be read back. `gamebench.py run` sends `OPTION NOCHECK OFF` before each LOAD, or `ON` with `--nocheck` for an H5 A/B. |
| Run order does not matter | `OPTION LOCAL VARIABLES` also survives programs (until a reset). Elite's library and Prince of Pico set 128, and the Exile programs now set 256, the RP2350 default, themselves. Every program therefore runs with the same variable tables whatever ran before it. |
| After flashing a different *variant* | Upload everything again. A variant change moves the A: drive window and can format it (`docs/Boot_Robustness_Review.md`). A new build of the same variant keeps A:. |

## 1. Exile player kernel: `gbxphys` (interpreted) and `gbxcsub` (CSUB)

Sources: `Bas/exile/physics.bas` (made from `exilephys.bas` by `gen_phystest.py`)
and `Bas/exile/physcsub.bas` (made by `gen_csubtest.py`, `CSUB EXILEUPDATE`
built from `csub/exilephys.c`). Data comes from `Bas/exile_tools/out/`. None of
these files is tracked in git, because they carry the game's own tables.

| file | bytes | on the board | what |
|---|---|---|---|
| `exile/gbx_world.bin` | 65,536 | `A:/gb/gbx_world.bin` | The first 64 KB of `out/world_types.bin`: the only part `MEMORY INPUT 3, 65536, world()` reads. |
| `exile/gbx_tables.bin` | 960 | `A:/gb/gbx_tables.bin` | `out/phys/tables.bin`, the packed tables the CSUB reads. |
| `exile/gbx_feed.txt` | 135,990 | `A:/gb/gbx_feed.txt` | All 18 `phys_*.txt` feeds in `phys_list.txt` order: a `name,sx,sy,n` header, then `n` lines of 17 values; 2,384 ticks in all. |
| `exile/gbxphys.bas` / `gbxphysp.bas` | 57,357 each | `A:/gb/` | The BASIC kernel. `GBREPS = 1`. |
| `exile/gbxcsub.bas` / `gbxcsubp.bas` | 37,369 each | `A:/gb/` | The CSUB kernel, with the same harness. `GBREPS = 15`. |

- **Options, libraries, slots:** none. The program sets `OPTION CONSOLE SERIAL`
  and `OPTION LOCAL VARIABLES 256` itself. It must not see a flash library (see
  above).
- **Upload:** 392,320 bytes after XMODEM padding, about **39 s at 10 KB/s**
  (about 78 s at the 5 KB/s the Prince of Pico README measured). Without the
  two `p` twins it is 297,472 bytes, about 30 s.
- **Output:** `GAMEBENCH exile_phys 2384 <ms> <ms/tick>` is the whole timed
  loop. `GAMENULL` is the same loop with the kernel call left out, timed
  *before* the timed region. `GAMEKERNEL` is the difference, the kernel alone.
  Both passes run exactly the same harness statements. The per-tick unread-feed
  check is one unconditional statement, `gbMiss = gbMiss + doK * (feedI < feedN)`,
  for that reason.
  `GAMECHECK ... PASS` means all 18 end states match the game's own
  (`out/traces/*.json`, the last tick), with no feed value left unread and no
  fault. `GAMEDIFF` names any scenario that does not match. The CSUB twin
  prints the same lines as `exile_csub` over 35,760 ticks.
- **Expected share in SUB/FUNCTION calls: 20-35% of the kernel's time.**
  The kernel has 923 statements in 56 routines and about 200 call sites, which
  is 0.22 per statement. There are 39 SUBs, and 17 FUNCTIONs such as `Add8`,
  `Sub8`, `Asr`, `KeepRange` and `AbsC`. The FUNCTIONs are called inside
  expressions with 1-5 byte arguments and a `LOCAL`, and their bodies are 2-5
  statements. By the Phase 0 costs on this build (a 2-argument FUNCTION is
  9,748 cycles against 1,348-4,695 for a simple statement), each of those calls
  is 2-5 statements' worth of machinery. The review found the same: "the tick is
  calls and array traffic, not tight arithmetic" (Exile design review, S2).
- **Interpreted/CSUB pair: yes, this is one.** It is the same transcription in
  both, checked on the same 2,384 ticks: 9.8 ms against 0.1 ms per tick,
  measured on HDMIWEB at 378 MHz, kernel time only. The overhead fraction is
  O = (K_basic − K_csub) / K_basic, from the two `GAMEKERNEL` lines, and should
  come out about 98-99%.

## 2. Elite: `gbelite` (DEMOSCENE 1, flight and combat, 600 frames)

Source: `Bas/elite/src/*.bas` + `data/ships.bas` + `data/tokens.bas`, copied to
`elite/src` and `elite/data`, patched, and built with the repository's own
`Bas/elite_tools/build.py`, redirected to this directory. The unpatched copy
rebuilds the repository's `elite.bas` and `elite_lib.bas` byte for byte;
`make_games.py` checks that on every build.

| file | bytes | on the board | what |
|---|---|---|---|
| `elite/gbelite_lib.bas` | 33,300 | `A:/gb/gbelite_lib.bas` | The library (declarations and ship data), loaded by the program's first line as `LIBRARY LOAD MM.INFO(PATH) + "gbelite_lib.bas", RAM`. |
| `elite/gbelite.bas` / `gbelitep.bas` | 116,980 each | `A:/gb/` | The program (106 K of the 152 K program memory). |

- **Options:** `MODE 2` (320x240) and `FRAMEBUFFER CREATE`, both set by the
  program. The RAM library occupies RAM slot 5 (image slot 8) while the program
  runs and is released at END. `title.jpg` is not needed, because
  `DEMOFRAMES > 0` never shows the title.
- **Changes from the game** (all in the copy):
  - No trace cache.
  - `DEMOFRAMES = 600`.
  - `SOUNDON = 0`.
  - `PROFILE = 0`, so the game's own stage timers are off.
  - A fixed tick `GBTICK = 0.5` and a virtual 40 ms-per-frame clock `gbNow`,
    in place of `TIMER` in `NextTick`, `ResetTick` and the message timeout.
  - All 86 `RND` uses replaced by `Math(Rand)`, seeded by
    `MATH RANDOMIZE 20260924`.
  - No screenshots, and no death screen or pause after the loop.
- **Upload:** 267,392 bytes, about **26 s at 10 KB/s**, or 15 s without the
  `p` twin.
- **Output:** `GAMEBENCH elite_demo <frames> <ms> <ms/frame>` covers the frame
  loop. `<frames>` is 600 unless the ship dies or docks first. `GAMECHECK
  elite_demo frames .. shots .. hits .. kills .. energy .. fuel .. slots .. dead
  .. witch .. objs ..` is the end state. It must be identical on every run and
  every build, and the first run sets the reference. `objs` is `maxObj`, the
  number of ships that may be drawn as meshes. `ProbeObjects` works it out from
  `MM.INFO(HEAP)` at start-up, so it is the one input to the workload that
  depends on the build. A PC3 counts its PSRAM as heap and should always report
  32, the firmware's cap (`Bas/elite/README.md`).
- **Expected share in SUB/FUNCTION calls: 10-20% of the frame.** There are 260
  routines: 201 SUBs and 59 FUNCTIONs, with on average 0.7 parameters and 1.9
  LOCALs each. The frame loop calls about 20 top-level SUBs, and those loop
  over up to 20 ships calling `ViewXform`, `ViewOrient`, the AI and so on. The
  split: firmware drawing (`Draw3D` meshes, stardust `PIXEL`/`LINE`, the
  dashboard `BOX`/`TEXT`, `CLS`, `FRAMEBUFFER COPY`) takes an estimated
  20-35%, which is the H4 part, and the rest is the interpreter. Read the
  drawing share with `pcs_games.py`, not `pcs_report.py`, which has no graphics
  bucket (see Run commands). The sampler sees functions, not statements. It
  cannot say how much of a drawing statement's cost is argument marshalling,
  because `getargs`/`evaluate`/`getint` land in the evaluator buckets whichever
  statement called them. H4's marshalling criterion therefore comes from the
  graphics micro pairs; this profile gives the drawing share and the busiest
  drawing lines.
- **Interpreted/CSUB pair:** none exists. `mmb2csub` could make one for a
  kernel such as `DrawStardust` or `MoveShips` if the profile singles one out.

## 3. Prince of Pico: `gbpop` (the scenario harness, headless)

Source: `Bas/sandglass_tools/player.bas` plus the patches, stripped by the
repository's `build.py` into `pop/gbpop.bas`. `pop/gbpop.bas.map` maps
stripped lines back to `pop/gbpop_src.bas`, the patched full source.
`gbscen.txt` holds every `scen/*.txt` except the `discover*` ones, joined the
way `run_scentest.py` joins them: 31 files, 93 scenarios, 97 `AT`/`START`
level resets, 274 checks, and at most 2,732 frames.

| file | bytes | on the board | what |
|---|---|---|---|
| `pop/gbpop.bas` / `gbpopp.bas` | 146,361 each | `A:/gb/` | The engine. **Load it with `LOAD "A:/gb/gbpop.bas", C` on every build that is compared.** LOAD holds the text in a buffer of heap − 3,072 − 3 × HRes − 2,560 bytes (`misc/FileIO.c:4357-4377`, `configuration.h:561`). That is 148,096 bytes on HDMIUSB at 640 wide, so the raw file only just fits (by 1.7 KB); on HDMIWEB's 144 K heap it does not. The C also strips inner spaces, which changes the per-statement pre-scan cost (H7), so the load mode is part of the workload. A load that does not fit says so in one line and leaves the previous program in memory. |
| `pop/gbscen.txt` | 57,852 | `A:/gb/gbscen.txt` | The scenarios. The copy looks for `gbscen.txt`, never `scen.txt`, and without it prints a message and ENDs instead of playing. |
| `tables.idx` | 3,787 | `A:/gb/` | **Converted game data. It is not in this directory and must not be copied or distributed.** It is taken from `C:/Users/peter/AppData/Local/Temp/claude/popdata/`, the local `convert.py` output of 2026-09-20. |
| `blocks.dat` | 755 | `A:/gb/` | as above |
| `seq.dat` | 2,546 | `A:/gb/` | as above |
| `frames.dat` | 2,001 | `A:/gb/` | as above |
| `art.bin` | 10,812 | `A:/gb/` | as above |
| `sounds.dat` | 80 | `A:/gb/` | as above |
| `levels.dat` | 34,560 | `A:/gb/` | as above |
| `sheet1.bmp` ... `sheet4.bmp` | 139,894 / 139,382 / 144,502 / 91,766 | `A:/gb/` | as above. They are loaded into image slots 4-7 (RAM slots 1-4) at every start, before the timed region. The start-up screen draws from them even though the scenarios draw nothing. |

- **Not needed:** `cutroom.bmp`, which is skipped when absent and would
  otherwise land in slot 8, the RAM-library slot; `title.jpg`; `art.idx`; the
  `music/` files.
- **Options:** `Option Console Serial` and `Option Local Variables 128`, both
  set by the program. The second is the reason no flash library may be present.
  `MODE 2`, two framebuffers and PSRAM are also needed.
- **Upload:** 921,088 bytes after XMODEM padding (the program and its twin are
  293 KB, the scenarios 58 KB, the data 570 KB), about **90 s at 10 KB/s**. The
  Prince of Pico README measured about 5 KB/s, which would make it 3 minutes.
  Without the `p` twin it is 628,224 bytes, about 61 s.
- **Output:** `GAMEBENCH pop_scen <frames> <ms> <ms/frame>` is the whole timed
  region: scenario parsing, the level loads for each `AT`, and the frames.
  `GAMEFRAME pop_scen <frames> <ms> <ms/frame>` is the `GameFrame` calls alone,
  which is the game logic. `GAMECHECK pop_scen n of 274 checks passed`, then
  `PASS` or `FAIL`. The scenario report is switched off during the timed region
  (`Option Console None`) so that the UART is not timed. For FAIL details, set
  the console line back or run the repository's `run_scentest.py`.
  **The profile mixes set-up and frames.** The sampler covers the whole timed
  region. The PC samples cannot be split by BASIC routine, so the level loads
  (file reads from A:) and the directive parsing are in the shares too. Quote
  the H3 share together with GAMEFRAME / GAMEBENCH, the fraction of the region
  spent in `GameFrame`. The `[PCSLINE]` table shows the busiest lines.
- **Expected share in SUB/FUNCTION calls: 15-30% of `GameFrame`.** There are
  252 routines: 171 SUBs and 81 FUNCTIONs, 91 of them 6 statements or fewer.
  `GameFrame` is a chain of calls (`KeepTime`, `MiscTimers`, `AnimMobs`,
  `CheckAlert`, `StepCharacter` twice, `SaveChar`, `LoadKidWOp`, ...), and
  those call small FUNCTIONs (`Advance`, `GetDist`, `BType`, `Sgn8`,
  `FrameRowOf`, ...) inside conditions.
- **Interpreted/CSUB pair:** none.

## Run commands

`PORT` is the PC3's console UART (COM16 on the current bench). The driver never
interrupts a run on a timeout. It stops only at the prompt, on an error, or on
an unexpected INPUT prompt. It answers the "PRESS ANY KEY" pages of the
`[PERF]`/`[PCS]` report. Before each LOAD it sends `OPTION NOCHECK OFF`
(`--nocheck` sends `ON`). The JSON line records which.

```
python gamebench.py PORT cmd "LIBRARY DELETE" "FLASH LIST"
      (if FLASH LIST still shows slot 3 in use: python gamebench.py PORT cmd "FLASH ERASE 3")
python gamebench.py PORT put exile
python gamebench.py PORT put elite
python gamebench.py PORT put pop                      (reads the converted data from popdata/)

python gamebench.py PORT run gbxphys.bas --runs 3 --label xphys
python gamebench.py PORT run gbxcsub.bas --runs 3 --label xcsub
python gamebench.py PORT run gbelite.bas --runs 3 --label elite
python gamebench.py PORT run gbpop.bas --crunch --runs 3 --label pop

python gamebench.py PORT run gbxphysp.bas --label xphys-prof
python gamebench.py PORT run gbxcsubp.bas --label xcsub-prof
python gamebench.py PORT run gbelitep.bas --label elite-prof
python gamebench.py PORT run gbpopp.bas --crunch --label pop-prof

python D:/Dropbox/PicoMite/PicoMite/Bas/bench/pcs_report.py xphys-prof_1.txt <ELF> --src exile/gbxphys.bas
python D:/Dropbox/PicoMite/PicoMite/Bas/bench/pcs_report.py xcsub-prof_1.txt <ELF> --src exile/gbxcsub.bas
python pcs_games.py elite-prof_1.txt <ELF> --src elite/gbelite.bas
python D:/Dropbox/PicoMite/PicoMite/Bas/bench/pcs_report.py pop-prof_1.txt <ELF> --src pop/gbpop.bas
python D:/Dropbox/PicoMite/PicoMite/Bas/bench/pcs_lines.py xphys-prof_1.txt <ELF> DefinedSubFun makeargs findvar
```

`<ELF>` is `../uf2/HDMIUSB_0096659.elf` while the board carries 0096659.
Otherwise rebuild the ELF from the commit that was flashed. `pcs_games.py`
(this directory) is `pcs_report.py` with three graphics buckets in front,
taken from the ELF's own symbol-to-file map; it gives the same report
otherwise, and it can be used on all four. The `--src` line numbers are
right for all three programs. Exile and Elite are loaded without C, and
blank and comment lines are kept, as they were for the Phase 0 anchor.
`gbpop.bas` has no blank or comment lines, so C removes none, and
`gbpop.bas.map` then leads to `gbpop_src.bas`.

What the sampler can and cannot settle for H3. `pcs_lines.py` splits
`DefinedSubFun` by source line. `makeargs` and `findvar` are also called for
things that are not calls: every multi-argument command and every variable. So
their samples cannot be charged to the definition side of a call. In the Exile
kernel's timed region almost every `makeargs` is a call (it has no
multi-argument commands), so there the `makeargs` share is a fair upper bound.
H3's "definition side ≥ 25% of call cost" is an E5 micro-pair criterion. The
game profiles settle "call bucket ≥ 20% of game samples". Read that off the
`call machinery` line, and report `arg parsing / text walk` and `name lookup`
beside it.

A profiled run is perturbed. `OPTION PROFILING ON` also counts every statement
and every `findvar`, and stamps `time_us_64()` twice per call inside
`EnterLocalFrame`/`LeaveLocalFrame` (`core/MMBasic.c:355-400`). The stamps are
in the call path, so they inflate the call bucket slightly. Quote the ratio of
the p-twin's GAMEBENCH to the timing run's GAMEBENCH with every share.

## Expected duration on the PC3 (HDMIUSB, 378 MHz)

| step | estimate |
|---|---|
| uploads, all three, with twins | about 2.6 min at 10 KB/s (up to 5 min at 5 KB/s) |
| `gbxphys` timing run | `GAMEBENCH` 20-24 s; with the feed load and calibration pass, 25-30 s wall per run |
| `gbxcsub` timing run | `GAMEBENCH` 13-18 s, `GAMENULL` 9-13 s. The harness is 17 stores of the form `st(CONST) = gbFd(k, tk)` a tick, each a 2-D read (4,381 cycles in Phase 0) plus a 1-D write through a CONST: about 0.3-0.36 ms a tick over 35,760 ticks. About 30-40 s wall per run |
| `gbelite` timing run | `GAMEBENCH` 11-30 s. That is 18-48 ms a frame: 18 ms is the README's in-game figure with the trace cache, and 42 ms is DEMOSCENE 1 with PROFILE stage timers. Fewer frames if the ship dies or docks. 20-40 s wall per run including start-up |
| `gbpop` timing run | `GAMEBENCH` 12-35 s (the least certain estimate); plus 10-20 s for `LOAD ,C` once and 2-3 s start-up per run |
| each profiled run | the timed region runs 10-30% slower under PROFILING. The report then prints one `[PCS]` line per distinct PC: 4,099 for the anchor in Phase 0, and perhaps 4,000-7,000 for a game, with paging. `PcsReport` ranks by scanning the whole 16,384-entry table in PSRAM once per line, so allow 1-3 min |
| 3 timing runs of each plus one profiled run of each | about 15-20 min in all, after the uploads |

Not run on the RP2040 VGA: none of the three fits it (NOTES.md, "RP2040
PicoMiteVGA, 315 MHz: not applicable").

## Reserves (not prepared)

- **Thrust**: the built-in entity benchmark (`Bas/thrust.bas:594-649`, the
  `B` key). It is one 79 KB file with no data, and times simulation and drawing
  over 480 frames. It is not prepared because it is started by a key press,
  waits for a key at the end, uses `RND` (non-deterministic on the RP2350),
  calls `PLAY BBC SOUND`, and has only 57 routines.
- **`Testfiles/ExilePhysicsBench.bas`**: the S2 spike, self-contained. It is a
  stand-in for the kernel's shape rather than the game, and it rewrites flash
  slot 1 on every run.
- **`Bas/exile/exile.bas`** (the game): not a candidate. Its kernel is already
  a CSUB (0.28 ms a tick), the frame is 6.8 ms of `TILEMAP`/`BLIT` drawing, it
  is played from the keyboard with no headless mode, and it needs 12 files and
  two flash images.
