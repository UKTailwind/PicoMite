#ifndef MOS_TCC_TIME_H
#define MOS_TCC_TIME_H

#include <time.h>

unsigned mos_clock_ms(void);
/* 0: explicit UTC timestamp, -1: absent, -2: invalid/out of range. */
int mos_tcc_timestamp(time_t *value);

#endif
