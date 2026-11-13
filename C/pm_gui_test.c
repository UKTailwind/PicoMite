#include <picomite/system.h>
#include <picomite/graphics.h>
#include <picomite/gui.h>
#include <picomite/basic.h>
#include <stdio.h>
#include <string.h>

/* RP2040 VGA has the GUI cursor, but not the Ctrl[] GUI-control heap. */
static void gui_command(void *arg) { pm_gui((const char *)arg); }
static int cursor_smoke(void)
{
    static const char *const commands[] = {
        "CURSOR ON", "CURSOR 20,20", "CURSOR OFF"
    };
    unsigned i;
    for (i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        int err = pm_try(gui_command, (void *)commands[i]);
        if (err) {
            pm_cls(0);
            printf("GUI test: FAIL cursor command '%s': %d %s\n",
                   commands[i], err, pm_errmsg());
            (void)pm_try(gui_command, (void *)"CURSOR OFF");
            return 1;
        }
    }
    pm_cls(0);
    printf("GUI test: PASS (VGA cursor commands; GUI controls unavailable)\n");
    return 0;
}

static int downs, ups;
static void gui_event(int kind, int x, int y, int ref)
{
    (void)x; (void)y;
    if (kind == PM_GUI_DOWN && ref == 1) ++downs;
    if (kind == PM_GUI_UP && ref == 1) ++ups;
}

int main(void)
{
    pm_info info = {0};
    double v;
    info.size = sizeof(info);
    if (pm_get_info(&info) || !(info.caps & PM_CAP_DISPLAY)) {
        printf("GUI test: SKIP (display unavailable)\n");
        return 0;
    }
    if (!(info.caps & PM_CAP_GUI)) {
        if (info.caps & PM_CAP_VGA) return cursor_smoke();
        pm_cls(0);
        printf("GUI test: SKIP (GUI controls unavailable)\n");
        return 0;
    }
    /* Compiled-in GUICONTROLS does not imply that the user has enabled
       OPTION GUI CONTROLS.  The firmware allocates Ctrl[] on boot. */
    {
        int err = pm_try(gui_command, (void *)"BUTTON #1,\"C test\",10,10,80,30");
        if (err) {
            pm_cls(0);
            if (strstr(pm_errmsg(), "No memory allocated for GUI controls"))
                printf("GUI test: CONFIG REQUIRED: OPTION GUI CONTROLS 1 "
                       "at BASIC prompt (reboots)\n");
            else
                printf("GUI test: FAIL BUTTON creation: %d %s\n",
                       err, pm_errmsg());
            return 1;
        }
    }
    if (pm_ctrlval_get_number(1, &v) || v != 0 ||
        pm_ctrlval_set_number(1, 1) ||
        pm_ctrlval_get_number(1, &v) || v != 1 ||
        pm_ctrlval_set_number(1, 0)) {
        pm_gui("DELETE #1");
        pm_cls(0);
        printf("GUI test: FAIL CTRLVAL\n");
        return 1;
    }
    pm_gui_on_event(gui_event);
    pm_gui("CURSOR ON");
    pm_gui("CURSOR 20,20");
    pm_gui("CLICK DOWN");
    pm_pause(100);
    pm_gui("CLICK UP");
    pm_pause(100);
    pm_gui_poll();
    pm_gui_on_event(NULL);
    pm_gui("CURSOR OFF");
    pm_gui("DELETE #1");
    pm_cls(0);
    printf("GUI test: %s (down=%d, up=%d)\n",
           (downs && ups) ? "PASS" : "FAIL", downs, ups);
    return (downs && ups) ? 0 : 1;
}
