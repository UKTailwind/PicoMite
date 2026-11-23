/* pico/util/buffer.h: the Pico SDK's mem_buffer, which B-em's audio buffers (x_gui.h) hold their samples in */
#ifndef CMM2_PICO_UTIL_BUFFER_H
#define CMM2_PICO_UTIL_BUFFER_H
#include "pico/stdlib.h"
typedef struct mem_buffer {
    size_t size;
    uint8_t *bytes;
    uint8_t flags;
} mem_buffer_t;
#endif
