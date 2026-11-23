/* cmm2_port.h: what frank-quest's CMM2 overlay files share (main.c has them). GPL-2.0-or-later, as frank-quest.

   The sound's lock: the loader's audio_lock holds off the sound refill (PendSV), as frank-quest masked its I2S DMA
   interrupt; this one counts, so a lock taken inside another (the allocator's inside the OSystem mutex) leaves the
   outer one held. The refill itself may take it (the mixer allocates): it runs only while no lock is held. */
#ifndef CMM2_PORT_H
#define CMM2_PORT_H
#include "cmm2.h"
#ifdef __cplusplus
extern "C" {
#endif
void cmm2_sound_lock(void);
void cmm2_sound_unlock(void);
void cmm2_psram_region(void *base, unsigned size);   /* the allocator's "PSRAM": a block of the loader's heap */
#ifdef __cplusplus
}
#endif
#endif
