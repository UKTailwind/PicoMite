/* usbhid.h: the part of frank-micro's USB HID driver frank_keyboard.c calls: the keyboard's key actions (bbc_cmm2.c) */
#ifndef CMM2_USBHID_H
#define CMM2_USBHID_H
#include <stdint.h>
int usbhid_get_key_action(uint8_t *hid, int *down);
#endif
