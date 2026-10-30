/* PicoMite TinyCC: <picomite/graphics.h> - the display.
   Colours are 24-bit RGB as in MMBasic.  An MMBasic error in any of these
   (no display, an invalid font...) ends the program with that error, or in
   a C CSUB is reported at its call. */
#ifndef _PICOMITE_GRAPHICS_H
#define _PICOMITE_GRAPHICS_H

#ifndef RGB
#define RGB(r, g, b) ((((r) & 0xff) << 16) | (((g) & 0xff) << 8) | ((b) & 0xff))
#endif
#ifndef PM_BLACK
#define PM_BLACK 0x000000
#define PM_BLUE 0x0000ff
#define PM_GREEN 0x00ff00
#define PM_CYAN 0x00ffff
#define PM_RED 0xff0000
#define PM_MAGENTA 0xff00ff
#define PM_YELLOW 0xffff00
#define PM_WHITE 0xffffff
#define PM_NOFILL (-1)
#endif
#define PM_TRANSPARENT (-1) /* pm_text's background */

/* PM_DEFAULT for an argument is the argument left out in BASIC: the current
   font, scale or colour (FONT, COLOUR), BOX's line width 1 and no fill. */
#define PM_DEFAULT (-2)

/* drawing (also in <picomite.h>) */
int pm_hres(void);
int pm_vres(void);
void pm_cls(int colour);
void pm_pixel(int x, int y, int colour);
void pm_line(int x1, int y1, int x2, int y2, int width, int colour);
void pm_box(int x1, int y1, int x2, int y2, int colour); /* filled */
void pm_circle(int x, int y, int r, int width, int colour, int fill);
void pm_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int colour, int fill);
void pm_refresh(void); /* push the drawing to buffered panels */

/* TEXT x, y, s$, align$, font, scale, c, bc - as TEXT, argument by
   argument.  align is TEXT's justification ("LT", "CM", "RB", with an
   optional orientation letter "N" "V" "I" "U" "D"), or NULL or "" for the
   default "LT".  font is 1..16, scale 1..15; bc may be PM_TRANSPARENT; any
   of font, scale, c and bc may be PM_DEFAULT. */
void pm_text(int x, int y, const char *s, const char *align, int font, int scale, int c, int bc);
int pm_font_width(int font, int scale);  /* pixels per character (PM_DEFAULT: current) */
int pm_font_height(int font, int scale);

/* BOX x, y, w, h, lw, c, fill - as BOX: a box w by h from (x, y), negative
   w or h going left or up, a border lw pixels wide (0 for none) and fill a
   colour or PM_NOFILL.  lw, c and fill may be PM_DEFAULT (1, the current
   colour, no fill).  (pm_box above is the older filled-corners form.) */
void pm_draw_box(int x, int y, int w, int h, int lw, int c, int fill);

/* MODE n - as MODE on the VGA and HDMI builds: the display mode, the
   screen cleared.  An MMBasic error on other builds. */
void pm_mode(int mode);

/* COLOUR fc [, bc] - the colours that PM_DEFAULT stands for, as COLOUR:
   RGB, or with OPTION LEGACY ON the colour numbers 0..15.  bc may be
   PM_DEFAULT (left as it is). */
void pm_colour(int fc, int bc);

/* LOAD IMAGE file$ [, x] [, y] [, mode]  and  LOAD JPG file$ [, x] [, y],
   any of x, y and mode PM_DEFAULT (as left out) */
void pm_load_image(const char *file, int x, int y, int mode);
void pm_load_jpg(const char *file, int x, int y);

/* The framebuffer drawing goes to: the display memory itself on VGA and
   HDMI, or the buffer of FRAMEBUFFER CREATE / WRITE.  Writing pixels here
   is much faster than pm_pixel(); call pm_refresh() after it for buffered
   panels.  The buffer and its format change with MODE, RESOLUTION and
   FRAMEBUFFER WRITE: get it again after any of those. */
#define PM_FB_MONO   1  /* 1 bit: pixel x is bit (x & 7) of byte x / 8 */
#define PM_FB_RGB121 4  /* 4 bits: even x in the low nibble, odd x in the high one */
#define PM_FB_RGB332 8  /* 1 byte a pixel */
#define PM_FB_RGB555 16 /* 2 bytes a pixel (little-endian) */

typedef struct pm_framebuffer
{
    unsigned size;        /* in: sizeof(pm_framebuffer) */
    void *pixels;         /* row 0, pixel 0 */
    int width, height;    /* pixels */
    int stride;           /* bytes from one row to the next */
    int format;           /* PM_FB_...: also the bits per pixel */
} pm_framebuffer;

/* 0, or -1 if drawing goes straight to a panel with no framebuffer here
   (an SPI LCD with no FRAMEBUFFER CREATE) */
int pm_get_framebuffer(pm_framebuffer *fb);

/* an RGB colour as a pixel value of the framebuffer's current format */
unsigned pm_fb_colour(int rgb);

#endif /* _PICOMITE_GRAPHICS_H */
