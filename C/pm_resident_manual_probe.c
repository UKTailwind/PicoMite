/* Resident explicit-ownership smoke test. ARMv6-M soft-float ET_REL.
 * Cyclic PIO TX from a persistent system-heap allocation; no wires. */
#include <stdint.h>
#include <picomite.h>
#include <picomite/resident.h>

static void tick(void *state) { ++((uint32_t *)state)[0]; }

void ResidentManual(long long *cmd)
{
    const uint16_t program[] = {0x80a0, 0x0000}; /* pull block; jmp 0 */
    uint32_t *state = pm_resident_state(), *buffer;
    int pio, rc;
    if (*cmd == 0) {
        if (pm_resident_start(tick, 50)) {
            pm_print("MANUAL START REFUSED\r\n"); return;
        }
        state = pm_resident_state();
        buffer = pm_resident_alloc(256);
        if (!buffer) { pm_print("MANUAL OOM\r\n"); pm_resident_stop(); return; }
        state[4] = (uint32_t)(uintptr_t)buffer;
        for (unsigned i = 0; i < 64; ++i) buffer[i] = 0x34560000u + i;
        for (pio = 0; pio < 3; ++pio)
            if (pm_resident_pio_claim(pio) == 0) break;
        if (pio == 3) {
            pm_print("MANUAL SKIP: NO IDLE PIO\r\n");
            pm_resident_free(buffer); pm_resident_stop(); return;
        }
        rc = pm_resident_pio_program(program, 2);
        if (!rc) rc = pm_resident_pio_init_machine(0, 0, 0x1f000, 0xc0000,
                                                   0, 1000000, 0, 0, 0);
        if (!rc) rc = pm_resident_dma_claim(1);
        if (!rc) rc = pm_resident_pio_dma_transfer(1, 0, buffer, 256,
                                                   64, 32, 1, 0);
        if (rc) {
            pm_print("MANUAL DMA INIT FAILED\r\n");
            pm_resident_dma_halt(1);
            pm_resident_dma_release(1);
            pm_resident_pio_release();
            pm_resident_free(buffer);
            pm_resident_stop(); return;
        }
        if (pm_resident_stop() != -1) {
            pm_print("MANUAL STOP GUARD FAILED\r\n"); return;
        }
        pm_print("MANUAL DMA INSTALLED\r\n");
    } else if (*cmd == 1) {
        state = pm_resident_state();
        buffer = state ? (uint32_t *)(uintptr_t)state[4] : NULL;
        if (state && buffer && buffer[0] == 0x34560000u &&
            buffer[63] == 0x3456003fu && pm_resident_pio_dma_busy(1) == 1)
            pm_print("MANUAL DMA ACTIVE\r\n");
        else
            pm_print("MANUAL DMA INACTIVE\r\n");
    } else if (*cmd == 2) {
        buffer = state ? (uint32_t *)(uintptr_t)state[4] : NULL;
        rc = pm_resident_dma_halt(1);
        if (!rc) rc = pm_resident_dma_release(1);
        if (!rc) rc = pm_resident_pio_release();
        if (!rc) { pm_resident_free(buffer); state[4] = 0; }
        if (!rc) rc = pm_resident_stop();
        pm_print(rc ? "MANUAL RELEASE FAILED\r\n" : "MANUAL RELEASED\r\n");
    }
}
