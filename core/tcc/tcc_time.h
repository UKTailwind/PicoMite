#ifndef PICOMITE_TCC_TIME_H
#define PICOMITE_TCC_TIME_H

#include <time.h>

unsigned tcc_clock_ms(void);
/* 0: timestamp provided, -1: unavailable, -2: invalid SOURCE_DATE_EPOCH. */
int tcc_timestamp(time_t *value);

#endif
