/* TinyCC: <picomite.h>. See docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_H
#define _PICOMITE_H

#define RGB(r, g, b) ((((r) & 0xff) << 16) | (((g) & 0xff) << 8) | ((b) & 0xff))
#define PM_BLACK 0x000000
#define PM_BLUE 0x0000ff
#define PM_GREEN 0x00ff00
#define PM_CYAN 0x00ffff
#define PM_RED 0xff0000
#define PM_MAGENTA 0xff00ff
#define PM_YELLOW 0xffff00
#define PM_WHITE 0xffffff
#define PM_NOFILL (-1)

int pm_hres(void);
int pm_vres(void);
void pm_cls(int colour);
void pm_pixel(int x, int y, int colour);
void pm_line(int x1, int y1, int x2, int y2, int width, int colour);
void pm_box(int x1, int y1, int x2, int y2, int colour);
void pm_circle(int x, int y, int r, int width, int colour, int fill);
void pm_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int colour, int fill);
void pm_refresh(void);

void pm_print(const char *s);
int pm_inkey(void);

unsigned long long pm_timer_us(void);
void pm_delay_us(unsigned int us);
void pm_pause(unsigned int ms);

#define PM_DIN 2
#define PM_DOUT 8
void pm_pin_mode(int gp, int mode);
void pm_pin_write(int gp, int value);
int pm_pin_read(int gp);

#include "picomite/gui.h"

#endif
