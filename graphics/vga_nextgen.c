/* PicoMiteVGA RGB121-framebuffer -> RGB222 VGA scanout.
 * Based on the 8-bit GPIO/DMA stream of drivers/vga-nextgen/vga.c.
 * Selected by VGA_NEXTGEN and usable by any board with consecutive RGB222,
 * HSYNC, VSYNC GPIOs; pixel and synchronization outputs share one PIO SM.
 * Unlike the normal PicoMiteVGA driver, sync occupies bits 6 and 7 of
 * every output byte: GP12..17 RGB222, GP18 HSYNC, GP19 VSYNC.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "VGA.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/sio.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include "vga_nextgen.pio.h"
#include "vga_nextgen.h"
#include "PicoMiteI2S.pio.h"

#if defined(PICOMITEVGA) && !defined(HDMI) && defined(VGA_NEXTGEN)

/* All supported VGA modes fit within 1088 samples per scanline. */
#define M2_MAX_LINE VGA_NEXTGEN_MAX_LINE
#define M2_HSYNC_BIT 0x40u
#define M2_VSYNC_BIT 0x80u
#define M2_NO_SYNC 0xc0u

/* Direct four-bit RGB121 to six-bit RGB222, with both syncs inactive. */
static uint8_t m2_colour[16] = {
    0xc0, 0xc3, 0xc4, 0xc7, 0xc8, 0xcb, 0xcc, 0xcf,
    0xf0, 0xf3, 0xf4, 0xf7, 0xf8, 0xfb, 0xfc, 0xff
};
/* Two copies of one RGB222 pixel, used by MODE 2 (32 bytes SRAM). */
static uint16_t m2_colour2[16];
/* Pack a 4-pixel RGB121 word into one aligned RGB222 DMA word. */
static inline __attribute__((always_inline)) uint32_t m2_pack4(uint16_t w)
{
    return (uint32_t)m2_colour[w & 15] |
           ((uint32_t)m2_colour[(w >> 4) & 15] << 8) |
           ((uint32_t)m2_colour[(w >> 8) & 15] << 16) |
           ((uint32_t)m2_colour[w >> 12] << 24);
}
/* The 720x400 timing places the active area on a 2-byte boundary.
 * Do not issue an unaligned 32-bit store on RP2040. */
static inline __attribute__((always_inline)) void m2_store4(uint8_t *p, uint32_t word)
{
    if (((uintptr_t)p & 3u) == 0)
        *(uint32_t *)p = word;
    else
    {
        *(uint16_t *)p = (uint16_t)word;
        *(uint16_t *)(p + 2) = (uint16_t)(word >> 16);
    }
}

/* Immutable sync templates and three independent pixel-line buffers.
 * Keep the scanout allocation within the existing RP2350 SRAM budget. */
#define M2_VSYNC_BUFFER 0
#define M2_BLANK_BUFFER 1
#define M2_ACTIVE_BUFFER 2
#define M2_ACTIVE_COUNT 3
#define M2_RENDER_AHEAD 2
static uint32_t m2_scanout[M2_ACTIVE_BUFFER + M2_ACTIVE_COUNT][M2_MAX_LINE / 4];
static uint32_t *m2_line_ptr[2] = {m2_scanout[M2_VSYNC_BUFFER],
                                   m2_scanout[M2_VSYNC_BUFFER]};
/* core1 worker request: frame-relative line number plus one, zero = idle */
static volatile uint32_t m2_render_request;
/* -1 means the requested line has not been completely rendered yet. */
static volatile int m2_render_ready[M2_ACTIVE_COUNT];
static uint32_t m2_dma_count = UINT32_MAX;
static int m2_line_words;
static int m2_active_x;
static int m2_hsync_pixels;
static int m2_active_y;
static int m2_vsync_lines;
static int m2_vback_lines;
static int m2_scanlines;
static uint32_t m2_pio_offset;

volatile int QVgaScanLine;
volatile uint32_t QVgaFrame;
volatile int X_TILE = 80, Y_TILE = 40;
uint QVGAOff; /* retained for the PicoMiteVGA VGA module ABI */
extern uint I2SOff;
void (*volatile Core1Fnc)() = NULL;

