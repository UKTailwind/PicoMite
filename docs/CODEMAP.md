# PicoMite firmware — code map

Overview of every source file, grouped by the subsystem directory it lives in.
The tree was reorganised from a flat directory into per-subsystem folders; the
core0 entry point and main loop stay in `PicoMite.c`, with each subsystem in its
own folder.

Build variants are selected by `-DCOMPILE=<variant>` (see `buildpicomite.bat`)
and gated by macros: `PICOMITEVGA`, `HDMI`, `HDMICUTDOWN`, `PICOMITEWEB`,
`PICOMITEWEB_TLS`, `PICOMITEBT`/`PICOMITEBTH`, `USBKEYBOARD`, `PICOMITEMIN`,
`rp2350`.

---

## Root — entry point, build, project-wide config

| File | Contents |
|---|---|
| `PicoMite.c` | Main entry point: hardware/clock init, boot/reset, the main loop and `CheckAbort`/`routinechecks`, console plumbing, and the **core1 launch sites** (`QVgaCore`, `HDMICore`, `UpdateCore`, `init_vga222`). Holds the per-subsystem integration glue (calls into VGA/HDMI/WiFi/BT/USB), not the subsystem bodies. |
| `configuration.h` | The 896-byte `Option` struct layout, per-variant memory/flash/CPU budgets (`FLASH_TARGET_OFFSET`, `HEAP_MEMORY_SIZE`, `MagicKey`, `MAX_CPU`/`MIN_CPU`), pin maps. Pulls in `graphics/Screens.h`. |
| `Hardware_Includes.h` | Central aggregator: pico-sdk hardware includes, the `extern` declarations for shared globals used across translation units, common macros (`nop`, etc.), and `PS2Keyboard.h`. |
| `MMBasic_Includes.h` | Small interpreter aggregator (pulls the `core/` headers + `MMtrace.h`). |
| `AllCommands.h` | The BASIC keyword dispatch tables (command/function/token tables), variant-gated. |
| `Version.h` | Version and date strings. |
| `buildpicomite.bat` | Multi-variant build driver (reuses one build dir per platform group). |

## core/ — the MMBasic interpreter engine

| File | Contents |
|---|---|
| `MMBasic.c/.h` | Interpreter core: tokeniser, executor, variable/array/sub-fun management, the program run loop. |
| `Commands.c/.h` | BASIC command implementations (`cmd_*`). |
| `Functions.c/.h` | BASIC function implementations (`fun_*`). |
| `Operators.c/.h` | Operator handlers (arithmetic, comparison, logical). |
| `MM_Misc.c/.h` | `OPTION`, `configure()`, settings, `MM.INFO`, runtime CPU speed, and miscellaneous commands. Heaviest per-variant config in the codebase. |
| `MATHS.c/.h` | `MATH` command set (matrices, vectors, FFT, sensor fusion) and math functions. |
| `Memory.c/.h` | The MMBasic heap/memory manager (`GetMemory`, `AllMemory`, program/variable storage). |
| `CFunction.c` | `CSUB`/`CFUNCTION` embedded-machine-code support (call table, execution). |
| `MMtrace.c/.h` | Source-level `TRACE`/debug facility. |

## graphics/ — display, drawing, GUI

