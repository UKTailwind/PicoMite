/* TinyCC: <picomite/memory.h>. See docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_MEMORY_H
#define _PICOMITE_MEMORY_H

#include <stddef.h>

#define PM_MEM_ANY   0
#define PM_MEM_SRAM  1
#define PM_MEM_PSRAM 2

void *pm_alloc(size_t size, int where);
void pm_free(void *p);

int pm_mem_where(const void *p);

typedef struct pm_mem_info
{
    unsigned size;
    unsigned sram_total;
    unsigned sram_free;
    unsigned psram_total;
    unsigned psram_free;
} pm_mem_info;

int pm_get_mem_info(pm_mem_info *info);

#endif
