/* PicoMite TinyCC: <picomite/input.h> - the keyboard.
   (API version 1, as <picomite/system.h>.) */
#ifndef _PICOMITE_INPUT_H
#define _PICOMITE_INPUT_H

/* INKEY$: the next key waiting in the console's queue, or -1 if there is
   none (never waits).  The code is INKEY$'s: ASCII, and 128.. for the keys
   MMBasic names (UP 128, DOWN 129, LEFT 130, RIGHT 131, INSERT 132, DEL 127,
   HOME 134, END 135, PGUP 136, PGDN 137, F1 145 .. F12 156).
   (Also in <picomite.h>.) */
int pm_inkey(void);

/* KEYDOWN(n): n 0 the number of keys down, 1..6 the code of each, 8 the
   lock keys (as KEYDOWN) */
int pm_keydown(int n);

#endif /* _PICOMITE_INPUT_H */
