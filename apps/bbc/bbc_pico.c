/* bbc_pico.c: frank-micro's drivers on the PicoMite (the PC3), by the loader's services: the Colour Maximite 2's
   bbc_cmm2.c (apps/bbc there, Peter's, 11-13 Nov 2026) with a display of the PicoMite's. This file has what frank-micro's
   frank_platform.c, frank_gui.c and frank_audio.c give the rest of it (those files are the RP2350's start-up, its HDMI
   picture and its HDMI, I2S and PWM sound, and are not built): main, which starts B-em; the display; the sound; the USB
   keyboard and gamepads as frank-micro's USB HID driver hands them over; and the pace of the frames.

   - The display: B-em draws each of the BBC's 256 lines as a row of 640 pixels (RGB555), and each is kept whole here
     (frank-micro's own 320 x 256 halves MODE 0's): the BBC's eight colours as frank-micro has them (palette 0-7, which
     its monitor setting changes), MODE 7's teletext as frank-micro samples it, the six columns of each SAA5050 character
     at full intensity, each twice across. It is drawn into MODE 3's page (640 x 480, 16 colours: the BBC's 8 in 0-7,
     frank-micro's menus' 240-247 in 8-15) as each line is made, the 256 lines spread over the page's 480 (the first
     step: 576p, each line twice, is to come). frank-micro's menus (F11 the discs, F12 the settings) draw at 320 x 256,
     over the picture, each pixel twice across.
   - The pace: the BBC's 50 frames a second by the clock (the display runs at its own rate), unless frank-micro's
     Limit Speed is off.
   - The sound: B-em's SN76489 at 31,250 samples a second goes through a ring to the firmware's sound stream, at its rate
     by a phase step trimmed to hold the ring's level: the emulator keeps its own time, 50 frames a second.
   - The keyboard: the USB keyboard's keys as they are pressed and let go (its raw reports), through frank-micro's own
     USB path, so that frank-micro's keys work: F11 the disc menu, F12 the settings, Print Screen a screenshot
     (bbc/screenshot), Ctrl+Alt+Delete starts the BBC again. What is on a key is what the BBC types, in any OPTION
     KEYBOARD layout (keys_by_character); CAPS LOCK is on Caps Lock, CTRL on Ctrl, COPY on End, f0-f9 on F1-F10, as B-em
     puts them. Pause/Break is the BBC's BREAK (Shift+Break boots the disc in drive 0). Ctrl+Alt+End leaves.
   - The joystick: the first USB gamepad, as frank-micro has it, keys of the BBC by its gamepad setting (cursor keys, or
     Z X : / and RETURN).
   - The serial console types too, for remote use: each character as the BBC's keyboard makes it (" is SHIFT and 2, *
     SHIFT and :, small letters SHIFT and the letter, as with CAPS LOCK on), Return as RETURN, Backspace and Delete as
     DELETE, Ctrl+letter as CTRL and the letter; Ctrl-F then 0-9 for f0-f9, u d l r for the cursor keys, p n for Page Up
     and Down, b for BREAK, B for CTRL and BREAK, m for the disc menu, s for the settings (Return, Escape and those keys
     are the menus' while one is up); Ctrl-] leaves.
   - The ROMs: frank-micro's are built in; the .rom and .bin files of the folder roms go into the free sideways RAM banks
     as the BBC starts (roms_load).
   - The discs: RUN "bbc.app", disc [disc] puts them in drives 0 and 1 (and SHIFT-BREAKs), F11's menu others. What the
     BBC writes to a disc goes to its image on the SD card at once (patches/0002), unless the file is read only or RUN's
     -ro is given: then the disc is write protected.
   RUN "bbc.app", -stats prints the frames a second, how much of the time the emulator has to spare, the sound ring's
   level and the heap in use, every 5 s. */
#include <malloc.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cmm2.h"
#include "x_gui.h"
#include "HDMI.h"
#include "ff.h"
#include "frank_gui.h"
#include "frank_ui.h"
#include "frank_disc.h"
#include "frank_loader.h"
#include "frank_settings.h"
#include "frank_screenshot.h"
#include "usbhid_wrapper.h"
#include "nespad.h"

extern int _al_mangled_main(int argc, char **argv);   /* B-em's main (main.c: #define main _al_mangled_main) */
extern void key_down(int code);
extern void key_up(int code);
extern void m6502_reset(void);
extern void frank_gamepad_init(void);
extern bool g_frank_disc_readonly;              /* (frank_disc.c, patches/0002: the disc images opened for reading only) */

static bool has_arg(const char *a) {
    for(int i = 1; i < cmm2_start->argc; i++) if(strcmp(cmm2_start->argv[i], a) == 0) return true;
    return false;
}

/* where B-em reads an empty ROM slot (mem.c's g_garbage_read: the RP2350's flash) */
uint8_t cmm2_garbage[0x4000];

/* the Pico SDK's panic: B-em's, when it cannot go on */
void panic(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "bbc: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(3);
}

void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms) { (void)pc; (void)sp; (void)delay_ms; exit(0); }   /* (not called: patches/) */

/* ---- the time ---- */
uint64_t time_us_64(void) { return cmm2->micros(); }
void sleep_us(uint64_t us) { usleep((useconds_t)us); }
void sleep_ms(uint32_t ms) { usleep((useconds_t)ms * 1000); }
void tight_loop_contents(void) { }

/* -stats: the frames, the time the emulator had to spare and the sound ring, since the last report */
static int stats;
static uint32_t stat_frames;
static uint64_t stat_idle, stat_from;
static volatile uint32_t ring_in, ring_out;     /* (the sound's: ring_out in samples, ring_phase its fraction) */
static int stat_peak;                           /* (the loudest of B-em's samples since the last report) */
static int audio_on;                            /* (the sound stream started) */

/* the M33's cycle counter (the DWT's), for -stats: the display's own share of the time */
#define DEMCR      (*(volatile uint32_t *)0xE000EDFCu)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)
static uint32_t stat_disp;                      /* cycles in the display's code since the last report */

static uint32_t *stack_floor, stack_top;       /* (the stack's lowest word, from PSPLIM, and main's SP: -stats' high-water mark) */
static void stack_paint(void) {
    uint32_t lim, sp;
    __asm volatile("mrs %0, psplim" : "=r"(lim));
    __asm volatile("mov %0, sp" : "=r"(sp));
    stack_floor = (uint32_t *)lim;
    stack_top = sp;
    for(uint32_t *p = stack_floor; p && (uint32_t)p < sp - 256; p++) *p = 0x5AA5C33Cu;
}
static uint32_t stack_used(void) {              /* bytes from the top that have been touched (the deepest so far) */
    uint32_t *p = stack_floor;
    if(!p) return 0;
    while(*p == 0x5AA5C33Cu) p++;
    return stack_top - (uint32_t)p;
}

