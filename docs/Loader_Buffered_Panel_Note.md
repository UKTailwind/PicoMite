# Loadable programs on PICOUSBRP2350 with a buffered panel: analysis note

**24 November 2026. Analysis only: no code has been changed.** Peter asked for
this note and wants no changes for the moment.

## Background

From V8.00.00b2 two builds have the loader: PicoMitePC3 and PicoMiteHDMIUSB
(commit 7770b0d). The other RP2350 builds are blocked by three things:

- **The screen services are written for HDMI.** `ld_frame` reads `v_scanline`,
  and `ld_palette` and `ld_screensave` write `map16pairs`, `map16quads` and
  `map256`. `ld_vmodebody` and `ld_screenbody` call `setmode`.
- **Flash.** The loader takes about 17 KB.
- **The display.** The plan's order (`docs/Loader_Plan.md`, Phase 11) was
  HDMIUSB, HDMI, the VGA builds, then the LCD builds.

The VGA builds can't show the apps' colours properly. MAP there works only in
MODE 2 and 3, with 16 colours (RGB121), and MODE 5 has no 256-colour palette.
Their flash margins also need measuring again.

Peter's interest is **PICOUSBRP2350 with a buffered panel**, the "NEXTGEN"
displays, because they give an RGB332 framebuffer.

## What a buffered panel has

| What | Where |
|---|---|
| Panel types `ST7796SPBUFF` (80, `NEXTGEN`) to `SSD1963_7_BUFF` (99), RP2350 only | `graphics/SPI-LCD.h:160-180` |
| Each panel's size and SPI clock | `graphics/SPI-LCD.c:136-155` |
| The framebuffer: HRes × VRes bytes, RGB332, in SRAM at the top of the heap | `PicoMite.c:4022`; `ScreenBuffer` is `FRAMEBUFFER` (`configuration.h:906`) |
| Drawing into it with a scroll offset (`ScrollStart`) and a changed rectangle (`low_x`, `high_x`, `low_y`, `high_y`) | `DrawRectangleMEM332`, `graphics/SPI-LCD.c:2807`; `ScrollLCDMEM332`, `:2330` |
| The 20 ms refresh: the 1 ms timer pushes the changed rectangle to core 1 (command 6) when `Option.Refresh` is on | `PicoMite.c:1116`; `BUFFER_REFRESH_INTERVAL_MS` 20 at `:287` |
| Core 1 sends the rectangle to the panel under `frameBufferMutex` | `PicoMite.c:3162`; `copybuffertoscreen`, `graphics/SPI-LCD.c:3623` |
| The colour table: each RGB332 byte goes through `RGB332_LUT[256]`, a real 256-entry palette | `graphics/SPI-LCD.c:3135` (`static`) |
| What the table holds: RGB565 on most panels; RGB888 on ILI9488BUFF and ILI9488PBUFF; RGB888 in the SSD order on the SSD1963 12- and 24-bit types | `init_RGB332_to_RGB565_LUT` `:3141`, `..._RGB888_LUT` `:3175`, `..._RGB888_LUT_SSD` `:3243` |
| MAP, MAP SET, MAP RESET on these panels (`remap_LUT`, then copied into `RGB332_LUT`) | `cmd_map`, `graphics/SPI-LCD.c:3265` |
| REFRESH (pushes the changed rectangle when auto-refresh is off) | `cmd_refresh`, `graphics/Draw.c:5264` |

## Why it suits the apps

- **The palette:** `RGB332_LUT` is a true palette, so the palette service can
  write into it. Doom, the NES and ScummVM get their exact colours.
- **Doom** (`apps/doom/doomgeneric_pico.c:41`): asks for `video_mode(5, 8)`
  and a palette, and centres 320×200 with `v.width` and `v.pitch`. It runs on
  any panel size.
- **The NES** (`apps/nes/nes_pico.c:71`): asks for `video_mode(5, 8)` and
  centres 256×240.
- **ScummVM** (`apps/scummvm/overlay/src/main.c:78`): asks for
  `video_mode(5, 8)` and refuses unless the screen is exactly 320×240. It runs
  only on the ILI9341BUFF and ST7789_320BUFF, and possibly on the SSD1963
  types where HRes is 320 (`core/MM_Misc.c:5612`).
- **The BBC Micro** (`apps/bbc/bbc_pico.c:169`): asks for `video_mode(3, 4)`,
  640×480 in 16 colours. It is refused and says so.
