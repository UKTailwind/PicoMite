# Murmulator 2 / RP2350 VGA

The RP2350 `M2P2VGA` build uses the **PicoMiteVGA** graphics engine and
its standard `MODE`, `TILE`, and framebuffer layouts. The 4-bit RGB121
scanline is converted to RGB222 immediately before DMA/PIO output in
the default 6-bit build. The 4-bit physical-output alternative keeps
the original PIO driver. Details are in [M2 VGA RGB222](M2_VGA_RGB222.md).

| Output | RGB data | HSYNC | VSYNC | Build command |
|---|---|---|---|---|
| RGB222 (6 bit) | GP12..GP17 | GP18 | GP19 | `buildpicomite.bat M2P2VGA` |
| RGB121 (4 bit) | GP13..GP16 | GP18 | GP19 | `buildpicomite.bat M2P2VGA4` |

Both profiles share the same BASIC `MODE` functionality. The 6-bit path
still has 16 logical RGB121 colours (the red and blue bits are duplicated).
It does not consume a 245,760-byte full-screen RGB222 framebuffer.

The old `PICOMITE`/`VGA222` M2P2VGA build uses a different flash map from
the new `PICOMITEVGA` variant. Back up programs and files before flashing:
a version/family switch can reset flash storage. `OPTION RESET` restores
the current firmware's factory pinout and graphics defaults.
