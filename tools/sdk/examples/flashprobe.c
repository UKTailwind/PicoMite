/* flashprobe.c: the flash's timing measured on this board, at the clock it runs at (RP2350; a program with an SRAM part).

   1. The flash's JEDEC ID and status registers, read in the QMI's direct mode at a slow clock.
   2. XIP reads (QMI M0, as the firmware reads its code): for each CLKDIV and RXDELAY, 64 KB of the firmware read three
      times through the uncached window and compared with what the firmware's own timing reads.
   3. Direct-mode reads (as the boot ROM talks to the flash while it erases and programs: its busy polls are such
      reads): for each CLKDIV and direct RXDELAY, 4 KB read with the fast read command (0Bh) and compared.

   While a timing under test is set the code runs from the SRAM (.sram), with interrupts off; it reads into the
   PSRAM, which the flash's timing does not touch, and puts the timing back before anything is compared or printed.
   Settings that would clock the flash above MAX_SCK are not tried. RUN "flashprobe.app"
   RUN "flashprobe.app", "-write" instead tries the firmware's way of erasing and programming (see write_test). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cmm2.h"

#define REG(a)          (*(volatile uint32_t *)(a))
#define QMI_DIRECT_CSR  REG(0x400D0000u)
#define QMI_DIRECT_TX   REG(0x400D0004u)
#define QMI_DIRECT_RX   REG(0x400D0008u)
#define QMI_M0_TIMING   REG(0x400D000Cu)
#define QMI_M0_RFMT     REG(0x400D0010u)
#define QMI_M0_RCMD     REG(0x400D0014u)
#define DEMCR           REG(0xE000EDFCu)
#define DWT_CTRL        REG(0xE0001000u)
#define DWT_CYCCNT      REG(0xE0001004u)
#define CSR_EN          0x00000001u
#define CSR_BUSY        0x00000002u
#define CSR_CS0         0x00000004u
#define CSR_TXFULL      0x00000400u
#define CSR_RXEMPTY     0x00010000u
#define CSR_CLKDIV(n)   ((uint32_t)(n) << 22)
#define CSR_RXDELAY(n)  ((uint32_t)(n) << 30)
#define T_CLKDIV        0x000000FFu
#define T_RXDELAY       0x00000700u
#define FLASH_UNCACHED  0x14000000u           /* the flash, not through the XIP cache */
#define TEST_OFF        0x10000u              /* the firmware's code from 64 KB on */
#define TEST_WORDS      (65536 / 4)
#define DTEST_BYTES     4096
#define MAX_SCK         160000000u            /* (no faster: well past any flash's rating) */
#define SRAM_CODE       __attribute__((section(".sram"), noinline))

static uint32_t ref[TEST_WORDS], got[TEST_WORDS];   /* (the PSRAM: XIP reads may go there) */
static uint8_t dref[DTEST_BYTES];
static uint8_t dgot[DTEST_BYTES] __attribute__((section(".sram_bss")));   /* in the SRAM: while direct mode is on, the
                                                  QMI serves no memory-mapped access, the PSRAM's included (an access
                                                  to it then faults, and the fault handler is in the flash) */

/* with interrupts off, M0's timing set to t, n words read from the flash's uncached window, the timing put back:
   the cycles the reads took */
SRAM_CODE uint32_t xip_read(uint32_t t, uint32_t *dst, int n) {
    uint32_t was = QMI_M0_TIMING, c;
    const volatile uint32_t *src = (const volatile uint32_t *)(FLASH_UNCACHED + TEST_OFF);
    __asm volatile("cpsid i" ::: "memory");
    QMI_M0_TIMING = t;
    for(volatile int i = 0; i < 200; i++) { }  /* (the last transfer's cooldown over) */
    (void)src[n - 1];                          /* (a first read at the new timing, not counted) */
    c = DWT_CYCCNT;
    for(int i = 0; i < n; i++) dst[i] = src[i];
    c = DWT_CYCCNT - c;
    QMI_M0_TIMING = was;
    for(volatile int i = 0; i < 200; i++) { }
    __asm volatile("cpsie i" ::: "memory");
    return c;
}

/* with interrupts off, a direct-mode transfer at DIRECT_CSR's clkdiv and rxdelay (csr): tx sent, as many bytes into rx
   (the first skip of them dropped); direct mode left as it was */
