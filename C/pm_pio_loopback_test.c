/* On-chip PIO loopback without wires or target-specific recompile.
 * Automatically selects a block reported idle by the firmware.  Run only
 * when other BASIC programs are not relying on preloaded PIO instructions:
 * PROGRAM/CLEAR replace ALL 32 instructions in the selected block. */
#include <picomite/pio.h>
#include <picomite/system.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile int rx_events, rx_dma_done, tx_dma_done;
static void on_fifo(int pio, int sm) { (void)pio; (void)sm; ++rx_events; }
static void on_dma(int tx) { if (tx) ++tx_dma_done; else ++rx_dma_done; }
static const uint16_t echo_program[] = {
    0x80a0, /* pull block */
    0xa0c7, /* mov isr, osr */
    0x8020, /* push block */
    0x0000  /* jmp 0 */
};
int main(void)
{
    int pio = -1;
    const int sm = 0;
    unsigned i;
    uint32_t got = 0;
    uint32_t *tx = NULL, *rx = NULL, *ring = NULL;
    void *ring_alloc = NULL, *rxring_alloc = NULL;
    uint32_t *rxring = NULL;
    int result = 1, armed_rx = 0, armed_tx = 0;
    pm_pio_config config = {0};
    unsigned long long deadline;
    /* Exact bitfields from BASIC PIO(PINCTRL/EXECCTRL/SHIFTCTRL).
     * Independent of PIO availability and therefore tested on both MCUs. */
    if (pm_pio_encode_pinctrl(3,2,7,11,12,13,14) != 0x6875b1aeu ||
        pm_pio_encode_execctrl(11,3,27,1,1) != 0x6b01b180u ||
        pm_pio_encode_shiftctrl(14,29,1,1,1,0,1,0,1,0) != 0x7ae74000u ||
        pm_pio_encode_pinctrl(0,0,0,0,0,0,0) != 0 ||
        pm_pio_encode_execctrl(0,0,0,0,0) != 0 ||
        pm_pio_encode_shiftctrl(0,0,0,0,0,0,0,0,0,0) != 0) {
        puts("PIO register encoding FAIL");
        return 1;
    }
    puts("PIO register encoding PASS");
    for (i = 0; i < 3; ++i) {
        int available = pm_pio_available((int)i);
        int idle = pm_pio_idle((int)i);
        printf("PIO%u: available=%d idle=%d\n", i, available, idle);
        if (pio < 0 && idle) pio = (int)i;
    }
    if (pio < 0) {
        puts("SKIP: no available idle PIO block (video/audio may reserve it)");
        return 0;
    }
    printf("Testing PIO%d SM%d\n",pio,sm);
    pm_pio_clear(pio);
    pm_pio_assemble(pio, ".PROGRAM LOOPBACK");
    pm_pio_assemble(pio, "PULL BLOCK");
    pm_pio_assemble(pio, "MOV ISR, OSR");
    pm_pio_assemble(pio, "PUSH BLOCK");
    pm_pio_assemble(pio, "JMP 0");
    pm_pio_assemble(pio, ".END PROGRAM");
    config.clock_hz = 15000000;
    config.jmp_pin = -1;
    config.wrap = 3;
    config.in_shift_right = 1;
    config.out_shift_right = 1;
    pm_pio_configure(pio, sm, &config);
    pm_pio_sync(pio, 1u << sm, 0, 0);
    pm_pio_start(pio, sm);
    pm_pio_on_fifo(pio, sm, 0, on_fifo);
    pm_pio_write(pio, sm, 0x13579bdfu);
    deadline = pm_timer_us() + 1000000u;
    while (pm_timer_us() < deadline && !rx_events) pm_pio_poll();
    pm_pio_on_fifo(pio, sm, 0, NULL);
    if (!rx_events || !pm_pio_read(pio, sm, &got) || got != 0x13579bdfu) {
        printf("FIFO failed: events=%d got=%08lx\n", rx_events, (unsigned long)got);
        goto finish;
    }
    /* FSTAT RXEMPTY[n] (RP2040/RP2350: bit 8+n) must agree with
     * the empty RX FIFO following the single-word loopback test. */
    if (!(pm_pio_fstat(pio) & (1u << (8 + sm))) ||
        pm_pio_flevel(pio,sm,0) != 0) {
        puts("PIO FSTAT: RXEMPTY does not match empty FIFO");
        goto finish;
    }
    printf("PIO FSTAT/FDEBUG PASS (FDEBUG=%08lx)\n",
           (unsigned long)pm_pio_fdebug(pio));
    puts("FIFO loopback / PIO assembler PASS");
    /* Keep testing the direct binary PROGRAM path as well. */
    pm_pio_stop(pio,sm);
    pm_pio_program(pio,echo_program,4);
    pm_pio_configure(pio,sm,&config);
    pm_pio_start(pio,sm);
    tx = (uint32_t *)malloc(16 * sizeof(uint32_t));
    rx = (uint32_t *)malloc(16 * sizeof(uint32_t));
    if (!tx || !rx) { puts("malloc failed"); goto finish; }
    for (i = 0; i < 16; ++i) tx[i] = 0x07000000u + i;
    memset(rx, 0, 16 * sizeof(uint32_t));
    pm_pio_on_dma_done(0, on_dma);
    pm_pio_on_dma_done(1, on_dma);
    pm_pio_dma_rx(pio, sm, rx, 16 * sizeof(uint32_t), 16, 32);
    armed_rx = 1;
    pm_pio_dma_tx(pio, sm, tx, 16 * sizeof(uint32_t), 16, 32);
    armed_tx = 1;
    deadline = pm_timer_us() + 1000000u;
    while (pm_timer_us() < deadline && (!rx_dma_done || !tx_dma_done))
        pm_pio_poll();
    if (rx_dma_done && tx_dma_done && !memcmp(tx, rx, 16 * sizeof(uint32_t))) {
        puts("DMA loopback PASS");
    } else {
        printf("DMA failed: RX done=%d TX done=%d\n", rx_dma_done, tx_dma_done);
        goto finish;
    }
    /* Continuous TX in a 16-byte hardware ring, consumed by one-shot RX.
     * The C allocation is deliberately overallocated for DMA alignment. */
    pm_pio_dma_stop(0);
    pm_pio_dma_stop(1);
    armed_rx = armed_tx = 0;
    pm_pio_stop(pio, sm);
    /* startPIO() restarts shift state but does NOT reset the PIO PC.
     * Reconfigure the SM to start at instruction 0 for this phase. */
    pm_pio_configure(pio, sm, &config);
    pm_pio_start(pio, sm);
    ring_alloc = malloc(16 + 15);
    if (!ring_alloc) { puts("ring malloc failed"); goto finish; }
    ring = (uint32_t *)(((uintptr_t)ring_alloc + 15u) & ~(uintptr_t)15u);
    for (i = 0; i < 4; ++i) ring[i] = 0x11220000u + i;
    memset(rx, 0, 16 * sizeof(uint32_t));
    rx_dma_done = 0;
    pm_pio_dma_rx(pio,sm,rx,16*sizeof(uint32_t),16,32);
    armed_rx = 1;
    /* C capacity measures the accessible aligned subrange, not the raw
     * malloc size. RX callback still uses the normal BASIC completion rule. */
    pm_pio_dma_cyclic_tx(pio,sm,ring,16,4,32,1);
    armed_tx = 1;
    deadline = pm_timer_us() + 1000000u;
    while (pm_timer_us() < deadline && !rx_dma_done) pm_pio_poll();
    pm_pio_dma_stop(1);
    armed_tx = 0;
    if (!rx_dma_done) { puts("cyclic TX: RX timeout"); goto finish; }
    for (i = 0; i < 16; ++i) {
        if (rx[i] != ring[i % 4]) {
            printf("cyclic TX: wrong RX[%u] = %08lx\n", i,
                   (unsigned long)rx[i]);
            goto finish;
        }
    }
    puts("Cyclic TX ring PASS");
    pm_pio_dma_stop(0);
    armed_rx = 0;
    pm_pio_stop(pio,sm);
    pm_pio_configure(pio,sm,&config);
    pm_pio_start(pio,sm);
    memset(rx,0,16*sizeof(uint32_t));
    rx_dma_done = 0;
    pm_pio_dma_rx(pio,sm,rx,16*sizeof(uint32_t),16,32);
    armed_rx = 1;
    pm_pio_dma_cyclic_tx(pio,sm,ring,16,4,32,0);
    armed_tx = 1;
    deadline = pm_timer_us() + 1000000u;
    while (pm_timer_us() < deadline && !rx_dma_done) pm_pio_poll();
    pm_pio_dma_stop(1);
    armed_tx = 0;
    if (!rx_dma_done) { puts("cyclic TX retrigger: RX timeout"); goto finish; }
    for (i = 0; i < 16; ++i) {
        if (rx[i] != ring[i % 4]) {
            printf("cyclic TX retrigger: wrong RX[%u] = %08lx\n",i,
                   (unsigned long)rx[i]);
            goto finish;
        }
    }
    puts("Cyclic TX retrigger PASS");
    pm_pio_dma_stop(0);
    armed_rx = 0;
    pm_pio_stop(pio,sm);
    /* pio_sm_init() invoked by configure resets the program counter to 0,
     * preventing a stale PUSH from the preceding cyclic-TX phase. */
    pm_pio_configure(pio, sm, &config);
    pm_pio_start(pio,sm);
    /* Cyclic RX overwrites four words. The TX-DMA done callback is NOT a
     * safe end-of-stream marker: TX FIFO empty does not imply that the SM
     * has executed PUSH for the last word. Its callback also stops the SM.
     * Instead wait for TX DMA to finish AND for all four expected RX words. */
    pm_pio_on_dma_done(1, NULL);
    memset(rx,0,16*sizeof(uint32_t));
    pm_pio_dma_cyclic_rx(pio,sm,rx,16,4,32);
    armed_rx = 1;
    pm_pio_dma_tx(pio,sm,tx,16*sizeof(uint32_t),16,32);
    armed_tx = 1;
    deadline = pm_timer_us() + 1000000u;
    while (pm_timer_us() < deadline) {
        if (!pm_pio_dma_busy(1) &&
            rx[0] == tx[12] && rx[1] == tx[13] &&
            rx[2] == tx[14] && rx[3] == tx[15])
            break;
        pm_pio_poll();
    }
    {
        int tx_busy = pm_pio_dma_busy(1);
        pm_pio_dma_stop(0);
        armed_rx = 0;
        if (tx_busy || rx[0] != tx[12] || rx[1] != tx[13] ||
            rx[2] != tx[14] || rx[3] != tx[15]) {
            printf("cyclic RX: busy=%d RX=%08lx,%08lx,%08lx,%08lx "
                   "expected=%08lx,%08lx,%08lx,%08lx\n",
                   tx_busy,
                   (unsigned long)rx[0], (unsigned long)rx[1],
                   (unsigned long)rx[2], (unsigned long)rx[3],
                   (unsigned long)tx[12], (unsigned long)tx[13],
                   (unsigned long)tx[14], (unsigned long)tx[15]);
            goto finish;
        }
    }
    puts("Cyclic RX reload PASS");
    pm_pio_dma_stop(1);
    armed_tx = 0;
    pm_pio_stop(pio, sm);
    pm_pio_configure(pio, sm, &config);
    pm_pio_start(pio, sm);
    /* Finite RX: 16 words into a four-word hardware-wrapping destination.
     * Extra guard words catch a missing ring configuration. */
    rxring_alloc = malloc(32 + 15);
    if (!rxring_alloc) { puts("finite RX: malloc failed"); goto finish; }
    rxring = (uint32_t *)(((uintptr_t)rxring_alloc + 15u) & ~(uintptr_t)15u);
    for (i = 0; i < 8; ++i) rxring[i] = 0xfeed0000u + i;
    rx_dma_done = 0;
    pm_pio_dma_rx_ring(pio, sm, rxring, 16, 16, 32, 4);
    armed_rx = 1;
    pm_pio_dma_tx(pio, sm, tx, 16*sizeof(uint32_t), 16, 32);
    armed_tx = 1;
    deadline = pm_timer_us() + 1000000u;
    while (pm_timer_us() < deadline && !rx_dma_done) pm_pio_poll();
    if (!rx_dma_done) { puts("finite RX ring: timeout"); goto finish; }
    pm_pio_dma_stop(0);
    armed_rx = 0;
    pm_pio_dma_stop(1);
    armed_tx = 0;
    for (i = 0; i < 4; ++i) {
        if (rxring[i] != tx[12 + i]) {
            printf("finite RX ring: wrong [%u] = %08lx, expected %08lx\n",
                   i, (unsigned long)rxring[i], (unsigned long)tx[12 + i]);
            goto finish;
        }
    }
    for (i = 4; i < 8; ++i) {
        if (rxring[i] != 0xfeed0000u + i) {
            printf("finite RX ring: guard [%u] overwritten\n", i);
            goto finish;
        }
    }
    puts("Finite RX ring PASS");
    pm_pio_stop(pio, sm);
    pm_pio_configure(pio, sm, &config);
    pm_pio_start(pio, sm);
    /* A one-element window must repeatedly overwrite only word zero. */
    for (i = 0; i < 8; ++i) rxring[i] = 0xfeed0000u + i;
    rx_dma_done = 0;
    pm_pio_dma_rx_ring(pio, sm, rxring, 4, 16, 32, 1);
    armed_rx = 1;
    pm_pio_dma_tx(pio, sm, tx, 16*sizeof(uint32_t), 16, 32);
    armed_tx = 1;
    deadline = pm_timer_us() + 1000000u;
    while (pm_timer_us() < deadline && !rx_dma_done) pm_pio_poll();
    if (!rx_dma_done) { puts("finite RX single: timeout"); goto finish; }
    pm_pio_dma_stop(0);
    armed_rx = 0;
    pm_pio_dma_stop(1);
    armed_tx = 0;
    if (rxring[0] != tx[15]) {
        printf("finite RX single: got %08lx, expected %08lx\n",
               (unsigned long)rxring[0], (unsigned long)tx[15]);
        goto finish;
    }
    for (i = 1; i < 8; ++i) {
        if (rxring[i] != 0xfeed0000u + i) {
            printf("finite RX single: guard [%u] overwritten\n", i);
            goto finish;
        }
    }
    puts("Finite RX single destination PASS");
    result = 0;
finish:
    if (armed_rx) pm_pio_dma_stop(0);
    if (armed_tx) pm_pio_dma_stop(1);
    pm_pio_on_dma_done(0, NULL);
    pm_pio_on_dma_done(1, NULL);
    pm_pio_stop(pio, sm);
    pm_pio_clear(pio);
    free(rxring_alloc);
    free(ring_alloc);
    free(tx);
    free(rx);
    return result;
}
