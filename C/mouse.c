#include <stdio.h>
#include <picomite/system.h>
#include <picomite/basic.h>
#include <picomite/graphics.h>
#include <picomite/input.h>

/* mouse.c - find the mouse as MMbg does (MM.INFO(USB n) = 2 is a mouse on
   the USB builds; else channel 2) and link the GUI cursor to it.  Without a
   mouse (LINK MOUSE fails) the program moves the cursor itself, from the
   top left corner down the diagonal, to show pm_cursor_move.  Then it shows
   the mouse's X, Y and buttons; a pixel where the left button is held, a
   message for a double click.  ESC ends. */
int main(void)
{
    long long t;
    int ch = 2, n, x = -1, y = -1, l = -1;
    if (pm_eval_int_try("MM.INFO(USB 0)", &t) == 0) /* (a USB keyboard build) */
        for (n = 1; n <= 4; n++)
            if (pm_usb_device(n) == 2)
                ch = n;
    printf("mouse channel %d\n", ch);
    pm_cls(PM_BLACK);
    pm_cursor_on(0, PM_DEFAULT, PM_DEFAULT, PM_DEFAULT);
    if (pm_execute_try("GUI CURSOR LINK MOUSE"))
    {
        printf("%s - moving the cursor by program\n", pm_errmsg());
        for (n = 0; n < 200; n++)
            pm_cursor_move(n, n), pm_pause(10);
    }
    while (pm_inkey() != 27)
    {
        int nx = pm_mouse(ch, 'X'), ny = pm_mouse(ch, 'Y'), nl = pm_mouse(ch, 'L');
        if (nx != x || ny != y || nl != l)
        {
            char b[48];
            x = nx, y = ny, l = nl;
            snprintf(b, sizeof b, "X %4d  Y %4d  L %d  R %d  ", x, y, l, pm_mouse(ch, 'R'));
            pm_text(0, 0, b, "LT", PM_DEFAULT, PM_DEFAULT, PM_WHITE, PM_BLACK);
            if (l)
                pm_pixel(x, y, PM_YELLOW);
        }
        if (pm_mouse(ch, 'D'))
            pm_text(0, 16, "double click", "LT", PM_DEFAULT, PM_DEFAULT, PM_CYAN, PM_BLACK);
        pm_pause(10);
    }
    pm_cursor_off();
    return 0;
}
