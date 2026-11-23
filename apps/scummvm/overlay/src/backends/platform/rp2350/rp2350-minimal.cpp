/*
 * FRANK Quest
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-quest
 *
 * Derived from Cabal (https://github.com/project-cabal/cabal).
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Minimal RP2350 backend implementation.
 *
 * CMM2: the graphics, the overlay and the cursor are frank-quest's, unchanged. The input is the loader's: the keys held
 * (KEYDOWN's set, read at each poll and its changes made key events, with ScummVM's key codes) and the mouse (MM.MOUSE's,
 * whose position the firmware keeps in the screen's pixels: 320x240, the game's 320x200 20 lines down). Ctrl+Alt+Del
 * still goes back to the game list; Ctrl-C ends the program (the loader puts BASIC's screen back). The PS/2, USB HID and
 * crash-record code of the RP2350 is gone (the CMM2's firmware reports a fault itself). Time is the loader's.
 */

#include "backends/platform/rp2350/rp2350-minimal.h"
#include "board_config.h"
#include "pico/time.h"
#include "cmm2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// CMM2: the last mouse position reported (a move is an event)
static int s_lastX = -1, s_lastY = -1;

//============================================================================
// Internal State
//============================================================================

// SCUMM v6 (Sam & Max / DOTT) ships cursors up to ~80x80; cap at 80
// to keep the static buffer bounded while accommodating every cursor
// actually used by the supported engine set.
#define CABAL_MAX_CURSOR_DIM 80

static struct {
    // Graphics
    CabalSurface screen;
    CabalSurface overlay;
    uint8_t *framebuffer;
    uint8_t palette[256 * 3];
    bool paletteDirty;
    bool overlayVisible;
    int screenWidth;
    int screenHeight;

    // Cursor
    bool cursorVisible;
    int cursorX, cursorY;
    int cursorHotX, cursorHotY;
    int cursorW, cursorH;
    uint8_t cursorKeyColor;
    uint8_t *cursorData;

    // Input
    int16_t mouseX, mouseY;
    uint8_t mouseButtons;
    uint8_t prevMouseButtons;

    // Timing
    uint32_t startTime;

    // Initialized flag
    bool initialized;
} g_state;

//============================================================================
// Initialization
//============================================================================

void cabal_system_init(void) {
    if (g_state.initialized) return;

    printf("Cabal Minimal System: Initializing...\n");

    memset(&g_state, 0, sizeof(g_state));

    // Record start time
    g_state.startTime = time_us_32();

    // CMM2: the keyboard and the mouse are the firmware's (the loader's keys_held and mouse): nothing to set up

    // Default mouse position
    g_state.mouseX = 160;
    g_state.mouseY = 100;
    s_lastX = s_lastY = -1;

    g_state.initialized = true;
    printf("Cabal Minimal System: Ready.\n");
}

//============================================================================
// Graphics
//============================================================================

// External double-buffer functions from main.c
extern "C" {
    uint8_t *cabal_get_framebuffer(void);
    uint8_t *cabal_get_back_buffer(void);
    void cabal_swap_buffers(void);
}

void cabal_init_graphics(int width, int height) {
    printf("Cabal Graphics: initSize(%d, %d)\n", width, height);

    g_state.screenWidth = width;
    g_state.screenHeight = height;

    // Screen and overlay surfaces live in the PSRAM mspace. On a
    // return-to-selector teardown psram_reset() wipes the mspace, so
    // any surviving g_state.screen.pixels / overlay.pixels pointers
    // are dangling. Null them out before reallocating so we never
    // hand an engine back a pointer into freed storage.
    g_state.screen.pixels = (uint8_t *)psram_malloc(width * height);
    if (g_state.screen.pixels) {
        memset(g_state.screen.pixels, 0, width * height);
    }
    g_state.screen.width = width;
    g_state.screen.height = height;
    g_state.screen.pitch = width;
    g_state.screen.bytesPerPixel = 1;

    g_state.overlay.pixels = (uint8_t *)psram_malloc(width * height);
    if (g_state.overlay.pixels) {
        memset(g_state.overlay.pixels, 0, width * height);
    }
    g_state.overlay.width = width;
    g_state.overlay.height = height;
    g_state.overlay.pitch = width;
    g_state.overlay.bytesPerPixel = 1;

    // Use double-buffered framebuffer from main.c (already allocated)
    // Get the back buffer for initial drawing
    g_state.framebuffer = cabal_get_back_buffer();
    printf("Cabal Graphics: Using double-buffered framebuffer at %p\n", g_state.framebuffer);
}