static void stat_frame(void) {
    uint64_t now = cmm2->micros();
    if(!stats) return;
    stat_frames++;
    if(now - stat_from >= 5000000) {
        uint32_t ms = (uint32_t)((now - stat_from) / 1000), idle = (uint32_t)(stat_idle / 1000);
        uint32_t disp = (uint32_t)((uint64_t)stat_disp * 100 / ((uint64_t)cmm2->cpu_hz() / 1000 * ms));
        printf("bbc: %lu.%lu frames a second, %lu%% spare, display %lu%%, sound ring %lu (peak %d, audio %d), heap %lu, stack %lu of %lu\n",
               (unsigned long)(stat_frames * 1000 / ms), (unsigned long)(stat_frames * 10000 / ms % 10), (unsigned long)(idle * 100 / ms),
               (unsigned long)disp, (unsigned long)(ring_in - ring_out), stat_peak, audio_on, (unsigned long)mallinfo().uordblks,
               (unsigned long)stack_used(), (unsigned long)(stack_top - (uint32_t)stack_floor));
        stat_peak = 0;
        stat_frames = 0;
        stat_idle = 0;
        stat_disp = 0;
        stat_from = now;
    }
}

/* ---- the display ---- */
#define PIC_W 640                               /* the BBC's picture: 256 lines of 640 */
#define PIC_H 256
#define UI_W 320                                /* frank-micro's menus: 320 x 256, over the picture */
#define UI_H 256
#define UI_CLEAR 255                            /* (a colour the menus do not use: where the picture shows through) */
#define UI_BASE 240                             /* the menus' colours, 240-247 (ui_draw.h): MODE 3's 8-15 */
#define FRAME_US 20000                          /* the BBC's frame: 50 a second */
#ifdef BBC_SRAM_DISPLAY                         /* (make.py sram=display) */
#define HOT __attribute__((section(".sram.bbc_display")))   /* (the program's SRAM part: the display's hot code) */
#else
#define HOT
#endif
static cmm2_video video;
static uint8_t __attribute__((aligned(4))) ui_fb[UI_W * UI_H];
static bool ui_frame;                           /* (the menus are drawn over this frame: ui_fb made at its start) */
static uint8_t palette[256 * 3], shown16[16 * 3];
static volatile bool palette_new;
static int row_first[PIC_H + 1];                /* BBC line y on the page's rows row_first[y] to row_first[y + 1] - 1 */
static int pic_xoff;                            /* the picture's first byte in each row (its left edge) */
static uint32_t frame_us = FRAME_US;            /* the pace: the BBC's 20 ms, or 576p's own frame (50 a second) */
static uint8_t *shot;                           /* a frame to be saved (frank_screenshot_save): its 8-bit picture */
static bool shot_frame;
static uint64_t next_frame;                     /* when the next frame of the BBC's may begin */
uint8_t *SCREEN[2];                             /* (frank_gui.h's: not used by the rest) */
volatile uint32_t current_buffer;

static int display_start(void) {
    if(cmm2->video_mode(3, 4, &video) < 0 || video.width < PIC_W || video.height < PIC_H) {
        fprintf(stderr, "bbc: cannot set MODE 3 at 640 across\n");
        return -1;
    }
    if(video.height >= 2 * PIC_H) {             /* (576p: each line twice, the picture in the middle) */
        int top = (int)(video.height - 2 * PIC_H) / 2;
        for(int y = 0; y <= PIC_H; y++) row_first[y] = top + 2 * y;
    } else
        for(int y = 0; y <= PIC_H; y++) row_first[y] = (int)((uint32_t)y * video.height / PIC_H);
    pic_xoff = (int)(video.width - PIC_W) / 4 & ~3;  /* (half the spare pixels, two a byte, a whole word) */
    if(video.height == 576)                     /* 576p: 864 x 625 at clk_sys / 15, a frame 20.05 ms at 404 MHz */
        frame_us = (uint32_t)(540000ull * 15000000ull / cmm2->cpu_hz());
    return 0;
}

void graphics_set_palette(uint8_t i, uint32_t rgb) {
    palette[i * 3] = (uint8_t)(rgb >> 16);
    palette[i * 3 + 1] = (uint8_t)(rgb >> 8);
    palette[i * 3 + 2] = (uint8_t)rgb;
    palette_new = true;
}

uint32_t graphics_get_palette(uint8_t i) { return (uint32_t)palette[i * 3] << 16 | (uint32_t)palette[i * 3 + 1] << 8 | palette[i * 3 + 2]; }
void graphics_set_bgcolor(uint32_t rgb) { (void)rgb; }
void graphics_set_buffer(uint8_t *buffer) { (void)buffer; }

static void palette_show(void) {               /* MODE 3's 16: the BBC's 0-7 and the menus' 240-247, from the next frame */
    uint8_t p[16 * 3];
    if(!palette_new) return;
    palette_new = false;
    memcpy(p, palette, 8 * 3);
    memcpy(p + 8 * 3, palette + UI_BASE * 3, 8 * 3);
    if(memcmp(p, shown16, sizeof(p)) == 0) return;
    memcpy(shown16, p, sizeof(p));
    cmm2->video_palette(0, 16, shown16);
}

/* frank_gui.c's MODE 7 line, at twice its width: each of the 40 characters is 16 of B-em's pixels, its six SAA5050 columns at the offsets where
   they are at full intensity (0, 2, 5, 8, 11, 14). There each pixel is one of the BBC's colours whole, so each column is drawn in its own
   colour (frank_gui.c's rule, by brightness, made a character on a coloured background all one colour), twice across, and the sides in the
   first column's: a letter's space, a joined graphic's left block. The BBC's colour of a pixel is the top bit of its red, green and blue
   (x_gui.h's RGB555: bits 15, 10 and 4), gathered for two pixels at a time by one multiply: shifted down 4, bits 11, 6, 0 and 27, 22, 16 times
   1 + 2^6 + 2^13 land on 11-13 and 27-29 as red, green, blue (the only two products on one bit, 6 and 22, carry into 7 and 23, clear of them).
   Both make the line's 640 colours 0-7, a byte each */
