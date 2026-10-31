#ifndef PICOMITE_TCC_TIME_H
#define PICOMITE_TCC_TIME_H

#include <time.h>

unsigned tcc_clock_ms(void);
/* 0: timestamp provided, -1: the PicoMite clock is not set */
int tcc_timestamp(time_t *value);

#endif