- **The other services need nothing new on PICOUSBRP2350:**
  - Keyboard, mouse and gamepads use the USB host code (`USBKEYBOARD`) that
    HDMIUSB already uses.
  - Sound is `AppSound*` in `io/Audio.c`, built for any `LOADER` build and
    not tied to a display.
  - Files, I/O, time and maths are shared by every build.
- **Memory:** core 1 sends the picture to the panel, so it costs the app none
  of core 0's time. The framebuffer is already taken off the SRAM heap at
  boot, so the app's stack and SRAM area come from what is left: 360 KB less
  75 KB (320×240) to 150 KB (480×320).

## Proposed design (Loader.c, plus one small function in SPI-LCD.c)

1. **`video_mode` on a buffered panel.**
   - Accepts MODE 5 at 8 bpp only.
   - Gives `fb = FRAMEBUFFER`, width HRes, height VRes and pitch HRes.
   - Sets `ScrollStart` to 0, so the buffer is linear, and clears the screen.
   - Refuses every other mode (-1).
   - On a build or board without a buffered panel, refuses every mode.
     Programs that don't use the screen still run.
2. **Getting the picture to the panel.** The app writes the framebuffer
   directly, so nothing marks the changed rectangle. Proposal:
   - While an app has the screen, the poll service marks the whole screen as
     changed, but only once core 1 has finished the previous copy. Core 1
     would clear a flag after `copybuffertoscreen`, or the service tries
     `frameBufferMutex`.
   - The existing 20 ms timer stays the one place that pushes to core 1. That
     avoids two traps:
     - **Interleaved commands:** a second pusher in thread mode could be
       interrupted by the timer's four-word push halfway through its own.
     - **A blocked timer:** a copy longer than about 40 ms (a 480×320 panel,
       or the ILI9488 with three bytes a pixel) would make the timer interrupt
       block on a full FIFO.
   - `Option.Refresh` is forced on for the run and put back afterwards. A
     BASIC program may have turned auto-refresh off.
   - `video_wait` waits until that copy has finished, with a time limit:
     the panel's version of a vertical sync. In Doom with `-vsync`, the order
     `video_wait`, copy the frame, poll gives frames without tearing.
3. **The palette.** A small exported function in SPI-LCD.c converts a colour
   into the panel's table format, as `cmd_map` does, and writes it into
   `RGB332_LUT`. It then marks the whole screen as changed. The loader saves
   the table (1 KB) when the app starts and restores it at the end, so a BASIC
   program's MAP colours survive.
4. **At the end.** The table, `ScrollStart`, the font and colours, and
   `Option.Refresh` are restored. The screen is then cleared and refreshed.
5. **The HDMI-only code** (`setmode`, `v_scanline`, the map tables and their
   save area in `ld`) goes behind `#ifdef HDMI`. The VGA build would need its
   own path later.

## Speed (theoretical limits)

A full-screen copy at the panel's SPI clock limits the frame rate. These are
upper bounds; the per-pixel `spi_write_fast` calls will cost more.

| Panel | Clock | Bytes a pixel | One frame | Best rate |
|---|---|---|---|---|
| 320×240 (ILI9341BUFF, ST7789_320BUFF) | 50 MHz | 2 | ~25 ms | ~40 fps |
| 320×320 (ST7796SPBUFF) | 60 MHz | 2 | ~27 ms | ~36 fps |
| 480×320 (ST7796SBUFF) | 60 MHz | 2 | ~41 ms | ~24 fps |
| 480×320 (ILI9488BUFF) | 45 MHz | 3 | ~82 ms | ~12 fps |

Doom and the NES fill only the middle of a larger panel. A later refinement
could copy only that area.

## Open questions for Peter

1. **Flash margin:** what is PICOUSBRP2350's spare flash? Its offset is
   1184 KB and it has not been measured. The loader takes about 17 KB, so it
   will probably need a 16 KB move, which resets it at its first boot of b2.
2. **PICORP2350:** the same code would serve it, with a PS/2 keyboard instead
   of USB. It had 7.2 KB spare on 16 November, so it would need a move too.
   Should it come at the same time?
3. **Test board.** COM29 (a Pico Plus 2 W in the PicoCalc) has PSRAM and a
   buffered panel. But PICOUSBRP2350 makes its USB port a host, so its console
   would have to be the CH340 on COM25, which did not answer in October 2026.
   Which board should be used?
