/* TinyCC: <picomite/graphics.h>. See docs/PicoMite-native-C-support.md. */
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
#define PM_TRANSPARENT (-1)

#define PM_DEFAULT (-2)

int pm_hres(void);
int pm_vres(void);
void pm_cls(int colour);
void pm_pixel(int x, int y, int colour);
void pm_line(int x1, int y1, int x2, int y2, int width, int colour);
void pm_box(int x1, int y1, int x2, int y2, int colour);
void pm_circle(int x, int y, int r, int width, int colour, int fill);
void pm_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int colour, int fill);
void pm_refresh(void);

void pm_text(int x, int y, const char *s, const char *align, int font, int scale, int c, int bc);
int pm_font_width(int font, int scale);
int pm_font_height(int font, int scale);

void pm_draw_box(int x, int y, int w, int h, int lw, int c, int fill);

void pm_draw_rbox(int x, int y, int w, int h, int r, int c, int fill);

void pm_mode(int mode);

void pm_colour(int fc, int bc);

void pm_load_image(const char *file, int x, int y, int mode);
void pm_load_jpg(const char *file, int x, int y);

#define PM_FB_MONO   1
#define PM_FB_RGB121 4
#define PM_FB_RGB332 8
#define PM_FB_RGB555 16

typedef struct pm_framebuffer
{
    unsigned size;
    void *pixels;
    int width, height;
    int stride;
    int format;
} pm_framebuffer;

int pm_get_framebuffer(pm_framebuffer *fb);

unsigned pm_fb_colour(int rgb);

#endif
