/* PicoMite TinyCC bootstrap.  This stage compiles the library only. */
#ifndef TCC_PICOMITE_CONFIG_H
#define TCC_PICOMITE_CONFIG_H
#define TCC_VERSION "0.9.27-picomite-bootstrap"
#ifndef ONE_SOURCE
#define ONE_SOURCE 1
#endif
/* Preserved from the tested RP2350 port; execution on RP2040 is not ready. */
#define TCC_TARGET_ARM_THUMB 1
#define TCC_ARM_EABI 1
#define TCC_ARM_VFP 1
#define TCC_ARM_HARDFLOAT 1
#define CONFIG_TCC_STATIC 1
/* Do not retain MOS2 filesystem paths.  Include/library paths will be
   configured through libtcc when PicoMite provides a file-I/O adapter. */
#define CONFIG_TCCDIR ""
#define CONFIG_TCC_SYSINCLUDEPATHS ""
#define CONFIG_TCC_LIBPATHS ""
#endif
