# Loadable programs: Doom, the first port (Phase 10), 22 November 2026

The plan's go/no-go measurement (docs/Loader_Plan.md, Phase 10): doomgeneric's timedemo of demo1.

**Board:** the COM10 Pico Computer 3, 315 MHz (OPTION RESOLUTION 640x480 @ 315000KHz), OPTION AUDIO I2S, firmware 8.00.00b1
from branch `v8` (Phase 9, 1a12349, plus the file services' fast-seek map).
**Program:** `apps/doom` (doomgeneric e257a095 with `doomgeneric_pico.c`), `doom.app` 342,040 bytes, 64 KB stack in the SRAM
heap, its zone 4 MB of the PSRAM heap. MODE 5: Doom's 320x200 frame in the middle of 320x240, MAP's palette.
**Data:** `B:/Doom/freedoom1.wad` (Freedoom: Phase 1, 28,795,076 bytes) on the SD card.

    DRIVE "B:" : CHDIR "/Doom"
    RUN "doom.app", "-timedemo demo1 -nosound"
    RUN "doom.app", "-timedemo demo1"

| | PC3, 315 MHz | CMM2 Gen 1, 400 MHz |
|---|---|---|
| Silent | **28.1 fps** (7117 gametics in 8852 and 8858 realtics) | 77.8 fps |
| With sound | **25.9 fps** (effects only: 16 voices mixed in the stream's fill; 9617 realtics) | 33.2 fps (effects and OPL3 music) |
| The plan's estimate (4 November) | 25-40 fps | |

The PicoMite has no music: it has no synth (the CMM2's OPL3 was ruled out for it).

**Start-up** (from M_Init to R_Init, reading the WAD's lumps):

| | |
|---|---|
| Without a fast-seek map | 66 s (and the silent timedemo 27.0 fps, the one with sound 25.0) |
| With FatFs's fast-seek map (Loader.c's `ld_map`) | 29-30 s |

The rest of the start-up is about 2 s. What is left in R_Init is probably the cost of each read (the C library's
buffered reads, one service call each) rather than the card: not yet profiled.

`doom_timedemo_silent.log` and `doom_timedemo_sound.log` are the consoles' output, each line with the seconds since the RUN
(scratchpad `doomrun.py`). "doom.app ended with code -1" is the timedemo's own end (I_Error with the result), not a fault.