SRAM_CODE void direct_xfer(uint32_t csr, const uint8_t *tx, int ntx, uint8_t *rx, int n, int skip) {
    uint32_t was = QMI_DIRECT_CSR & (CSR_CLKDIV(255) | CSR_RXDELAY(3));
    int ti = 0, ri = 0, total = n + skip;
    __asm volatile("cpsid i" ::: "memory");
    QMI_DIRECT_CSR = csr | CSR_EN;
    while(QMI_DIRECT_CSR & CSR_BUSY) { }        /* (the last XIP transfer's cooldown) */
    QMI_DIRECT_CSR = csr | CSR_EN | CSR_CS0;
    while(ti < total || ri < total) {
        uint32_t f = QMI_DIRECT_CSR;
        if(!(f & CSR_TXFULL) && ti < total) {
            QMI_DIRECT_TX = ti < ntx ? tx[ti] : 0;
            ti++;
        }
        if(!(f & CSR_RXEMPTY) && ri < total) {
            uint8_t b = (uint8_t)QMI_DIRECT_RX;
            if(ri >= skip) rx[ri - skip] = b;
            ri++;
        }
    }
    while(QMI_DIRECT_CSR & CSR_BUSY) { }
    QMI_DIRECT_CSR = csr | CSR_EN;              /* (chip select up) */
    QMI_DIRECT_CSR = was;                       /* (direct mode off) */
    __asm volatile("cpsie i" ::: "memory");
}

/* ---- -write: the firmware's way of erasing and programming (misc/FileIO.c's safe_flash_range_erase and _program),
   tried on the last 64 KB of the PC3's MOD buffer (flash 0x3F0000: the fixed map of a 16 MB flash; only a MOD of
   960 KB or more reaches it). Each command in direct mode at FLASH_CMD_HZ or less, RXDELAY 1 */
#define WTEST_OFF       0x3F0000u
#define FLASH_CMD_HZ    50000000u
static uint8_t wpage[256] __attribute__((section(".sram_bss")));

SRAM_CODE void qspi_cmd(uint32_t csr, const uint8_t *h, int nh, const uint8_t *data, int n, uint8_t *rx, int nrx) {
    int total = nh + n + nrx, ti = 0, ri = 0;
    QMI_DIRECT_CSR = csr | CSR_EN | CSR_CS0;
    while(ti < total || ri < total) {
        uint32_t f = QMI_DIRECT_CSR;
        if(!(f & CSR_TXFULL) && ti < total) {
            QMI_DIRECT_TX = ti < nh ? h[ti] : ti < nh + n ? data[ti - nh] : 0;
            ti++;
        }
        if(!(f & CSR_RXEMPTY) && ri < total) {
            uint8_t b = (uint8_t)QMI_DIRECT_RX;
            if(ri >= nh + n) rx[ri - nh - n] = b;
            ri++;
        }
    }
    while(QMI_DIRECT_CSR & CSR_BUSY) { }
    QMI_DIRECT_CSR = csr | CSR_EN;
}

/* with interrupts off: write enable, then cmd (20h, D8h or 02h) with its address (and, for 02h, wpage), then the busy
   polls until it is done; direct mode as it was: the cycles it took */
SRAM_CODE uint32_t flash_op(uint32_t csr, uint8_t cmd, uint32_t addr) {
    uint32_t was = QMI_DIRECT_CSR & (CSR_CLKDIV(255) | CSR_RXDELAY(3)), c;
    uint8_t h[4], s;
    __asm volatile("cpsid i" ::: "memory");
    c = DWT_CYCCNT;
    QMI_DIRECT_CSR = csr | CSR_EN;
    while(QMI_DIRECT_CSR & CSR_BUSY) { }
    h[0] = 0x06;
    qspi_cmd(csr, h, 1, 0, 0, 0, 0);
    h[0] = cmd;
    h[1] = (uint8_t)(addr >> 16);
    h[2] = (uint8_t)(addr >> 8);
    h[3] = (uint8_t)addr;
    qspi_cmd(csr, h, 4, wpage, cmd == 0x02 ? 256 : 0, 0, 0);
    h[0] = 0x05;
    do qspi_cmd(csr, h, 1, 0, 0, &s, 1); while(s & 1);
    QMI_DIRECT_CSR = was;
    c = DWT_CYCCNT - c;
    __asm volatile("cpsie i" ::: "memory");
    return c;
}

static void cache_invalidate(uint32_t off, uint32_t n) {   /* the XIP cache's lines of the flash there, by address */
    for(uint32_t o = off & ~7u; o < off + n; o += 8) *(volatile uint8_t *)(0x18000000u + o + 2) = 0;
    __asm volatile("dsb\n isb" ::: "memory");
}

static uint8_t pattern(uint32_t i) { return (uint8_t)((i * 7u + (i >> 8) * 13u) ^ 0x5A); }

