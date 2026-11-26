# Threads on the PicoMite: review of the plan

26 November 2026. Peter: "review Threads_PicoMite_Plan.md and create a threads branch to start the development.
The objective is that this should be available on all firmware variants if possible, fallback is rp2350 only.
PSRAM should not be considered a requirement."

The plan is `D:\Dropbox\VSCode-STM32\CMM2V1.5\docs\Threads_PicoMite_Plan.md` (CMM2V1.5 c8c62a5, written by the
Colour Maximite 2 session from a read of this tree at d68c9e6). This review reads it against the PicoMite's
source at eebcb7b and measures each build's margins. The work is on branch `threads`, worktree
`D:\Dropbox\PicoMite\pm_threads`, made from main at eebcb7b (local, not pushed).

## Decisions taken

- **Plan decision 1 (which builds):** every build if it can be done, the RP2350 builds if not. So Stage P0 is
  built and measured on all eighteen builds from the start, not the RP2350 ones first.
- **PSRAM is not required.** The plan already takes threads' stacks and records from SRAM (`GetSRAMMemoryNull`),
  which on a build without PSRAM is the whole heap. Nothing in it needs PSRAM; keep it that way.

## 1. Flash, not RAM, decides "every build"

The plan says "the PicoMite's flash has room". It has not. On the CMM2 the threads cost 14.6 KB of flash
(`Thread.c` 12.1 KB, the hooks 2.5 KB) in Thumb-2. The RP2040 runs Thumb-1, which is usually larger (*to
measure*). Every build is closer to its FLASH_TARGET_OFFSET than that:

| Build | Chip | FLASH_TARGET_OFFSET | Flash left | Room after one 16 KB step | RAM left |
|---|---|---|---|---|---|
| PICO | RP2040 | 928 KB | 3.9 KB | 19.9 KB | 6.0 KB |
| PICOMIN | RP2040 | 720 KB | 8.7 KB | 24.7 KB | 5.4 KB |
| PICOUSB | RP2040 | 960 KB | 3.7 KB | 19.7 KB | 2.6 KB |
| VGA | RP2040 | 848 KB | 2.9 KB | 18.9 KB | 3.9 KB |
| VGAUSB | RP2040 | 896 KB | 12.9 KB | 28.9 KB | 3.5 KB |
| WEB | RP2040 | 1328 KB | 5.3 KB | 21.3 KB | 2.4 KB |
| PICORP2350 | RP2350 | 1152 KB | 5.5 KB | 21.5 KB | 4.8 KB |
| PICOUSBRP2350 | RP2350 | 1184 KB | 3.0 KB | 19.0 KB | 1.5 KB |
| VGARP2350 | RP2350 | 1072 KB | 4.9 KB | 20.9 KB | 4.3 KB |
| VGAUSBRP2350 | RP2350 | 1120 KB | 8.0 KB | 24.0 KB | 4.4 KB |
| WEBRP2350 | RP2350 | 1536 KB | 48 bytes | 16.0 KB | 3.9 KB |
| PICOBTRP2350 | RP2350 | 1456 KB | 7.6 KB | 23.6 KB | 3.5 KB |
| PICOBTHRP2350 | RP2350 | 1536 KB | 5.6 KB | 21.6 KB | 4.5 KB |
| HDMI | RP2350 | 1104 KB | 4.9 KB | 20.9 KB | 2.2 KB |
| HDMIUSB | RP2350 | 1184 KB | 14.3 KB | 30.3 KB | 1.8 KB |
| HDMIWEB | RP2350 | 1584 KB | 6.0 KB | 22.0 KB | 1.7 KB |
| HDMIBTH | RP2350 | 1520 KB | 6.6 KB | 22.6 KB | 4.6 KB |
| PC3 | RP2350 | 1792 KB | 8.7 KB | 24.7 KB | 1.5 KB |

