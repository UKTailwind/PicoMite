/* TinyCC: <picomite/framebuffer.h>. See docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_FRAMEBUFFER_H
#define _PICOMITE_FRAMEBUFFER_H

#ifndef PM_DEFAULT
#define PM_DEFAULT (-2)
#endif

void pm_framebuffer_create(int buffer);
void pm_framebuffer_layer(int transparent);
void pm_framebuffer_layer_top(int transparent);
void pm_framebuffer_write(int buffer);
void pm_framebuffer_copy(int from, int to, int at_blank);

void pm_framebuffer_merge(int colour, int mode, int rate);
void pm_framebuffer_sync(void);
void pm_framebuffer_wait(void);
void pm_framebuffer_close(int buffer);

void pm_blit_framebuffer(int from, int to, int x1, int y1, int x2, int y2, int w, int h, int colour);

void pm_blit(int x1, int y1, int x2, int y2, int w, int h);
void pm_blit_read(int n, int x, int y, int w, int h);
void pm_blit_write(int n, int x, int y, int mode);

void pm_blit_load(int n, const char *file, int x, int y, int w, int h);
void pm_blit_close(int n);

#endif
