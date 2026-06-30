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
toolchain and the Raspberry Pi Pico SDK. The helper script
[`buildpicomite.bat`](buildpicomite.bat) drives the whole matrix:

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