static inline uint32_t two_bbc(uint32_t w) { return ((w >> 4) & 0x08410841u) * 0x2041u; }

static HOT void row_teletext(const uint16_t *src, uint8_t *d) {
    for(int c = 0; c < PIC_W / 16; c++) {
        const uint16_t *cell = src + c * 16;
        uint32_t a = two_bbc(cell[0] | (uint32_t)cell[2] << 16), b = two_bbc(cell[5] | (uint32_t)cell[8] << 16),
                 e = two_bbc(cell[11] | (uint32_t)cell[14] << 16);
        uint8_t *k = d + c * 16;
        k[0] = k[1] = k[14] = k[15] = (uint8_t)(a >> 11 & 7);
        k[2] = k[3] = (uint8_t)(a >> 11 & 7);
        k[4] = k[5] = (uint8_t)(a >> 27 & 7);
        k[6] = k[7] = (uint8_t)(b >> 11 & 7);
        k[8] = k[9] = (uint8_t)(b >> 27 & 7);
        k[10] = k[11] = (uint8_t)(e >> 11 & 7);
        k[12] = k[13] = (uint8_t)(e >> 27 & 7);
    }
}

static HOT void row_pixels(const uint16_t *src, uint8_t *d) {
    const uint32_t *s = (const uint32_t *)src;
    uint32_t *o = (uint32_t *)d;
    for(int x = 0; x < PIC_W / 4; x++) {
        uint32_t a = two_bbc(s[2 * x]), b = two_bbc(s[2 * x + 1]);
        o[x] = (a >> 11 & 7) | (a >> 27 & 7) << 8 | (b >> 11 & 7) << 16 | (b >> 27 & 7) << 24;
    }
}

/* a line of frank-micro's menus over the picture's: drawn at 320 x 256 at the frame's start, each pixel twice across where they draw */
static void ui_row(int y, uint8_t *d) {
    const uint8_t *s = ui_fb + y * UI_W;
    for(int x = 0; x < UI_W; x++) if(s[x] != UI_CLEAR) d[2 * x] = d[2 * x + 1] = (uint8_t)((s[x] & 7) | 8);
}

/* the line onto the page: MODE 3 has two pixels a byte, the left in the low nibble; on each of the line's rows */
static HOT void rows_repeat(int r0, int r1) {  /* (the line's first row on its others) */
    const uint32_t *d = (const uint32_t *)(video.fb + (size_t)r0 * video.pitch + pic_xoff);
    for(int r = r0 + 1; r < r1; r++) {
        uint32_t *o = (uint32_t *)(video.fb + (size_t)r * video.pitch + pic_xoff);
        for(int i = 0; i < PIC_W / 8; i++) o[i] = d[i];
    }
}

static HOT void row_out(int y, const uint8_t *line) {
    int r0 = row_first[y], r1 = row_first[y + 1];
    if(r0 >= r1) return;
    const uint32_t *s = (const uint32_t *)line;
    uint16_t *o = (uint16_t *)(video.fb + (size_t)r0 * video.pitch + pic_xoff);
    for(int x = 0; x < PIC_W / 4; x++) {
        uint32_t w = s[x];                      /* four pixels, a byte each */
        o[x] = (uint16_t)((w & 0xFu) | (w >> 4 & 0xF0u) | (w >> 8 & 0xF00u) | (w >> 12 & 0xF000u));
    }
    rows_repeat(r0, r1);
}

/* a line of B-em's (no menus over it, no screenshot) straight onto the page: row_pixels' colours as MODE 3's nibbles */
static HOT void row_direct(int y, const uint16_t *src) {
    int r0 = row_first[y], r1 = row_first[y + 1];
    if(r0 >= r1) return;
    const uint32_t *s = (const uint32_t *)src;
    uint32_t *o = (uint32_t *)(video.fb + (size_t)r0 * video.pitch + pic_xoff);
    for(int x = 0; x < PIC_W / 8; x++) {        /* eight pixels: four words of B-em's, one of the page's */
        uint32_t a = two_bbc(s[4 * x]), b = two_bbc(s[4 * x + 1]), c = two_bbc(s[4 * x + 2]), d = two_bbc(s[4 * x + 3]);
        o[x] = (a >> 11 & 7) | (a >> 23 & 0x70) | (b >> 3 & 0x700) | (b >> 15 & 0x7000) |
               (c << 5 & 0x70000) | (c >> 7 & 0x700000) | (d << 13 & 0x7000000) | (d << 1 & 0x70000000);
    }
    rows_repeat(r0, r1);
}

/* the frame's end: the BBC's 50 frames a second by the clock (at 576p the display's own frame, a hair slower), unless
   frank-micro's Limit Speed is off. A frame late is caught up; one far behind (the start, a disc being read) is not */
static void frame_pace(void) {
    uint64_t now = cmm2->micros();
    if(!g_frank_limit_speed || next_frame == 0 || (int64_t)(now - next_frame) > 5 * (int64_t)frame_us) next_frame = now;
    else if(next_frame > now) {
        cmm2->delay_us((uint32_t)(next_frame - now));
        if(stats) stat_idle += next_frame - now;
    }
    next_frame += frame_us;
}

/* B-em's lines (display.c, built X_GUI): frank_gui.c's bookkeeping, a frame of 256, numbered as scanvideo numbers them */
/* B-em's line (RGB555) and MODE 7's second, and the line's colours: in the SRAM part (B-em writes 1,280 bytes a line;
   in the PSRAM they took half the time) */
static uint16_t row0[PIC_W] __attribute__((section(".sram_bss"), aligned(32)));
static uint16_t row1[PIC_W] __attribute__((section(".sram_bss"), aligned(32)));
static uint8_t line8[PIC_W] __attribute__((section(".sram_bss"), aligned(4)));
static struct scanvideo_scanline_buffer s_buffer;
static int s_scanline_number;
static uint32_t s_frame_number_part;

int x_gui_init(void) { return 0; }

HOT struct scanvideo_scanline_buffer *x_gui_begin_scanline(void) {
    if(s_scanline_number++ == 0) {              /* a frame begins */
        current_buffer ^= 1u;
        s_frame_number_part += 0x10000u;
        shot_frame = g_frank_screenshot_pending && (shot != NULL || (shot = malloc(PIC_W * PIC_H)) != NULL);
        ui_frame = frank_ui_is_visible();
        if(ui_frame) {
            memset(ui_fb, UI_CLEAR, sizeof(ui_fb));
            frank_ui_render(ui_fb, UI_W, UI_H);
        }
    }
    s_buffer.row0 = row0;
    s_buffer.row1 = s_buffer.double_height ? row1 : NULL;
    s_buffer.scanline_id = s_frame_number_part | (uint32_t)s_scanline_number;
    for(int i = 0; i < PIC_W / 2; i++) ((uint32_t *)row0)[i] = 0;
    if(s_buffer.row1) for(int i = 0; i < PIC_W / 2; i++) ((uint32_t *)row1)[i] = 0;
    return &s_buffer;
}

