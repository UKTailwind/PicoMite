#ifndef MOS_TCC_TRACE_H
#define MOS_TCC_TRACE_H

/* Optional hang localization. MOS slot 41 writes directly to the console,
 * without using the application FILE streams or their buffering. */
#if defined(MOS_TCC_TRACE_ENABLED) && defined(__arm__)
#define MOS_TRACE(...) do { \
    typedef void (*mos_trace_fn)(const char *, ...); \
    const unsigned long *mos_trace_table = (const unsigned long *)0x10fff000ul; \
    ((mos_trace_fn)mos_trace_table[41])("[tcc] " __VA_ARGS__); \
} while (0)
#elif defined(MOS_TCC_TRACE_ENABLED)
#include <stdio.h>
#define MOS_TRACE(...) do { \
    fprintf(stderr, "[tcc] " __VA_ARGS__); \
    fflush(stderr); \
} while (0)
#else
#define MOS_TRACE(...) ((void)0)
#endif

#endif
