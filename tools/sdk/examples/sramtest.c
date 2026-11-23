/* sramtest.c: a program with an SRAM part (app.ld's .sram and .sram_bss): calls from the PSRAM to the SRAM and back
   (through the linker's veneers), a pointer to an SRAM function kept in the PSRAM, data in the SRAM set and zeroed,
   and the same loop timed with its code and data in each.

       python tools/sdk/build.py out/sramtest.app tools/sdk/examples/sramtest.c --psram
       RUN "sramtest.app" */
#include <stdint.h>
#include <stdio.h>
#include "cmm2.h"

#define IN_SRAM __attribute__((section(".sram"), noinline))

static uint32_t ps_data[256];                                   /* the PSRAM's .bss */
static uint32_t sr_data[256] __attribute__((section(".sram_bss")));   /* the SRAM's, zeroed by the loader */
static uint32_t sr_init[4] __attribute__((section(".sram.data"))) = { 1, 2, 3, 4 };   /* (code and data need sections of their own) */

__attribute__((noinline)) uint32_t ps_add(uint32_t a, uint32_t b) { return a + b; }

#define LOOP_BODY                                                \
    uint32_t s = 0;                                              \
    for(int k = 0; k < n; k++)                                   \
        for(int i = 0; i < 256; i++) {                           \
            d[i] += (uint32_t)(i ^ k);                           \
            s += d[i] >> (k & 3);                                \
        }                                                        \
    return s;

IN_SRAM uint32_t sr_loop(uint32_t *d, int n) { LOOP_BODY }
__attribute__((noinline)) uint32_t ps_loop(uint32_t *d, int n) { LOOP_BODY }

IN_SRAM uint32_t sr_calls_ps(uint32_t a) { return ps_add(a, sr_init[2]); }   /* the SRAM to the PSRAM */

uint32_t (*sr_ptr)(uint32_t *, int) = sr_loop;                  /* a pointer to the SRAM, in the PSRAM's .data */

int main(void) {
    int ok = 1;
    printf("sr_loop at %p, sr_data at %p, sr_init %lu %lu %lu %lu\n", (void *)sr_loop, (void *)sr_data,
           (unsigned long)sr_init[0], (unsigned long)sr_init[1], (unsigned long)sr_init[2], (unsigned long)sr_init[3]);
    ok &= sr_init[0] == 1 && sr_init[3] == 4 && (uintptr_t)sr_loop >= 0x20000000u && (uintptr_t)sr_loop < 0x20082000u;
    for(int i = 0; i < 256; i++) ok &= sr_data[i] == 0;
    uint32_t c = sr_calls_ps(10);
    printf("sr_calls_ps(10) = %lu (13)\n", (unsigned long)c);
    ok &= c == 13;
    uint64_t t0 = cmm2->micros();
    uint32_t a = sr_ptr(sr_data, 4000);
    uint64_t t1 = cmm2->micros();
    uint32_t b = ps_loop(ps_data, 4000);
    uint64_t t2 = cmm2->micros();
    uint32_t e = sr_loop(ps_data, 4000);                        /* (SRAM code, PSRAM data) */
    uint64_t t3 = cmm2->micros();
    printf("SRAM code and data %lu us, PSRAM code and data %lu us, SRAM code with PSRAM data %lu us\n",
           (unsigned long)(t1 - t0), (unsigned long)(t2 - t1), (unsigned long)(t3 - t2));
    ok &= a == b && e != 0;
    printf("%s\n", ok ? "sramtest: all as expected" : "sramtest: FAILED");
    return ok ? 0 : 1;
}