| File | Contents |
|---|---|
| `Draw.c/.h` | Core 2D drawing engine: pixels, lines, boxes, text, the `FontTable`. |
| `Draw3D.c` | 3D drawing (`MATH`/`DRAW3D` rotation/projection rendering). |
| `DrawFill.c` | Fill / flood / polygon fill. |
| `DrawInternal.h` | Internal shared declarations for the `Draw*` translation units. |
| `TileMap.c` | Tile-map rendering. |
| `Sprite.c` | Sprites (`SPRITE`). |
| `Blit.c` | `BLIT` operations including `BLIT MEMORY`. |
| `Pointer.c` | Mouse/touch pointer rendering. |
| `FrameBuffer.c` | Framebuffer management and display modes. |
| `GUI.c/.h` | `GUICONTROLS` widgets (buttons, switches, gauges, …) and touch stubs. |
| `Turtle.c/.h` | Turtle graphics and `fill_patterns[]`. |
| `Raycaster.c/.h` | Raycaster (pseudo-3D) engine. |
| `Screens.h` | All VGA/HDMI **display config**: per-resolution mode tables, framebuffer sizes, `FRAMEBUFFER_POOL_SIZE`, QVGA GPIO/timing, HDMI/DVI scanout timing, CPU-frequency defines, resolution enum. |
| `VGA.c/.h` | PIO VGA (QVGA) **core1 scanout** — `QVgaCore` + `QVgaLine1`/`QVgaPioInit`/`QVgaBufInit`/`QVgaDmaInit`/`QVgaInit`. (VGA variants only.) |
| `HDMI.c/.h` | HDMI/DVI **core1 scanout** — `HDMICore` + `HDMIloop0/1/2/3/X`, the `HDMICUTDOWN` RGB332 path, `MAP256DEF`, HSTX/DMA setup, resolution dispatch. (HDMI variants only.) |
| `PicoMiteVGA.pio` | VGA PIO program (compiled to a header at build time). |
| `SSD1963.c/.h`, `SSD1963min.c` | SSD1963 SPI-LCD controller driver (`min` = cut-down for PICOMITEMIN). |
| `SPI-LCD.c/.h` | SPI LCD panel drivers (ILI9341/ST7789/etc.). |
| `VGA222.c/.h` | 2-2-2 composite/VGA DAC driver (RP2350 PicoMite). |
| `RGB121.c/.h` | RGB121 (3-bit) framebuffer colour format. |
| `Touch.c/.h` | Touch panel (resistive + capacitive FT6x36 / GT911) and gesture recognition. |
| `BmpDecoder.c` | BMP image decoder. |

## io/ — peripheral drivers

| File | Contents |
|---|---|
| `I2C.c/.h` | I2C master/slave and `I2C`/device commands. |
| `SPI.c/.h` | SPI bus support. |
| `Serial.c/.h` | UART serial COM ports (COM1..6). |
| `Onewire.c/.h` | 1-Wire bus. |
| `GPS.c/.h` | GPS NMEA parser and `GPS` command. |
| `Audio.c/.h` | Audio output (PWM / I2S / SPI DAC), WAV/FLAC/MP3/MOD playback, tones. |
| `VS1053.c/.h`, `vs1053b-patches.h` | VS1053 audio-codec chip driver + firmware patch blob. |
| `stepper.c/.h` | Stepper-motor / G-code motion control (RP2350). |
| `psram.c/.h` | PSRAM driver (RP2350). |
| `Connect.h`, `Remove.h` | Embedded USB plug-in / unplug sound-effect WAV data. |
| `PicoMiteI2S.pio` | I2S audio PIO program. |

## input/ — keyboard & mouse

| File | Contents |
|---|---|
| `Keyboard.c` | PS/2 keyboard driver. |
| `mouse.c` | PS/2 mouse driver. |
| `KeyboardMap.c/.h` | Shared HID keymap + report decoder (used by USB and BLE HID hosts). |
| `USBKeyboard.c` | USB HID host (keyboard / mouse / multi-touch). |
| `PS2Keyboard.h` | PS/2 scan-code definitions. |

## net/ — networking (WEB variants)

| File | Contents |
|---|---|
| `WiFi.c/.h` | WiFi / lwIP / web runtime: `ProcessWeb` (lwIP poll pump), `WebConnect`, deferred async-error helpers, TLS/NTP time handling. |
| `MMTCPclient.c` | `WEB TCP CLIENT` commands (incl. TLS via altcp). |
| `MMtcpserver.c` | `WEB TCP SERVER` commands. |
| `MMMqtt.c` | MQTT BASIC commands (wraps the lwIP MQTT app). |
| `MMtelnet.c` | Telnet server. |
| `MMntp.c` | NTP time sync. |
| `MMtftp.c` | TFTP commands (wraps the lwIP TFTP app). |
| `MMudp.c` | UDP commands. |
| `lwipopts.h`, `lwipopts_examples_common.h` | lwIP configuration. |
| `mbedtls_config.h` | mbedTLS configuration (referenced via `MBEDTLS_CONFIG_FILE`). |

