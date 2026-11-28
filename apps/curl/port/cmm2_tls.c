/* cmm2_tls.c: what mbedTLS asks of the PicoMite (port/mbedtls_user_config.h). MIT License, as pmimg.h.
     mbedtls_hardware_poll   MBEDTLS_ENTROPY_HARDWARE_ALT: the RP2350's true random number generator (TRNG, RP2350
                             datasheet 12.12) in its checked mode: von Neumann corrector, the CRNGT and autocorrelation
                             tests on, 192 bits a time from EHR_DATA0-5. A failed test, or no bits in time, is an error,
                             never a weaker source. The firmware's pico_rand uses the same block in its raw mode, and
                             writes all it needs each time it samples (pico-sdk rand.c), as this does: neither upsets
                             the other, and a program runs only between the firmware's own uses of it.
     mbedtls_ms_time         MBEDTLS_PLATFORM_MS_TIME_ALT: milliseconds since start-up. */
#include <string.h>
#include "cmm2.h"
#include "mbedtls/build_info.h"
#include "mbedtls/platform_time.h"
#include "mbedtls/entropy.h"

#define TRNG(off)        (*(volatile uint32_t *)(0x400F0000u + (off)))
#define RNG_ISR          TRNG(0x104)            /* 1 EHR valid, 2 autocorrelation error, 4 CRNGT error, 8 VN error */
#define RNG_ICR          TRNG(0x108)
#define TRNG_CONFIG      TRNG(0x10C)            /* the ROSC's chain length (0-3) */
#define EHR_DATA(i)      TRNG(0x114 + 4 * (i))
#define RND_SOURCE_EN    TRNG(0x12C)
#define SAMPLE_CNT1      TRNG(0x130)            /* clocks between samples */
#define DEBUG_CONTROL    TRNG(0x138)            /* 0: the corrector and both tests on */
#define TRNG_SW_RESET    TRNG(0x140)
#define TRNG_BUSY        TRNG(0x1B8)

/* 24 bytes (192 bits), or -1 */
static int trng_block(uint32_t w[6]) {
    int tries, i;
    uint64_t t0;
    for(tries = 0; tries < 4; tries++) {
        if(tries) {                             /* (an autocorrelation error stops the block until it is reset) */
            TRNG_SW_RESET = 1;
            t0 = cmm2->micros();
            while(TRNG_BUSY && cmm2->micros() - t0 < 1000) {}
        }
        RND_SOURCE_EN = 0;
        TRNG_CONFIG = 0;
        SAMPLE_CNT1 = 1000;                     /* (pico_rand's 0 samples too fast for the tests) */
        DEBUG_CONTROL = 0;
        RNG_ICR = 0xFFFFFFFFu;
        RND_SOURCE_EN = 1;
        t0 = cmm2->micros();
        while(!(RNG_ISR & 0xF) && cmm2->micros() - t0 < 200000u) {}
        if((RNG_ISR & 0xF) == 1) {
            for(i = 0; i < 6; i++) w[i] = EHR_DATA(i);
            RNG_ICR = 0xFFFFFFFFu;
            return 0;
        }
    }
    return -1;
}

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
    uint32_t w[6];
    size_t n;
    (void)data;
    *olen = 0;
    while(*olen < len) {
        if(trng_block(w)) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        n = len - *olen < sizeof(w) ? len - *olen : sizeof(w);
        memcpy(output + *olen, w, n);
        *olen += n;
    }
    memset(w, 0, sizeof(w));
    return 0;
}

mbedtls_ms_time_t mbedtls_ms_time(void) {
    return (mbedtls_ms_time_t)(cmm2->micros() / 1000u);
}
