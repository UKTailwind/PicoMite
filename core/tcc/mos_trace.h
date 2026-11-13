#ifndef MOS_TCC_TRACE_H
#define MOS_TCC_TRACE_H

/* Disabled by default.  Debug output is independent of any OS2 syscall
 * table; a PicoMite console adapter can be installed in a later stage. */
#if defined(MOS_TCC_TRACE_ENABLED)
#include <stdio.h>
#define MOS_TRACE(...) do { \
    fprintf(stderr, "[tcc] " __VA_ARGS__); \
    fflush(stderr); \
} while (0)
#else
#define MOS_TRACE(...) ((void)0)
#endif

#endif