## bluetooth/ — BLE (BT variants)

| File | Contents |
|---|---|
| `BTConsole.c/.h` | SPP/RFCOMM console over the CYW43 Bluetooth radio + TLV bond storage (`hal_flash_bank`). |
| `BTKeyboard.c/.h` | BLE HID host (Bluetooth keyboard input). |
| `btstack_config.h` | btstack configuration. |
| `nus_gatt.gatt` | GATT attribute database (compiled to a header at build time). |

## misc/ — standalone command/utility modules

| File | Contents |
|---|---|
| `External.c/.h` | Pin / PWM / PIO / `SETPIN` / `PORT` / pulse / one-shot hardware command modules. |
| `Custom.c/.h` | Custom device + PIO + low-level/`POKE`-style commands. |
| `FileIO.c/.h` | File-system commands (`OPEN`/`PRINT #`/`LOAD`/`SAVE`/…), flash / LittleFS / FatFs glue, file manager. |
| `Editor.c/.h` | Full-screen program editor (`EDIT`), syntax colouring. |
| `SDCard.c` | SD-card FatFs disk-IO driver (the `diskio` implementation; formerly `mmc_stm32.c`). |
| `XModem.c/.h` | XMODEM file transfer. |

## third_party_mod/ — vendored libraries (carry local modifications)

| File | Contents |
|---|---|
| `ff.c/.h`, `ffsystem.c`, `ffunicode.c`, `ffconf.h`, `diskio.h` | FatFs (FAT filesystem). |
| `lfs.c/.h`, `lfs_util.c/.h` | LittleFS (internal-flash filesystem). |
| `cJSON.c/.h` | JSON parser (`JSON$`). |
| `re.c/.h` | Regular-expression engine. |
| `aes.c/.h` | AES (tiny-AES). |
| `hxcmod.c/.h` | MOD music player. |
| `picojpeg.c/.h` | JPEG decoder. |
| `upng.c/.h` | PNG decoder. |
| `dr_flac.h`, `dr_mp3.h`, `dr_wav.h` | Header-only FLAC/MP3/WAV decoders. |
| `mqtt.c` | lwIP MQTT client app. |
| `tftp.c` | lwIP TFTP app. |

## fonts/ — font data headers

`ArialNumFontPlus.h`, `Fnt_10x16.h`, `Font_8x6.h`, `Hom_16x24_LE.h`,
`Inconsola.h`, `Misc_12x20_LE.h`, `arial_bold.h`, `font-8x10.h`, `font1.h`,
`smallfont.h` — bitmap/font glyph tables used by `graphics/Draw.c`.

## tools/ — host-side / build scripts (not compiled into firmware)

| File | Contents |
|---|---|
| `GetHighestHexAddress.py` | Post-build flash/RAM fit checker (run by `buildpicomite.bat`). |
| `find_static_globals.py` | Finds file-scope globals with no external references (→ `static`), via `nm`; build-aware, with `--build-all`. |
| `find_dead_globals.py` | Finds never-used globals (deletion candidates) from the linker map's discarded sections. |
| `armcfgen.py` | Converts an ARM ELF object to PicoMite `CSUB` hex. |
| `fetch_ca.py` | Fetches CA root certs into a PEM bundle. |
| `add_manual_bookmarks.py` | Adds PDF bookmarks to the user manual. |
| `generate_picomitebt_pdf.py` | Generates the PicoMiteBT PDF. |
| `ble_bridge.py`, `ble_term.py`, `build_exe.bat` | PC-side BLE bridge/terminal host tools (+ PyInstaller build). |

## basic-addons/ — resources for BASIC programmers (not part of the build)

`PicoCFunctions.h` (the CSUB/CFUNCTION call-table API header for hand-written
CSUBs) and the TLS CA bundles `ca.pem` / `ca-mismatch.pem`.
