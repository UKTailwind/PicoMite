/* PicoMite TinyCC configuration (RP2350, run in memory: tcc -run). */
#ifndef TCC_PICOMITE_CONFIG_H
#define TCC_PICOMITE_CONFIG_H
#define TCC_VERSION "0.9.27-picomite"
#ifndef ONE_SOURCE
#define ONE_SOURCE 1
#endif
/* Hosted by the PicoMite firmware: libc, files and console go through the
   adapter in tcc_picomite.c (see tcc_picomite.h). */
#define TCC_PICOMITE 1
#define TCC_TARGET_ARM_THUMB 1
#define TCC_ARM_EABI 1
#define TCC_ARM_VFP 1
/* No TCC_ARM_HARDFLOAT: the firmware is built -mfloat-abi=softfp, so code
   compiled here passes floating-point values in core registers too and can
   call the firmware's functions (sin(), printf(), ...) directly. */
#define CONFIG_TCC_STATIC 1
/* No install directory: the system headers are built into the firmware
   (core/tcc/include, served by the adapter under "rom:/include"), and the
   run-time library is the firmware itself (tcc_pm_resolve_symbol). */
#define CONFIG_TCCDIR ""
#define CONFIG_TCC_SYSINCLUDEPATHS "rom:/include"
#define CONFIG_TCC_LIBPATHS ""
#endif
