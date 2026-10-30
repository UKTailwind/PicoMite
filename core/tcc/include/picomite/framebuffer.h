/* PicoMite TinyCC: <picomite/framebuffer.h> - FRAMEBUFFER and BLIT.
   (API version 3.)  Each function is the command of its name, its
   arguments in their BASIC order, checked the same way and failing with
   the same MMBasic errors (pm_try in <picomite/basic.h> is ON ERROR SKIP).
   A buffer is its BASIC letter: 'N' the display, 'F' the framebuffer, 'L'
   the layer, and on the RP2350 VGA/HDMI builds 'T' the top layer and '2'
   framebuffer 2.  An argument BASIC lets you leave out is PM_DEFAULT.
   Drawing goes to the buffer FRAMEBUFFER WRITE chose, as in BASIC. */
#ifndef _PICOMITE_FRAMEBUFFER_H
#define _PICOMITE_FRAMEBUFFER_H

#ifndef PM_DEFAULT
#define PM_DEFAULT (-2)
#endif

void pm_framebuffer_create(int buffer);           /* FRAMEBUFFER CREATE ('F') / CREATE 2 ('2') */
void pm_framebuffer_layer(int transparent);       /* FRAMEBUFFER LAYER [transparent] */
void pm_framebuffer_layer_top(int transparent);   /* FRAMEBUFFER LAYER TOP [transparent] */
void pm_framebuffer_write(int buffer);            /* FRAMEBUFFER WRITE N | F | L | T | 2 */
void pm_framebuffer_copy(int from, int to, int at_blank); /* FRAMEBUFFER COPY from, to [, B] */
/* FRAMEBUFFER MERGE [colour] [, mode [, rate]]: mode 'B', 'R', 'A' (the
   LCD builds) or PM_DEFAULT; on VGA/HDMI only 'B' (wait for the blanking) */
void pm_framebuffer_merge(int colour, int mode, int rate);
void pm_framebuffer_sync(void);                   /* FRAMEBUFFER SYNC (LCD builds) */
void pm_framebuffer_wait(void);                   /* FRAMEBUFFER WAIT */
void pm_framebuffer_close(int buffer);            /* FRAMEBUFFER CLOSE [F | L | T | 2]; PM_DEFAULT: all */

/* BLIT FRAMEBUFFER from, to, x1, y1, x2, y2, w, h [, colour]: the area at
   x1, y1 of from to x2, y2 of to; colour (or PM_DEFAULT) is transparent.
   Calls the firmware directly: fit for every frame of an animation. */
void pm_blit_framebuffer(int from, int to, int x1, int y1, int x2, int y2, int w, int h, int colour);

void pm_blit(int x1, int y1, int x2, int y2, int w, int h); /* BLIT x1, y1, x2, y2, w, h */
void pm_blit_read(int n, int x, int y, int w, int h);       /* BLIT READ n, x, y, w, h */
void pm_blit_write(int n, int x, int y, int mode);          /* BLIT WRITE n, x, y [, mode] */
/* BLIT LOAD n, file$ [, x] [, y] [, w] [, h]: a part of a BMP file */
void pm_blit_load(int n, const char *file, int x, int y, int w, int h);
void pm_blit_close(int n);                                  /* BLIT CLOSE n */

#endif /* _PICOMITE_FRAMEBUFFER_H */
