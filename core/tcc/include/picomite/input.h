/* TinyCC: <picomite/input.h>. See docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_INPUT_H
#define _PICOMITE_INPUT_H

#ifndef PM_DEFAULT
#define PM_DEFAULT (-2)
#endif

int pm_inkey(void);

int pm_keydown(int n);

int pm_mouse(int channel, int what);

int pm_usb_device(int port);

void pm_cursor_on(int no, int x, int y, int colour);
void pm_cursor_off(void);
void pm_cursor_hide(void);
void pm_cursor_show(void);
void pm_cursor_colour(int colour);
void pm_cursor_link(int link);
void pm_cursor_load(const char *file);
void pm_cursor_move(int x, int y);

#endif
