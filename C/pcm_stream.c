/* TinyCC example: PCM callback outside the audio interrupt.
   Requires OPTION AUDIO with PWM, I2S or Bluetooth output. */
#include <stdint.h>
#include <stdio.h>
#include <picomite/audio.h>
#include <picomite/system.h>

struct generator {
    unsigned phase;
};

/* 22.05 kHz mono, approximately 441 Hz square wave. */
static unsigned fill(int16_t *dst, unsigned capacity, void *userdata)
{
    struct generator *g = (struct generator *)userdata;
    for (unsigned i = 0; i < capacity; ++i) {
        dst[i] = ((g->phase++ % 50) < 25) ? 5000 : -5000;
    }
    return capacity;
}

int main(void)
{
    if (pm_api_version() < 11) {
        printf("PCM requires PicoMite C API 11\n");
        return 1;
    }
    struct generator g = {0};
    pm_pcm_start(22050, 1, fill, &g);
    const uint64_t deadline = pm_timer_us() + 2000000ULL;
    while (pm_pcm_playing() && pm_timer_us() < deadline) {
        pm_pcm_poll(); /* refill occurs here, never inside the audio IRQ */
        pm_pause(1);
    }
    pm_pcm_stop();
    return 0;
}
