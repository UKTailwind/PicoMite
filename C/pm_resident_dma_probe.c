/* Flash-LIBRARY CSUB, ARMv6-M soft-float ET_REL. No external wires. */
#include <stdint.h>
#include <stdio.h>
#include <picomite.h>
#include <picomite/resident.h>

/* One PULL per two PIO instructions; DMA TX must keep the FIFO supplied. */
static void tick(void *state)
{
    ++((uint32_t *)state)[0];
}
void ResidentDMA(long long *command)
{
    uint32_t *state;
    const uint16_t program[2] = {0x80a0, 0x0000}; /* PULL BLOCK; JMP 0 */
    if (*command == 0) {
        int pio, status;
        if (pm_resident_start(tick, 50) != 0) {
            pm_print("DMA RESIDENT REFUSED\r\n"); return;
        }
        for (pio = 0; pio < 3; ++pio)
            if (pm_resident_pio_claim(pio) == 0) break;
        if (pio == 3) {
            pm_print("DMA SKIP: no idle PIO\r\n");
            pm_resident_stop(); return;
        }
        state = pm_resident_state();
        if (!state) { pm_print("DMA NO STATE\r\n"); pm_resident_pio_release(); pm_resident_stop(); return; }
        state[8] = 0x12345678u;
        state[9] = 0xabcdef01u;
        state[10] = 0x02040608u;
        state[11] = 0xffeeddccu;
        status = pm_resident_pio_program(program, 2);
        if (!status)
            status = pm_resident_pio_init_machine(0, 0, 0x1f000, 0xc0000,
                                                  0, 1000000u, 0, 0, 0);
        if (!status)
            status = pm_resident_pio_dma_start(1, 0, 8, 4, 1);
        if (status) {
            pm_print("DMA INIT FAILED\r\n");
            pm_resident_pio_dma_stop(1);
            pm_resident_pio_release();
            pm_resident_stop(); return;
        }
        pm_print("DMA RESIDENT INSTALLED\r\n");
    } else if (*command == 1) {
        char diag[112];
        int first = pm_resident_pio_dma_busy(1);
        unsigned busy_samples = first == 1;
        unsigned i;
        state = pm_resident_state();
        /* A cyclic channel can pass through an idle gap during reload.
         * Report the raw observation separately from the resident timer. */
        for (i = 0; i < 1024 && first >= 0; ++i)
            if (pm_resident_pio_dma_busy(1) == 1) ++busy_samples;
        snprintf(diag, sizeof(diag),
                 "DMA DIAG: first=%d busy=%u/1025 ticks=%lu state=%s\r\n",
                 first, busy_samples,
                 (unsigned long)(state ? state[0] : 0),
                 state ? "OK" : "NULL");
        pm_print(diag);
        if (first == -1)
            pm_print("DMA NOT OWNED\r\n");
        else if (!state || !state[0])
            pm_print("DMA RESIDENT TIMER NOT ADVANCING\r\n");
        else if (!busy_samples)
            pm_print("DMA BUSY NOT OBSERVED\r\n");
        else
            pm_print("DMA BUSY OBSERVED\r\n");
    } else if (*command == 2) {
        pm_resident_pio_dma_stop(1);
        pm_resident_pio_release();
        pm_resident_stop();
        pm_print("DMA RESIDENT STOPPED\r\n");
    }
}
