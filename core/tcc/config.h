/* Initial MOS build configuration; compiler backend is unchanged. */
#ifndef TCC_MOS_CONFIG_H
#define TCC_MOS_CONFIG_H
#define TCC_VERSION "0.9.27-mos-bootstrap"
#ifndef ONE_SOURCE
#define ONE_SOURCE 1
#endif
#define TCC_TARGET_ARM_THUMB 1
#define TCC_ARM_EABI 1
#define TCC_ARM_VFP 1
#define TCC_ARM_HARDFLOAT 1
#define CONFIG_TCC_STATIC 1
#define CONFIG_TCCDIR "/mos2/lib"
#define CONFIG_TCC_SYSINCLUDEPATHS "/mos2/include"
#define CONFIG_TCC_LIBPATHS "/mos2/lib"
#endif
