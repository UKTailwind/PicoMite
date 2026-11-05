# Murmulator 2 / RP2350 VGA

`COMPILE=M2P2VGA` targets `MURMULATOR_M2P2` hardware with PS/2 keyboard,
mouse, SD, PWM audio, and optional PSRAM. Video uses the board's analog VGA
connector rather than the HDMI/HSTX output.

## Pinout

| Signal | 4-bit RGB121 | 6-bit RGB222 |
|---|---|---|
| HSYNC | GP18 | GP18 |
| VSYNC | GP19 | GP19 |
| RGB data | GP13..GP16 | GP12..GP17 |
| First colour pin (`Option.VGA_BLUE`) | GP13 | GP12 |

Both video drivers require consecutive data pins. The signal order is set by
the existing PicoMite PIO VGA drivers.

## Build

The two profiles select different video pipelines at CMake configuration time.
They cannot be switched with the BASIC `MODE` command:

| Profile | CMake | Batch command | Firmware driver |
|---|---|---|---|
| RGB222 (default) | `-DCOMPILE=M2P2VGA -DM2P2_VGA_BITS=6` | `buildpicomite.bat M2P2VGA` | `graphics/VGA222.c` |
| RGB121 | `-DCOMPILE=M2P2VGA -DM2P2_VGA_BITS=4` | `buildpicomite.bat M2P2VGA4` | `graphics/VGA.c` |

The batch commands emit distinct files, `PicoMiteM2P2VGA*.uf2`, to the
`uf2/` directory, with the usual version suffix. They explicitly set the
colour depth so reusing a CMake cache cannot silently change the profile.
`M2P2VGA4` is a batch-script alias; CMake's `COMPILE` value is `M2P2VGA`
for both builds. `buildpicomite.bat rp2350` builds both profiles.

## Display modes and memory

The RGB222 build defaults to `VGA222` (640x480) at 252 MHz. The previous
test default `VGA222X320` (320x240) substantially reduced the console
columns. `VGA222X320` remains supported as an explicit display choice.
The 640x480 mode uses a 245,760-byte framebuffer plus a 1,920-byte line
map (247,680 bytes before alignment); this reduces the RAM left for the
BASIC heap. The linker and RAM-budget checks must pass for each build.

The RGB121 build uses the existing VGA tiled 4-bit scanout, with its
default SCREENMODE1 and the GP13..GP16 output group.

An existing installation stores the old `Option.DISPLAY_TYPE` in flash,
so changing the firmware's factory default alone does not change that
saved option. On a device still configured for 320x240, select 640x480
using `OPTION LCDPANEL VGA222_640x480, GP18, GP12`, or reset options to
factory defaults. Do not change `MagicKey` just for this display default:
an options signature mismatch invokes the flash-reset path, potentially
erasing stored data.

Hardware verification of the RGB222 640x480 signal and available
application heap is required before calling this a validated build.