HOT void x_gui_end_scanline(struct scanvideo_scanline_buffer *buffer) {
    int y = s_scanline_number - 1;
    uint32_t c0 = DWT_CYCCNT;
    if(y >= 0 && y < PIC_H) {
        if(!buffer->double_height && !shot_frame && !ui_frame) row_direct(y, buffer->row0);
        else {
            if(buffer->double_height) row_teletext(buffer->row0, line8);
            else row_pixels(buffer->row0, line8);
            if(shot_frame) memcpy(shot + y * PIC_W, line8, PIC_W);   /* (the BBC's picture, before the menus) */
            if(ui_frame) ui_row(y, line8);
            row_out(y, line8);
        }
    }
    stat_disp += DWT_CYCCNT - c0;
    if(s_scanline_number >= PIC_H) {            /* the frame's end */
        s_scanline_number = 0;
        if(shot_frame) {
            shot_frame = g_frank_screenshot_pending = false;
            frank_ui_toast(frank_screenshot_save(shot) == 0 ? "Screenshot saved" : "Screenshot failed");
        }
        frame_pace();
    }
}

/* the page as it is, for tests (Ctrl-F then P on the serial console): page.bin, MODE 3's 16 colours (R, G, B) then its
   rows (two pixels a byte, the left in the low nibble) */
static void page_dump(void) {
    int h = cmm2->file_open("page.bin", CMM2_O_WRITE | CMM2_O_CREATE);
    if(h < 0) {
        printf("bbc: cannot write page.bin\n");
        return;
    }
    cmm2->file_write(h, shown16, sizeof(shown16));
    for(uint32_t r = 0; r < video.height; r++) cmm2->file_write(h, video.fb + r * video.pitch, video.width / 2);
    cmm2->file_close(h);
    printf("bbc: page.bin written\n");
}

void x_gui_refresh_menu_display(void) { }
void x_gui_set_force_aspect_ratio(bool far) { (void)far; }

/* ---- the sound ---- */
#define RING 8192                               /* a power of 2 */
#define RING_LEVEL 3000                         /* samples: a stream buffer of 1024, a frame of 625 and more */
#define RING_FULL 7000
#define BBC_HZ 31250                            /* B-em's samples a second, of the BBC's time */
#define CHUNK 256                               /* B-em's buffer: even */
#define SOUND_GAIN 8                            /* B-em's SN76489 makes about a tenth of full scale (frank-micro's HDMI: the set's volume) */
static int16_t ring[RING];
static uint32_t ring_phase, ring_step;
bool x_gui_audio_init_failed;
volatile int g_frank_audio_driver;              /* (frank_settings.h's: the RP2350's HDMI, I2S or PWM; one here) */

void frank_audio_set_driver(int drv) { g_frank_audio_driver = drv; }

/* in PendSV: no services. The emulator keeps its own time and the stream the board's, so the step is trimmed, by up to 1%, to hold the ring at RING_LEVEL */
static void fill(int16_t *buf, uint32_t frames) {
    static bool playing;
    static int16_t v;
    static int32_t smooth = RING_LEVEL << 4;
    uint32_t out = ring_out, in = ring_in, ph = ring_phase, level = in - out;
    smooth += (int32_t)level - (smooth >> 4);
    int32_t err = (smooth >> 4) - RING_LEVEL;
    if(err > RING_LEVEL) err = RING_LEVEL;
    if(err < -RING_LEVEL) err = -RING_LEVEL;
    uint32_t step = ring_step + (int32_t)(((int64_t)ring_step * err) / (RING_LEVEL * 100));
    if(!playing && level >= RING_LEVEL) playing = true;
    while(frames--) {
        if(playing) {
            if(out == in) playing = false;      /* (empty: waits for RING_LEVEL again) */
            else {
                v = ring[out & (RING - 1)];
                ph += step;
                out += ph >> 16;
                ph &= 0xFFFF;
                if((int32_t)(in - out) < 0) out = in;
            }
        }
        *buf++ = v;
        *buf++ = v;
    }
    ring_out = out;
    ring_phase = ph;
}

/* frank_audio.c's: B-em's one buffer of mono samples, taken and given back full */
static int16_t s_mono[CHUNK];
static mem_buffer_t s_mem = { .size = sizeof(s_mono), .bytes = (uint8_t *)s_mono, .flags = 0 };
static struct audio_format s_fmt;
static struct audio_buffer_format s_bfmt = { .format = &s_fmt, .sample_stride = 2 };
static struct audio_buffer s_buf = { .buffer = &s_mem, .format = &s_bfmt, .max_sample_count = CHUNK };
static struct audio_buffer_pool s_pool;

struct audio_buffer_pool *x_gui_audio_init(uint freq) {
    s_fmt.sample_freq = freq;
    s_fmt.format = AUDIO_BUFFER_FORMAT_PCM_S16;
    s_fmt.channel_count = 1;
    x_gui_audio_init_failed = false;
    if(stats) {                                 /* (the largest block of the SRAM heap left: alloc takes the SRAM first) */
        uint32_t n = 28 * 1024;
        for(; n >= 1024; n -= 512) {
            void *p = cmm2->alloc(n);
            bool s = p != NULL && (uintptr_t)p >= 0x20000000u && (uintptr_t)p < 0x20082000u;
            if(p) cmm2->release(p);
            if(s) break;
        }
        printf("bbc: before audio_start, the SRAM heap's largest free block %lu bytes\n", (unsigned long)n);
    }
    int r = cmm2->audio_start(fill);
    audio_on = r > 0;
    if(stats || !audio_on) printf("bbc: audio_start %d%s\n", r, r == -3 ? " (no room in the SRAM heap for its buffers)" : "");
    if(audio_on) ring_step = (uint32_t)(((uint64_t)freq << 16) / (uint32_t)r);
    return &s_pool;
}

struct audio_buffer *take_audio_buffer(struct audio_buffer_pool *ac, bool block) {
    (void)ac; (void)block;
    s_buf.sample_count = 0;
    return &s_buf;
}

