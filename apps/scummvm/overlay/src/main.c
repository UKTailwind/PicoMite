/*
 * FRANK Quest - ScummVM port for RP2350 with 8MB QSPI PSRAM
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-quest
 *
 * Derived from Cabal (https://github.com/project-cabal/cabal).
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Main entry point with hardware initialization.
 *
 * PicoMite (the PC3, a loadable program: RUN "scummvm.app"), from the Colour Maximite 2's port: the RP2350's clocks,
 * PSRAM and HDMI set-up are replaced by the loader's services. The "PSRAM" is the loader's heap, less what newlib keeps;
 * the HDMI driver's calls (graphics_*) are made here on MODE 5 (320x240 in MAP's 256 colours, the frame frank-quest
 * builds: 320x200 letterboxed); the sound's lock is the loader's (cmm2_port.h). The double buffer is frank-quest's.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board_config.h"
#include "psram_allocator.h"
#include "audio.h"
#include "cmm2_port.h"

// Version string
#ifndef CABAL_VERSION
#define CABAL_VERSION "0.1.0"
#endif

// Forward declarations
void cabal_init(void);
int cabal_main(void);
void cabal_enable_psram_heap(void);
void graphics_set_buffer(uint8_t *buffer);

// Double-buffered framebuffer
// Note: Framebuffers are allocated from PSRAM at runtime since RP2350
// scratch sections are only 4KB each (too small for 76KB framebuffers)
static uint8_t *framebuffer_0 = NULL;
static uint8_t *framebuffer_1 = NULL;
static uint8_t *current_framebuffer = NULL;
static int current_buffer_index = 0;

uint8_t *cabal_get_framebuffer(void) {
    return current_framebuffer;
}

// Get the back buffer (not currently being displayed) for drawing
uint8_t *cabal_get_back_buffer(void) {
    // Return the buffer that's NOT currently being displayed
    return current_buffer_index ? framebuffer_0 : framebuffer_1;
}

void cabal_swap_buffers(void) {
    current_buffer_index = 1 - current_buffer_index;
    current_framebuffer = current_buffer_index ? framebuffer_1 : framebuffer_0;
    graphics_set_buffer(current_framebuffer);
}

/* ---- CMM2: the display (the HDMI driver's calls) ----------------------------------------------------------------- */

#define CMM2_NEWLIB_HEAP (512u * 1024u)        // the heap newlib keeps (its own buffers, before the "PSRAM" is ready)

static cmm2_video v;
static int res_w = 320, res_h = 240;
static uint8_t pal[256 * 3];
static int pal_dirty;

void graphics_set_res(int w, int h) {
    res_w = w;
    res_h = h;
}

void graphics_init(int mode) {
    (void)mode;
    // PicoMite: MODE 5 is 320x240 in 256 colours at OPTION RESOLUTION 640x480
    if (cmm2->video_mode(5, 8, &v) < 0 || (int)v.width != res_w || (int)v.height != res_h) {
        fprintf(stderr, "ScummVM: MODE 5 is not %dx%d here (it needs OPTION RESOLUTION 640x480)\n", res_w, res_h);
        exit(1);
    }
}

void graphics_set_palette(uint8_t i, uint32_t color888) {
    pal[i * 3] = color888 >> 16;
    pal[i * 3 + 1] = color888 >> 8;
    pal[i * 3 + 2] = color888;
    pal_dirty = 1;
}

// the frame shown: the palette (at the frame's start, which video_palette waits for) and the 320x240 buffer copied to
// the page, each line twice (MODE 7 shows each line twice)
void graphics_set_buffer(uint8_t *buffer) {
    if (!v.fb || !buffer) return;
    if (pal_dirty) {
        pal_dirty = 0;
        cmm2->video_palette(0, 256, pal);
    }
    const uint8_t *s = buffer;
    uint8_t *d = v.fb;
    for (int y = 0; y < res_h; y++, s += res_w, d += v.pitch) {
        memcpy(d, s, res_w);
        if (v.copies == 2) memcpy(d + v.width, s, res_w);
    }
}

/* ---- CMM2: the sound (the I2S driver's calls: stub/audio.h) ------------------------------------------------------ */

static void (*mixer_cb)(uint8_t *stream, int len);
static unsigned stream_rate;

static void stream_fill(int16_t *buf, uint32_t frames) {   // (the loader's refill, PendSV)
    if (mixer_cb) mixer_cb((uint8_t *)buf, (int)(frames * 4));
    else memset(buf, 0, frames * 4);
}

