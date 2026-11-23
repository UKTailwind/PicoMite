/* ps2kbd_wrapper.h: no PS/2 keyboard here: the USB keyboard comes by usbhid_wrapper.c */
#ifndef CMM2_PS2KBD_WRAPPER_H
#define CMM2_PS2KBD_WRAPPER_H
#define ps2kbd_init() do { } while(0)
#define ps2kbd_tick() do { } while(0)
#define ps2kbd_get_key(pressed, key) 0
#endif
