#ifndef _PICOMITE_INPUT_H
#define _PICOMITE_INPUT_H

#ifndef PM_DEFAULT
#define PM_DEFAULT (-2)
#endif

int pm_inkey(void);

int pm_keydown(int n);

int pm_mouse(int channel, int what);

int pm_usb_device(int port);

#include <stdint.h>
/* Modifier bit positions match KEYDOWN(7) on HID; PS/2 also reports
 * physical right Ctrl independently, which legacy KEYDOWN(7) does not. */
#define PM_KMOD_LALT    0x01u
#define PM_KMOD_LCTRL   0x02u
#define PM_KMOD_LGUI    0x04u
#define PM_KMOD_LSHIFT  0x08u
#define PM_KMOD_RALT    0x10u
#define PM_KMOD_RCTRL   0x20u
#define PM_KMOD_RGUI    0x40u
#define PM_KMOD_RSHIFT  0x80u

/* Event flags. EMIT means the returned byte enters the console path;
 * otherwise the callback is notification-only and its return is ignored.
 * DOWN/UP describe physical transitions. REPEAT is a repeated DOWN.
 * Key is a HID usage (USB/BT, including modifiers 0xE0..0xE7), or a
 * PS/2 set-2 scan with bit 8 set for its E0 prefix (PM_REMAP_PS2).
 * PM_REMAP_MEDIA uses a HID Consumer usage instead of keyboard usage. */
#define PM_REMAP_EMIT   0x01u
#define PM_REMAP_DOWN   0x02u
#define PM_REMAP_UP     0x04u
#define PM_REMAP_REPEAT 0x08u
#define PM_REMAP_PS2    0x10u
#define PM_REMAP_MEDIA  0x20u

/* Physical keyboards only, not CDC/UART/network terminal input.
 * On EMIT, code is the legacy PicoMite byte and return 0 suppresses it.
 * Without EMIT, code=0 and the return value is ignored (including plain
 * Ctrl/Shift/Alt presses and all key-release notifications).
 * NULL installs the identity mapping. The previous callback is returned.
 * The caller owns registration and lifetime; no automatic reset on
 * BASIC END/RUN or resident stop. A resident must restore its predecessor
 * before unloading. PS/2 calls from IRQ: no blocking, BASIC or heap. */
typedef uint8_t (*pm_remap_fn)(uint8_t code, uint16_t key,
                               uint8_t modifiers, uint8_t flags);
pm_remap_fn pm_set_remap_fn(pm_remap_fn fn);
/* Override firmware console/editor isprint handling (not libc isprint).
 * Only byte values 0..255 are passed by the firmware; the default uses
 * isprint((unsigned char)code). NULL restores the default. As with remap,
 * ownership and explicit restoration are the resident's responsibility. */
typedef int (*pm_isprint_fn)(int code);
pm_isprint_fn pm_set_isprint_fn(pm_isprint_fn fn);
int pm_isprint(int code);
#define PM_GAMEPAD_PS4       128u
#define PM_GAMEPAD_PS3       129u
#define PM_GAMEPAD_GENERIC   130u
#define PM_GAMEPAD_XBOX      131u
#define PM_GAMEPAD_SWITCHPRO 135u
#define PM_GAMEPAD_BLE       0x4254u

typedef struct pm_gamepad_state {
    unsigned size;
    unsigned type;
    unsigned vid, pid;
    uint16_t buttons;
    uint8_t hat;
    uint8_t reserved;
    int lx, ly, rx, ry, l, r;
    int16_t gyro[3], accel[3];
} pm_gamepad_state;

int pm_gamepad_get(int channel, pm_gamepad_state *out);
void pm_gamepad_on_change(int channel, unsigned mask,
                          void (*callback)(int channel, unsigned buttons));
void pm_gamepad_poll(void);

#define PM_GPIO_RISE 1
#define PM_GPIO_FALL 2
#define PM_GPIO_BOTH 3
#define PM_GPIO_NOPULL 0
#define PM_GPIO_PULLUP 1
#define PM_GPIO_PULLDOWN 2
void pm_gpio_on_edge(int,int,int,void (*)(int,int));
void pm_gpio_poll(void);

void pm_cursor_on(int no, int x, int y, int colour);
void pm_cursor_off(void);
void pm_cursor_hide(void);
void pm_cursor_show(void);
void pm_cursor_colour(int colour);
void pm_cursor_link(int link);
void pm_cursor_load(const char *file);
void pm_cursor_move(int x, int y);

#include "gui.h"

#endif
