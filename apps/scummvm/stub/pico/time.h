/* pico/time.h for the CMM2: the Pico SDK's microsecond clock, which the GOB engine reads for its input timing, from
   the loader's (cmm2->micros). GPL-2.0-or-later, as frank-quest. */
#ifndef CMM2_PICO_TIME_H
#define CMM2_PICO_TIME_H
#include <stdint.h>
#include "cmm2.h"
static inline uint32_t time_us_32(void) { return (uint32_t)cmm2->micros(); }
static inline uint64_t time_us_64(void) { return cmm2->micros(); }
#endif
