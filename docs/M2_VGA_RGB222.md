# M2P1VGA / M2P2VGA RGB222 scanout

Both Murmulator 2 VGA variants run the **PicoMiteVGA** graphics engine,
including `MODE`, `TILE`, `MAP`, and the normal framebuffer layouts. The
scanline renderer produces 4-bit RGB121 pixels; an in-place, reverse-order
converter expands them to six physical RGB222 bits before PIO/DMA.

| Variant | CPU | RGB data | HSYNC | VSYNC |
|---|---|---|---|---|
| `M2P1VGA` | RP2040 | GP12..17 | GP18 | GP19 |
| `M2P2VGA` (6-bit default) | RP2350 | GP12..17 | GP18 | GP19 |
| `M2P2VGA` (`M2P2_VGA_BITS=4`) | RP2350 | GP13..16 | GP18 | GP19 |

The 4-bit input has the form `R G1 G0 B`; the six physical outputs are
`R R G1 G0 B B`. This **does not** create 64 independent colour values:
there are still 16 input colour values. Normal palette mapping (`MAP`) and
all existing `SCREENMODE` layouts stay unchanged.

The new `graphics/vga_nextgen.c` driver is based on the existing
`drivers/vga-nextgen/vga.c` from PicoMiteAllVersions. The video output
is a contiguous eight-bit GPIO bus: RGB222 on bits 0..5, HSYNC on bit 6,
VSYNC on bit 7. For the M2 board its base is GP12, so the signals occupy
GP12..GP19. Sync is active-low and is part of every byte in the DMA stream.

The PIO program `graphics/vga_nextgen.pio` contains only `OUT PINS, 8`;
DMA feeds four output pixels per word (32-bit autopull), including blanking
and sync samples. This replaces five-pixel/30-bit packing and the old
`PicoMiteVGA222.pio` pipeline. Two full scanline buffers (1088 bytes each)
replace the old small line buffers; no full-frame RGB222 framebuffer is used.
`MODE 1/2/3`, `MAP` and `TILE` continue to consume PicoMiteVGA data.

`vga_nextgen` is not M2-specific: any future target can select it in CMake,
provided the board has eight consecutive output GPIOs ordered as RGB222,
HSYNC and VSYNC. Pixel clock timing and DMA IRQ latency require hardware
verification on each board and supported resolution.

Build:

```bat
buildpicomite.bat M2P1VGA
buildpicomite.bat M2P2VGA
buildpicomite.bat M2P2VGA4
```

The last batch command is the legacy 4-bit physical-output option, using
`COMPILE=M2P2VGA` and `M2P2_VGA_BITS=4`. Standard `M2P2VGA` uses
`M2P2_VGA_BITS=6` and the 4-bit input / 6-bit output converter.

After installing the new M2P2VGA firmware, `OPTION RESET` restores the
factory `SCREENMODE1` video default, subject to the usual reset effects.
The M2P2VGA video family changed from the old `PICOMITE`/`VGA222` driver
to `PICOMITEVGA`: it therefore uses a distinct flash `MagicKey`, so back up
flash files and saved programs before installing. M2P1VGA keeps its existing
flash memory layout and migrates the old GP13 data-pin default to GP12.
