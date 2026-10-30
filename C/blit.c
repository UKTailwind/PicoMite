/* blit.c - the animation scheme of MMbg: the background is drawn once into
   the framebuffer F and shown; a ball then moves over the display N, and
   BLIT FRAMEBUFFER F, N puts the background back where it was.  Then the
   time of a 29 x 29 BLIT FRAMEBUFFER, and an error caught with pm_try.
   Any key ends the animation. */
#include <stdio.h>
#include <picomite/system.h>
#include <picomite/basic.h>
#include <picomite/graphics.h>
#include <picomite/input.h>
#include <picomite/framebuffer.h>

static void blit_t(void *p)
{
    (void)p;
    pm_blit_framebuffer('T', 'N', 0, 0, 0, 0, 10, 10, PM_DEFAULT); /* no top layer: an error */
}

int main(void)
{
    int w, h, x = 20, y = 20, dx = 3, dy = 2, ox = -1, oy = -1, i;
    unsigned long long t0;

    pm_mode(2);
    w = pm_hres(), h = pm_vres();
    pm_framebuffer_create('F');
    pm_framebuffer_write('F'); /* the background, into F */
    pm_cls(PM_BLUE);
    for (i = 0; i < 12; i++)
        pm_draw_box(i * w / 12, h / 2 - 20 + (i & 1) * 40, w / 12 - 4, 30, 1, PM_WHITE, i & 1 ? PM_GREEN : PM_RED);
    pm_text(w / 2, 8, "BLIT FRAMEBUFFER F, N", "CT", PM_DEFAULT, PM_DEFAULT, PM_YELLOW, PM_TRANSPARENT);
    pm_framebuffer_copy('F', 'N', 1);
    pm_framebuffer_write('N'); /* the ball, onto the display */

    while (pm_inkey() < 0)
    {
        if (ox >= 0) /* the background back over the old ball */
            pm_blit_framebuffer('F', 'N', ox, oy, ox, oy, 29, 29, PM_DEFAULT);
        pm_circle(x + 14, y + 14, 13, 1, PM_WHITE, PM_YELLOW);
        ox = x, oy = y;
        x += dx, y += dy;
        if (x < 0 || x > w - 29)
            dx = -dx, x += 2 * dx;
        if (y < 0 || y > h - 29)
            dy = -dy, y += 2 * dy;
        pm_pause(15);
    }

    t0 = pm_timer_us();
    for (i = 0; i < 500; i++)
        pm_blit_framebuffer('F', 'N', 100, 100, 100, 100, 29, 29, PM_DEFAULT);
    printf("BLIT FRAMEBUFFER 29 x 29: %llu us\n", (pm_timer_us() - t0) / 500);
    t0 = pm_timer_us();
    for (i = 0; i < 500; i++)
        pm_execute("BLIT FRAMEBUFFER F, N, 100, 100, 100, 100, 29, 29");
    printf("the same through pm_execute: %llu us\n", (pm_timer_us() - t0) / 500);

    if (pm_try(blit_t, NULL))
        printf("caught: %s\n", pm_errmsg());
    pm_framebuffer_close(PM_DEFAULT);
    return 0;
}