void give_audio_buffer(struct audio_buffer_pool *ac, struct audio_buffer *buffer) {
    const int16_t *src = (const int16_t *)buffer->buffer->bytes;
    int vol = g_frank_sound_on ? g_frank_volume : 0;
    (void)ac;
    if(vol > 100) vol = 100;
    for(uint32_t i = 0; i < buffer->sample_count; i++) {
        uint32_t in = ring_in;
        int a = src[i] < 0 ? -src[i] : src[i];
        if(a > stat_peak) stat_peak = a;
        if(!audio_on || in - ring_out >= RING_FULL) break;
        int32_t v = (int32_t)src[i] * vol * SOUND_GAIN / 100;   /* (B-em's four channels at their loudest are about 3,000) */
        ring[in & (RING - 1)] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        ring_in = in + 1;
    }
}

/* B-em's disc noise streams samples from the RP2350's flash (sector_read's xip_stream): not here (frank_audio.c's) */
#include "xip_stream.h"
struct xip_stream_dma *xip_stream_head;
void xip_stream_init(void) { }
void xip_stream_dma_start(struct xip_stream_dma *dma) { if(dma) dma->state = COMPLETE; }
void xip_stream_dma_cancel(struct xip_stream_dma *dma) { if(dma) dma->state = NONE; }
void xip_stream_dma_poll(void) { }

/* ---- the keyboard: frank-micro's USB HID driver's key actions (usbhid.h), from the USB keyboard's raw reports ---- */
#define M_CTRL 0x11                             /* the report's modifier bits: left and right */
#define M_SHIFT 0x22
#define M_ALT 0x44
#define H_PAUSE 0x48                            /* HID usages */
#define H_END 0x4D
#define KEYQ 64
static struct { uint8_t hid, down; } keyq[KEYQ];
static uint32_t keyq_in, keyq_out;
static bool usb_keys_down;                      /* (a USB key's character reaches the serial console too: it is not typed twice) */

static void key_action(uint8_t hid, int down) {
    if(keyq_in - keyq_out >= KEYQ) return;
    keyq[keyq_in % KEYQ].hid = hid;
    keyq[keyq_in % KEYQ].down = (uint8_t)down;
    keyq_in++;
}

int usbhid_get_key_action(uint8_t *hid, int *down) {
    if(keyq_out == keyq_in) return 0;
    *hid = keyq[keyq_out % KEYQ].hid;
    *down = keyq[keyq_out % KEYQ].down;
    keyq_out++;
    return 1;
}

static bool in_report(const uint8_t *r, uint8_t h) {
    for(int i = 2; i < 8; i++) if(r[i] == h) return true;
    return false;
}

static void bbc_break(void) { m6502_reset(); }  /* (as frank-micro's SHIFT-BREAK autoboot: the 6502 reset, the BBC's memory as it is) */

/* each frame (frank_keyboard_poll): the USB devices seen to, the keys that went down and up since the last report as frank-micro's driver gives them
   (its shared Ctrl, Shift and Alt as 0xE0-0xE2), Pause as BREAK, Ctrl+Alt+End to leave */
static void typing_frame(void);
static void keys_by_character(uint8_t *r);
void usbhid_wrapper_init(void) { }
int usbhid_wrapper_keyboard_connected(void) { return 1; }
int usbhid_wrapper_gamepad_connected(void) { return 1; }
int usbhid_wrapper_get_key(int *pressed, unsigned char *key) { (void)pressed; (void)key; return 0; }

void usbhid_wrapper_tick(void) {
    static uint8_t last[8];
    uint8_t r[8];
    cmm2->poll();
    cmm2->break_take();                         /* (Ctrl-C is the BBC's) */
    memset(r, 0, sizeof(r));
    cmm2->keys_raw(r);
    if((r[0] & M_CTRL) && (r[0] & M_ALT) && in_report(r, H_END)) exit(0);   /* Ctrl+Alt+End leaves */
    keys_by_character(r);
    static const uint8_t mod[3] = { M_CTRL, M_SHIFT, M_ALT };
    for(int m = 0; m < 3; m++) if((r[0] & mod[m]) != (last[0] & mod[m])) key_action((uint8_t)(0xE0 + m), (r[0] & mod[m]) != 0);
    for(int i = 2; i < 8; i++) if(last[i] >= 4 && !in_report(r, last[i])) key_action(last[i], 0);
    for(int i = 2; i < 8; i++) if(r[i] >= 4 && !in_report(last, r[i])) {
        if(r[i] == H_PAUSE) bbc_break();
        else key_action(r[i], 1);
    }
    usb_keys_down = r[0] != 0 || r[2] != 0 || last[0] != 0 || last[2] != 0;
    memcpy(last, r, sizeof(last));
    typing_frame();
}

/* the first gamepad, as frank-micro's USB driver gives it (frank_keyboard.c's BTN_ bits), its left stick as the d-pad */
unsigned int usbhid_wrapper_get_joystick(void) {
    enum { BTN_LEFT = 0x0001, BTN_RIGHT = 0x0002, BTN_UP = 0x0004, BTN_DOWN = 0x0008, BTN_FIREA = 0x0010, BTN_FIREB = 0x0020, BTN_START = 0x0100,
           BTN_SELECT = 0x0200, BTN_FIREX = 0x0800, BTN_FIREY = 0x1000 };
    for(int slot = 1; slot <= 8; slot++) {
        int x = 128, y = 128, b = cmm2->gamepad(slot, &x, &y);
        unsigned int m = 0;
        if(b < 0) continue;
        if((b >> 7 & 1) || y < 64) m |= BTN_UP;
        if((b >> 5 & 1) || y > 192) m |= BTN_DOWN;
        if((b >> 8 & 1) || x < 64) m |= BTN_LEFT;
        if((b >> 6 & 1) || x > 192) m |= BTN_RIGHT;
        if(b >> 11 & 1) m |= BTN_FIREA;         /* a/circle */
        if(b >> 13 & 1) m |= BTN_FIREB;         /* b/cross */
        if(b >> 10 & 1) m |= BTN_FIREX;         /* x/triangle */
        if(b >> 12 & 1) m |= BTN_FIREY;         /* y/square */
        if(b >> 1 & 1) m |= BTN_START;
        if(b >> 3 & 1) m |= BTN_SELECT;
        if(stats) {                             /* (-stats: each change of the pad, as the firmware gives it) */
            static int last_b = -1, last_x = -1, last_y = -1, last_slot;
            if(b != last_b || slot != last_slot || (x >> 4) != (last_x >> 4) || (y >> 4) != (last_y >> 4)) {
                printf("bbc: pad %d buttons %04X stick %d,%d -> %04X\n", slot, b, x, y, m);
                last_b = b; last_x = x; last_y = y; last_slot = slot;
            }
        }
        return m;
    }
    return 0;
}