CabalSurface *cabal_lock_screen(void) {
    return &g_state.screen;
}

void cabal_unlock_screen(void) {
    // Nothing to do
}

static void update_hardware_palette(void) {
    if (!g_state.paletteDirty) return;

    for (int i = 0; i < 256; i++) {
        uint32_t color = ((uint32_t)g_state.palette[i * 3] << 16) |
                         ((uint32_t)g_state.palette[i * 3 + 1] << 8) |
                         (uint32_t)g_state.palette[i * 3 + 2];
        graphics_set_palette(i, color);
    }
    g_state.paletteDirty = false;
}

static void draw_cursor(void) {
    if (!g_state.cursorData || !g_state.framebuffer) return;
    if (!g_state.cursorVisible) return;

    // Validate cursor dimensions to prevent buffer overflows
    int cursorW = g_state.cursorW;
    int cursorH = g_state.cursorH;
    if (cursorW <= 0 || cursorW > CABAL_MAX_CURSOR_DIM ||
        cursorH <= 0 || cursorH > CABAL_MAX_CURSOR_DIM) return;

    // Calculate vertical offset for letterboxing (320x200 in 320x240)
    int yOffset = (240 - g_state.screenHeight) / 2;
    int drawX = g_state.cursorX - g_state.cursorHotX;
    int drawY = g_state.cursorY - g_state.cursorHotY + yOffset;

    for (int cy = 0; cy < cursorH; cy++) {
        int screenY = drawY + cy;
        if (screenY < 0 || screenY >= 240) continue;

        for (int cx = 0; cx < cursorW; cx++) {
            int screenX = drawX + cx;
            if (screenX < 0 || screenX >= 320) continue;

            uint8_t pixel = g_state.cursorData[cy * cursorW + cx];
            if (pixel != g_state.cursorKeyColor) {
                g_state.framebuffer[screenY * 320 + screenX] = pixel;
            }
        }
    }
}

void cabal_update_screen(void) {
    static uint32_t total_update_time = 0;
    static int update_count = 0;
    uint32_t start = time_us_32();

    update_hardware_palette();

    // Get the back buffer (not being displayed) for drawing
    g_state.framebuffer = cabal_get_back_buffer();
    if (!g_state.framebuffer) return;

    // Calculate vertical centering offset (320x200 in 320x240)
    int yOffset = (240 - g_state.screenHeight) / 2;

    // Clear top border
    if (yOffset > 0) {
        memset(g_state.framebuffer, 0, 320 * yOffset);
    }

    // Select source surface
    CabalSurface *src = g_state.overlayVisible ? &g_state.overlay : &g_state.screen;
    if (!src->pixels) return;

    // Copy to framebuffer
    uint8_t *dst = g_state.framebuffer + yOffset * 320;
    const uint8_t *srcPtr = src->pixels;
    int copyHeight = g_state.screenHeight;
    if (yOffset + copyHeight > 240) {
        copyHeight = 240 - yOffset;
    }

    for (int row = 0; row < copyHeight; row++) {
        int copyWidth = (src->width < 320) ? src->width : 320;
        memcpy(dst, srcPtr, copyWidth);
        srcPtr += src->pitch;
        dst += 320;
    }

    // Clear bottom border
    int bottomY = yOffset + g_state.screenHeight;
    if (bottomY < 240) {
        memset(g_state.framebuffer + bottomY * 320, 0, 320 * (240 - bottomY));
    }

    // Draw cursor (now on back buffer, not displayed yet)
    if (g_state.cursorVisible && !g_state.overlayVisible) {
        draw_cursor();
    }

    // Swap buffers - display what we just drew, get new back buffer for next frame
    cabal_swap_buffers();

    // Timing stats
    uint32_t elapsed = time_us_32() - start;
    total_update_time += elapsed;
    update_count++;
    if (update_count == 600) {
        total_update_time = 0;
        update_count = 0;
    }
}

