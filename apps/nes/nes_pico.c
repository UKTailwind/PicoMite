/* nes_pico.c: frank-nes's drivers on the PicoMite (the PC3), by the loader's services. From the Colour Maximite 2's nes_cmm2.c
   (Peter, 5 Nov 2026), changed where the PicoMite differs (16 Nov 2026):

   - The display: MODE 5, 320 x 240 in 256 colours from MAP's palette: frank-nes's soft framebuffer (the 8-bit 256 x 240 picture it
     makes for its PIO HDMI output) copied to the middle of the page as the next frame starts to be emulated (usbhid_task, which the
     emulator calls first each frame). The PicoMite has no frame hook, so a frame can be shown torn. Its palette goes to MAP's colours
     when it changes.
   - The sound: the mono samples go through a ring to the firmware's sound stream, each to both channels. A full ring holds the
     emulator back, as a full I2S DMA does on the RP2350: that is what sets its speed, 60.1 frames a second, the display's 59.94 not
     coming into it. Until the PicoMite's services have sound (audio_start returns -1), the samples are counted against the clock
     instead, which sets the same speed.
   - The input: the USB gamepads and keyboard as frank-nes's usbhid.h has them, from the firmware's gamepad service and keys_raw. Its
     NES pads, Wii-style and PS/2 drivers have nothing to do here.
   - The serial console types too, for remote use, as in pc386: W A S D, X, Z, Enter, Space and Esc are the keyboard's keys for a
     moment, Q is F11 and E is F12, while no USB key is held (a USB key's character reaches the console as well, and is left out so).
   - poll, each frame, sees to the USB and tells of Ctrl-C, which ends the program.
   RUN "nes.app", -stats prints the frames a second, and how much of the time the emulator is waiting for the sound, every 5 s. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cmm2.h"
#include "hdmi.h"
#include "psram_init.h"
#include "gamepad_cal.h"
#include "i2s_audio.h"
#include "nespad.h"
#include "ps2kbd_wrapper.h"
#include "pwm_audio.h"
#include "usbhid.h"

/* ---- the memory PSRAM was: the ROM at 0, the tile cache at 2 MB, the selector's lists at 4 MB (main_pico.c, rom_selector.c) ---- */
#define PSRAM_SIZE ((4u << 20) + (320u << 10))  /* (the PicoMite: the ROM list, its metadata and the file browser's
                                                     entries at 4 MB take 294 KB; 5 MB did not fit the PC3) */
uint8_t *cmm2_psram;

void psram_init(uint cs_pin) {
    (void)cs_pin;
    if(cmm2_psram == NULL && (cmm2_psram = malloc(PSRAM_SIZE)) == NULL) {
        fprintf(stderr, "nes: not enough memory\n");
        exit(1);
    }
}

/* ---- time ---- */
uint64_t time_us_64(void) { return cmm2->micros(); }
void sleep_us(uint64_t us) { usleep((useconds_t)us); }
void sleep_ms(uint32_t ms) { usleep((useconds_t)ms * 1000); }

/* ---- the display ---- */
#define NES_W 256
#define NES_H 240
bool SELECT_VGA;
int graphics_buffer_width, graphics_buffer_height;
static cmm2_video video;
static const uint8_t *volatile picture;
static uint8_t palette[256 * 3], shown[256 * 3];

/* the last frame, to the middle of the page */
static void frame(void) {
    const uint8_t *s = picture;
    uint8_t *d = video.fb + (video.width - NES_W) / 2;
    if(s == NULL) return;
    for(int y = 0; y < NES_H; y++, s += NES_W, d += video.pitch) {
        memcpy(d, s, NES_W);
        if(video.copies == 2) memcpy(d + video.width, s, NES_W);
    }
}

void graphics_init(void) {
    if(cmm2->video_mode(5, 8, &video) < 0) {
        fprintf(stderr, "nes: cannot set MODE 5\n");
        exit(1);
    }
    cmm2->con_screen(0);                        /* (its messages to the serial console alone) */
}

void graphics_set_buffer(uint8_t *buffer) { picture = buffer; }
void graphics_set_offset(int x, int y) { (void)x; (void)y; }
void graphics_set_mode(int mode) { (void)mode; }
uint8_t testPins(uint32_t a, uint32_t b) { (void)a; (void)b; return 1; }   /* (not VGA: graphics_restore_sync_colors is called) */

void graphics_set_palette(uint8_t i, uint32_t rgb) {
    palette[i * 3] = rgb >> 16;
    palette[i * 3 + 1] = rgb >> 8;
    palette[i * 3 + 2] = rgb;
}

void graphics_set_palette_hdmi(uint8_t i, uint32_t rgb) { graphics_set_palette(i, rgb); }