unsigned cabal_audio_rate(void) {
    if (!stream_rate) {
        int r = cmm2->audio_start(stream_fill);
        stream_rate = r > 0 ? (unsigned)r : 44100;       // (no sound: the mixer still runs, unheard)
        printf("Audio: %u Hz%s\n", stream_rate, r > 0 ? "" : " (the stream could not start)");
    }
    return stream_rate;
}

bool cabal_audio_init(void) {
    return cabal_audio_rate() != 0;
}

void cabal_audio_shutdown(void) {
    mixer_cb = NULL;
    cmm2->audio_stop();
    stream_rate = 0;
}

void cabal_audio_process_frame(void) {
}

void cabal_audio_set_mixer_callback(void (*callback)(uint8_t *stream, int len)) {
    cmm2_sound_lock();
    mixer_cb = callback;
    cmm2_sound_unlock();
}

/* ---- CMM2: the sound's lock (cmm2_port.h) ------------------------------------------------------------------------ */

static volatile int lock_depth;

void cmm2_sound_lock(void) {
    if (lock_depth++ == 0) cmm2->audio_lock();
}

void cmm2_sound_unlock(void) {
    if (--lock_depth == 0) cmm2->audio_unlock();
}

int main(void) {
    // CMM2: the program's text to the serial console only (it would be drawn over the game on the screen)
    cmm2->con_screen(0);

    printf("\n\n");
    printf("================================\n");
    printf("  Frank-Cabal v%s\n", CABAL_VERSION);
    printf("  ScummVM for the CMM2\n");
    printf("================================\n");
    printf("\n");
    printf("CPU: %lu MHz\n", (unsigned long)(cmm2->cpu_hz() / 1000000));
    printf("\n");

    // CMM2: the "PSRAM" heap is the loader's, less newlib's
    printf("Initializing the heap...\n");
    uint32_t total = cmm2_start->heap_size;
    if (total < CMM2_NEWLIB_HEAP + 2u * 1024u * 1024u) {
        fprintf(stderr, "ScummVM: only %lu KB of heap\n", (unsigned long)(total / 1024));
        return 1;
    }
    uint32_t size = total - CMM2_NEWLIB_HEAP;
    void *base = cmm2_heap_take(size);
    if (!base) {
        fprintf(stderr, "ScummVM: no heap\n");
        return 1;
    }
    cmm2_psram_region(base, size);
    psram_set_sram_mode(0); // Use PSRAM for allocations
    cabal_enable_psram_heap(); // Enable PSRAM for malloc/new heap
    printf("  Heap: %lu KB at %p\n", (unsigned long)(size / 1024), base);

    // Use the persistent HDMI framebuffer slots — two 96 kB blocks
    // back-to-back. Both survive psram_reset(); HDMI keeps scanning
    // whichever is currently the front buffer while the engine
    // writes to the back, so a full-screen update never tears and
    // a cursor draw-on-top can't be caught mid-write by the scanout.
    printf("Initializing double-buffered framebuffer...\n");
    framebuffer_0 = (uint8_t *)psram_get_framebuffer();
    framebuffer_1 = (uint8_t *)psram_get_framebuffer_back();
    memset(framebuffer_0, 0, CABAL_FRAMEBUFFER_SIZE);
    memset(framebuffer_1, 0, CABAL_FRAMEBUFFER_SIZE);
    current_framebuffer = framebuffer_0;
    current_buffer_index = 0;
    printf("  FB0: %p  FB1: %p  (%d bytes each)\n",
           framebuffer_0, framebuffer_1, CABAL_FRAMEBUFFER_SIZE);

    // Initialize the display (PicoMite: MODE 5)
    printf("Initializing the display...\n");
    graphics_set_res(CABAL_SCREEN_WIDTH, CABAL_HDMI_HEIGHT);
    graphics_init(0);
    printf("  Resolution: %dx%d\n", CABAL_SCREEN_WIDTH, CABAL_HDMI_HEIGHT);

    // Set initial palette to grayscale
    for (int i = 0; i < 256; i++) {
        uint32_t gray = (i << 16) | (i << 8) | i;
        graphics_set_palette(i, gray);
    }
    graphics_set_buffer(current_framebuffer);

    printf("\nCabal initialized successfully!\n");
    printf("Game directory: /quest/\n\n");

    // Initialize Cabal engine
    cabal_init();

    // Run main loop
    return cabal_main();
}

// cabal_init() and cabal_main() are implemented in cabal_main.cpp
