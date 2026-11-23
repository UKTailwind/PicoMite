/* fwdouble.S: double precision by the firmware's routines (tools/sdk). MIT License, as pmimg.h.

   Each routine of loader/pmdouble.h (__aeabi_dadd... sqrt, sin, pow...) is defined here as a jump through the
   services' table of doubles to the firmware's own, the Pico SDK's DCP-accelerated one, so libgcc's and newlib's
   software versions are never linked in (they are several times slower on the Cortex-M33). The arguments and the
   result stay in the registers (softfp), so the jump changes nothing: ip is the only register used. cmm2_doubles is
   set by crt0.c before any constructor runs. (No '#' before an immediate: in a macro it would be cpp's.) */
#include "../../loader/pmdouble.h"

    .syntax unified
    .thumb
    .text

#define FW(name) \
    .section .text.fw_##name, "ax", %progbits; \
    .global name; \
    .type name, %function; \
    .thumb_func; \
name: \
    ldr ip, =cmm2_doubles; \
    ldr ip, [ip]; \
    ldr ip, [ip, #(4 * fw_index_##name)]; \
    bx ip; \
    .ltorg;

/* each name's index in the table: its place in the list */
    .set fw_next, 0
#define FW_SETINDEX(name) .set fw_index_##name, fw_next; .set fw_next, fw_next + 1;
PMDOUBLE_LIST(FW_SETINDEX)

#define FW_DEFINE(name) FW(name)
PMDOUBLE_LIST(FW_DEFINE)
