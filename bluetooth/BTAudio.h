/*
 * BTAudio.h — Bluetooth Classic A2DP source (speaker / headphone output)
 * for the BT host builds (PICOMITEBTH, PICOMITEHDMIBTH).
 *
 * Shares the btstack instance BTKeyboard.c brings up; bt_audio_init() must
 * be called from bt_keyboard_init() before HCI is powered on.
 */

#ifndef BTAUDIO_H
#define BTAUDIO_H

#if defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the Classic services (SDP, A2DP source endpoint) and the
   Classic GAP settings. Called once, after l2cap_init()/sm_init() and
   before hci_power_control(HCI_POWER_ON). */
void bt_audio_init(void);

/* True while an A2DP media stream is running - BTKeyboard lowers its LE
   scan duty cycle so the radio has time for the audio link. */
bool bt_audio_streaming(void);

/* Main-loop service for PLAY output (OPTION AUDIO BLUETOOTH): starts and
   suspends the stream, and discards sound while no speaker takes it.
   Called from bt_keyboard_poll(). */
void bt_audio_service(void);

/* The BLUETOOTH command. */
void cmd_bluetooth(void);

/* MM.INFO(BLUETOOTH SPEAKER | KEYBOARD): the connected device's address in
   out, or "" when none. */
void bt_info(unsigned char *tp, char *out);

#ifdef __cplusplus
}
#endif

#endif /* PICOMITEBTH || PICOMITEHDMIBTH */
#endif /* BTAUDIO_H */
