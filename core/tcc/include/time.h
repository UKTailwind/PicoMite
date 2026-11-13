/* PicoMite TinyCC: <time.h>.  time() reads the PicoMite clock (TIME$/DATE$,
   RTC, NTP; -1 when it has not been set); clock() counts milliseconds. */
#ifndef _TIME_H
#define _TIME_H

#include <stddef.h>

typedef long long time_t;
typedef long clock_t;
#define CLOCKS_PER_SEC 1000

time_t time(time_t *t);
clock_t clock(void);

#endif /* _TIME_H */
