/* PicoMite TinyCC: the firmware's display, keyboard, timing and pins.
   Colours are 24-bit RGB as in MMBasic.  An MMBasic error in any of these
   (no display, an invalid pin...) ends the program with that error, and
   CTRL-C ends it as it ends a BASIC program. */
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

/* display (MM.HRES, MM.VRES, CLS, PIXEL, LINE, BOX filled, CIRCLE, TRIANGLE) */
int pm_hres(void);
int pm_vres(void);
void pm_cls(int colour);
void pm_pixel(int x, int y, int colour);
void pm_line(int x1, int y1, int x2, int y2, int width, int colour);
void pm_box(int x1, int y1, int x2, int y2, int colour);
void pm_circle(int x, int y, int r, int width, int colour, int fill); /* fill: a colour or PM_NOFILL */
void pm_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int colour, int fill);
void pm_refresh(void); /* push the drawing to buffered panels */

/* console */
void pm_print(const char *s);
int pm_inkey(void); /* a key, or -1 if none is waiting (INKEY$) */

/* time */
unsigned long long pm_timer_us(void); /* microseconds since power-up */
void pm_delay_us(unsigned int us);
void pm_pause(unsigned int ms); /* PAUSE */

/* GPIO by GP number (SETPIN / PIN()) */
#define PM_DIN 2  /* digital input */
#define PM_DOUT 8 /* digital output */
void pm_pin_mode(int gp, int mode);
void pm_pin_write(int gp, int value);
int pm_pin_read(int gp);

#endif /* _PICOMITE_H */
