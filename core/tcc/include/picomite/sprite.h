/* TinyCC: <picomite/sprite.h>. See docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_SPRITE_H
#define _PICOMITE_SPRITE_H

#ifndef PM_DEFAULT
#define PM_DEFAULT (-2)
#endif

void pm_sprite_load(const char *file, int start, int mode);
void pm_sprite_show(int n, int x, int y, int layer, int rotation);
void pm_sprite_hide(int n);
void pm_sprite_write(int n, int x, int y, int orientation);
void pm_sprite_close_all(void);

#endif