unsigned int usbhid_wrapper_get_joystick_idx(int idx) { return idx == 0 ? usbhid_wrapper_get_joystick() : 0; }

/* no wired NES pad (frank_keyboard.c's nespad) */
uint32_t nespad_state, nespad_state2;
bool nespad_begin(uint32_t cpu_khz, uint8_t clkPin, uint8_t dataPin, uint8_t dataPin2, uint8_t latPin) {
    (void)cpu_khz; (void)clkPin; (void)dataPin; (void)dataPin2; (void)latPin;
    return false;
}
void nespad_read(void) { }

/* ---- the serial console's typing: each character as the BBC's own keyboard makes it, by B-em's key codes (keyboard.c's allegro2bbc) ---- */
enum { AK_A = 1, AK_0 = 27, AK_1 = 28, AK_F1 = 47, AK_ESCAPE = 59, AK_TILDE = 60, AK_MINUS, AK_EQUALS, AK_BACKSPACE, AK_TAB, AK_OPENBRACE,
       AK_CLOSEBRACE, AK_ENTER, AK_SEMICOLON, AK_QUOTE, AK_BACKSLASH, AK_BACKSLASH2, AK_COMMA, AK_FULLSTOP, AK_SLASH, AK_SPACE,
       AK_LSHIFT = 215, AK_LCTRL = 217 };
#define S 0x100                                 /* with SHIFT */
#define C 0x200                                 /* with CTRL */

/* the BBC's key for character c (and SHIFT or CTRL), or 0 */
static int bbc_key(int c) {
    static const char sym[] = "!\"#$%&'()";      /* SHIFT and 1-9 */
    const char *q;
    if(c >= 'A' && c <= 'Z') return AK_A + c - 'A';              /* (CAPS LOCK on, as the BBC starts) */
    if(c >= 'a' && c <= 'z') return (AK_A + c - 'a') | S;
    if(c == '0') return AK_0;
    if(c >= '1' && c <= '9') return AK_1 + c - '1';
    if(c && (q = strchr(sym, c)) != NULL) return (AK_1 + (int)(q - sym)) | S;
    switch(c) {
        case ' ': return AK_SPACE;
        case '-': return AK_MINUS;
        case '=': return AK_MINUS | S;
        case '^': return AK_EQUALS;
        case '~': return AK_EQUALS | S;
        case '@': return AK_OPENBRACE;
        case '[': return AK_CLOSEBRACE;
        case '{': return AK_CLOSEBRACE | S;
        case ';': return AK_SEMICOLON;
        case '+': return AK_SEMICOLON | S;
        case ':': return AK_QUOTE;
        case '*': return AK_QUOTE | S;
        case ']': return AK_BACKSLASH;
        case '}': return AK_BACKSLASH | S;
        case '\\': return AK_BACKSLASH2;
        case '|': return AK_BACKSLASH2 | S;
        case '_': return AK_TILDE;
        case '`': case 163: return AK_TILDE | S;   /* (the BBC's pound sign, its character 96; 163 is a French keyboard's) */
        case ',': return AK_COMMA;
        case '<': return AK_COMMA | S;
        case '.': return AK_FULLSTOP;
        case '>': return AK_FULLSTOP | S;
        case '/': return AK_SLASH;
        case '?': return AK_SLASH | S;
        case 13: return AK_ENTER;
        case 8: case 127: return AK_BACKSPACE;
        case 9: return AK_TAB;
        case 27: return AK_ESCAPE;
    }
    if(c >= 1 && c <= 26) return (AK_A + c - 1) | C;
    return 0;
}

/* ---- the USB keyboard by character: what is on the key is what the BBC types, in any OPTION USBKEYBOARD layout (Peter, 12 Nov 2026), as the CPC's
   (apps/cpc). frank-micro's USB path takes a key by its place on a PC's keyboard and B-em puts the BBC's keys there (keyboard.c), so Shift and 8 was the
   BBC's SHIFT and 8, which is (. Here each key of the main block, as it goes down, becomes the place of the BBC key that types the character the
   firmware's layout makes of it (keys_held; bbc_key's table), and the BBC's SHIFT is as that character needs and not as the PC's is: * is SHIFT and the
   BBC's : key, @ its @ key with SHIFT up. Decided as the key goes down, so that a Shift let go first does not change a key still held. Letters keep
   the PC's Shift (CAPS LOCK is the BBC's own); the keypad, the arrows, the function keys, Return and the rest keep their places. As the CPC found, a key
   that needs SHIFT up is held back SHIFT_SETTLE frames from when it went up; with no Shift held there is no wait. RUN's -keys prints each key */
static int keytrace = -1;
#define SHIFT_SETTLE 3

static uint8_t ak_place(int ak) {               /* the USB key at the place of B-em's key ak (frank_keyboard.c's hid_to_xt and patches/0003) */
    if(ak >= AK_A && ak < AK_A + 26) return (uint8_t)(0x04 + ak - AK_A);
    if(ak == AK_0) return 0x27;
    if(ak >= AK_1 && ak < AK_1 + 9) return (uint8_t)(0x1E + ak - AK_1);
    switch(ak) {
        case AK_TILDE: return 0x35;
        case AK_MINUS: return 0x2D;
        case AK_EQUALS: return 0x2E;
        case AK_OPENBRACE: return 0x2F;
        case AK_CLOSEBRACE: return 0x30;
        case AK_BACKSLASH: return 0x31;
        case AK_SEMICOLON: return 0x33;
        case AK_QUOTE: return 0x34;
        case AK_COMMA: return 0x36;
        case AK_FULLSTOP: return 0x37;
        case AK_SLASH: return 0x38;
        case AK_BACKSLASH2: return 0x64;
    }
    return 0;
}

