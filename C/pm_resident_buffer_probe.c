/* Flash-LIBRARY CSUB (ARMv6-M soft-float ET_REL). No GPIO wiring. */
#include <stdint.h>
#include <picomite.h>
#include <picomite/resident.h>

static void tick(void *state)
{
    ++((uint32_t *)state)[0];
}
void ResidentBuffer(long long *cmd)
{
    const uint16_t program[2] = {0x80a0, 0x0000};
    uint32_t *buffer, *state;
    unsigned i;
    if (*cmd == 0) {
        int pio, status;
        if (pm_resident_start(tick, 50) != 0) {
            pm_print("BUFFER RESIDENT REFUSED\r\n"); return;
        }
        for (pio = 0; pio < 3; ++pio)
            if (pm_resident_pio_claim(pio) == 0) break;
        if (pio == 3) {
            pm_print("BUFFER SKIP: no idle PIO\r\n");
            pm_resident_pio_release();
            pm_resident_stop(); return;
        }
        if (pm_resident_dma_buffer_alloc(64) != 0) {
            pm_print("BUFFER ALLOCATION FAILED\r\n");
            pm_resident_pio_release();
            pm_resident_stop(); return;
        }
        buffer = pm_resident_dma_buffer();
        for (i = 0; i < 64; ++i) buffer[i] = 0x45000000u + i;
        status = pm_resident_pio_program(program, 2);
        if (!status)
            status = pm_resident_pio_init_machine(0, 0, 0x1f000, 0xc0000,
                                                  0, 1000000u, 0, 0, 0);
        if (!status)
            status = pm_resident_pio_dma_start_buffer(1, 0, 0, 64, 1);
        if (status) {
            pm_print("BUFFER DMA INIT FAILED\r\n");
            pm_resident_pio_dma_stop(1);
            pm_resident_pio_release();
            pm_resident_dma_buffer_free();
            pm_resident_stop(); return;
        }
        /* A live DMA buffer must not be freed. */
        if (pm_resident_dma_buffer_free() != -1) {
            pm_print("BUFFER FREE GUARD FAILED\r\n");
            pm_resident_pio_dma_stop(1);
            pm_resident_pio_release();
            pm_resident_dma_buffer_free();
            pm_resident_stop(); return;
        }
        pm_print("BUFFER RESIDENT INSTALLED\r\n");
    } else if (*cmd == 1) {
        state = pm_resident_state();
        buffer = pm_resident_dma_buffer();
        if (state && state[0] && buffer &&
            pm_resident_dma_buffer_words() == 64 &&
            buffer[0] == 0x45000000u &&
            buffer[63] == 0x4500003fu &&
            pm_resident_pio_dma_busy(1) == 1)
            pm_print("BUFFER DMA ACTIVE\r\n");
        else
            pm_print("BUFFER DMA NOT ACTIVE\r\n");
    } else if (*cmd == 2) {
        pm_resident_pio_dma_stop(1);
        pm_resident_pio_release();
        pm_resident_dma_buffer_free();
        if (pm_resident_stop() == 0)
            pm_print("BUFFER RESIDENT STOPPED\r\n");
        else
            pm_print("BUFFER RELEASE INCOMPLETE\r\n");
    }
}
