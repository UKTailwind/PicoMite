/* Standalone TinyCC time support.  No Murmulator-OS2 syscall table. */
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "tcc_time.h"

unsigned tcc_clock_ms(void)
{
    clock_t ticks = clock();
    if (ticks == (clock_t)-1)
        return 0;
    return (unsigned)((uint64_t)ticks * 1000u / CLOCKS_PER_SEC);
}

int tcc_timestamp(time_t *value)
{
    const char *p = getenv("SOURCE_DATE_EPOCH");
    uint64_t seconds = 0;
    if (!p) {
        time_t now = time(NULL);
        if (now == (time_t)-1 || now == (time_t)0)
            return -1;
        *value = now;
        return 0;
    }
    if (!*p)
        return -2;
    for (; *p; ++p) {
        unsigned digit = (unsigned)(*p - '0');
        if (digit > 9 || seconds > (UINT64_C(253402300799) - digit) / 10)
            return -2;
        seconds = seconds * 10 + digit;
    }
    *value = (time_t)seconds;
    if (*value < 0 || (uint64_t)*value != seconds)
        return -2;
    return 0;
}
