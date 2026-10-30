/* PicoMite TinyCC: <picomite/input.h> - the keyboard, the mouse and the
   GUI cursor.  (API version 1, as <picomite/system.h>.) */
#ifndef _PICOMITE_INPUT_H
#define _PICOMITE_INPUT_H

#ifndef PM_DEFAULT
#define PM_DEFAULT (-2)
#endif

/* INKEY$: the next key waiting in the console's queue, or -1 if there is
   none (never waits).  The code is INKEY$'s: ASCII, and 128.. for the keys
   MMBasic names (UP 128, DOWN 129, LEFT 130, RIGHT 131, INSERT 132, DEL 127,
   HOME 134, END 135, PGUP 136, PGDN 137, F1 145 .. F12 156).
   (Also in <picomite.h>.) */
int pm_inkey(void);

/* KEYDOWN(n): n 0 the number of keys down, 1..6 the code of each, 8 the
   lock keys (as KEYDOWN) */
int pm_keydown(int n);

/* DEVICE(MOUSE channel, what): channel 1..4, what one of the letters of
   BASIC's DEVICE(MOUSE): 'X', 'Y' the position, 'L', 'R', 'M' the buttons
   (1 pressed), 'W' the scroll wheel, 'B' all the buttons, 'D' a double
   click of the left button (reported once), 'T' 3 if the mouse has a
   wheel - the same values, from the same mice, as in BASIC. */
int pm_mouse(int channel, int what);

/* MM.INFO(USB port): port 0 the number of USB devices, 1..4 the type of
   the device on it (USB keyboard builds; an MMBasic error elsewhere, as in
   BASIC) */
int pm_usb_device(int port);

/* GUI CURSOR - the mouse cursor drawn over the screen (VGA and HDMI builds,
   and those with GUI controls):
     pm_cursor_on(no, x, y, colour)   GUI CURSOR ON [no [, x, y [, colour]]]
                                      (PM_DEFAULT for those left out)
     pm_cursor_off()                  GUI CURSOR OFF
     pm_cursor_hide(), _show()        GUI CURSOR HIDE / SHOW
     pm_cursor_colour(c)              GUI CURSOR COLOUR c
     pm_cursor_link(1 / 0)            GUI CURSOR LINK MOUSE / UNLINK MOUSE
     pm_cursor_load(file)             GUI CURSOR LOAD file$
     pm_cursor_move(x, y)             GUI CURSOR x, y                      */
void pm_cursor_on(int no, int x, int y, int colour);
void pm_cursor_off(void);
void pm_cursor_hide(void);
void pm_cursor_show(void);
void pm_cursor_colour(int colour);
void pm_cursor_link(int link);
void pm_cursor_load(const char *file);
void pm_cursor_move(int x, int y);

#endif /* _PICOMITE_INPUT_H */