static void __not_in_flash_func(m2_render_active)(uint8_t *dst, int y)
{
    int i;
    if (DISPLAY_TYPE == SCREENMODE1)
    {
        volatile uint8_t *p = &DisplayBuf[y * vgaloop8];
        volatile uint8_t *pp = &LayerBuf[y * vgaloop8];
        /* The renderer may skip a line when overloaded; tile selection
         * must therefore depend on the actual scanline, not past calls. */
        int pos = (y / ytileheight) * X_TILE;
        /* Every source nibble selects four pixels from the tile mask. */
        uint8_t *out = dst;
        for (i = 0; i < vgaloop16; i++)
        {
            uint8_t d = *p++ | *pp++;
            uint32_t fg = tilefcols[pos], bg = tilebcols[pos];
            uint8_t c = d & 15;
            uint16_t w = (M_Foreground[c] & fg) | (M_Background[c] & bg);
            m2_store4(out, m2_pack4(w)); out += 4;
            c = d >> 4;
            w = (M_Foreground[c] & fg) | (M_Background[c] & bg);
            m2_store4(out, m2_pack4(w)); out += 4;
            pos++;
            d = *p++ | *pp++;
            fg = tilefcols[pos]; bg = tilebcols[pos];
            c = d & 15;
            w = (M_Foreground[c] & fg) | (M_Background[c] & bg);
            m2_store4(out, m2_pack4(w)); out += 4;
            c = d >> 4;
            w = (M_Foreground[c] & fg) | (M_Background[c] & bg);
            m2_store4(out, m2_pack4(w)); out += 4;
            pos++;
        }
    }
#ifdef rp2350
    else if (DISPLAY_TYPE == SCREENMODE3)
    {
        uint8_t *p = &DisplayBuf[y * vgaloop2];
        uint8_t *q = &LayerBuf[y * vgaloop2];
        const uint8_t trans = (uint8_t)transparent;
        for (i = 0; i < vgaloop2; i++)
        {
            int a = map16[p[i] & 15];
            int b = map16[p[i] >> 4];
            int oa = map16[q[i] & 15];
            int ob = map16[q[i] >> 4];
            if (oa != trans) a = oa;
            if (ob != trans) b = ob;
            *dst++ = m2_colour[a & 15];
            *dst++ = m2_colour[b & 15];
        }
    }
#endif
    else
    {
        int row = y >> 1;
        uint8_t *p = &DisplayBuf[row * vgaloop4];
        uint8_t *q = &LayerBuf[row * vgaloop4];
        const uint8_t trans = (uint8_t)transparent;
#ifdef rp2350
        uint8_t *s = &SecondLayer[row * vgaloop4];
        uint8_t trans_s = (uint8_t)transparents;
        if (s == p) { s = q; trans_s = trans; }
#endif
        for (i = 0; i < vgaloop4; i++)
        {
            uint8_t d = p[i], l = q[i];
#ifdef rp2350
            uint8_t t = s[i];
#endif
            for (int n = 0; n < 2; n++)
            {
                int shift = n * 4;
                int c = map16[(d >> shift) & 15];
                int over = map16[(l >> shift) & 15];
                if (over != trans) c = over;
#ifdef rp2350
                over = map16[(t >> shift) & 15];
                if (over != trans_s) c = over;
#endif
                *(uint16_t *)dst = m2_colour2[c & 15];
                dst += 2;
            }
        }
    }
}

static void __not_in_flash_func(m2_prepare_line)(int line, uint32_t *memory)
{
    uint8_t *dst = (uint8_t *)memory;
    uint8_t blank = M2_NO_SYNC;
    if (line < m2_vsync_lines)
        blank &= ~M2_VSYNC_BIT;
    /* Keep scanline preparation entirely on SRAM-resident code/data.
     * All supported HSYNC widths are multiples of four samples. */
    uint32_t fill = (uint32_t)blank * 0x01010101u;
    uint32_t sync = (uint32_t)(blank & ~M2_HSYNC_BIT) * 0x01010101u;
    for (int i = 0; i < m2_line_words; i++)
        memory[i] = fill;
    for (int i = 0; i < m2_hsync_pixels / 4; i++)
        memory[i] = sync;
    int y = line - m2_vsync_lines - m2_vback_lines;
    if (y >= 0 && y < m2_active_y)
    {
        /* These bytes carry RGB222 with both sync signals inactive. */
        m2_render_active(dst + m2_active_x, y);
    }
}

/* Controller DMA starts each scanline and raises IRQ0. The IRQ only
 * switches already prepared buffers and posts a job for core1. A late
 * render produces a blank line, never half-written pixels or torn sync. */
