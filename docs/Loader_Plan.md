# Loadable Programs and USER Modules: Implementation Plan

**Status, 22 November 2026:**
- The decisions are made: Peter accepted every recommendation in section 2, and on 17 November settled D13: the loadable programs are part of V8, in line with the Colour Maximite 2.
- The branch is **`v8`**, on GitHub as `origin/v8`, in the worktree `D:\Dropbox\PicoMite\pm_loader`. It was created as `loader` from main at 544be7b on 16 November, and renamed and pushed on 18 November at Peter's word. Main at the V7.00.00 release (c187ef5) is merged in (a17616d), and the version is 8.00.00b1 (`MM.VER` 8.000001).
- Phase 0 is done (16 November): the flash and SRAM margins are in section 10 (Per-variant figures), and the solar_eclipse baseline is in Phase 0 step 4. 
- **16 November, later:** Phase 1 is done on the PC3 (committed in 96ab930). For Peter's "try the simplest and smallest CMM2 app", parts of Phases 3, 4 and 5 were done ahead of their turn: all the file and directory services (A:, B:), `video_mode`/`video_palette`/`video_wait` with BASIC's screen put back, `keys_held`, `keys_raw`, `gamepad` (a generic pad's sticks read 128, as on the CMM2), and sound as stubs (`audio_start` returns -1). The CMM2's NES (apps/nes, frank-nes) runs Nova the Squirrel at about 55 frames a second at 315 MHz, silent, and its keyboard works. The loader is about 8 KB of flash; with V7.00.00 merged in, the PC3 has 4.7 KB of flash and 1.9 KB of SRAM left.
- **22 November:** Phase 6 (sound) is done on the PC3 (see Phase 6): `audio_start`, `audio_stop`, `audio_lock` and `audio_unlock`, with the spare-interrupt refill and D8. Tested by ear on I2S with `sndtest.app`. The PC3 now has 3.3 KB of flash and 1.9 KB of SRAM left.
- **22 November, later:** Phase 7 (I/O) is done on the PC3, with the mouse and `gamepad_axes` (see Phase 7). To fit, Loader.c is now compiled with `-Os` (OPTIMIZED_SOURCES), which left the PC3 352 bytes; then, at Peter's word, the PC3's flash target moved **1760 -> 1776 KB**: 16.3 KB of flash left, 1.9 KB of SRAM. On the PC3's 16 MB map A: stays where it is, but the options and the flash slots move: they reset on the first boot after upgrading from V7.00.00, and on every change between V7 and V8 firmware (a line for V8's release notes). COM10's were saved with OPTION DISK SAVE and loaded back.
- **22 November, evening:** Phase 9 (USER modules) is done on the PC3 (see Phase 9): 2.5 KB, leaving 13.5 KB of flash and 1.8 KB of SRAM. Phase 8 (calling BASIC) was skipped at Peter's word.
- Next: Phase 10 (first ports) and the phases after it.

This plan was written against:
- PicoMite `main` at db2c2d7;
- the Colour Maximite 2 at 8.00.00b1 (`D:\Dropbox\VSCode-STM32\CMM2V1.5`, branch `v8-wifi`);
- the CMM2 manual *Colour Maximite 2 Loadable Programs* (its source is `docs/loadable/text/*.md` in that repo).

Line numbers are as of those commits. The 4 November review that Peter decided not to pursue then is summarised in section 11.

---

## 1. What this adds

Two features, ported from the CMM2:

- **Loadable programs.** `RUN "doom.app", "-timedemo demo1"` loads a separately compiled C or C++ program from A:, B: or C: and calls it with a table of firmware services. The program runs at full speed and then returns to MMBasic, which tidies up after it. At the prompt, control goes back to the prompt. In a BASIC program, it carries on with the next statement, with the program's variables intact.
- **USER modules.** `USER LOAD "mymod.mod"` loads resident C code that adds `USER ...` commands and a `USER(...)` function. The module uses the interpreter's own routines (getargs, evaluate, findvar, error), so it is in effect a CSUB that comes from a file, with a C library, malloc and files.

**Builds.** The work targets RP2350 builds. The PC3 comes first; other variants follow once their flash room has been measured. The RP2040 is not a target: it has no PSRAM, uses a different instruction set and has no flash to spare.

**Not ported:**
- the CMM2's C compiler and bytecode runtime (`cc`, `mmbrun`);
- the OPL synthesiser and MIDI player, which Peter ruled out on 4 November as too resource-hungry for the firmware (an app can carry its own synth, which costs app CPU time, not firmware flash);
- the DMA2D;
- the frame hook, because the PicoMite has no vsync interrupt to hang it on.

---

## 2. Decisions

**Decided 16 November 2026:** Peter accepted every recommendation below ("accept all recommendations").
- D7: the branch stayed local, which was the recommendation, until Peter had it pushed on 18 November (as `v8`).
- D13 had no recommendation; Peter decided on 17 November that it goes into V8.

| # | Question | Recommendation |
|---|---|---|
| D1 | Does the app API match the CMM2's? | **Source-compatible, not binary-compatible.** Every service the two firmwares share has the same name, signature and meaning (`cmm2->video_mode`, `cmm2_start->heap`, `cmm2user.h`...). The PicoMite has its own header, its own table order and its own version numbers, plus a few PicoMite-only services. The binaries can never be shared (different CPU and addresses), so matching layouts would buy nothing, while matching names makes the CMM2 ports' portable parts compile unchanged. |
| D2 | Where does the SDK live? | `tools/sdk/` in the PicoMite repo, so that it moves in step with the firmware's header. It starts as a copy of the CMM2's tools/sdk with the target changed. |
| D3 | Which builds get it first? | The **PC3 only**, until Phase 10's measurements say it is worth having. Then the others, one at a time, each after measuring its flash margin (Phase 11). |
| D4 | RUN syntax for the arguments | **PicoMite's existing form**: `RUN "x.app", args$`, a string expression, as `MM.CMDLINE$` already works. The CMM2 takes raw words after the comma, and that tokenised them (`-timedemo` arrived as `\x83TIMEDEMO`). |
| D5 | RUN "x.app" inside a program | **Carries on** with the next statement, as on the CMM2. The app path branches off before anything is cleared, so returning from `do_run` continues the program. |
| D6 | The software `WATCHDOG` while an app runs | **Suspended**, and resumed when the app returns. Otherwise a program that set WATCHDOG and then ran an app would be reset in the middle of it. |
| D7 | Push the branch to GitHub? | **Local only** until Peter says otherwise. Pushing would give a backup, but the repo is public. *Pushed on 18 November at Peter's word, as `v8`.* |
| D8 | Priority of the audio wrap interrupt while an app plays sound | **Raised above the refill interrupt** for as long as the app's sound runs (it is 255, the lowest, today). The alternative, refilling from `poll()`, broke music on the CMM2 whenever a frame was slow. |
| D9 | Lend the PSRAM reserve (context area, Route B stream) as well? | **No** at first. Revisit only if a port runs short of memory. |
| D10 | Fault recovery (Phase 12) | **Yes, but late.** A fault in an app returns to the prompt with "x.app faulted at +0x1234" instead of rebooting. The CMM2 planned this and never did it. |
| D11 | Calling BASIC from an app (`basic_cmd` and friends) | **Later**, if at all (Phase 8). None of the big ports needs it. |
| D12 | USER modules in scope? | **Yes** (Phase 9). They are the part that fits the PicoMite best, and small modules need no PSRAM. |
| D13 | Which release carries it | **Open.** Peter's call. It changes the PC3's flash map if the loader does not fit, which would move A: (section 4.10). *Decided 17 November: V8, in line with the Colour Maximite 2; V7.00.00 was released without it.* |

---

## 3. Branch and workflow

The aim is that main stays the release line and keeps taking bug fixes while this work goes on. Nothing from this work reaches main until Peter says so.

**Branch.** `v8` (on GitHub as `origin/v8`), created as `loader` on 16 November 2026 from main at 544be7b and renamed and pushed on 18 November. Its first commit is this plan, moved from main's working tree, where it was written untracked. Main stays the V7 release line; `v8` is the V8 development line.

**Worktree.** `D:\Dropbox\PicoMite\pm_loader`, created with `git worktree add ../pm_loader -b loader main`; it follows the branch's new name.
- It sits beside the main checkout, so `../tinyusb-master` and the other sibling trees resolve as they do for main (the pc3-all branch worked this way in October).
- Peter's main checkout and `buildpicomite.bat` stay on main, so his full builds never pick up loader work.

**Builds on the branch.** Private Ninja builds into the scratchpad, as described in the `reference_private_ninja_builds` memory:
- `-DCOMPILE=PC3` and so on, with `PICOTOOL_FETCH_FROM_GIT_PATH` pointing at a copy of picotool;
- about 20 s per variant;
- never while Peter's build is running.

