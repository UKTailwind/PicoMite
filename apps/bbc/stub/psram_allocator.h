/* psram_allocator.h: the CMM2 has no PSRAM, and its heap is as big */
#ifndef CMM2_PSRAM_ALLOCATOR_H
#define CMM2_PSRAM_ALLOCATOR_H
#include <stdlib.h>
#define psram_malloc(n) malloc(n)
#define psram_free(p) free(p)
#endif