void cabal_set_palette(const uint8_t *colors, int start, int num) {
    memcpy(g_state.palette + start * 3, colors, num * 3);
    g_state.paletteDirty = true;
}

void cabal_get_palette(uint8_t *colors, int start, int num) {
    memcpy(colors, g_state.palette + start * 3, num * 3);
}

void cabal_fill_screen(uint8_t color) {
    if (g_state.screen.pixels) {
        memset(g_state.screen.pixels, color,
               g_state.screen.width * g_state.screen.height);
    }
}

int cabal_screen_width(void) {
    return g_state.screenWidth;
}

int cabal_screen_height(void) {
    return g_state.screenHeight;
}

//============================================================================
// Overlay
//============================================================================

void cabal_show_overlay(void) {
    g_state.overlayVisible = true;
}

void cabal_hide_overlay(void) {
    g_state.overlayVisible = false;
}

bool cabal_overlay_visible(void) {
    return g_state.overlayVisible;
}

CabalSurface *cabal_get_overlay(void) {
    return &g_state.overlay;
}

//============================================================================
// Cursor
//============================================================================

void cabal_show_mouse(bool visible) {
    g_state.cursorVisible = visible;
}

void cabal_set_mouse_pos(int x, int y) {
    g_state.cursorX = x;
    g_state.cursorY = y;
}

// Static cursor buffer to avoid repeated allocations; sized for the
// largest cursor any supported engine uses (see CABAL_MAX_CURSOR_DIM
// at the top of this file).
static uint8_t s_cursorBuffer[CABAL_MAX_CURSOR_DIM * CABAL_MAX_CURSOR_DIM];

void cabal_set_mouse_cursor(const uint8_t *data, int w, int h,
                            int hotX, int hotY, uint8_t keycolor) {
    const int srcW = w;
    const int srcH = h;
    const int dstW = (w > CABAL_MAX_CURSOR_DIM) ? CABAL_MAX_CURSOR_DIM : w;
    const int dstH = (h > CABAL_MAX_CURSOR_DIM) ? CABAL_MAX_CURSOR_DIM : h;

    g_state.cursorW = dstW;
    g_state.cursorH = dstH;
    g_state.cursorHotX = hotX;
    g_state.cursorHotY = hotY;
    g_state.cursorKeyColor = keycolor;

    // Use static buffer instead of allocating each time. Respect the
    // source stride when truncating so we don't smear pixels from row
    // N+1 into the end of row N.
    g_state.cursorData = s_cursorBuffer;
    for (int y = 0; y < dstH; y++) {
        memcpy(g_state.cursorData + y * dstW, data + y * srcW, dstW);
    }
    (void)srcH;
}


//============================================================================
// Events
//============================================================================

// ---- Ctrl+Alt+Del → cooperative quit -----------------------------------
//
// Detected at the lowest event layer so it works even if the active
// engine doesn't forward key events. On a match we set a pending
// flag; the OSystem pollEvent translator consumes it and returns a
// Common::EVENT_QUIT so ScummVM engines exit run() cleanly and the
// selector loop in cabal_main() gets control back. No hardware reboot.
// (CMM2: Ctrl and Alt are the modifier bits of the keys held.)

namespace {
    volatile int g_fq_quit_pending = 0;
}

// Public hooks used by OSystem_RP2350::pollEvent to translate the
// pending flag into a Common::EVENT_QUIT for the engine.
extern "C" int frank_quest_cad_consume(void) {
    if (g_fq_quit_pending) {
        g_fq_quit_pending = 0;
        return 1;
    }
    return 0;
}

// Profiling globals - accessible from engine.
// gob/util.cpp and gob/game.cpp reference cabal_profile_get_*
// unconditionally for input-latency tracing.
static uint32_t g_last_click_time = 0;
static uint32_t g_click_count = 0;

extern "C" {
    uint32_t cabal_profile_get_last_click_time(void) { return g_last_click_time; }
    uint32_t cabal_profile_get_click_count(void) { return g_click_count; }
}

// ---- CMM2: the keys held turned into key events ---------------------------
//
// keys_held gives up to six of MMBasic's key codes (ASCII as typed, &H80-&H89
// the arrows and editing keys, &H91-&H9C F1-F12) and the modifier bits
// (1/16 Alt, 2/32 Ctrl, 8/128 Shift, left/right). A code new in the set is a
// key down, one gone a key up (with the key code it went down with); a change
// of a modifier is its own key down or up, as SDL gives them.

