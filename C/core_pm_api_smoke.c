/* RP2040/RP2350: external relocatable ARM ELF tests common named exports.
 * Compile with -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -ffreestanding
 * -fno-unwind-tables -O1 -c -I.  Do not link an ET_EXEC file.
Example:
  C:\Users\New\.pico-sdk\toolchain\13_3_Rel1\bin\arm-none-eabi-gcc \
   -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -ffreestanding \
   -fno-unwind-tables -O1 -I. -c C\core_pm_api_smoke.c -o core_pm_api_smoke.elf
 */
#include "core/tcc/include/picomite/graphics.h"
extern const void *const __picomite_calltable[];

static void say(const char *s)
{
    void (*put)(int, int) = (void (*)(int, int))__picomite_calltable[1];
    while (*s) put((unsigned char)*s++, 1);
}

int main(int argc, char **argv)
{
    int w = pm_hres(), h = pm_vres();
    (void)argc;
    (void)argv;
    if (w <= 20 || h <= 20) return 1;
    pm_cls(0);
    pm_line(10, 10, w - 10, h - 10, 1, 0xffffff);
    pm_pixel(w / 2, h / 2, 0xff0000);
    pm_refresh();
    say("CORE PM API OK\r\n");
    return 0;
}
