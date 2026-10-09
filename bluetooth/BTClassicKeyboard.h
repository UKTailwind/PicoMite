/*
 * BTClassicKeyboard.h - Bluetooth Classic HID keyboards (and the touchpad
 * or mouse built into one) for the BT host builds (PICOMITEBTH,
 * PICOMITEHDMIBTH).
 *
 * Found by BLUETOOTH SCAN (its Class of Device says keyboard), paired by
 * BLUETOOTH CONNECT. Reports go to the same decoders as USB and BLE
 * keyboards, so OPTION KEYBOARD layouts apply.
 */
#ifndef BTCLASSICKEYBOARD_H
#define BTCLASSICKEYBOARD_H
#if defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the HID host. Called from bt_audio_init(), after sdp_init()
   and before HCI is powered on. */
void bt_ckbd_init(void);

/* Class of Device major class Peripheral with the keyboard bit set. */
bool bt_ckbd_cod_is_keyboard(uint32_t cod);

/* True for the remembered Classic keyboard's address. */
bool bt_ckbd_is_remembered(const uint8_t *addr);

/* BLUETOOTH CONNECT on a keyboard: connects and pairs it, printing the
   passkey to type. Interpreter context; reports failure with error(). */
void bt_ckbd_connect(const uint8_t *addr);

/* True while CONNECT is pairing this address: BTAudio.c's pairing
   handler leaves the request to this module. */
bool bt_ckbd_owns_pairing(const uint8_t *addr);

/* MM.INFO(BLUETOOTH KEYBOARD): the connected Classic keyboard's address,
   or "". */
void bt_ckbd_address(char *buf);

/* BLUETOOTH STATUS: one line describing the Classic keyboard. */
void bt_ckbd_status(char *buf, int len);

/* BLUETOOTH FORGET (addr NULL = all): drop the keyboard's link if it is
   the one, and forget it. Call with the async-context lock held. */
void bt_ckbd_forget(const uint8_t *addr);

/* Show the lock keys (kbd_lock_leds() bits) on the Classic keyboard.
   btstack context: KeyboardMap.c calls it for a lock key from this keyboard. */
void bt_ckbd_set_leds(uint8_t leds);

/* Where a keyboard's LED output report keeps Num, Caps and Scroll Lock,
   from its HID report descriptor - shared with the BLE keyboard. Bit
   positions count from after the report ID. */
typedef struct
{
    uint8_t present; /* the descriptor has an LED output report */
    uint8_t has_id;
    uint8_t id;
    uint8_t len;     /* bytes, without the report ID */
    uint16_t pos[3]; /* Num, Caps, Scroll; 0xFFFF where absent */
} bt_led_layout_t;

void bt_hid_led_layout(const uint8_t *descriptor, uint16_t len, bt_led_layout_t *l);

/* The LED report for leds (kbd_lock_leds() bits) into out (8 bytes);
   returns its length. */
uint8_t bt_hid_led_report(const bt_led_layout_t *l, uint8_t leds, uint8_t *out);

/* Where a keyboard's input report keeps its modifier byte and key slots,
   from its HID report descriptor - shared with the BLE keyboard. Bit
   positions count from after the report ID. */
typedef struct
{
    uint8_t present;    /* the descriptor has a keyboard report with key slots */
    uint8_t has_id;
    uint8_t id;
    uint8_t keys;       /* 8-bit key slots, at most the 6 a boot report holds */
    uint16_t mod_pos;   /* the modifier byte (usages E0-E7); 0xFFFF if absent */
    uint16_t keys_pos;  /* the first key slot */
    uint8_t len;        /* the report's length in bytes, without the ID */
    uint8_t len_unique; /* no other input report has this length */
} bt_kbd_layout_t;

void bt_hid_kbd_layout(const uint8_t *descriptor, uint16_t len, bt_kbd_layout_t *k);

/* The boot keyboard report (modifier, reserved, 6 keys) for a keyboard
   report p of n bytes, without its report ID. */
void bt_hid_kbd_report(const bt_kbd_layout_t *k, const uint8_t *p, uint16_t n, uint8_t out[8]);

#ifdef __cplusplus
}
#endif

#endif /* PICOMITEBTH || PICOMITEHDMIBTH */
#endif /* BTCLASSICKEYBOARD_H */
