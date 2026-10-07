/*
 * BTKeyboard.h — BLE HID host (HOG) keyboard input for PicoMiteBTH.
 *
 * Pairs with a BLE keyboard and feeds incoming HID reports into the
 * same console-RX path the USB HID host uses (USR_KEYBRD_ProcessData /
 * process_kbd_report). The USB CDC console stays as-is — this module
 * is an *input source*, not a console replacement.
 *
 * Step 1 (this file): brings up cyw43_arch + btstack HCI and blinks
 * the cyw43 LED to confirm Bluetooth firmware actually loaded. No
 * scanning, no pairing, no HID yet.
 */

#ifndef BTKEYBOARD_H
#define BTKEYBOARD_H

#if defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Brings up cyw43_arch + btstack, powers on HCI. Call once after the
   rest of the hardware is up (same place WEB calls cyw43_arch_init,
   same place PICOMITEBT calls bt_console_init). */
void bt_keyboard_init(void);

/* Cooperative poll (ProcessBT() in Hardware_Includes.h) — called from the
   same places as the WiFi builds' ProcessWeb(). Pumps cyw43_arch_poll on
   the polled async_context, prints any bt_notice() messages and runs a
   heartbeat that toggles the cyw43 LED so we can visually confirm the BT
   firmware is alive. */
void bt_keyboard_poll(void);

/* Post a console message from a btstack callback. Callbacks run inside the
   poll, so they must not print; the next bt_keyboard_poll() prints it. */
void bt_notice(const char *msg);

/* True once HCI has reached HCI_STATE_WORKING. */
bool bt_keyboard_ready(void);

/* BTAudio.c calls this (btstack context) when a speaker stream starts or
   stops, so a running LE scan picks up the matching duty cycle. */
void bt_keyboard_scan_duty_changed(void);

/* Diagnostics for BLUETOOTH STATUS (the save counters reset on each call). */
void bt_keyboard_stats(uint32_t *saves, uint32_t *save_max_us, uint32_t *save_total_us,
                       uint32_t *pairings, uint32_t *reencryptions);
/* The last keyboard pairing - address and whether a bond was stored. */
void bt_keyboard_last_pairing(char *buf, int len);

/* MM.INFO(BLUETOOTH KEYBOARD): the connected keyboard's address, or "". */
void bt_keyboard_address(char *buf);

/* BLUETOOTH FORGET: delete keyboard bonds (addr NULL = all), dropping the
   keyboard's link if it is one of them. Call with the async-context lock. */
void bt_keyboard_forget(const uint8_t *addr);

#ifdef __cplusplus
}
#endif

#endif /* PICOMITEBTH || PICOMITEHDMIBTH */
#endif /* BTKEYBOARD_H */
