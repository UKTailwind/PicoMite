#include <stdlib.h>
#include <math.h>
#include <picomite/graphics.h>
#include <picomite/input.h>
#include <picomite/system.h>

int main(void)
{
    pm_mode(2);

    const int w = pm_hres();
    const int h = pm_vres();
    const int cx = w / 2;
    const int cy = h / 2;
    const int cr = 7;
    const int rd = cy - cr;
    const int c = PM_CYAN;

    do {
        for (int i = 0; i < 1000; i++) {
            pm_pixel(rand() % w, rand() % h, PM_BLACK);
        }

        double t = (double)pm_timer_us() / 5000000.0;
        int x = (int)round(cx + rd * cos(3.0 * t));
        int y = (int)round(cy + rd * sin(4.0 * t));

        pm_circle(x, y, cr, 1, c, c);

    } while (pm_inkey() == -1);

    return 0;
}
