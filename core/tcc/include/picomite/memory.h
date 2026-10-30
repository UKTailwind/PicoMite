/* PicoMite TinyCC: <picomite/memory.h> - the MMBasic heap: SRAM and PSRAM.
   (Not available in a C CSUB: it runs outside any C program.) */
#ifndef _PICOMITE_MEMORY_H
#define _PICOMITE_MEMORY_H

#include <stddef.h>

#define PM_MEM_ANY   0 /* SRAM, or PSRAM for a big block or when SRAM is full */
#define PM_MEM_SRAM  1 /* fast; needed for anything DMA or an interrupt reads */
#define PM_MEM_PSRAM 2 /* large, slower */

/* A block of the MMBasic heap, zeroed, or NULL if there is no room where
   asked.  Like malloc() memory it is all given back when the program ends;
   pm_free() gives a block back sooner.  Blocks are rounded up to 256 bytes,
   so use it for buffers rather than many small objects (malloc). */
void *pm_alloc(size_t size, int where);
void pm_free(void *p);

/* Where is p: PM_MEM_SRAM or PM_MEM_PSRAM for the MMBasic heap, else 0 (the
   stack, flash, the firmware's own RAM). */
int pm_mem_where(const void *p);

/* pm_get_mem_info(): set size first (as pm_get_info) */
typedef struct pm_mem_info
{
    unsigned size;        /* in: sizeof(pm_mem_info) */
    unsigned sram_total;  /* the SRAM heap */
    unsigned sram_free;
    unsigned psram_total; /* 0 if there is no PSRAM */
    unsigned psram_free;
} pm_mem_info;

int pm_get_mem_info(pm_mem_info *info); /* 0, or -1 if info->size is too small */

#endif /* _PICOMITE_MEMORY_H */