Margins from `tools/GetHighestHexAddress.py` on private Ninja builds of eebcb7b (the same figures as
buildpicomite.bat's). RAM is what is left after the C heap and stack reserves.

So threads on every build means moving FLASH_TARGET_OFFSET up one 16 KB step on each, two where the margin is
small and the code comes out over 16 KB (WEBRP2350 has 48 bytes now). Moving it wipes A: and resets the options
when a board is updated. Done once, in one V8 beta, with a release note, that is the usual price of a layout
change. **This is Peter's decision** (see the end). Stage P0 can go ahead without it: it adds only pointer loads.

## 2. Static RAM must stay near zero

On the CMM2 the threads take 1.4 KB of RAM before any thread starts. PC3 and PICOUSBRP2350 have 1.5 KB to spare, HDMIWEB 1.7 KB and HDMIUSB 1.8 KB. The
rule for the port: the thread table and every per-thread record come from the heap at the first THREAD START and go
back when the last thread ends; the static part is a pointer to that block, the running thread's number and the
tick's slice fields, a few dozen bytes. The `*_main` arrays of Stage P0 are today's arrays, so they cost nothing.

`ExecuteProgram` and `check_interrupt` are `__not_in_flash_func` on every build, so the slice test and the pointer
loads cost RAM on the RP2350 builds too, not only the RP2040's (plan section 11). PC3's and PICOUSBRP2350's 1.5 KB are the tightest.
(The plan's VGAUSB with 0.5 KB spare has 3.5 KB now.) Measure each build's RAM margin after every stage.

## 3. The landing after an error, corrected

Plan section 4 says: at the landing, if SPSEL is set, clear it, "the stack pointer the jump restored is then MSP".
It is not. With SPSEL set, `longjmp`'s write of SP goes to PSP; clearing SPSEL then brings back the old MSP, which
is where the main program was switched out, below the frame `setjmp` saved. The landing must move the value across:
read SP, clear SPSEL, ISB, write it to MSP. Until then an interrupt stacks its frame on PSP, which now points into
the main stack below live frames, and that is harmless.

The CMM2 does not have this fault (checked 26 November, CMM2V1.5 `threads`): its threads never use PSP.
`ThreadSwap` (`Src/Thread.c` 160) moves MSP itself to the thread's stack, so `longjmp` restores MSP and the landing
is on the main stack, which `ThreadsEnd` checks (DTCM). The CMM2 runs with SPSEL set only in its Loader, whose
services run on the program's PSP; it keeps `error()` from leaving one by rule (each error-raising call is inside
`ld_try`, whose `setjmp` and `longjmp` are both on PSP, and Ctrl-C is left to the program). The PicoMite's Loader is
safe by construction: `ld_veneer` clears SPSEL before any service runs.

## 4. What exists, and what is new

- The Loader's process-stack call (`misc/Loader.c` 1251-1338) is ARMv8-M only (`msr psplim`, d8-d15 saved) and
  built on two builds only (PC3, HDMIUSB). It is the model for the RP2350's switch; the RP2040's is new code
  (ARMv6-M: no PSPLIM, no FPU, r8-r11 only through the low registers).
- A PSPLIM overflow today reaches the fault handler (`PicoMite.c` 2937), which prints the dump and restarts. The
  plan's "Thread n overflowed its stack" as a BASIC error needs a handler that returns to thread mode at an error
  routine, on MSP. That is new work for Stage P1; until it exists, an overflow restarts the board with the dump.
- `TestStackOverflow` (`MMBasic.c` 1480) already compares MSP with the variable `stackwarn`, not `heapend`:
  Stage P0 step 2 is only to make the register it reads follow the running thread.

## 5. Order of work

- The CMM2's `threads` branch is not merged into v8-dev yet and 8.00.00b5 is not tagged (its G1 regression was
  running on 26 November). Stage P1 copies `Thread.c` from that tag, so it waits; Stage P0 does not need it.
- Stage P4b's prerequisite is done: issue #166's fix is on main (9f20b28). Issue #165 is fixed on main too
  (8486f06) but differently from the CMM2: a ring of four chunk buffers passed to lwIP without a copy, because
  lwIP's copies come from the C heap here (MEM_LIBC_MALLOC, 12.7 KB in all on an RP2040 WebMite). The network
  yields must be merged around that, not over it.

## Next on the branch

Stage P0 as the plan has it, on all eighteen builds, each build's flash and RAM margin recorded before and after
each step:

1. The interpreter's stacks as pointers with limits, by script (`g_forstack`, `g_dostack`, `gosubstack`,
   `errorstack`, `substack`, `datastore`, `g_StrTmp`, `g_StrTmpLocalIndex`, `g_hashlist`, with `*_main` arrays).
2. The stack check reading the register in use.
3. `IntReady`'s tick field; `cmd_pause`'s end time out of the function.
4. `LocalBase` and `LocalLimit`.
5. One function for the frame waits a program asks for.

## For Peter

1. **FLASH_TARGET_OFFSET up one 16 KB step on every build** (two on WEBRP2350 if the code needs it), wiping A: and
   the options at that update. Without it, threads fit on no build.
2. Plan decisions 2 to 6, unchanged, with the plan's recommendations: threads on PSP with interrupts and the
   housekeeping on the main stack; defaults SIZE 4 KB, LOCALS 16, NEST 8, sixteen threads (to be measured);
   loadable programs, SQL and USER modules from the main program only; OPTION NOCHECK keeps the slice test; the
   FRAMEBUFFER WRITE target each thread's own.

## Decisions, 26 November (Peter)

- Plan decision 2: threads run on the process stack (PSP); interrupts and the housekeeping stay on the main stack.
- Plan decision 3: THREAD START's defaults SIZE 8 KB, LOCALS 24, NEST 8; sixteen threads. (The plan proposed SIZE 4,
  LOCALS 16.) Still to be checked against Stage P5's measurements.
- Plan decisions 4 and 5 as proposed: loadable programs, SQL and USER modules from the main program only; OPTION
  NOCHECK keeps the slice test while a thread runs.
- Plan decision 6: each thread has its own FRAMEBUFFER WRITE target.
- Thread.c is copied from the CMM2's head (78d4172) now, not from a b5 tag; FLASH_TARGET_OFFSET goes up wherever the
  development needs it. WEBRP2350's went up on main (0dc2553), where #164 had taken it 160 bytes over.
- RP2040 testing only once everything is proven on the RP2350, on the Pico that is now the debug probe.
- Later the same day (Peter): "Up the local variable slots to 256 on the PC3, reduce the maximum threads on the
  PicoMite to 8, drop the rounding". The PC3's local region is 256 with threads (480 globals: the same total, no RAM;
  OPTION LOCAL VARIABLES 192 gives a program the 544), MAXTHREADS is 8, and a thread's local slots are packed below the
  next range, where the CMM2 starts each on a multiple of 16. Eight threads of LOCALS 24 take 200 slots.
