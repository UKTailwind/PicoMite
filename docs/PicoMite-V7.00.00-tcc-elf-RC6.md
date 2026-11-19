# PicoMite V7.00.00 — Native C / TCC / ELF RC6

**Git tag:** `PicoMite-V7.00.00-tcc-elf-RC6`
**Integration commit:** `ff63ab4`
**Upstream baseline:** [UKTailwind/PicoMite `1751e3c`](https://github.com/UKTailwind/PicoMite/commit/1751e3c2e34b8d205bee7ec403e8185740b3b32f) (18 November 2026)

RC6 integrates the upstream PicoMite V7.00.00 release and its subsequent USB-host diagnostic fix into the native-C fork. It retains the fork's TinyCC, ELF loader, C CSUB support, resident services, Murmulator board variants, and C-aware editor and file manager.

## Changes integrated from upstream

- **WebMite:** Fixes for TCP/UDP interrupt lifetime and cancellation, TCP server acknowledgement timeout, TLS configuration safety, the TLS receive window, and TCP client reads into large arrays.
- **Serial:** Fixes for TX starting reliably, COM1/COM2 `CLOSE`/`FLUSH` draining output, transmission at normal program end, and inverted UART pin initialization. Ctrl+C can still interrupt output.
- **USB host:** Improved hub and RP2 host recovery, RX-timeout handling and diagnostics, including the new `hub.patch` for TinyUSB.
- **MMBasic:** The documented `PAUSE` range (up to 2,147,483,647 ms), and `TURTLE ARC LEFT` / `ARC RIGHT` command matching.
- **Graphics and editor:** Avoid leaving a stale GUI cursor during scrolling; restore terminal line wrapping correctly when leaving the editor.
- **mmb2csub:** Typed and multidimensional `STATIC` declarations and explicit rejection of source lines that cannot be translated safely.
- **Manuals:** Incorporates upstream's V7.00.00 manual and its V6.03.01-to-V7.00.00 changes guide, plus updated BLIT, TILEMAP and mmb2csub documentation.

For the complete upstream release history, see [`V7.00.00_release_notes.md`](V7.00.00_release_notes.md). Its upstream firmware layout and migration notes are **not** a substitute for the native-C fork's flash-layout instructions in that same file.

## Native C functionality retained

- RP2350: `RUN "file.c"`, `TCC`, native ARM code execution; RP2040 and RP2350: prebuilt ELF objects and C/ELF CSUBs.
- The common C API (currently `PM_API_VERSION` **35**) and classic `PicoCFunctions.h` CSUB interface.
- Resident flash-library CSUBs and explicit pin, PIO and DMA resource ownership.
- C-aware editor and file manager, keyboard remapping and user-selected console fonts.
- M1/M2 Murmulator VGA/HDMI hardware configurations and other fork-specific variants.

See [`PicoMite-native-C-support.md`](PicoMite-native-C-support.md) for syntax, APIs, limits and examples.

## Build prerequisites and verification

The fork is built with Pico SDK **2.3.1**, GCC **13.3.1**, Python 3 (for RP2350 compiler headers), and TinyUSB master **`efab607b8`** plus the **five** patches in `tinyusb-patches/`. The patched TinyUSB checkout lives in the sibling `../tinyusb-master` directory: it must be recreated after upgrading from an older patch set.

After merging upstream, the `PC3` build linked successfully once the TinyUSB checkout was regenerated. The following **24** UF2 targets were subsequently built successfully:

`HDMI`, `HDMIBTH`, `HDMIUSB`, `HDMIWEB`, `M1P1VGA`, `M1P2VGA`, `M2P1VGA`, `M2P2HDMI`, `M2P2HDMIUSB`, `M2P2VGA`, `PC3`, `PICO`, `PICOBTHRP2350`, `PICOBTRP2350`, `PICOMIN`, `PICORP2350`, `PICOUSB`, `PICOUSBRP2350`, `VGA`, `VGARP2350`, `VGAUSB`, `VGAUSBRP2350`, `WEB`, `WEBRP2350`.

**Verification scope:** compilation and linking across these configurations; this does not constitute hardware regression testing.

## Flash-layout warning

Native C requires additional firmware flash space. On the first transition from an upstream PicoMite firmware to this native-C fork, the options sector, saved program/flash slots and—depending on board and flash capacity—the A: drive may be reset. Save settings and data before installing. See the **native-C-specific** upgrade warning and board offsets in [`V7.00.00_release_notes.md`](V7.00.00_release_notes.md); do not rely on the stock upstream V7.00.00 "all settings retained" statement.
