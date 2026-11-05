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

The alternate PIO image sends five 6-bit pixels per 32-bit FIFO word,
with two unused bits. It uses an autopull threshold of 30 instead of 32.
Both scanline buffers expand in place, so no additional full-frame buffer
is allocated. The additional static scanline memory is 640 bytes on RP2040
(two 680-byte buffers replacing two 360-byte buffers) and 512 bytes on
RP2350 (two 680-byte buffers replacing two 424-byte buffers). The per-line conversion runs in the DMA IRQ;
its latency must be checked on hardware for all supported resolutions.

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