/* n bytes at off as expected (want < 0: the pattern, else that byte), through the uncached window and the cache */
static int check(const char *what, uint32_t off, uint32_t n, int want) {
    int bad = 0, badc = 0;
    const volatile uint8_t *u = (const volatile uint8_t *)(FLASH_UNCACHED + off);
    const volatile uint8_t *k = (const volatile uint8_t *)(0x10000000u + off);
    for(uint32_t i = 0; i < n; i++) {
        uint8_t e = want < 0 ? pattern(off + i) : (uint8_t)want;
        bad += u[i] != e;
        badc += k[i] != e;
    }
    printf("  %-34s %s%s\n", what, bad ? "WRONG" : "ok", badc ? " (the cache: WRONG)" : "");
    return bad || badc;
}

static int write_test(uint32_t hz, int cap) {
    uint32_t div = (hz + FLASH_CMD_HZ - 1) / FLASH_CMD_HZ, csr, c;
    int fail = 0;
    if(cap < 24) {
        printf("-write needs a 16 MB flash (the PC3's fixed map): this one is 2^%d bytes\n", cap);
        return 1;
    }
    if(div < 2) div = 2;
    csr = CSR_CLKDIV(div) | CSR_RXDELAY(1);
    printf("\nWrite test at flash 0x%06lX, commands at %lu MHz (CLKDIV %lu, RXDELAY 1)\n", (unsigned long)WTEST_OFF,
           (unsigned long)(hz / div / 1000000), (unsigned long)div);
    c = flash_op(csr, 0xD8, WTEST_OFF);
    cache_invalidate(WTEST_OFF, 0x10000);
    printf("  64 KB block erase: %lu ms\n", (unsigned long)((uint64_t)c * 1000 / hz));
    fail |= check("the 64 KB erased", WTEST_OFF, 0x10000, 0xFF);
    uint32_t most = 0;
    for(uint32_t p = 0; p < 16; p++) {             /* 4 KB of pages at the start, one page at +32 KB */
        for(int i = 0; i < 256; i++) wpage[i] = pattern(WTEST_OFF + p * 256 + i);
        c = flash_op(csr, 0x02, WTEST_OFF + p * 256);
        if(c > most) most = c;
    }
    for(int i = 0; i < 256; i++) wpage[i] = pattern(WTEST_OFF + 0x8000 + i);
    flash_op(csr, 0x02, WTEST_OFF + 0x8000);
    cache_invalidate(WTEST_OFF, 0x10000);
    printf("  17 pages programmed: the slowest %lu us\n", (unsigned long)((uint64_t)most * 1000000 / hz));
    fail |= check("the 16 pages", WTEST_OFF, 0x1000, -1);
    fail |= check("the page at +32 KB", WTEST_OFF + 0x8000, 256, -1);
    fail |= check("still erased between them", WTEST_OFF + 0x1000, 0x7000, 0xFF);
    c = flash_op(csr, 0x20, WTEST_OFF);
    cache_invalidate(WTEST_OFF, 0x1000);
    printf("  4 KB sector erase: %lu ms\n", (unsigned long)((uint64_t)c * 1000 / hz));
    fail |= check("the first 4 KB erased", WTEST_OFF, 0x1000, 0xFF);
    fail |= check("the page at +32 KB kept", WTEST_OFF + 0x8000, 256, -1);
    flash_op(csr, 0xD8, WTEST_OFF);
    cache_invalidate(WTEST_OFF, 0x10000);
    fail |= check("the 64 KB erased again (left so)", WTEST_OFF, 0x10000, 0xFF);
    printf("write test: %s\n", fail ? "FAILED" : "all as expected");
    return fail;
}

static int bad_words(void) {
    int bad = 0;
    for(int i = 0; i < TEST_WORDS; i++) bad += got[i] != ref[i];
    return bad;
}

static int bad_bytes(void) {
    int bad = 0;
    for(int i = 0; i < DTEST_BYTES; i++) bad += dgot[i] != dref[i];
    return bad;
}

