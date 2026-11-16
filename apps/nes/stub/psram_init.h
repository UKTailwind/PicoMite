/* psram_init.h: on the CMM2 "PSRAM" is a block of the program's heap (nes_cmm2.c), which psram_init takes */
#ifndef CMM2_PSRAM_INIT_H
#define CMM2_PSRAM_INIT_H
#include "pico/stdlib.h"
#define PSRAM_CS_PIN_RP2350A 47
#define PSRAM_CS_PIN_RP2350B 19
void psram_init(uint cs_pin);
#endif
