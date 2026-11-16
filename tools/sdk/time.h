/* time.h: newlib's, with POSIX's clocks, for a loadable program (tools/sdk). MIT License, as pmimg.h.

   newlib declares clock_gettime, clock_getres, nanosleep and CLOCK_MONOTONIC only for the systems that have them
   (RTEMS, Cygwin), and a program written for POSIX uses them: PDCurses times its keys and clicks by
   clock_gettime(CLOCK_MONOTONIC). syscalls.c has them on the services: CLOCK_MONOTONIC (and the CPU-time clocks, the
   program having the processor to itself) the microseconds since start-up, CLOCK_REALTIME gettimeofday's. build.py's
   -I puts this file before newlib's, which it then includes. */
#ifndef CMM2_TIME_H
#define CMM2_TIME_H

#include_next <time.h>

#ifndef CLOCK_PROCESS_CPUTIME_ID
#define CLOCK_PROCESS_CPUTIME_ID ((clockid_t)2)
#endif
#ifndef CLOCK_THREAD_CPUTIME_ID
#define CLOCK_THREAD_CPUTIME_ID ((clockid_t)3)
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC ((clockid_t)4)
#endif

#ifndef _POSIX_TIMERS
#ifdef __cplusplus
extern "C" {
#endif
int clock_gettime(clockid_t clock_id, struct timespec *tp);
int clock_getres(clockid_t clock_id, struct timespec *res);
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);
#ifdef __cplusplus
}
#endif
#endif

#endif