int main(int argc, char **argv) {
    uint32_t hz = cmm2->cpu_hz(), t0 = QMI_M0_TIMING, csr0 = QMI_DIRECT_CSR;
    int div0 = (int)(t0 & T_CLKDIV), divmin = (int)((hz + MAX_SCK - 1) / MAX_SCK);
    uint8_t tx[8] = { 0x9F }, rx[8];
    DEMCR |= 1u << 24;
    DWT_CTRL |= 1u;
    printf("flashprobe: clk_sys %lu MHz; M0 timing %08lX (CLKDIV %d, RXDELAY %lu: SCK %lu MHz), RFMT %08lX, RCMD %08lX, "
           "DIRECT_CSR %08lX\n", (unsigned long)(hz / 1000000), (unsigned long)t0, div0, (unsigned long)((t0 & T_RXDELAY) >> 8),
           (unsigned long)(hz / (div0 ? div0 : 256) / 1000000), (unsigned long)QMI_M0_RFMT, (unsigned long)QMI_M0_RCMD,
           (unsigned long)csr0);

    /* 1. the chip */
    uint32_t slow = CSR_CLKDIV((hz + 9999999) / 10000000);   /* (10 MHz at most) */
    direct_xfer(slow, tx, 1, rx, 3, 1);
    printf("JEDEC ID %02X %02X %02X (manufacturer %02X, type %02X, capacity 2^%d bytes)\n", rx[0], rx[1], rx[2], rx[0], rx[1],
           rx[2]);
    static const uint8_t sr[3] = { 0x05, 0x35, 0x15 };
    for(int i = 0; i < 3; i++) {
        tx[0] = sr[i];
        direct_xfer(slow, tx, 1, rx, 1, 1);
        printf("status register %d (%02Xh): %02X\n", i + 1, sr[i], rx[0]);
    }

    if(argc > 1 && strcmp(argv[1], "-write") == 0) return write_test(hz, rx[2]);

    /* the reference: at the firmware's own timing, and the same through the cache */
    xip_read(t0, ref, TEST_WORDS);
    if(memcmp(ref, (const void *)(0x10000000u + TEST_OFF), sizeof(ref)) != 0) {
        printf("the reference read differs from the cached flash: stopped\n");
        return 1;
    }
    tx[0] = 0x0B;
    tx[1] = (uint8_t)(TEST_OFF >> 16);
    tx[2] = (uint8_t)(TEST_OFF >> 8);
    tx[3] = (uint8_t)TEST_OFF;
    tx[4] = 0;
    memcpy(dref, ref, DTEST_BYTES);
    direct_xfer(slow, tx, 5, dgot, DTEST_BYTES, 5);
    printf("direct read at %lu MHz: %s\n", (unsigned long)(hz / ((hz + 9999999) / 10000000) / 1000000),
           bad_bytes() ? "DIFFERS (stopped)" : "matches");
    if(bad_bytes()) return 1;

    /* 2. XIP reads */
    printf("\nXIP reads (M0), 64 KB x3 a point: ok, or the most words wrong; then the microseconds for 64 KB\n");
    printf("CLKDIV  SCK MHz | RXDELAY 0     1     2     3     4     5     6     7 |   us\n");
    for(int div = divmin < 2 ? 2 : divmin; div <= 6; div++) {
        uint32_t us = 0;
        printf("  %d    %6.1f  |        ", div, (double)hz / div / 1e6);
        for(int rxd = 0; rxd <= 7; rxd++) {
            uint32_t t = (t0 & ~(T_CLKDIV | T_RXDELAY)) | (uint32_t)div | (uint32_t)rxd << 8;
            int worst = 0;
            for(int k = 0; k < 3; k++) {
                memset(got, 0xA5, sizeof(got));
                uint32_t c = xip_read(t, got, TEST_WORDS);
                int b = bad_words();
                if(b > worst) worst = b;
                if(!b) us = (uint32_t)((uint64_t)c * 1000000 / hz);
            }
            if(worst) printf(" %5d", worst);
            else printf("    ok");
        }
        printf(" | %4lu\n", (unsigned long)us);
    }

    /* 3. direct-mode reads (0Bh) */
    printf("\nDirect-mode reads (0Bh, 4 KB a point): ok, or the bytes wrong\n");
    printf("CLKDIV  SCK MHz | RXDELAY 0     1     2     3\n");
    for(int div = divmin < 2 ? 2 : divmin; div <= 8; div++) {
        printf("  %d    %6.1f  |        ", div, (double)hz / div / 1e6);
        for(int rxd = 0; rxd <= 3; rxd++) {
            int worst = 0;
            for(int k = 0; k < 3; k++) {
                memset(dgot, 0xA5, sizeof(dgot));
                direct_xfer(CSR_CLKDIV(div) | CSR_RXDELAY(rxd), tx, 5, dgot, DTEST_BYTES, 5);
                int b = bad_bytes();
                if(b > worst) worst = b;
            }
            if(worst) printf(" %5d", worst);
            else printf("    ok");
        }
        printf("\n");
    }
    printf("\nM0 timing now %08lX, DIRECT_CSR %08lX (were %08lX, %08lX)\n", (unsigned long)QMI_M0_TIMING,
           (unsigned long)QMI_DIRECT_CSR, (unsigned long)t0, (unsigned long)csr0);
    return 0;
}