#define CMM2_KQ 32
static CabalEvent s_kq[CMM2_KQ];
static int s_kqIn, s_kqOut;
static int s_held[6], s_heldCode[6], s_heldAscii[6], s_mods;

static void kq_put(CabalEventType type, int keycode, int ascii) {
    if ((s_kqIn + 1) % CMM2_KQ == s_kqOut) return;
    CabalEvent *e = &s_kq[s_kqIn];
    e->type = type;
    e->kbd.keycode = keycode;
    e->kbd.ascii = ascii;
    e->kbd.flags = ((s_mods & 0x88) ? CABAL_MOD_SHIFT : 0) | ((s_mods & 0x22) ? CABAL_MOD_CTRL : 0) |
                   ((s_mods & 0x11) ? CABAL_MOD_ALT : 0);
    s_kqIn = (s_kqIn + 1) % CMM2_KQ;
}

// an MMBasic key code to ScummVM's (Common::KeyCode: SDL's), and the
// character it types
static int cmm2_keycode(int c, int mods, int *ascii) {
    static const char shifted[] = "!1\"2@2#3$4%5^6&7*8(9)0_-+=<,>.?/:;{[}]|\\~`";
    *ascii = 0;
    if (c >= 1 && c <= 26 && (mods & 0x22) && c != 8 && c != 9 && c != 13) {   // a letter with Ctrl
        *ascii = c;
        return 'a' + c - 1;
    }
    switch (c) {
    case 0x80: return CABAL_KEY_UP;
    case 0x81: return CABAL_KEY_DOWN;
    case 0x82: return CABAL_KEY_LEFT;
    case 0x83: return CABAL_KEY_RIGHT;
    case 0x84: return 277;                              // INSERT
    case 0x86: return 278;                              // HOME
    case 0x87: return 279;                              // END
    case 0x88: return 280;                              // PAGEUP
    case 0x89: return 281;                              // PAGEDOWN
    case 0x7F: return CABAL_KEY_DELETE;
    case 0x0A: case 0x0D: *ascii = 13; return CABAL_KEY_RETURN;
    case 0x08: *ascii = 8; return CABAL_KEY_BACKSPACE;
    case 0x09: *ascii = 9; return CABAL_KEY_TAB;
    case 0x1B: *ascii = 27; return CABAL_KEY_ESCAPE;
    }
    if (c >= 0x91 && c <= 0x9C) {                       // (ScummVM's ASCII_F1 up: SCUMM drops a key with no ascii)
        *ascii = 315 + c - 0x91;
        return CABAL_KEY_F1 + c - 0x91;
    }
    if (c < 32 || c > 126) return 0;
    *ascii = c;
    if (c >= 'A' && c <= 'Z') return c + 32;
    for (const char *p = shifted; *p; p += 2)
        if (*p == c) return p[1];
    return c;
}

static void cmm2_keys(void) {
    int k[7], i, j, m, ascii;
    if (cmm2->poll()) {                                 // Ctrl-C: the program ends (the BASIC program stops too)
        printf("\nScummVM: Ctrl-C\n");
        exit(0);
    }
    cmm2->keys_held(k);
    m = k[6];
    for (i = 0; i < 6; i++) {                           // released: no longer in the set
        if (!s_held[i]) continue;
        for (j = 0; j < 6 && k[j] != s_held[i]; j++) {}
        if (j == 6) {
            if (s_heldCode[i]) kq_put(CABAL_EVENT_KEYUP, s_heldCode[i], s_heldAscii[i]);
            s_held[i] = 0;
        }
    }
    static const struct { int bits, code; } modkeys[] = {
        { 0x02, 306 }, { 0x20, 305 }, { 0x08, 304 }, { 0x80, 303 }, { 0x01, 308 }, { 0x10, 307 } };
    for (i = 0; i < 6; i++) {                           // the modifiers: LCTRL RCTRL LSHIFT RSHIFT LALT RALT
        if ((m ^ s_mods) & modkeys[i].bits) {
            s_mods = (s_mods & ~modkeys[i].bits) | (m & modkeys[i].bits);
            kq_put((m & modkeys[i].bits) ? CABAL_EVENT_KEYDOWN : CABAL_EVENT_KEYUP, modkeys[i].code, 0);
        }
    }
    for (j = 0; j < 6; j++) {                           // pressed: new in the set
        if (!k[j]) continue;
        for (i = 0; i < 6 && s_held[i] != k[j]; i++) {}
        if (i < 6) continue;
        for (i = 0; i < 6 && s_held[i]; i++) {}
        if (i == 6) break;
        s_held[i] = k[j];
        s_heldCode[i] = cmm2_keycode(k[j], m, &ascii);
        s_heldAscii[i] = ascii;
        if (s_heldCode[i] == CABAL_KEY_DELETE && (m & 0x22) && (m & 0x11)) {
            printf("\nFRANK Quest: Ctrl+Alt+Del — requesting engine quit\n");
            g_fq_quit_pending = 1;
        }
        if (s_heldCode[i]) kq_put(CABAL_EVENT_KEYDOWN, s_heldCode[i], ascii);
    }
}

