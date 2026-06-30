# PicoMite

Firmware source for **MMBasic** running on the Raspberry Pi RP2040 and RP2350 —
the PicoMite, WebMite, PicoMiteVGA and PicoMiteHDMI variants.

This repository is the continuation of
[**PicoMiteAllVersions**](https://github.com/UKTailwind/PicoMiteAllVersions),
reorganised into a per-subsystem source tree with a single reusable build
directory. All new development and releases happen here.

## Releases

Pre-built `.uf2` binaries for every supported variant are attached to each
release — see **[Releases](https://github.com/UKTailwind/PicoMite/releases/latest)**.
Download the file matching your board and configuration and copy it to the
Pico in BOOTSEL mode.

## Building

Builds use CMake (NMake Makefiles generator) with the arm-none-eabi GCC
toolchain and the Raspberry Pi Pico SDK.

### Prerequisites

- **Raspberry Pi Pico SDK v2.2.0** — used **unmodified**. The build relocates
  the SDK's GPIO interrupt dispatcher (`gpio_default_irq_handler`) into RAM
  automatically at link time (see below); no edit to the SDK's `gpio.c` is
  required.
- **arm-none-eabi GCC 13.3.1**
- **TinyUSB v0.20.0** — replace the TinyUSB version supplied with the Pico SDK
  with [TinyUSB v0.20.0](https://github.com/hathach/tinyusb/releases).

> **GPIO interrupt latency:** PicoMite's GPIO interrupt callback must run from
> RAM for timing-critical features (IR, COUNT/FREQ, PS2). The SDK's shared
> dispatcher that calls it normally sits in flash. Rather than patch the SDK,
> the build renames that one function's section to `.time_critical.*` in the
> compiled object ([`cmake/relocate_gpio_irq_to_ram.cmake`](cmake/relocate_gpio_irq_to_ram.cmake)),
> which the SDK linker script already copies to RAM. A post-build check
> ([`cmake/assert_gpio_irq_in_ram.cmake`](cmake/assert_gpio_irq_in_ram.cmake))
> fails the build if it ever ends up in flash.

### Build

The helper script [`buildpicomite.bat`](buildpicomite.bat) drives the whole
matrix:

```
buildpicomite.bat            build every variant
buildpicomite.bat rp2040     all RP2040 variants
buildpicomite.bat rp2350     all RP2350 variants
buildpicomite.bat HDMIWEB    a single named variant
```

Each variant is selected with `-DCOMPILE=<variant>`, which forces the matching
`PICO_BOARD`. Built `.uf2` images are collected in the `uf2/` directory.

## Documentation

User manuals and per-feature guides live in [`docs/`](docs/), including the
main PicoMite user manual and the Advanced Graphics manual (GUI controls).
