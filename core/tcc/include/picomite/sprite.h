/* PicoMite TinyCC: <picomite/sprite.h> - SPRITE.  (API version 1.)
   Each function is the SPRITE subcommand of its name, its arguments in
   their BASIC order, checked the same way and failing with the same
   MMBasic errors.  An argument BASIC lets you leave out is PM_DEFAULT
   (<picomite/graphics.h>). */
#ifndef _PICOMITE_SPRITE_H
#define _PICOMITE_SPRITE_H

#ifndef PM_DEFAULT
#define PM_DEFAULT (-2)
#endif

void pm_sprite_load(const char *file, int start, int mode);     /* SPRITE LOAD file$ [, start] [, mode] */
void pm_sprite_show(int n, int x, int y, int layer, int rotation); /* SPRITE SHOW n, x, y, layer [, rotation] */
void pm_sprite_hide(int n);                                     /* SPRITE HIDE n */
void pm_sprite_write(int n, int x, int y, int orientation);     /* SPRITE WRITE n, x, y [, orientation] */
void pm_sprite_close_all(void);                                 /* SPRITE CLOSE ALL */

#endif /* _PICOMITE_SPRITE_H */
