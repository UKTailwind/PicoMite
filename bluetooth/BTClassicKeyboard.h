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

#ifdef __cplusplus
}
#endif

#endif /* PICOMITEBTH || PICOMITEHDMIBTH */
#endif /* BTCLASSICKEYBOARD_H */