/* the end of each palette (main_pico.c): shown if it differs, as video_palette waits for the next frame */
void graphics_restore_sync_colors(void) {
    if(memcmp(palette, shown, sizeof(shown)) == 0) return;
    memcpy(shown, palette, sizeof(shown));
    cmm2->video_palette(0, 256, shown);
}

/* ---- the sound ---- */
#define RING 4096                               /* a power of 2 */
#define RING_FULL 2800                          /* samples: 63 ms, a buffer of 1024 and two frames more */
static int16_t ring[RING];
static volatile uint32_t ring_in, ring_out;
static int audio_on;
static uint32_t audio_rate = 44100;

/* without sound: count samples played against the clock, as the sound stream would play them. More than 100 ms behind (a
   ROM loading, a menu), and the count starts again from now */
static uint64_t pace_from, pace_samples;
static void pace(int count) {
    uint64_t now = cmm2->micros(), due;
    if(pace_from == 0 || now > pace_from + pace_samples * 1000000ull / audio_rate + 100000) {
        pace_from = now;
        pace_samples = 0;
    }
    pace_samples += count;
    due = pace_from + pace_samples * 1000000ull / audio_rate;
    if(due > now) usleep((useconds_t)(due - now));
}

/* in an interrupt: no services */
static void fill(int16_t *buf, uint32_t frames) {
    uint32_t out = ring_out, in = ring_in;
    static int16_t last;
    while(frames--) {
        if(out != in) last = ring[out++ & (RING - 1)];
        *buf++ = last;
        *buf++ = last;
    }
    ring_out = out;
}

static void audio_begin(void) {
    if(audio_on) return;
    audio_on = 1;
    if(cmm2->audio_start(fill) < 0) audio_on = -1;
}

static void poll_quit(void) {
    if(cmm2->poll()) exit(0);                   /* Ctrl-C */
}

/* -stats: frames and the time spent waiting, since the last report */
static int stats = -1;
static uint32_t stat_frames;
static uint64_t stat_wait, stat_from;

static void stat_frame(void) {
    uint64_t now = cmm2->micros();
    if(stats < 0) {
        stats = 0;
        for(int i = 1; i < cmm2_start->argc; i++) if(strcmp(cmm2_start->argv[i], "-stats") == 0) stats = 1;
        stat_from = now;
    }
    if(!stats) return;
    stat_frames++;
    if(now - stat_from >= 5000000) {
        uint32_t ms = (uint32_t)((now - stat_from) / 1000), wait = (uint32_t)(stat_wait / 1000);
        printf("nes: %lu.%lu frames a second, %lu%% waiting\n", (unsigned long)(stat_frames * 1000 / ms),
               (unsigned long)(stat_frames * 10000 / ms % 10), (unsigned long)(wait * 100 / ms));
        stat_frames = 0;
        stat_wait = 0;
        stat_from = now;
    }
}

void i2s_audio_push_samples(const int16_t *buf, int count) {
    audio_begin();
    if(count > 100) stat_frame();               /* (a frame's samples, not the selector's silence) */
    if(audio_on < 0) {
        uint64_t from = cmm2->micros();
        pace(count);
        stat_wait += cmm2->micros() - from;
        return;
    }
    uint64_t from = cmm2->micros();
    while(ring_in - ring_out > RING_FULL) {     /* (the emulator's pace) */
        usleep(500);
    }
    stat_wait += cmm2->micros() - from;
    for(int i = 0; i < count; i++) ring[(ring_in + i) & (RING - 1)] = buf[i];
    ring_in += count;
}

void i2s_audio_fill_silence(int count) {
    static const int16_t zero[256];
    while(count > 0) {
        int n = count > 256 ? 256 : count;
        i2s_audio_push_samples(zero, n);
        count -= n;
    }
}

void i2s_audio_init(unsigned int data_pin, unsigned int clock_pin_base, uint32_t sample_rate) {
    (void)data_pin; (void)clock_pin_base;
    if(sample_rate) audio_rate = sample_rate;
    audio_begin();
}
void i2s_audio_set_frame_rate(int frame_rate) { (void)frame_rate; }
void i2s_audio_shutdown(void) { }

void pwm_audio_init(uint pin0, uint pin1, uint32_t sample_rate) { i2s_audio_init(pin0, pin1, sample_rate); }
void pwm_audio_push_samples(const int16_t *buf, int count) { i2s_audio_push_samples(buf, count); }
void pwm_audio_fill_silence(int count) { i2s_audio_fill_silence(count); }
void pwm_audio_set_frame_rate(int frame_rate) { (void)frame_rate; }

/* ---- the USB keyboard: frank-nes's hid_to_kbd_state_bit, on the report as keys_raw gives it ---- */
static uint8_t report[8];

