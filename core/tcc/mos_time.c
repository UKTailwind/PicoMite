/* TCC-specific clock and SOURCE_DATE_EPOCH policy.
 * Runtime time/localtime/gettimeofday are supplied by libmos.a. */
#include <stdint.h>
#include <stdlib.h>
#include "mos_time.h"

unsigned mos_clock_ms(void)
{
    typedef uint64_t (*fn_t)(void);
    return (unsigned)(((fn_t)_sys_table_ptrs[263])() / 1000);
}

int mos_tcc_timestamp(time_t *value)
{
    const char *p = getenv("SOURCE_DATE_EPOCH");
    uint64_t seconds = 0;
    if (!p)
        return -1;
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
