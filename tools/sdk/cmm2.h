/* cmm2.h: what a loadable program sees of the firmware (tools/sdk). cmm2 is the services table (loader/pmimg.h), set
   by crt0.c before main() is called; cmm2_start is the start block the loader passed. The names are the Colour
   Maximite 2's, so that a program's source builds for either. Ordinary C (stdio, stdlib, time) works through
   syscalls.c: stdout and stderr are the console, stdin its keys. MIT License, as pmimg.h. */
#ifndef CMM2_H
#define CMM2_H
#include "../../loader/pmimg.h"         /* (by its path: the firmware's other headers stay out of a program's way) */
#ifdef __cplusplus
extern "C" {
#endif
extern const struct cmm2_services *cmm2;
extern cmm2img_start *cmm2_start;
void *cmm2_heap_take(uint32_t size);   /* size bytes from the top of the heap, for the program's own allocator: malloc
                                          keeps the rest (syscalls.c); NULL if there isn't room */
#ifdef __cplusplus
}
#endif
#endif