static void __not_in_flash_func(m2_dma_irq)(void)
{
    dma_hw->ints0 = (1u << QVGA_DMA_CB);
    m2_dma_count++;
    int current = m2_dma_count == 0 ? 0 : QVgaScanLine + 1;
    if (current == m2_scanlines) { current = 0; QVgaFrame++; }
    QVgaScanLine = current;
    int next = current + 1;
    if (next == m2_scanlines) next = 0;
    int active_start = m2_vsync_lines + m2_vback_lines;
    int next_buffer = (m2_dma_count + 1) & 1;
    if (next < m2_vsync_lines)
        m2_line_ptr[next_buffer] = m2_scanout[M2_VSYNC_BUFFER];
    else if (next < active_start || next >= active_start + m2_active_y)
        m2_line_ptr[next_buffer] = m2_scanout[M2_BLANK_BUFFER];
    else
    {
        int slot = next % M2_ACTIVE_COUNT;
        /* Only a completely prepared line can be exposed to DMA. */
        m2_line_ptr[next_buffer] =
            m2_render_ready[slot] == next ? m2_scanout[M2_ACTIVE_BUFFER + slot]
                                          : m2_scanout[M2_BLANK_BUFFER];
    }
    dma_channel_set_read_addr(QVGA_DMA_CB, &m2_line_ptr[next_buffer], false);

    /* The three-slot ring can prepare N+2 without writing the active
     * DMA line or the following line already published for DMA. */
    int future = current + M2_RENDER_AHEAD;
    if (future >= m2_scanlines) future -= m2_scanlines;
    if (future >= active_start && future < active_start + m2_active_y)
    {
        int slot = future % M2_ACTIVE_COUNT;
        m2_render_ready[slot] = -1;
        m2_render_request = (uint32_t)future + 1u;
    }
}

static void __not_in_flash_func(m2_render_pending)(void)
{
    /* IRQ runs on this same core: atomically take its single-slot job. */
    uint32_t state = save_and_disable_interrupts();
    uint32_t job = m2_render_request;
    m2_render_request = 0;
    restore_interrupts(state);
    if (!job) return;
    int line = (int)job - 1;
    int y = line - m2_vsync_lines - m2_vback_lines;
    if (y < 0 || y >= m2_active_y) return;
    int slot = line % M2_ACTIVE_COUNT;
    uint8_t *dst = (uint8_t *)m2_scanout[M2_ACTIVE_BUFFER + slot] + m2_active_x;
    m2_render_active(dst, y);
    __dmb();
    m2_render_ready[slot] = line;
}