static uint16_t hid_to_kbd_state_bit(uint8_t keycode) {
    switch(keycode) {
        case 0x52: return KBD_STATE_UP;
        case 0x51: return KBD_STATE_DOWN;
        case 0x50: return KBD_STATE_LEFT;
        case 0x4F: return KBD_STATE_RIGHT;
        case 0x1A: return KBD_STATE_UP;         /* W */
        case 0x16: return KBD_STATE_DOWN;       /* S */
        case 0x04: return KBD_STATE_LEFT;       /* A */
        case 0x07: return KBD_STATE_RIGHT;      /* D */
        case 0x1B: return KBD_STATE_A;          /* X */
        case 0x1D: return KBD_STATE_B;          /* Z */
        case 0x28: return KBD_STATE_START;      /* Enter */
        case 0x58: return KBD_STATE_START;      /* the keypad's */
        case 0x2C: return KBD_STATE_SELECT;     /* Space */
        case 0x29: return KBD_STATE_ESC;
        case 0x3C: return KBD_STATE_F3;
        case 0x44: return KBD_STATE_F11;
        case 0x45: return KBD_STATE_F12;
        case 0x4B: return KBD_STATE_PGUP;
        case 0x4E: return KBD_STATE_PGDN;
        case 0x4A: return KBD_STATE_HOME;
        case 0x4D: return KBD_STATE_END;
    }
    return 0;
}

/* ---- the serial console's keys: a key held for a moment, and its character for the selector's search ---- */
#define TYPED_US 120000
static uint16_t typed_state;
static uint64_t typed_until;
static char typed_chars[32];
static int typed_in, typed_out;

static void console_keys(void) {
    static int quiet;
    int c, usb = report[2] != 0, ok = !usb && quiet;
    quiet = !usb;
    while((c = cmm2->con_getc()) >= 0) {
        uint16_t bit = 0;
        typed_chars[typed_in++ & 31] = c;
        if(!ok) continue;
        switch(c) {
            case 'w': bit = KBD_STATE_UP; break;
            case 's': bit = KBD_STATE_DOWN; break;
            case 'a': bit = KBD_STATE_LEFT; break;
            case 'd': bit = KBD_STATE_RIGHT; break;
            case 'x': bit = KBD_STATE_A; break;
            case 'z': bit = KBD_STATE_B; break;
            case 13: bit = KBD_STATE_START; break;
            case ' ': bit = KBD_STATE_SELECT; break;
            case 27: bit = KBD_STATE_ESC; break;
            case 'q': bit = KBD_STATE_F11; break;
            case 'e': bit = KBD_STATE_F12; break;
        }
        if(bit) {
            typed_state = bit;
            typed_until = cmm2->micros() + TYPED_US;
        }
    }
    if(typed_state && cmm2->micros() >= typed_until) typed_state = 0;
}

/* ---- the USB gamepads: the first two in the firmware's HID slots ---- */
#define SLOTS 8
#define STICK 64                                /* how far from the centre a stick is the d-pad */
static usbhid_gamepad_state_t pad[2];

/* DEVICE(GAMEPAD n, B)'s bitmap, to frank-nes's d-pad and buttons */
static void pad_state(usbhid_gamepad_state_t *g, int b, int lx, int ly) {
    g->connected = 1;
    g->axis_x = lx - 128 > 127 ? 127 : lx - 128;
    g->axis_y = ly - 128 > 127 ? 127 : ly - 128;
    g->dpad = (b >> 7 & 1) | (b >> 5 & 1) << 1 | (b >> 8 & 1) << 2 | (b >> 6 & 1) << 3;
    if(lx < 128 - STICK) g->dpad |= 4;
    if(lx > 128 + STICK) g->dpad |= 8;
    if(ly < 128 - STICK) g->dpad |= 1;
    if(ly > 128 + STICK) g->dpad |= 2;
    g->buttons = (b >> 11 & 1)                  /* a/circle */
               | (b >> 13 & 1) << 1             /* b/cross */
               | (b >> 10 & 1) << 2             /* x/triangle */
               | (b >> 12 & 1) << 3             /* y/square */
               | (b >> 4 & 1) << 4              /* L */
               | (b & 1) << 5                   /* R */
               | (b >> 1 & 1) << 6              /* start */
               | (b >> 3 & 1) << 7;             /* select */
}

/* -stats: what the game is given by a pad, when its buttons change or its stick moves */
static void stat_pad(int slot, int b, int lx, int ly) {
    static int last[SLOTS + 1][3];
    int *w = last[slot];
    if(stats <= 0 || (b == w[0] && abs(lx - w[1]) < 8 && abs(ly - w[2]) < 8)) return;
    w[0] = b;
    w[1] = lx;
    w[2] = ly;
    printf("pad %d: buttons %04X, stick %d,%d\n", slot, b, lx, ly);
}