static void keys_by_character(uint8_t *r) {
    static uint8_t was[6], place[256];
    static int8_t shift[256];
    static int shift_up = SHIFT_SETTLE;         /* frames the BBC's SHIFT has been up */
    uint8_t out[6] = {0};
    int keys[7], total = 0, force = -1, i, fresh = 0, mods = r[0];
    if(keytrace < 0) keytrace = has_arg("-keys");
    for(i = 0; i < 6 && r[i + 2]; i++) total++;
    cmm2->keys_held(keys);
    for(i = 0; i < total; i++) {
        uint8_t h = r[i + 2];
        if(memchr(was, h, sizeof(was)) == NULL) {
            int c = keys[total - 1 - i], k = 0;   /* (keys_held has the last key of the report first) */
            uint8_t p = 0;
            bool letter = h >= 0x04 && h <= 0x1D;
            if(c >= 1 && c <= 26) c += 96;      /* (Ctrl with a letter) */
            if(h < 0x28 || (h >= 0x2D && h <= 0x38) || h == 0x64) {
                k = bbc_key(c);
                if(!(k & C)) p = ak_place(k & 0xFF);
            }
            place[h] = p ? p : h;
            shift[h] = !p || (p >= 0x04 && p <= 0x1D && letter) ? -1 : (k & S) != 0;
            fresh = 1;
            if(keytrace) printf("bbc: key %02X of %d: character %d, place %02X, shift %d\n", h, total, c, place[h], shift[h]);
        }
        out[i] = place[h];
        if(shift[h] >= 0) force = shift[h];
    }
    if(force == 0) r[0] &= ~M_SHIFT;
    else if(force == 1) r[0] |= 0x02;
    if(r[0] & M_SHIFT) shift_up = 0;
    else if(shift_up < SHIFT_SETTLE) {
        shift_up++;
        for(i = 0; i < total; i++) if(shift[r[i + 2]] == 0) out[i] = 0;     /* (the next frames offer it again) */
    }
    memcpy(was, r + 2, sizeof(was));
    memcpy(r + 2, out, sizeof(out));
    if(force >= 0) r[0] &= ~0x40;               /* (AltGr made the character) */
    if(keytrace && fresh) printf("bbc:   modifiers %02X became %02X, keys %02X %02X\n", mods, r[0], r[2], r[3]);
}

#define HOLD 3                                  /* frames a key is held, then frames up before the next */
#define GAP 2
static int typed_q[1024];
static uint32_t typed_in, typed_out;

static void typed(int k) {
    if(typed_in - typed_out < sizeof(typed_q) / sizeof(typed_q[0])) typed_q[typed_in++ % (sizeof(typed_q) / sizeof(typed_q[0]))] = k;
}

/* each frame: the console's characters queued (Ctrl-F then 0-9 f0-f9, u d l r the cursor keys, p n Page Up and Down, b BREAK, B CTRL and BREAK, m the disc menu,
   s the settings; Ctrl-] leaves), and the next typed key pressed and let go; while frank-micro's menus are up, Return, Escape, Backspace and
   those keys are the menu's */
enum { AK_PGUP = 80, AK_PGDN, AK_LEFT, AK_RIGHT, AK_UP, AK_DOWN };
static unsigned int menu_key(int k) {           /* (frank-micro's keysym for a typed key, or 0) */
    switch(k) {
        case AK_ENTER: return FRANK_KS_Return;
        case AK_ESCAPE: return FRANK_KS_Escape;
        case AK_BACKSPACE: return FRANK_KS_BackSpace;
        case AK_UP: return FRANK_KS_Up;
        case AK_DOWN: return FRANK_KS_Down;
        case AK_LEFT: return FRANK_KS_Left;
        case AK_RIGHT: return FRANK_KS_Right;
        case AK_PGUP: return FRANK_KS_Page_Up;
        case AK_PGDN: return FRANK_KS_Page_Down;
    }
    return 0;
}

static void typing_frame(void) {
    static int prefix, held, phase, last, ctrl_hold;
    int c;
    if(ctrl_hold && --ctrl_hold == 0) key_up(AK_LCTRL);
    while((c = cmm2->con_getc()) >= 0) {
        if(c == 0x1D) exit(0);                  /* Ctrl-], whatever the USB keyboard is doing */
        if(usb_keys_down) continue;             /* (the USB keyboard's own) */
        if(prefix) {
            static const char keys[] = "udlrpn";
            static const int codes[] = { AK_UP, AK_DOWN, AK_LEFT, AK_RIGHT, AK_PGUP, AK_PGDN };
            const char *q = c ? strchr(keys, c) : NULL;
            prefix = 0;
            if(c >= '0' && c <= '9') typed(AK_F1 + c - '0');
            else if(q) typed(codes[q - keys]);
            else if(c == 'b') typed(-1);
            else if(c == 'B') typed(-4);
            else if(c == 'm') typed(-2);
            else if(c == 's') typed(-3);
            else if(c == 'P') typed(-5);
            continue;
        }
        if(c == 6) {
            prefix = 1;
            continue;
        }
        if(c == 0x1D) exit(0);                  /* Ctrl-] */
        if(c == 10 && last == 13) continue;     /* (CR LF: one RETURN) */
        last = c;
        if(c == 10) c = 13;
        int k = bbc_key(c);
        if(k) typed(k);
    }
    if(phase > 0) {
        if(--phase == GAP) {                    /* let go */
            key_up(held & 0xFF);
            if(held & S) key_up(AK_LSHIFT);
            if(held & C) key_up(AK_LCTRL);
        }
        return;
    }
    if(typed_out == typed_in) return;
    held = typed_q[typed_out++ % (sizeof(typed_q) / sizeof(typed_q[0]))];
    if(held == -1) {
        bbc_break();
        return;
    }
    if(held == -4) {                            /* CTRL and BREAK: the hard reset (a Master's ROMs in sideways RAM seen), CTRL held a second */
        key_down(AK_LCTRL);
        bbc_break();
        ctrl_hold = 50;
        return;
    }
    if(held == -2) {
        frank_ui_open_disk_menu();
        return;
    }
    if(held == -3) {
        frank_ui_toggle();
        return;
    }
    if(held == -5) {
        page_dump();
        return;
    }
    if(frank_ui_wants_keys()) {                 /* (the menu's) */
        unsigned int ks = menu_key(held & 0xFF);
        if(ks) frank_ui_handle_key(ks);
        return;
    }
    if(held & S) key_down(AK_LSHIFT);
    if(held & C) key_down(AK_LCTRL);
    key_down(held & 0xFF);
    phase = HOLD + GAP;
}

/* ---- the sideways ROMs of the folder roms (A:/bbc/roms; Peter, 12 Nov 2026) ----
   Each .rom or .bin file there, in the order of their names, goes into a free sideways RAM bank, from the lowest: the Master's 4 to 7, the Model B's
   from 0 (B-em's banks are frank-micro's heap blocks: NO_USE_RAM_ROMS, alloc_swram). An 8 KB ROM is put in twice, as a 16 KB socket shows it, a shorter
   one with &FF after it; a longer one is left out. Then the BBC is started again as from switching on (B-em's main_reset, Ctrl+Alt+Delete's), so that
   its MOS finds them, as it finds ROMs only then (the Master's after CTRL-BREAK too). Done before the first frame's end and again after the model is
   changed, which makes the banks afresh */
