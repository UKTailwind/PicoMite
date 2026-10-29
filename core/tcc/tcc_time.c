/* TinyCC time support: the PicoMite clock, through the adapter. */
#include <time.h>
#include "tcc_time.h"
#include "tcc_picomite.h"

unsigned tcc_clock_ms(void)
{
    return tcc_pm_clock_ms();
}

/* The PicoMite clock (TIME$/DATE$, RTC, NTP) as an epoch read with gmtime():
   0 when it is set, -1 when it is not. */
int tcc_timestamp(time_t *value)
{
    time_t now = tcc_pm_time();
    if (now == (time_t)-1)
        return -1;
    *value = now;
    return 0;
}
