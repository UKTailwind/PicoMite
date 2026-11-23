/* audio.h for the CMM2: frank-quest's I2S driver calls (drivers/audio/audio.h), made on the loader's sound stream in
   overlay/src/main.c. The mixer's callback is called from the stream's refill (PendSV): mixCallback for each 23 ms
   buffer of 16-bit stereo. GPL-2.0-or-later, as frank-quest. */
#ifndef CMM2_AUDIO_H
#define CMM2_AUDIO_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
bool cabal_audio_init(void);                      /* the stream started (once) */
void cabal_audio_shutdown(void);
void cabal_audio_process_frame(void);             /* nothing: the stream refills itself */
void cabal_audio_set_mixer_callback(void (*callback)(uint8_t *stream, int len));
unsigned cabal_audio_rate(void);                  /* CMM2: the stream's rate (it is started if need be) */
#ifdef __cplusplus
}
#endif
#endif