#include <strings.h>
#include "mem.h"
extern void main_reset(void);
extern int8_t curmodel;
#define ROMS_MAX 16

static int name_cmp(const void *a, const void *b) { return strcasecmp(*(char *const *)a, *(char *const *)b); }

static int roms_load(void) {
    char *names[ROMS_MAX];
    cmm2_dirent e;
    int n = 0, loaded = 0, slot = 0, d = cmm2->dir_open("roms");
    if(d < 0) return 0;
    while(cmm2->dir_read(d, &e) == 1) {
        const char *dot = strrchr(e.name, '.');
        if(e.is_dir || dot == NULL || (strcasecmp(dot, ".rom") != 0 && strcasecmp(dot, ".bin") != 0)) continue;
        if(n == ROMS_MAX) {
            printf("bbc: roms/%s left out: no more than %d are read\n", e.name, ROMS_MAX);
            continue;
        }
        if((names[n] = strdup(e.name)) != NULL) n++;
    }
    cmm2->dir_close(d);
    qsort(names, (size_t)n, sizeof(names[0]), name_cmp);
    for(int i = 0; i < n; i++) {
        char path[300];
        int h;
        int32_t size;
        while(slot < ROM_NSLOT && !(rom_slots[slot].swram && rom_slots[slot].data)) slot++;
        snprintf(path, sizeof(path), "roms/%s", names[i]);
        free(names[i]);
        if(slot == ROM_NSLOT) {
            printf("bbc: %s left out: no sideways RAM bank is free\n", path);
            continue;
        }
        if((h = cmm2->file_open(path, CMM2_O_READ)) < 0) {
            printf("bbc: cannot open %s\n", path);
            continue;
        }
        size = cmm2->file_size(h);
        if(size <= 0 || size > ROM_SIZE) {
            printf("bbc: %s left out: %ld bytes, not 1 to 16384\n", path, (long)size);
            cmm2->file_close(h);
            continue;
        }
        uint8_t *bank = (uint8_t *)rom_slots[slot].data;   /* (sideways RAM: frank-micro's heap) */
        memset(bank, 0xFF, ROM_SIZE);
        int32_t got = cmm2->file_read(h, bank, (uint32_t)size);
        cmm2->file_close(h);
        if(got != size) {
            printf("bbc: cannot read %s\n", path);
            memset(bank, 0xFF, ROM_SIZE);
            continue;
        }
        if(size == ROM_SIZE / 2) memcpy(bank + ROM_SIZE / 2, bank, ROM_SIZE / 2);
        if(bank[7] + 4 <= size && memcmp(bank + bank[7], "\0(C)", 4) == 0) {
            char title[64];
            int t = 0;
            for(int k = 9; k < bank[7] && t < (int)sizeof(title) - 1; k++) title[t++] = bank[k] >= 32 && bank[k] < 127 ? (char)bank[k] : ' ';
            title[t] = 0;
            printf("bbc: %s in bank %d: %s\n", path, slot, title);
        } else
            printf("bbc: %s in bank %d has no ROM header: the BBC will not see it\n", path, slot);
        slot++;
        loaded++;
    }
    return loaded;
}

static void roms_check(void) {
    static int roms_model = -1;
    if(curmodel == roms_model) return;
    roms_model = curmodel;
    int n = roms_load();
    if(n > 0) {
        char msg[40];
        main_reset();
        snprintf(msg, sizeof(msg), "%d ROM%s from roms", n, n == 1 ? "" : "s");
        frank_ui_toast(msg);
    }
}

/* ---- frank_platform.c's frank_perf_tick, called by al_wait_for_event once a frame of the BBC's (40,000 cycles, 20 ms); the pace is
   frame_pace's, at the frame's end ---- */
void frank_perf_tick(void) {
    static bool inited;
    roms_check();                               /* (the first frame, and after a change of model) */
    if(!inited) {                               /* (B-em's discs are ready once main_init has run) */
        inited = true;
        frank_settings_apply_live();
        frank_disk_autoload();
        for(int i = 1, drive = 0; i < cmm2_start->argc && drive < 2; i++)   /* RUN "bbc.app", disc [disc]: drives 0 and 1 */
            if(cmm2_start->argv[i][0] != '-' && cmm2_start->argv[i][0] != ',') {
                char name[256];
                snprintf(name, sizeof(name), "%s", cmm2_start->argv[i]);
                size_t n = strlen(name);
                if(n && name[n - 1] == ',') name[n - 1] = 0;   /* (RUN's words are split at spaces: "disc," is the disc) */
                if(frank_disc_mount(drive, name) != 0) printf("bbc: cannot open %s\n", name);
                drive++;
            }
        if(frank_disc_name(0)) frank_disc_request_boot();
    }
    frank_disc_autoboot_tick();
    palette_show();
    stat_frame();
}

/* ---- the start ---- */
int main(int argc, char **argv) {
    static const uint32_t bbc[8] = { 0x000000, 0xFF0000, 0x00FF00, 0xFFFF00, 0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF };
    static FATFS fs;
    (void)argc; (void)argv;
    stats = has_arg("-stats");
    if(stats) stack_paint();
    g_frank_disc_readonly = has_arg("-ro");
    for(int i = 0; i < 256; i++) graphics_set_palette((uint8_t)i, bbc[i & 7]);   /* (frank_platform.c's) */
    frank_ui_init();                            /* (the menus' colours, 240-247) */
    if(display_start() < 0) return 1;
    cmm2->con_screen(0);                        /* (its messages to the serial console alone, from here on) */
    if(stats) printf("bbc: %lu x %lu, line buffers at %p, the display's code at %p\n", (unsigned long)video.width,
                     (unsigned long)video.height, (void *)row0, (void *)row_direct);
    DEMCR |= 1u << 24;                          /* (the DWT on, its cycle counter running: -stats' display share) */
    DWT_CTRL |= 1u;
    memset(cmm2_garbage, 0xFF, sizeof(cmm2_garbage));
    palette_show();
    frank_gamepad_init();
    f_mount(&fs, "", 1);
    frank_settings_load();
    stat_from = cmm2->micros();
    _al_mangled_main(0, NULL);
    return 0;
}