Test images are named `PicoMitePC3_loader_<sha>.uf2` and are kept in the scratchpad, never in `uf2/`, so the release pre-flight never sees them.

**Keeping up with main.** After every bug fix pushed to main, or at least before each board-test round, run `git merge main` on `v8`. Conflicts are resolved on the branch, never on main. Two things keep conflicts small:
- Fencing. Everything new goes into new files, behind one `#ifdef LOADER`:
  - `misc/Loader.c` and `misc/Loader.h`: the loader and the services;
  - `misc/User.c`: the USER command;
  - `loader/pmimg.h`: the image format and the services table;
  - `loader/pmuser.h`: the USER module interface;
  - `tools/sdk/`.
- Short hooks. Edits to shared files are hooks of a few lines each, all `#ifdef LOADER`, listed in section 10.

**Merging back.** Only on Peter's go, after Phase 13. The steps:
1. Merge main into `v8` one last time.
2. Run the full goldens on the PC3.
3. Peter runs a full `buildpicomite.bat` build from the branch.
4. Merge to main. This is a fast-forward if nothing landed on main after step 1, otherwise a merge commit.

**Working rules** (Peter's standing ones):
- One change at a time, each built and run on a board before the next. No big untested rewrites.
- Goldens on the PC3 about every five changes, with functional checks between rounds.
- Ask before measuring, before reflashing a board, and before using a board Peter may be using. One board at a time.
- Discuss a change before making it. Nothing in this plan is decided until Peter rules on section 2.

**Boards:**
- COM16 or COM17, both PC3s flashed over SWD (`reference_pc3_swd_flashing`), as the main rig.
- A board without PSRAM (COM13 RP2350A or COM20 VGA) for SRAM-only placement.
- COM29 (a PicoCalc with a Pico Plus 2 W, 8 MB PSRAM) for an SPI-LCD target.
- Each one only with Peter's OK.

---

## 4. Design

### 4.1 Memory

**The PSRAM.** The reserve stays as it is. A program borrows the PSRAM heap while it runs, as the CMM2 Generation 1 does.
- **The reserve has no room.** Every byte of it is assigned: the context area (384 KB), five RAM slots, then the Route B stream (configuration.h:730-756). On the 208 KB-program builds that is 384 + 8 × 208 = 2048 KB, with nothing spare. The PC3 has 128 KB above the stream's minimum.
- **A carve-out would cost everyone.** It would come off BASIC's heap permanently, whether or not anyone ever runs an app.
- **What can be borrowed.** At the prompt the PSRAM heap is nearly empty: 6 MB on an 8 MB part, 5.5 MB on the PC3. A CMM2 Gen 1 gets 4.2 MB.
- **The bottom is usually free.** The PSRAM allocator works top-down (Memory.c:2198 `GetPSMemory`, and the searches at 2434, 2514 and 2548), so the bottom is the last part to be used. The symbol blocks that `PrepareProgram` makes at every prompt line go in at the top too (Symbols.c:506-517). Apps are therefore linked at **0x11000000** (`PSRAMbase`) and usually load there with nothing moved.
- **Claiming the area.** A new function in Memory.c, `LoaderClaimPS(min, &base, &size)`, finds the largest free run in `psmap` and marks it used. That keeps any allocation made during the run (by a service, or by BASIC through `basic_cmd`) out of it. The run is given back with `FreeMemory` at the end. The function leaves 256 KB of the run free for the services, as the CMM2 does.
- **When the bottom is in use** (by a BASIC program that runs the app and holds large arrays), the app is moved by its relocations to wherever the largest run starts. See 4.2: on the M33 it can be moved by any amount.
- **The heap is not cleared** at END or after an error (Memory.c `InitHeap` only runs at RUN, NEW and LOAD). A previous program's arrays may therefore still be there. They are usually at the top, but the claim works with whatever is free.

**The SRAM.** Two uses:
- **The app's stack goes in the SRAM heap by default** (`GetMemoryNull`), not in PSRAM.
  - A stack in PSRAM would work while the app runs, because interrupts stack onto it through the XIP cache.
  - But it would be slower, and it is the one thing that must never be in PSRAM during a flash write (4.11).
  - The default is 32 KB; `--stack` sets another size. If the SRAM heap cannot give the stack, it goes at the top of the PSRAM area, as on the CMM2.
  - The SRAM heap is 108 KB on the PC3 and 212-364 KB on the other RP2350 builds (configuration.h; table in section 10).
- **A later `--sram` option** (Phase 11) puts named objects (hot code and data) into a block borrowed from the SRAM heap for the run. This is the PicoMite's version of the CMM2's `--axi` and `--itcm`.

**Without PSRAM**, the whole app goes into the largest free run of the SRAM heap (`LoaderClaimSRAM`). That is only practical for small programs and modules, but they run faster there than they would from PSRAM.

### 4.2 The image file

The layout is the CMM2's (`cmm2img.h:69-85`, Appendix B of the CMM2 manual): a 64-byte header, the image, then the relocation words. The differences:

**Header fields**

| Field | CMM2 | PicoMite |
|---|---|---|
| magic | `CMM2IMG\0` | `PICOIMG\0`. Each firmware refuses the other's files with "Not a loadable program". |
| link_base | 0xD1800000 | 0x11000000 |
| kind | 1 app, 2 module | the same |
| flags | bit 0 G2, bit 1 VRAM | bit 0 **needs PSRAM**; bit 2 **has MOVT relocations**, so it can only be moved by a multiple of 64 KB (see below) |
| reserved (offset 14) | 0 | the target, 0x2350 |
| api_version | 1-20 | the PicoMite's own numbering, from 1 |

**Relocations.**
- **Why the CMM2 moves in 64 KB steps.** A Cortex-M7 build emits MOVW/MOVT pairs for addresses. The loader patches only MOVT, which is why the CMM2 moves a program by multiples of 64 KB.
- **The M33 build doesn't need that.** GCC 13.3 with `-mcpu=cortex-m33 -mfloat-abi=softfp` loads addresses from literal pools instead. This was checked on 16 November: a test file, the toolchain's `thumb/v8-m.main+fp/softfp` `libc_nano.a` and its `libm.a` give only ABS32, TARGET1, PREL31, REL32, THM_CALL and THM_JUMP24, with no MOVW or MOVT at all.
- **So an M33 image can be moved by any multiple of its largest section alignment.** That makes placement anywhere in the SRAM heap practical: no 64 KB-aligned hole is needed, so a 3 KB module no longer costs 64 KB.
- mkimage still accepts MOVT (from hand-written assembly or `-mslow-flash-data`), but sets flag bit 2, and the loader then moves that image only in 64 KB steps.
- mkimage records the largest section alignment in the header, using the first byte of the name field's padding, or a new field if the header version goes to 2.

**Coherence.** The loader writes the image through the cached window (0x11000000). Instruction fetches go through the same XIP cache, so no cache maintenance is needed. The uncached alias 0x15000000 must never be used for an image (it is used only in psram.c:388-427 today). The SDK's flash erase and program calls clean the XIP cache before flushing it (FileIO.c:383-387), so an app's dirty PSRAM lines survive a flash write.

### 4.3 Starting and ending a program

**The RUN hook.** In `do_run` (Commands.c:2274), after `pcmd_args` is set (line 2315) and before `FileLoadProgram` (2326), so that nothing has been cleared yet:

```c
#ifdef LOADER
    if (*filename && HasExtension((char *)filename, ".APP")) { LoaderRun((char *)filename, (char *)pcmd_args); return; }
#endif
```

**What happens when the app returns:**
- **At the prompt,** the main loop sees that no BASIC program started and runs `do_end(false)` + `longjmp(mark)` (PicoMite.c:4944-4956), which tidies up as usual.
- **In a program,** `do_run` returns into `ExecuteProgram` and the next statement runs (D5).

**Other ways to run a file**, each needing the same diversion:
- `do_chain` (Commands.c:4535): refuse "Cannot CHAIN a loadable program", or run it. Peter to decide; refusing is simpler.
- The file manager: `fm_name_is_bas` (Editor.c:1208) and `fm_run_bas_program` (4067), called at 5738.
- `EXECUTE "RUN ..."` (Commands.c:11142-11195).
- The `*name args` shortcut, `transform_star_command` (PicoMite.c:3305-3390). Note that the CALCPROMPT form tries only `name` and `name.bas` (4858-4866).

**`LoaderRun` does, in order:**
1. **Stop sound.** `CloseAudio(1)` (the CMM2 found that a stale `WAV_fnbr` closed its program file mid-read: check the PicoMite's equivalent).
2. **Read and check the header:**
   - magic, kind, target and file size, giving "Not a loadable program" or "Not a program this firmware can load";
   - the API version, giving "Needs a newer firmware";
   - the PSRAM flag, giving "Needs PSRAM".
   - These are ordinary MMBasic errors, raised before anything changes, so `ON ERROR SKIP` catches them.
3. **Claim memory** (4.1). If it doesn't fit: "Needs n KB, n KB free in one block".
4. **Read the image** into place, then the relocations into the space where the stack or heap will go. Check the CRC-32 ("The program file is damaged"), apply the relocations, then clear .bss.
   - For B: and C:, build FatFs's fast-seek cluster map for files the app opens, as the CMM2 learned it had to (`ld_map`, Loader.c:157). Doom's start-up crawled without it.
5. **Build argv and the start block** (`svc`, `argc`, `argv`, `heap`, `heap_size`, `base`, and the PicoMite's `pm` pointer).
6. **Save the screen state** (mode, MAP palette, font, colours, framebuffer and layer state). Stop a running FRAMEBUFFER MERGE on core 1. Set `CursorSuspend` so `routinechecks` stops drawing the mouse pointer (PicoMite.c:911-927). Suspend the software WATCHDOG (D6).
7. **Set the flags:** `LoaderActive = 1`, `MMAbort = 0`.
8. **Call the app.** `code = LoaderCall(entry, &start, psp_top, psp_limit)`. This is the CMM2's naked routine (Loader.c:923-951) with PSPLIM added:
   - push r4-r11 and lr, and `vpush {d8-d15}` (valid on the M33's single-precision FPU, which has 16 double registers);
   - save MSP;
   - set PSP and **PSPLIM**;
   - set CONTROL.SPSEL;
   - call the entry.
   - `leave()` is a branch to `LoaderReturn`, which clears SPSEL and restores MSP.
   - Nothing in the firmware uses PSP or CONTROL today; only `sigbus` reads PSP, at PicoMite.c:2904.
9. **Afterwards:** `LoaderActive = 0`, then the tidy-up:
   - close the app's files and directory handles;
   - free its `alloc` blocks;
   - undo its pins, PWM, I2C, SPI and COM ports;
   - stop its sound;
   - put back the fonts;
   - restore the screen and the cursor;
   - free the claimed area and the stack;
   - discard keys typed while it ran (`ConsoleRxBufTail = ConsoleRxBufHead`);
   - print "x.app ended with code n" if n is not 0.
   - If Ctrl-C ended it, leave `MMAbort` set, so that a BASIC loop running apps stops too (a CMM2 lesson).

**Stack overflow.** PSPLIM makes it a UsageFault (STKOF), which escalates to HardFault and `sigbus`. That gives a report instead of silent heap corruption; the CMM2 has no check at all. Phase 12 can turn the report into a return to the prompt.

**Services run on the main stack.** Every service except the pure ones (4.4) is reached through a small veneer:
- it switches CONTROL.SPSEL to 0, so the service runs on MSP in SRAM, just below the loader's saved frame;
- it copies any stacked arguments across (at most 4 words: the largest services have 8 arguments);
- it calls the firmware function, then switches back to PSP.

Running services on MSP is required for four reasons:
- Firmware code then always runs on the stack it was written for, so `TestStackOverflow` (MMBasic.c:1480), which reads MSP, stays valid.
- A flash write inside a service (littlefs on A:, `SaveOptions`) switches off the whole XIP window, PSRAM included (FileIO.c:381-404). The service's own stack must therefore not be in PSRAM.
- An MMBasic `longjmp(mark)` must never land while SPSEL is 1. The veneer's error trap (4.5) guarantees that it never gets that far.
- The cost is a few cycles per call.

### 4.4 The services

The rule is the CMM2's: no service raises an MMBasic error. Each returns a status, negative on error, and the message is available from `io_error()`. The table only grows, and its version rises with each addition.

| Group | Services (CMM2 names) | PicoMite implementation | Phase |
|---|---|---|---|
| Console | con_putc, con_puts, con_getc, con_screen | Output as PRINT does: serial plus the screen console. `con_screen(0)` sends it to serial only. `con_getc` pops `ConsoleRxBuf` directly, not through `MMInkey`, which calls `CheckAbort` (PicoMite.c:1214). | 1 |
| Time | micros, delay_us, datetime | `time_us_64()`. `delay_us` waits in chunks of at most 10 ms, calling the poll body between them. The RTC fields MMBasic keeps. | 1 |
| System | poll, leave, cpu_hz, break_take, alloc, release | `poll` (4.5). `alloc` is `GetMemoryNull`, tracked (64 blocks) and freed at the end. | 1 |
| Pure helpers | mem_copy, mem_fill, mem_move, libm | Called directly, without the veneer (they touch neither flash nor MMBasic state): `memcpy`, `memset`, `memmove`, and libm from the firmware's DCP-accelerated `__wrap_` functions. | 1 / 3 |
| Files | file_open … file_truncate, file_stat, file_remove, file_rename | `BasicFileOpen`, `FileGetData`, `FilePutData`, `positionfile`, `filegetpos`, `ForceFileClose`, `FileRename` (FileIO.c; section 10), each under the error trap. Up to 8 handles, from `FindFreeFileNbr` (`MAXOPENFILES` is 10). Drives A:, B: and C:. **Set `cmdline` to " " first**: `getfullfilename` parses a drive prefix only when `*cmdline != 0` (FileIO.c:5912-5916; `ScreenShotSave` does the same at 8634). | 3 |
| Directories | dir_make, dir_open, dir_read, dir_close, dir_change, dir_current | A new FileIO.c helper. Listings exist only inline today (`fun_dir` 2515-2670, FILES, the file manager). Use `f_opendir`/`f_readdir` on B: and C:, and `lfs_dir_reopen`/`lfs_dir_read` on A:. `chdir` (2743) and `GetCWD` (1843). | 3 |
| Video | video_mode, video_scaled, video_palette, video_wait | `setmode(mode, clear)` (Draw.c:4932) with **PicoMite MODE numbers**; the depth argument must match the mode (MODE 5 is 8 bpp, MODE 4 is 16). The palette is `map256` (MODE 5) or `map16` (MODE 2 and 3), as MAP sets them. `video_wait` spins on `v_scanline` (HDMI.c:93) or `QVgaScanLine` (VGA.c:116). The returned `cmm2_video` has `fb = WriteBuf`, width, height, pitch and bpp. See 4.7. | 5 |
| Drawing | gfx_cls … gfx_save, gfx_target, font_size, font_define | MMBasic's own routines: `DrawRectangle`, `DrawLine`, `DrawCircle`, `DrawTriangle`, `DrawBuffer` and `ReadBuffer` (RGB888), text, and `FontTable`. Colours are RGB888 as MMBasic takes them. | 5 |
| DMA2D | dma2d_copy, dma2d_copy_nc, dma2d_busy | Row-by-row copies (no 2D DMA on the RP2350); busy always returns 0. They keep the CMM2 ports' calls working. | 5 |
| Input | keys_held, keys_raw, mouse, mouse_set, gamepad, gamepad_axes | Keys come from `KeyDown[7]` (KeyboardMap.c:686) and `PS2KeyDown[7]` (Keyboard.c:98; the PicoCalc keyboard writes it too). `keys_raw` needs the last USB HID report kept. Mouse and gamepads come from `nunstruct[]` (I2C.h:122-144). | 4 |
| Sound | audio_start, audio_stop, audio_lock, audio_unlock, sound_state | See 4.8. | 6 |
| OPL and music | opl_*, music_* | **Not provided.** Each returns NULL or -1, with `io_error()` saying "Not on the PicoMite". | — |
| I/O | pin_setup … pwm_stop, i2c_*, spi_*, com_*, com_connected, io_error | `ExtCfg`, `ExtSet` and `ExtInp`, so the E9 workaround is kept. `setpwm`/`PWMoff`; `i2c_enable`/`i2c_masterCommand`; the SDK's spi calls on the SETPIN pins; `SerialOpen("COMn:...")`, `SerialPutchar`, `SerialGetchar`, `SerialClose`. Pin numbers are GP numbers, as the CMM2 already defines them. | 7 |
| BASIC | basic_cmd, basic_eval, basic_prep, basic_exec | Optional (D11). Under the CMM2's `ld_interp` pattern (save the token buffer and interpreter pointers), with the PicoMite's tokens. | 8 |

**PicoMite-only services:**
- `psram_size`;
- `sram_take(size)`, an SRAM block for the run (the `--sram` support);
- `lcd_dirty(x1, y1, x2, y2)` for buffered SPI LCDs (4.7);
- `lcd_write(x, y, w, h, rgb565)`, a faster unbuffered-LCD write than RGB888 `DrawBuffer`.

### 4.5 Errors, Ctrl-C and polling

**The error trap.** Each service body runs under `ld_try`, the CMM2's `ld_try` (Loader.c:664) or the PicoMite's own `FileTry` (FileIO.c:8552-8577):
- it sets `OptionErrorSkip = 1` (and `OptionFileErrorAbort = 0` for files) and calls `setjmp(ErrNext)`;
- an MMBasic error inside the service then comes back as a status, with `MMErrMsg` copied for `io_error()`;
- it restores both settings afterwards.

**Routines that can longjmp** and therefore need the trap (from the survey):
- `getConsole` and `MMInkey`, through `CheckAbort`;
- `SerialPutchar` when `MMAbort` is set;
- `GetMemory` when memory runs out;
- file errors, through `ErrorCheck`;
- `ProcessWeb`;
- `routinechecks` itself: audio file reads, and the I2C keyboard error at I2C.c:716.

**CheckAbort** (PicoMite.c:2787-2822) runs `do_end(false)` and `longjmp(mark)` when `MMAbort` is set. While `LoaderActive`, it returns without doing that. This is the CMM2's `main.c:3232` fix ("cleanend from inside a service left its stack"). `poll()` reports the Ctrl-C to the app instead.

**`poll()`** returns `MMAbort != 0`. Under the trap, it calls:
- `routinechecks()`: USB host `tuh_task`, the HID task, the CDC console, GPS, the PicoCalc keyboard, the NEXTGEN LCD refresh, the 1 ms timer re-sync;
- `ProcessBT()` on Bluetooth builds;
- `ProcessWeb(1)` on WEB builds;
- `stepper_poll_events()`.

It must **not** call `check_interrupt()` (MM_Misc.c:11058), so BASIC interrupts never run while an app has the processor. ISRs keep counting events through `IntSignal`, so pending interrupts fire after the app returns to a program.

**Ctrl-C and `break_take`.** Ctrl-C reaches `MMAbort` from every console source: the UART IRQs, USB CDC in `routinechecks`, the HID and BLE keyboards in `tuh_task`, PS/2, the I2C and PicoCalc keyboards, the keypad and telnet. `break_take()` returns 1 once per Ctrl-C and clears it, for apps such as MicroPython that turn Ctrl-C into their own exception.

**Apps that don't poll.** The serial console's Ctrl-C is seen at once. USB keyboards, Bluetooth and WiFi move only while the app polls, as on the CMM2. An app that never polls can only be stopped by a reset, until Phase 12.

### 4.6 USER modules

These follow the CMM2's `Src/User.c` and Loader.c:1678-1839.
- **Commands.** `USER LOAD file$`, `USER CLOSE`, `USER anything` and `USER(anything)` are new entries in AllCommands.h. Commands go before the dummy entry (AllCommands.h:724) and the function goes in the token table.
  - Before adding the USER command, check the name rules from issue #149 (command names used as variable names).
- **Placement.** The module's code goes into the PSRAM heap if there is PSRAM, otherwise the SRAM heap, at any address (4.2). Its malloc heap comes from the ordinary heap (64 KB by default; `--heap` sets it).
- **The stack.** Module commands and functions run on **MMBasic's stack**, as CSUBs do (`CallCFunction`, CFunction.c:68-190). That is what lets them call the interpreter. The rule is the same as on the CMM2: no large local buffers; use `temp()` or malloc.
- **The interpreter's routines** (`struct cmm2_basic` in the CMM2's Inc/cmm2user.h:72-119), mapped to the PicoMite:
  - `checkstring`, `close_bracket`, `evaluate`, `getinteger`, `getint`, `getnumber`, `getstring`, `getCstring`, `GetTempMemory`, `error`, `CheckAbort`, `MMPrintString`, `IntToStr`, `FloatToStr`, `CtoM`, `MtoC` and OPTION BASE map directly.
  - `getargs` is a macro on the PicoMite (MMBasic.h:255-273), so it needs a function wrapper.
  - `var()` and `assign()` must translate between `cmm2_var` and the compact variable table (`g_slotrec`).
  - `MMFLOAT` is `double` (configuration.h:568), as the CMM2's `User.c` asserts.
  - `vsnprintf`: **check `%g`**, because the SDK's printf has a known broken `%g` (`project_pico_printf_g_broken`). If it is still broken, the module's float formatting goes through `FloatToStr`.
- **Lifetime.**
  - Anything that wipes the heap (RUN, NEW, CHAIN, LOAD, EDIT, the file manager) calls the module's `forget()` and drops it. The hook goes in `ClearRuntime` (MMBasic.c:7886).
  - The next `USER` reloads the module from its remembered full path.
  - Files the module opened belong to it and are closed when it goes.
  - Errors save and restore `mark` and `ErrNext`, as the CMM2's `usercall` does (User.c:187-210).

### 4.7 Display

The PicoMite has three kinds of display, and the services cover them differently.
- **HDMI and VGA builds: framebuffers in SRAM.**
  - At 640x480, MODE 5 is 320x240 at 8 bits through the `map256` RGB555 palette (Draw.c:4622, HDMI.c:81). That is Doom's and ScummVM's format (320x200 in 256 colours), letterboxed.
  - MODE 2 is 4 bits through `map16`; MODE 4 is RGB555.
  - At 1280x720, 1024x768 and 1024x600, MODE 5 shows each byte directly as RGB332. `video_palette` then returns -1, and the app converts its palette to RGB332 itself.
  - The framebuffer pool is 153,600 bytes on HDMI, HDMIUSB, PC3 and VGA RP2350, and 96,000 on HDMIBTH and HDMIWEB.
- **Unbuffered SPI LCDs** (the PicoCalc preset, ST7365P) have no framebuffer.
  - The app renders into a buffer of its own and pushes rectangles with `gfx_write` (RGB888 `DrawBufferSPI`) or with the faster `lcd_write`.
  - The SPI bus limits the frame rate. `video_mode` returns -1.
- **Buffered SPI LCDs (NEXTGEN, types ≥ 80)** keep an RGB332 framebuffer in the heap, which core 1 copies to the panel.
  - The app writes pixels and calls `lcd_dirty` to mark the changed rectangle. `poll()` sends core 1 its refresh command (PicoMite.c:1114-1146).
- **No frame hook.** There is no vsync interrupt. `video_wait` polls the scanline. A frame hook on core 1 is rejected, because the app's code isn't re-entrant.
- **Restore at the end:** the mode, MAP, font, colours, the cursor, and any framebuffer or layer.

### 4.8 Sound

The PicoMite plays sound from the PWM wrap interrupt, one sample per interrupt at priority 255, with no DMA (SDCard.c:405-575). It plays from two swing buffers that `PLAY ARRAY` fills (Audio.c:1743-1811; refilled in `checkWAVinput`, 3603).

**The app's sound** uses the same swing buffers:
1. `audio_start(fill)` allocates the two buffers and returns the rate (44,100). *As built: 1024 frames each (4 KB, 23 ms), the CMM2's size rather than `WAV_BUFFER_SIZE`, in the SRAM heap; both are made by `fill` before `audio_start` returns.*
2. When `on_pwm_wrap` finishes a buffer, it pends a **spare user interrupt** (the RP2350's `SPARE_IRQ_n`, claimed with `user_irq_claim_unused`) at the lowest priority.
3. That interrupt's handler calls the app's `fill(int16_t *buf, frames)`, then converts the result to 12-bit unsigned for PWM (`iconvert`) or signed 16-bit for I2S (`i2sconvert`).
4. While the app's sound runs, the wrap interrupt is raised above the refill interrupt's priority (D8), so a slow fill cannot starve it.

**Locking and stopping.** `audio_lock()` and `audio_unlock()` disable and enable that one spare interrupt in the NVIC. `audio_stop` and the tidy-up call `CloseAudio(1)`.

**What runs where.** The refill runs on MSP, in handler mode, executing the app's fill from PSRAM. That is safe, because flash writes happen with interrupts off. A refill delayed by a flash write to A: is a glitch; BASIC's PLAY has the same glitch today.

**Not used:**
- PendSV and SVC: they go to `sigbus` (PicoMite.c:4251-4255).
- The CMM2's PendSV-based refill does not port.

### 4.9 Floating point

**ABI.** The firmware is built with `-mfloat-abi=softfp` (`-march=armv8-m.main+fp+dsp`, confirmed in the build tree). Apps must use the same ABI, because float and double arguments cross the service table: `gfx_circle`'s aspect, libm, and the USER module's `get_number`, `float_to_str` and `cmm2_value.f`.

**Doubles.** The firmware uses the SDK's DCP-accelerated doubles (`pico_set_double_implementation(PicoMite pico)`, CMake 776-785) and VFP singles.
- An app built from the toolchain's libraries would get libgcc's software doubles, which are much slower.
- The SDK therefore ships `fwdouble.S`: `__aeabi_dadd`, `__aeabi_dmul` and the rest, `sqrt`, `sin`, `pow`... These are tail-calls through the table to the firmware's routines (the `__wrap_` versions), with no veneer, because they are pure.
- Alternatively, the app can link the SDK's `pico_double` sources itself (BSD-3).
- To check: whether any firmware interrupt handler uses the DCP. If one does, an app's double operation could be interrupted mid-sequence. The firmware's own main-line code has the same exposure today, so this is a check rather than a new risk.

**Precision.** The M33's FPU is single-precision. The CMM2's `fwlibm.c` inlines `vsqrt.f64` and similar instructions, so it does not carry over.

### 4.10 Firmware cost and enabling per variant

**The CMM2's cost.** `Loader.c.obj` on the CMM2 is 15,170 bytes of text and 2,318 of BSS on the M7 (from its Release map; that includes modules and the extra services). The PicoMite's should be of the same order, about 12-18 KB of flash.

**SRAM.** The PC3 has 1,996 bytes of SRAM margin (measured 16 November; the 4.2 KB of 4 November is out of date). So the loader's state lives in a block allocated at RUN or USER LOAD time; only a pointer and a few flags go in BSS. The CMM2's largest BSS items (the 1 KB saved CLUT, the module slots, the block table) all move into that block.

**Flash.** Measured 16 November (section 10): the PC3 has 13.0 KB at 1760 KB. That is likely enough for Phase 1's smallest loader but not for the full 12-18 KB estimate, so expect one step to 1776 KB before modules and the extra services. If the loader doesn't fit, the variant moves one 16 KB step (`project_flash_target_offset_alignment`), which reformats A: on that board. *Moved to 1776 KB on 22 November, in Phase 7. On the PC3 (16 MB, the fixed map) A: is kept; the options and flash slots reset.*

**Enabling a variant** follows the existing per-feature pattern (RAYCASTER, CMake 242-254, 336, 660-662, 832-834):
1. add the variant to a `LOADER_VARIANTS` list;
2. use `compile_is_in` → `target_sources` + `target_compile_definitions(LOADER)`;
3. add the `#ifdef LOADER` command entries.

**Options.** No new OPTION is needed. If one ever is, it takes a byte from `extensions[]`, as OPTION RADIO did, because the option struct is fixed at 896 bytes and must never grow.

**Interpreter speed.** The hooks are a flag test in `CheckAbort` and a branch in `do_run`. Neither is on a per-statement path, so interpreter speed is unaffected. Even so, solar_eclipse is benchmarked before and after, on the same board.

### 4.11 Hazards specific to the PicoMite and the RP2350

1. **Flash writes switch off the XIP window, PSRAM included.** This is why services run on MSP and the stack defaults to SRAM (4.3).
   - Interrupts are off during a flash write, so nothing stacks onto PSP then.
   - To verify on a board: an app writes a 1 MB PSRAM buffer to A:.
2. **Hidden longjmps.** See 4.5. Any path not trapped would land with SPSEL = 1 and corrupt both stacks.
3. **`getfullfilename` and `cmdline`.** See 4.4 (Files).
4. **`sigbus` owns SVC, PendSV, SysTick, NMI and HardFault.** Apps may not use SVC or PendSV.
5. **`routinechecks` draws the mouse cursor** on VGA and GUICONTROLS builds. It is held off with `CursorSuspend` (`project_routinechecks_in_display_loops`).
6. **Core 1** runs video (HDMI DMA_IRQ_0, the VGA scanout, `UpdateCore`, the NEXTGEN copy). Apps must not touch DMA channels, PIO or core 1, except through services.
7. **The watchdogs.** The software WATCHDOG is counted in `timer_callback` (D6). `WATCHDOG HW` is turned off by `do_end`; it is stopped while an app runs.
8. **`error()` calls `ReloadOptionsKeepLive()`** before reporting. An app never changes `Option`; the services must not either.
9. **The release pre-flight.** It must never see branch test builds, and must never see this plan untracked on main at release time.
10. **USB host polling.** `tuh_task` runs only from `poll()`. The CMM2 found 3-10 s without polling harmless (`nopoll.c`). The USB C: drive's handler race (`project_usb_c_drive_handler_race`) applies when an app reads C: while it polls.
11. **The CMM2 dialect.** `FileLoadCMM2Program` and `RUN` in CMM2 mode (Commands.c:2318) are unaffected. An `.app` is diverted before both.

---

## 5. The SDK (`tools/sdk/`)

The SDK is a copy of the CMM2's `tools/sdk`, retargeted.

**`build.py`:**
- Flags: `-mcpu=cortex-m33 -mthumb -mfloat-abi=softfp -mfpu=fpv5-sp-d16 -O2 -ffunction-sections -fdata-sections -fno-exceptions -fno-unwind-tables`. C++ adds `-fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit`, as on the CMM2.
- The default base is 0x11000000.
- Kept: `--stack`, `--heap`, `--api`, `--module`, `--user`, `--libc nano|full`, `--no-printf-float`, `--cflag`, `--cxxflag`, `--ldflag`, `--sources`. Objects stay named by path hash, because ScummVM's engines share file names.
- Dropped: `--g2`, `--vram`, `--axi`, `--itcm`, `--xip`.
- Added: `--psram` (sets the "needs PSRAM" flag) and later `--sram NAME`.
- The toolchain is GCC 13.3 (`Arm GNU Toolchain 13.3.Rel1`), the firmware's own. The multilib `thumb/v8-m.main+fp/softfp` is present.

**`app.ld`:** the CMM2's single-area script without `.itcm` and `.axi`; a `.sram` output section is added in Phase 11.

**`crt0.c`:** `_start` as on the CMM2: bind the services, run the constructors, call `main`, run the destructors, `exit`. The axi functions are dropped. The globals are named `cmm2` and `cmm2_start` (source compatibility, D1), with `pm` and `pm_start` as aliases.

**`syscalls.c`:** unchanged, apart from the drive rules:
- the newlib glue (console and file descriptors, termios, `_sbrk` and `cmm2_heap_take`, stat, dirent, time, `usleep` with polling);
- errno set in both places (a CMM2 lesson).

**`userlib.c`:** printf for modules through the firmware's formatter (subject to the `%g` check in 4.6).

**`fwdouble.S`:** see 4.9.

**`mkimage.py`:**
- the PicoMite magic and target;
- word relocations at any delta; MOVT accepted, but it sets flag bit 2;
- the alignment byte;
- the CRC as on the CMM2;
- every other relocation type still fails the build.

**Headers:** `pmimg.h` (the image format and the services; firmware copy in `loader/`), `pmuser.h`, `cmm2.h` and `cmm2user.h` (compatibility names), plus the CMM2's `time.h`, `sys/dirent.h` and `sys/termios.h`.

**Examples:** `hello`, `filetest`, `keytest`, `vidtest`, `sndtest`, `iotest`, `user/proforma.c`.

**Relocation testing.** A copy of each test app linked with `--base 0x11400000`, so that the relocation path runs on every test (the CMM2 does the same with 0x60000000).

**Do not put the firmware's include directories on the SDK's include path.** Windows ignores case, so the firmware's `Audio.h` was picked up in place of frank-quest's `audio.h`.

---

## 6. Phases

Each phase is a series of small changes, each built and run on a board before the next.

### Phase 0: set-up (no firmware change)

**Work:**
1. Peter rules on section 2. *Done 16 November: all recommendations accepted.*
2. Create the `loader` branch and worktree (section 3) and commit this plan. *Done 16 November* (the branch is now `v8`).
3. With Peter's OK, measure the flash and SRAM margins of PC3, HDMIUSB, HDMI, VGAUSBRP2350 and PICORP2350 from the branch head (private Ninja builds and `GetHighestHexAddress.py`). *Done 16 November: see section 10.*
4. Record the solar_eclipse baseline on the PC3 test board. *Done 16 November* on the COM17 PC3 (192.168.1.251), at 315 MHz, OPTION COMPILE ON, MODE 1, `A:/g/solar_eclipse.bas` with the standard five answers. The branch's PC3 build at 74f51e3 (main 544be7b's code, flashed over SWD) gave 5.988, 6.021, 5.989, 6.064 and 5.988 s: **median 5.989 s, minimum 5.988 s**. RC9 on the same board just before gave 5.997, 6.050, 6.063, 6.028 and 6.044 s (median 6.044 s). Runs on one image spread by about 1%, with a floor that repeats to 1 ms, so compare minimums. Later phases are measured against 5.988 s on this board with these settings.

**Exit:** the decisions are recorded and the margins are known.

### Phase 1: the smallest loader, PC3 only

**Work:**
- `LOADER` for PC3 in CMake.
- `misc/Loader.c` with:
  - header checks and the CRC;
  - PSRAM placement at the link base only;
  - the stack in SRAM;
  - `LoaderCall` with PSPLIM;
  - the veneer;
  - the error trap;
  - the `CheckAbort` guard.
- Services: console, time, poll, leave, cpu_hz, break_take, alloc, release, and the pure helpers.
- The `do_run` hook.
- The minimum tidy-up: blocks, keys, the code message.
- SDK: build.py, mkimage.py, app.ld, crt0.c, syscalls.c (console and exit), hello.

**Tests:**
- `hello.app` prints its argv and exits with 0 and with 3.
- Ctrl-C ends a looping app, and also stops a BASIC loop that runs it.
- Each refusal: a CMM2 `.app` file, a renamed text file, a truncated file, a corrupted byte (the CRC), a too-new API.
- Run from a program: the program continues, with its variables intact.
- `MM.INFO(HEAP)` and the PSRAM free figure are the same before and after.
- A BASIC loop running `hello.app` 200 times.

**Exit:** all of that passes on the PC3, and goldens are clean.

### Phase 2: placement, relocation, the other entry points

**Work:**
- Relocation (ABS32 and TARGET1 at any delta; MOVT in 64 KB steps).
- `LoaderClaimPS` and `LoaderClaimSRAM` in Memory.c.
- The bottom of the PSRAM in use: relocate.
- No PSRAM: SRAM-only placement.
- `--stack`, `heap_min`, the "Needs n KB" errors.
- The CHAIN, file manager, EXECUTE and `*name` entry points.
- WATCHDOG suspension.

**Tests:**
- The `--base 0x11400000` copy of every test.
- A BASIC program that fills the bottom of the PSRAM heap with arrays and then runs an app.
- A deliberate stack overflow, which `sigbus` must report with a PSP PC inside the app.
- An RP2350 board without PSRAM, running a small app.
- `MM.INFO` afterwards.

### Phase 3: files, directories, the C library, doubles

**Work:**
- The file and directory services, with handle ownership.
- The new directory-listing helper in FileIO.c.
- The fast-seek map.
- The syscalls file part.
- `fwdouble.S`.

**Tests:**
- `filetest.app`: create, write, seek, append, read back, stat, truncate, rename, remove, mkdir and list on A:, B: and C:, with results compared against the same operations in BASIC.
- A file left open, which is closed at the end.
- **A 1 MB PSRAM buffer written to A:** (the XIP hazard).
- Double-heavy loops timed against BASIC and against libgcc's soft-float.

### Phase 4: input

**Work:** the input services. `poll()` gets its full body (routinechecks, BT, WEB, stepper), and `CursorSuspend` is held while the app runs.

**Tests:**
- `keytest.app` with a USB keyboard, a PS/2 keyboard, a Bluetooth keyboard (PC3), a USB mouse and a gamepad.
- Ctrl-C from each keyboard.
- An app that does not poll for 10 s: USB must still be alive afterwards.

### Phase 5: display

**Work:**
- `video_mode`, `video_palette` and `video_wait` on HDMI and VGA.
- `gfx_*` and the DMA2D stand-ins.
- Saving and restoring the screen.

**Tests:**
- `vidtest.app` in MODE 5 with palette cycling, MODE 4 and MODE 2.
- The 720p RGB332 path.
- The screen state after return, at the prompt and inside a program.
- The FRAMEBUFFER MERGE and layers state before and after.

### Phase 6: sound

**Work:**
- `audio_start` and the rest on PWM and I2S.
- The spare-interrupt refill and the priority change (D8).

**Tests:**
- `sndtest.app`: a sine tone, an 8-voice mix, lock and unlock under load.
- Sound during reads from B: (expected clean) and writes to A: (expected short glitches).
- Sound stopped at the end and after Ctrl-C.
- BASIC's PLAY works normally afterwards.

*Done 22 November* on the COM10 PC3 (I2S, headphones). Built as section 4.8 describes: a new playing state `P_APP`; `advance_swing_buffer` (SDCard.c) pends the claimed spare interrupt on each change of buffer; its handler (`AppSoundRefill`, Audio.c) has `fill` make the buffer just played and converts it at PLAY VOLUME's level. The wrap interrupt goes from 0xF0 to 0xE0 while the program's sound plays, and back at `audio_stop`. `audio_lock`/`audio_unlock` disable and enable the spare interrupt, and a lock is kept across `audio_start` (as the CMM2's BASEPRI). `audio_start` returns -1 (no output for it: no OPTION AUDIO, a VS1053), -2 (already playing) or -3 (no room). The loader stops the sound when a program ends, before its memory (which holds `fill`) is freed. `sound_state` is left for Phase 9: until a program can start BASIC's PLAY there is nothing for it to report.

`tools/sdk/examples/sndtest.c` measures the fills a second (43.07 expected) and the longest gap between fills (23.2 ms; over 40 ms means a buffer played twice):
- a scale, left and right a fifth apart, the note changed under `audio_lock`: 43.4 a second, longest gap 23.3 ms. Peter: clean.
- under load (sums, `poll` every 10 ms, 5 s): 43.3, 23.3 ms.
- 1 MB written to B: and read back: 43.3 and 43.4, 23.3 ms; data PASS.
- 1 MB written to A:: 2.8 a second, gaps up to 407 ms (the flash writes have the interrupts off: BASIC's PLAY has the same); read back at 43.6.
- `audio_start` twice: -2; after `audio_stop` it starts again.
- Ending without `audio_stop`, and Ctrl-C from `poll()`: the sound stops; `MM.INFO(SOUND)` is OFF and `PLAY TONE` works afterwards.

Not yet tested: the PWM output, an 8-voice mix, and the NES with sound by ear (Peter was away: "assume all OK").

### Phase 7: I/O

**Work:** the pin, ADC, PWM, I2C, SPI and COM services, with their tidy-up.

**Tests:**
- `iotest.app`: a loop-back on COM1, I2C to the RTC, SPI to a known device, PWM measured with a pin read.
- System pins are refused.
- Everything is unconfigured afterwards. `MM.INFO(PINS)` shows it.

*Done 22 November* on the COM10 PC3, with Peter's rulings: a bus's pins are the ones SETPIN or OPTION SYSTEM gave it, or the ones a new PicoMite-only service, `bus_pins(kind, bus, a, b, c)`, names (it runs SETPIN); `com_open` uses OPEN "COMn:"'s numbers (3-6 the USB serial devices, not the CMM2's 4-7); `mouse`, `mouse_set` and `gamepad_axes` came with it. As on the CMM2, SETPIN, PWM, I2C OPEN, SPI OPEN and OPEN "COMn:" run as the interpreter runs them, their arguments as text, under `ld_try`; the transfers are the SDK's calls. A GP the board keeps (`IsInvalidPin`: the PC3's HDMI pins) is -1. An I2C bus already open (OPTION SYSTEM I2C's, or BASIC's) is used as it is and left open; an SPI bus already open is -2. Open drain is an input that `pin_write(0)` drives low. At the end `ld_iotidy` stops the program's PWM, closes its buses and ports (their pins then unconfigured, as I2C CLOSE leaves them) and unconfigures the pins it set up.

The services cost about 4.5 KB at `-O2`, 1.2 KB more than the PC3 had; Loader.c at `-Os` saves 1.6 KB.

`tools/sdk/examples/iotest.c`, with nothing wired: 60 checks pass. System pins (the console's GP8, the PSRAM's GP47, HDMI's GP14) refused; pulls, an output and open drain read back on GP5; PWM at 25% sampled on its own pin (24-25%), unchanged when the slice's other output is set to 75%; an analog input on GP40; the DS3231 on the system bus read against `datetime` (to the second), a NACK from an empty address, and the system bus left open by `i2c_close`; I2C2 on GP6/GP7 and SPI on GP2-GP4 opened, used (NACKs; transfers in all four forms) and closed; COM1 on GP1/GP0 opened, written and closed; COM2 (the console) refused with the firmware's message; `com_connected` 1 for COM3 (a USB serial device on that board) and 0 for COM4. `iotest.app leave` ends with PWM, SPI, I2C2, COM1 and an output all set up: afterwards `MM.INFO(PIN)` shows GP0-GP7 and GP40 OFF, and BASIC opens SPI, I2C2 and COM1 on those pins again. `sndtest`, `filetest` and `hello` run as before.

Not yet tested: COM1 and PWM through a wire (`iotest.app loop`, GP0 to GP1: the COM17 board has it), a USB serial port written to, an SPI device, the mouse and a pad's axes with the devices plugged in.

### Phase 8 (optional, D11): calling BASIC

**Work:** `basic_cmd`, `basic_eval`, `basic_prep` and `basic_exec` under `ld_interp`.

### Phase 9: USER modules

**Work:**
- The USER command and function.
- Module loading, placement, the `forget` hook in `ClearRuntime`, and lazy reload.
- The interpreter routine table, including the `getargs` wrapper, `var()` and `assign()`.
- The SDK's `--user` and `proforma.c`.

**Tests:**
- The pro-forma's eight commands and functions: HELLO, SWAP, FILL, TOTAL, ADD, REVERSE$, FIXED$, CALLS.
- An error inside the module, caught by `ON ERROR SKIP`.
- `USER(` used inside a USER command's arguments.
- Reload after RUN, NEW, CHAIN, EDIT and the file manager.
- A module on a board without PSRAM.
- Goldens with a module loaded.

*Done 22 November* on the COM10 PC3. As 4.6 and the CMM2's Src/User.c, almost line for line: `misc/User.c` (USER LOAD, USER CLOSE, USER ..., USER(...), the interpreter's routines as `loader/pmuser.h`'s `struct cmm2_basic`, `usercall` saving and restoring `mark` and `ErrNext`), `LoaderModuleLoad`, `LoaderModuleDrop` and `LoaderModuleTable` in Loader.c, "User" and "User(" in AllCommands.h. As built:
- The module's image and .bss are one block of the heap (GetMemoryNull: the SRAM heap for a small one), its C library's heap another (64 KB unless `--heap`). It has no stack: its start, like its functions, runs on MMBasic's.
- The services' state for a module (files, blocks, io_error's message, its start block) is a block of its own, `ldmod`, which is `ld` whenever no program runs; `LoaderRun` keeps it and puts it back, so a module survives a program run.
- The hook is at the top of `InitHeap` (as the CMM2's HeapWipe), not in ClearRuntime: every heap wipe goes through it (RUN, NEW, LOAD, CHAIN, EDIT, the file manager, and FILES in a program). The module's `forget()` runs under `ld_try`, then the program end's tidy-up (`ld_tidy`: sound, I/O, files, directories, blocks).
- Three loader changes a module needed: the veneer passes a call made on the main stack straight to the service (it used to switch to the process stack on the way out); `leave()` from a module is MMBasic's error "A USER module cannot leave()"; `poll()` in a module calls CheckAbort outside `ld_try`, so Ctrl-C breaks the statement and ON ERROR SKIP's state is not left on at the prompt.
- USER LOAD keeps the full path (`getfullfilename` and the drive letter), so a reload after CHDIR or DRIVE finds the same file.
- `vsnprintf` is the firmware's, the Pico SDK's compact printf: measured on the board through `modtest`'s FMT$, `%f` is right but `%e` and `%g` of 1e6 give 10.000000e+05, and `%g` of 0.5 gives 0.500000. pmuser.h, userlib.c and the README say to format those with `float_to_str`.
- SDK: `build.py --user` (a module with `userlib.c`'s printf family and no newlib float printf), `tools/sdk/cmm2user.h`, `examples/user/proforma.c` (3.1 KB) and `examples/user/modtest.c`.

Tests on COM10, all as expected: the pro-forma's eight (HELLO with and without a name; SWAP of numbers and of strings; FILL of an integer and a float array; TOTAL; ADD giving an integer or a float; REVERSE$; FIXED$; CALLS); `USER(` inside USER FILL's arguments and inside a FUNCTION; errors at the prompt (Unknown USER command / function) and in a program under ON ERROR SKIP (MM.ERRNO 16 and the module's message), and an error inside the module reported at the program's line; reload after RUN, NEW, LOAD, CHAIN, EDIT and the file manager (CALLS back to 1); USER CLOSE, then "USER LOAD first"; `modtest`: con_puts, micros, alloc, a file on A: written, read back and removed, `leave()` refused, Ctrl-C through `poll()` with errors reported normally after it, a file the module left open closed by USER CLOSE; a module kept across an app run; iotest, sndtest, filetest and hello unchanged after the veneer change. Goldens (compiled, `goldens_pc3_v8_phase9_compiled`) against V7.00.00's: the same counts (MATCH 115, DIFF 20, ERROR 66); changed only the timing programs and `i2c0`, whose "nothing at &h76" has a device there on COM10's board (a scan finds 0x68 and 0x76). The module load before the run collided with the run's start, so the run began with none loaded; the heap wipe with a module loaded is covered by the reload tests above. Afterwards the DS3231's registers were put back (14 = 28, 15 = 8: rtcreg.bas clears EN32kHz).

Not tested: a board without PSRAM (the loader is only in the PC3 build).

### Phase 10: first ports, and the go/no-go measurement

**Doom (doomgeneric)** is first:
- It needs a PicoMite platform layer: MODE 5, `map256`, keys, effects through `audio_start`, and no music unless the app carries a synth.
- Measure the timedemo (demo1) frame rate, silent and with effects.
- For comparison, the CMM2 Gen 1 at 400 MHz gives 77.8 fps silent and 33.2 with sound; the 4 November estimate for the PC3 was about 25-40 fps, which is unmeasured.
- **This is the decision point for the program half.**

**Then:**
- **MicroPython**, from the CMM2's `ports/cmm2`, compared with the PC3's native MicroPython.
- **A SCUMM-only ScummVM image** of about 1.5 MB. The full 3 MB image would leave about 2.5 MB of heap on the PC3.

**Results** go in `Bas/bench/results/loader_<date>/`.

### Phase 11: more variants, `--sram`

**More variants:** enable them one at a time (D3), each after a flash check.
- In order: HDMIUSB (Pico Plus 2), HDMI, VGAUSBRP2350 and VGARP2350, PICORP2350 and PICOUSBRP2350 (the LCD paths of 4.7), WEBRP2350 and HDMIWEB (`ProcessWeb` in `poll`), then the Bluetooth builds.
- Any offset move is recorded for the release notes' A: box.

**`--sram`:**
- A second linked segment at its own base.
- mkimage tags each relocation with the segment its *target* belongs to, so the loader applies the right delta.
- crt0 copies the segment into the block from `sram_take`.

### Phase 12 (D10): fault recovery and forced abort

**Fault recovery.** In `sigbus_c`, when `LoaderActive` is set, the fault came from thread mode on PSP and no service is running:
1. Rebuild an exception frame on the app's stack with the PC set to a recovery routine.
2. Clear the fault status.
3. Return. The recovery routine takes the `LoaderReturn` path with a fault code, the tidy-up runs, and the message is "x.app faulted at +0x1234".

**Forced abort.** Three Ctrl-Cs within a second, while the app is not inside a service, force the same path from the interrupt. That makes an app that never polls stoppable.

**Tests:** a null pointer, an undefined instruction, a stack overflow, and an infinite loop without poll.

### Phase 13: documentation

**Work:**
- *PicoMite Loadable Programs*, from the CMM2's text with the PicoMite's differences, generated to PDF by the existing pipeline.
- Manual entries for RUN (.app) and USER, and the help files (`gen_help.py`, both forms).
- Release-notes sections.
- A Performance Guide section.

**Tests:**
- Every example in the documents is run on a board.
- Full goldens on the PC3 and on one other enabled variant.

### Phase 14: merge into main

Section 3's merge-back, on Peter's go.

---

## 7. Testing summary

**Per phase:** the tests listed in section 6, each new change run on a board before the next.

**Regression:**
- Goldens on the PC3 about every five changes.
- `stdset` and the bench set on the PC3 before Phase 10 and before the merge.
- solar_eclipse at the same clock before and after, to confirm the hooks cost nothing.

**A standing app test set** in `Testfiles/loader/` (BASIC drivers) and `tools/sdk/examples/` (apps):
- hello, its relocated copy, filetest, keytest, vidtest, sndtest, iotest, proforma.mod;
- an `apps.bas` that runs them all and checks their exit codes;
- the app files are sent to A: by ZMODEM (`feedback_use_zmodem_for_transfers`).

**Errors:** every message in 4.3 and 4.6 is produced at least once and caught by `ON ERROR SKIP`.

---

## 8. Risks

| Risk | Effect | Mitigation |
|---|---|---|
| PC3 flash too small for the loader | A: moves on PC3s | Measure in Phase 0. Keep the loader lean (no `cc`, no OPL). One 16 KB step at release, announced in the notes. |
| PC3 SRAM margin (1.9 KB) | Build fails or stack too small | Loader state in the heap. A BSS check in `GetHighestHexAddress.py`, as for every build. |
| Running from PSRAM is slow | Doom or MicroPython too slow to be worth it | Phase 10 measures before anything else is built on top. `--sram` for hot code. Stack in SRAM. Keep the XIP cache for the app, since the firmware is idle while it runs. |
| A longjmp path not trapped | Corrupted stacks, crash | The CheckAbort guard, the trap round every service, and the Phase 12 fault path. A Ctrl-C hammered during each service is part of the tests. |
| Audio glitches | Sound breaks during file writes and slow fills | The refill interrupt and priority change (D8). Glitches during A: writes are documented, as for PLAY. |
| Main moves on | Merge conflicts | Fencing (section 3) and frequent merges of main into `v8`. |
| Ports drag on | The work never ships | The program half is gated on Phase 10. USER modules (Phase 9) stand on their own and can ship without any port. |
| GPL code | Licence mixing | As on the CMM2: each app is a separate work on the card. The firmware and SDK ship no GPL code. The SDK's own files are MIT, as the CMM2's `cmm2img.h` is. |

---

## 9. Lessons carried over from the CMM2

These come from its `docs/Loader_Plan.md`, the code comments and the manual.

1. **RUN's arguments were tokenised** unless RUN started the line. D4 avoids this by taking a string expression.
2. **A stale `WAV_fnbr`** made `CloseAudio` close the loader's own file mid-read.
3. **Ctrl-C stays set** after the app ends, so a BASIC loop of apps stops too. `break_take` is the app's way to keep it for itself.
4. **`cleanend` inside a service** left the service's stack in a broken state. The fix is the `CheckAbort` guard.
5. **`poll()` must also run the BASIC sound refill**; the music was slow until it did.
6. **The fast-seek cluster map** is needed for big files read with seeks (Doom's WAD).
7. **The SDK's `rename` is its own**, because newlib's links and unlinks.
8. **errno must be set in both places**; time must be monotonic; the fini array must run once.
9. **Header names collide on Windows** (`Audio.h` vs `audio.h`). Keep the firmware's include directories off the path.
10. **ScummVM needs NDEBUG**: with assertions on, `Common::String` stopped Sam & Max.
11. **A mouse warp** must move only the drawn cursor.
12. **Refilling sound from the main loop broke music** whenever a frame took more than about 23 ms. Use an interrupt.
13. **Measure, don't guess.** MicroPython's benchmark moved 31% when unrelated code moved.

---

## 10. Where things are (PicoMite, db2c2d7)

### Hooks to add, all `#ifdef LOADER`

| Where | What |
|---|---|
| Commands.c:2315-2317 (`do_run`) | Divert `.APP` before `FileLoadProgram`. |
| Commands.c:4535 (`do_chain`), Editor.c:1208/4067/5738 (FM), Commands.c:11142 (EXECUTE), PicoMite.c:3305 (`*name`) | The other entry points. |
| PicoMite.c:2787 (`CheckAbort`) | No `do_end`/`longjmp` while `LoaderActive`. |
| Serial.c:921 (`SerialPutchar`) | Its `CheckAbort` call is covered by the guard above. |
| MMBasic.c:7886 (`ClearRuntime`) | Drop the USER module (its `forget`). |
| PicoMite.c:2891-3056 (`sigbus_c`) | Phase 12 recovery. Until then, say "in x.app at +offset" when the PC is in the app's area. |
| Memory.c (new) | `LoaderClaimPS`, `LoaderClaimSRAM`. `SBitsGet`/`SBitsSet` exist (2150-2166); `MBitsGet`/`MBitsSet` are static inline (2169). |
| FileIO.c (new) | The directory-listing helper. |
| SDCard.c:405 (`on_pwm_wrap`) | Pend the app's refill interrupt when a buffer empties. |
| AllCommands.h:724 | USER, USER(. |
| CMakeLists.txt | `LOADER_VARIANTS`, `misc/Loader.c`, `misc/User.c`. |

### Routines the services use

| Area | Routines |
|---|---|
| Files | `BasicFileOpen` 5956, `FindFreeFileNbr` 5774, `FileGetData` 5588, `FilePutData` 5636, `positionfile` 3029, `filegetpos` 3054, `ForceFileClose` 5743, `FileRename` 8582, `FileSize` 663, `ExistsFile` 698, `chdir` 2743, `GetCWD` 1843, `FileTry` 8552 (all FileIO.c) |
| Memory | `GetMemoryNull` 2455, `GetPSMemoryNull` 2429, `FreeMemory` 2021, `GetTempMemory` 1931, `InitHeap` 2091 (Memory.c) |
| Errors | `error` MMBasic.c:6928; `mark`/`ErrNext`/`jmprun` MMBasic.c:132-134; `OptionErrorSkip`, `MMErrMsg`, `MMerrno` (Commands.c:214, 656-658) |
| Polling | `routinechecks` PicoMite.c:902; `ProcessBT` Hardware_Includes.h:880; `ProcessWeb` net/WiFi.c:320 |
| Display | `setmode` Draw.c:4932; `ResetDisplay` 6337; `map256` HDMI.c:81; `v_scanline` HDMI.c:93; `QVgaScanLine` VGA.c:116; the draw pointers Draw.c:241-250; `WriteBuf` and the others Memory.c:129-143 |
| Audio | `on_pwm_wrap` SDCard.c:405; PLAY ARRAY Audio.c:1743; `iconvert`/`i2sconvert` 647-690; `CloseAudio` 736; `setrate` 814 |
| Input | `KeyDown` KeyboardMap.c:686; `PS2KeyDown` Keyboard.c:98; `nunstruct` I2C.c:127; `ConsoleRxBuf` PicoMite.c:302-304 |
| I/O | `ExtCfg` External.c:769, `ExtSet` 578, `ExtInp` 1536, `ClearExternalIO` 5822, `setpwm` SDCard.c:2488, `PWMoff` External.c:2955, `i2c_enable` I2C.c:1568, `i2c_masterCommand` 1641, `SerialOpen` Serial.c:588 |

### Per-variant figures (configuration.h)

Margins measured 16 November 2026 at 74f51e3 (the same code as main's 544be7b), private Ninja Release builds and `GetHighestHexAddress.py`. Variants with no figure were not measured.

| Variant | Flash offset | SRAM heap | Program | Flash margin | SRAM margin |
|---|---|---|---|---|---|
| PC3 | 1760 K | 108 K | 256 K | 13,280 B (13.0 KB) | 1,996 B |
| HDMIUSB | 1152 K | 212 K | 152 K | 16,508 B (16.1 KB) | 2,072 B |
| HDMI | 1104 K | 220 K | 160 K | 14,372 B (14.0 KB) | 2,000 B |
| HDMIBTH | 1520 K | 220 K | 180 K | | |
| HDMIWEB | 1584 K | 208 K | 156 K | | |
| VGARP2350 | 1072 K | 228 K | 168 K | | |
| VGAUSBRP2350 | 1104 K | 220 K | 160 K | **716 B** | 4,240 B |
| PICORP2350 | 1152 K | 364 K | 208 K | 7,332 B (7.2 KB) | 4,668 B |
| PICOUSBRP2350 | 1184 K | 360 K | 208 K | | |
| PICOBTRP2350 | 1456 K | 336 K | 208 K | | |
| PICOBTHRP2350 | 1536 K | 316 K | 208 K | | |
| WEBRP2350 | 1536 K | 312 K | 208 K | | |

### The CMM2 side (CMM2V1.5)

| What | Where |
|---|---|
| Loader | `Src/Loader.c` (1922 lines); `ld_app` 1485, `LoaderCall` 923, `ld_tidy` 1348, `ld_fixup` 1451, `LoaderModuleLoad` 1750, `LoaderRun` 1879 |
| USER command | `Src/User.c` |
| Headers | `Inc/cmm2img.h` (services 165-386), `Inc/cmm2user.h`, `Inc/Loader.h` |
| SDK | `tools/sdk/` (build.py, mkimage.py, app.ld, crt0.c, syscalls.c, userlib.c) |
| Ports | `apps/doom`, `apps/scummvm`, etc. MicroPython is at `ports/cmm2` on branch `cmm2` of `~/src/micropython` in WSL |

---

## 11. Background: the 4 November review

On 4 November Peter asked whether the CMM2's loadable programs could run on the PC3. The answer was yes in mechanism, with binaries rebuilt for the M33:

| Program | Assessment |
|---|---|
| MicroPython | Easy. The gain is switching without a reflash, since the PC3 already runs MicroPython natively. |
| Doom | Likely playable, without music. |
| ScummVM | One engine per image. |
| pc386 | No: the CPU is too slow. |

Peter: "no, note the conclusions". He raised it again on 16 November, asking whether this would be PSRAM-only, with space carved from the hidden PSRAM reserve.

The answer then, and the basis of this plan:
- Borrow the PSRAM heap at RUN rather than carve the reserve.
- Lend the SRAM heap for the stack and hot code.
- PSRAM is needed for the big programs, not for the mechanism.