/* Ctrl+Alt+End (the keypad's too) leaves, as it does pc386 */
#define HID_END 0x4D
#define HID_KP1 0x59
static void ctrl_alt_end(void) {
    if(!(report[0] & 0x11) || !(report[0] & 0x44)) return;
    for(int i = 2; i < 8; i++) if(report[i] == HID_END || report[i] == HID_KP1) exit(0);
}

void usbhid_init(void) { }

void usbhid_task(void) {
    int n = 0, lx, ly, b;
    frame();                                    /* (the frame just made: the PicoMite has no frame hook) */
    poll_quit();
    cmm2->keys_raw(report);
    ctrl_alt_end();
    console_keys();
    memset(pad, 0, sizeof(pad));
    for(int slot = 1; slot <= SLOTS && n < 2; slot++) {
        lx = ly = 128;
        if((b = cmm2->gamepad(slot, &lx, &ly)) >= 0) {
            stat_pad(slot, b, lx, ly);
            pad_state(&pad[n++], b, lx, ly);
        }
    }
}

uint16_t usbhid_get_kbd_state(void) {
    uint16_t state = typed_state;
    for(int i = 2; i < 8; i++) if(report[i]) state |= hid_to_kbd_state_bit(report[i]);
    return state;
}

int usbhid_ctrl_alt_del_pressed(void) {
    if(!(report[0] & 0x11) || !(report[0] & 0x44)) return 0;
    for(int i = 2; i < 8; i++) if(report[i] == 0x4C) return 1;
    return 0;
}

int usbhid_get_raw_char(void) { return typed_out == typed_in ? -1 : (uint8_t)typed_chars[typed_out++ & 31]; }
int usbhid_keyboard_connected(void) { return 1; }
int usbhid_mouse_connected(void) { return 0; }
int usbhid_gamepad_connected(void) { return pad[0].connected; }
void usbhid_get_gamepad_state(usbhid_gamepad_state_t *state) { *state = pad[0]; }
int usbhid_gamepad_connected_idx(int idx) { return idx >= 0 && idx < 2 && pad[idx].connected; }

void usbhid_get_gamepad_state_idx(int idx, usbhid_gamepad_state_t *state) {
    if(idx >= 0 && idx < 2) *state = pad[idx];
    else memset(state, 0, sizeof(*state));
}

/* the firmware knows its pads' layouts, so frank-nes's menu A and B wizard has nothing to learn */
int usbhid_gamepad_needs_calibration(int idx) { (void)idx; return 0; }
void usbhid_gamepad_clear_calibration(int idx) { (void)idx; }
int usbhid_gamepad_get_raw_info(int idx, usbhid_gamepad_raw_info_t *out) { (void)idx; memset(out, 0, sizeof(*out)); return 0; }
int usbhid_gamepad_find_pressed_bit(int idx, uint8_t *byte_idx, uint8_t *mask) { (void)idx; (void)byte_idx; (void)mask; return 0; }
void usbhid_gamepad_set_menu_ab(int idx, uint8_t a_byte, uint8_t a_mask, uint8_t b_byte, uint8_t b_mask) {
    (void)idx; (void)a_byte; (void)a_mask; (void)b_byte; (void)b_mask;
}
int usbhid_learned_ab_add(usbhid_gp_src_t source, uint16_t vid, uint16_t pid, uint8_t a_byte, uint8_t a_mask, uint8_t b_byte,
                          uint8_t b_mask) {
    (void)source; (void)vid; (void)pid; (void)a_byte; (void)a_mask; (void)b_byte; (void)b_mask;
    return 0;
}
void gamepad_cal_load_all(void) { }
int gamepad_cal_save(const char *source, uint16_t vid, uint16_t pid, uint8_t a_byte, uint8_t a_mask, uint8_t b_byte,
                     uint8_t b_mask) {
    (void)source; (void)vid; (void)pid; (void)a_byte; (void)a_mask; (void)b_byte; (void)b_mask;
    return 0;
}
int gamepad_cal_clear(void) { return 0; }

/* ---- the drivers with nothing to drive ---- */
uint32_t nespad_state, nespad_state2;
bool nespad_begin(uint32_t cpu_khz, uint8_t clkPin, uint8_t dataPin, uint8_t data2Pin, uint8_t latPin) {
    (void)cpu_khz; (void)clkPin; (void)dataPin; (void)data2Pin; (void)latPin;
    return false;
}
void nespad_read(void) { }
void nespad_read_start(void) { }
void nespad_read_finish(void) { }

void ps2kbd_init(void) { }
void ps2kbd_tick(void) { }
uint16_t ps2kbd_get_state(void) { return 0; }
int ps2kbd_get_raw_char(void) { return -1; }
int ps2kbd_ctrl_alt_del_pressed(void) { return 0; }
void uart_logging_register(void) { }