static void m2_scanout_init(void)
{
    /* Same vertical and horizontal VGA timings as the PicoMiteVGA driver,
     * but one PIO instruction per pixel instead of five. */
    int total, visible, hsync, back, vactive, vsync, vback, vtotal;
    int pixel_hz;
    if (Option.Resolution == R848x480)
    {
        total=1088; visible=848; hsync=112; back=112;
        vactive=480; vsync=6; vback=23; vtotal=517;
        pixel_hz=33600000;
    }
    else if (Option.Resolution == R800x600)
    {
        total=1024; visible=800; hsync=72; back=128;
        vactive=600; vsync=2; vback=22; vtotal=625;
        pixel_hz=40000000;
    }
    else if (Option.Resolution == R720x400)
    {
        total=900; visible=720; hsync=108; back=54;
        vactive=400; vsync=2; vback=35; vtotal=449;
        pixel_hz=28320000;
    }
    else
    {
        total=800; visible=640; hsync=96; back=48;
        vactive=480; vsync=2; vback=33; vtotal=525;
        pixel_hz=25200000;
    }
    m2_line_words = total / 4;
    m2_hsync_pixels = hsync;
    m2_active_x = hsync + back;
    m2_active_y = vactive;
    m2_vsync_lines = vsync;
    m2_vback_lines = vback;
    m2_scanlines = vtotal;

    QVGA_HACT = visible;
    QVGA_VACT = vactive;
    QVGA_VTOT = vtotal;
    QVGA_VSYNC = vsync;
    QVGA_VBACK = vback;
    QVGA_VFRONT = vtotal - vsync - vback - vactive;
    QVGA_HSYNC = hsync;
    QVGA_BP = back;
    QVGA_FP = total - hsync - back - visible;
    QVGA_TOTAL = total;
    vgaloop4 = visible / 4;
    vgaloop8 = visible / 8;
    vgaloop16 = visible / 16;
    vgaloop2 = visible / 2;
    MODE1SIZE = visible * vactive / 8;
    MODE2SIZE = (visible / 2) * (vactive / 2) / 2;
    MODE3SIZE = visible * vactive / 2;
    HRes = visible;
    VRes = vactive;

    X_TILE = Option.X_TILE;
    Y_TILE = Option.Y_TILE;
    ytileheight = (X_TILE == 80 || X_TILE == 90 || X_TILE == 106 || X_TILE == 100) ? 12 : 16;
    if (piomap[QVGA_PIO_NUM] & (uint64_t)0xFFFF00000000)
        pio_set_gpio_base(QVGA_PIO, 16);
    I2SOff = pio_add_program(QVGA_PIO, &i2s_program);
    m2_pio_offset = pio_add_program(QVGA_PIO, &vga_nextgen_program);
    for (int pin = QVGA_GPIO_FIRST; pin < QVGA_GPIO_FIRST + 8; pin++)
    {
        pio_gpio_init(QVGA_PIO, pin);
        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_8MA);
    }
    pio_sm_set_consecutive_pindirs(QVGA_PIO, QVGA_SM, QVGA_GPIO_FIRST, 8, true);
    pio_sm_config cfg = vga_nextgen_program_get_default_config(m2_pio_offset);
    sm_config_set_out_pins(&cfg, QVGA_GPIO_FIRST, 8);
    sm_config_set_fifo_join(&cfg, PIO_FIFO_JOIN_TX);
    sm_config_set_out_shift(&cfg, true, true, 32);
    sm_config_set_clkdiv(&cfg, (float)clock_get_hz(clk_sys) / pixel_hz);
    pio_sm_init(QVGA_PIO, QVGA_SM, m2_pio_offset, &cfg);

    m2_dma_count = UINT32_MAX;
    QVgaFrame = 0;
    QVgaScanLine = 0;
    /* Small per-nibble RGB121 -> RGB222 expansion tables in SRAM. */
    for (int i = 0; i < 16; ++i)
    {
        uint8_t c = m2_colour[i];
        m2_colour2[i] = (uint16_t)c * 0x0101u;
    }
    /* Synchronization is precomputed once and does not depend on rendering. */
    m2_prepare_line(0, m2_scanout[M2_VSYNC_BUFFER]);
    m2_prepare_line(m2_vsync_lines, m2_scanout[M2_BLANK_BUFFER]);
    /* Diagnostic: magenta when the worker misses the DMA deadline.
     * Keep the synchronization template unchanged. */
    uint8_t *miss = (uint8_t *)m2_scanout[M2_BLANK_BUFFER] + m2_active_x;
    for (int x = 0; x < QVGA_HACT; x++)
        miss[x] = 0xf3; /* RGB222 red+blue, sync inactive */
    for (int slot = 0; slot < M2_ACTIVE_COUNT; slot++)
    {
        m2_prepare_line(m2_vsync_lines, m2_scanout[M2_ACTIVE_BUFFER + slot]);
        m2_render_ready[slot] = -1;
    }
    m2_render_request = 0;
    m2_line_ptr[0] = m2_scanout[M2_VSYNC_BUFFER];
    m2_line_ptr[1] = m2_scanout[m2_vsync_lines > 1 ? M2_VSYNC_BUFFER : M2_BLANK_BUFFER];

    dma_channel_config data = dma_channel_get_default_config(QVGA_DMA_PIO);
    channel_config_set_transfer_data_size(&data, DMA_SIZE_32);
    channel_config_set_read_increment(&data, true);
    channel_config_set_write_increment(&data, false);
    channel_config_set_dreq(&data, pio_get_dreq(QVGA_PIO, QVGA_SM, true));
    channel_config_set_chain_to(&data, QVGA_DMA_CB);
    channel_config_set_irq_quiet(&data, true);
    data.ctrl |= DMA_CH0_CTRL_TRIG_HIGH_PRIORITY_BITS;
    dma_channel_configure(QVGA_DMA_PIO, &data, &QVGA_PIO->txf[QVGA_SM], NULL,
                          m2_line_words, false);

    dma_channel_config ctrl = dma_channel_get_default_config(QVGA_DMA_CB);
    channel_config_set_transfer_data_size(&ctrl, DMA_SIZE_32);
    channel_config_set_read_increment(&ctrl, false);
    channel_config_set_write_increment(&ctrl, false);
    dma_channel_configure(QVGA_DMA_CB, &ctrl,
                          &dma_hw->ch[QVGA_DMA_PIO].al3_read_addr_trig,
                          &m2_line_ptr[0], 1, false);
    dma_channel_set_irq0_enabled(QVGA_DMA_CB, true);
    irq_set_exclusive_handler(DMA_IRQ_0, m2_dma_irq);
    irq_set_priority(DMA_IRQ_0, 0);
    irq_set_enabled(DMA_IRQ_0, true);
    pio_sm_set_enabled(QVGA_PIO, QVGA_SM, true);
    dma_channel_start(QVGA_DMA_CB);
}

void __not_in_flash_func(QVgaCore)(void)
{
#ifdef MURMULATOR_M2P2_VGA222
    multicore_lockout_victim_init();
#endif
    m2_scanout_init();
    while (true)
    {
        if (m2_render_request)
            m2_render_pending();
        __dmb();
#ifndef MURMULATOR_M2P2_VGA222
        if (multicore_fifo_rvalid())
        {
            int command = multicore_fifo_pop_blocking();
            if (command == 0x5555) irq_set_enabled(DMA_IRQ_0, false);
            if (command == 0xAAAA) irq_set_enabled(DMA_IRQ_0, true);
        }
#endif
    }
}

#endif
