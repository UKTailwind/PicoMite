/* ARMv6-M ET_REL smoke test: same object for RP2040 and RP2350.
 * arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft \
 *   -ffreestanding -fno-unwind-tables -O1 -Icore/tcc/include -c \
 *   C/core_pm_extended_smoke.c -o core_pm_extended_smoke.elf
 */
#include <picomite.h>
#include <picomite/graphics.h>
#include <picomite/input.h>
#include <picomite/framebuffer.h>
#include <picomite/basic.h>

int main(int argc, char **argv)
{
    pm_framebuffer fb;
    int w, h;
    (void)argc; (void)argv;
    w = pm_hres(); h = pm_vres();
    if (w < 40 || h < 40) return 1;
    pm_cls(PM_BLACK);
    pm_draw_box(8, 8, w - 16, h - 16, 1, PM_WHITE, PM_NOFILL);
    pm_draw_rbox(16, 16, w - 32, h - 32, 8, PM_GREEN, PM_NOFILL);
    pm_text(24, 24, "Common C API", "LT", 1, 1, PM_YELLOW, PM_BLACK);
    pm_refresh();
    fb.size = sizeof(fb);
    if (pm_get_framebuffer(&fb) == 0 && fb.pixels == 0) return 2;
    if (pm_keydown(0) < 0) return 3;
    pm_print("CORE EXTENDED PM API OK\r\n");
    return 0;
}