// the key events made at the last poll dropped, and nothing taken as held:
// keys still down come again as new (frank_quest_main: after a game)
extern "C" void ps2kbd_flush(void) {
    s_kqIn = s_kqOut = 0;
    int k[7];
    cmm2->keys_held(k);
    for (int i = 0; i < 6; i++) {
        s_held[i] = k[i];                               // (their key ups are not wanted either)
        s_heldCode[i] = 0;
    }
    s_mods = k[6];
}

extern "C" void frank_quest_cad_clear(void) {
    g_fq_quit_pending = 0;
}

bool cabal_poll_event(CabalEvent *event) {
    if (s_kqIn == s_kqOut) cmm2_keys();
    if (s_kqIn != s_kqOut) {
        *event = s_kq[s_kqOut];
        s_kqOut = (s_kqOut + 1) % CMM2_KQ;
        return true;
    }

    // CMM2: the mouse, its position the firmware's (320x240), the game's
    // screen 20 lines down
    int m[4];
    if (cmm2->mouse(m)) {
        int yOffset = (240 - g_state.screenHeight) / 2;
        int x = m[0], y = m[1] - yOffset;
        uint8_t buttons = m[2] & 7;
        if (x >= g_state.screenWidth) x = g_state.screenWidth - 1;
        if (y < 0) y = 0;
        if (y >= g_state.screenHeight) y = g_state.screenHeight - 1;
        g_state.cursorX = g_state.mouseX = x;
        g_state.cursorY = g_state.mouseY = y;
        if (buttons != g_state.prevMouseButtons) {
            static const CabalEventType down[3] = { CABAL_EVENT_LBUTTONDOWN, CABAL_EVENT_RBUTTONDOWN, CABAL_EVENT_MBUTTONDOWN };
            static const CabalEventType up[3] = { CABAL_EVENT_LBUTTONUP, CABAL_EVENT_RBUTTONUP, CABAL_EVENT_MBUTTONUP };
            for (int b = 0; b < 3; b++) {
                if ((buttons ^ g_state.prevMouseButtons) & (1 << b)) {
                    event->type = (buttons & (1 << b)) ? down[b] : up[b];
                    event->mouse.x = x;
                    event->mouse.y = y;
                    g_state.prevMouseButtons ^= 1 << b;
                    if (b == 0 && (buttons & 1)) {
                        g_last_click_time = time_us_32();
                        g_click_count++;
                    }
                    return true;
                }
            }
        }
        if (x != s_lastX || y != s_lastY) {
            s_lastX = x;
            s_lastY = y;
            event->type = CABAL_EVENT_MOUSEMOVE;
            event->mouse.x = x;
            event->mouse.y = y;
            return true;
        }
    }

    event->type = CABAL_EVENT_NONE;
    return false;
}

//============================================================================
// Timing
//============================================================================

uint32_t cabal_get_millis(void) {
    uint32_t now = time_us_32();
    return (now - g_state.startTime) / 1000;
}

void cabal_delay(uint32_t ms) {
    // CMM2: a millisecond at a time, Ctrl-C looked for (the selector's idle loop only waits)
    while (ms--) {
        cmm2->delay_us(1000);
        if (cmm2->poll()) {
            printf("\nScummVM: Ctrl-C\n");
            exit(0);
        }
    }
}
