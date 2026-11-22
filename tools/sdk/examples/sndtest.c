/* sndtest.c: the sound services (tools/sdk). MIT License, as pmimg.h.

     RUN "sndtest.app"                a scale, a note each half second, left and right a fifth apart; the note changed
                                      under audio_lock; then audio_stop. Prints the rate, the fills a second and the
                                      longest time between two fills (23 ms each: more than 40 ms is a buffer played
                                      twice)
     RUN "sndtest.app", "load"        a tone while the program keeps the processor busy (sums, poll every 10 ms), 5 s
     RUN "sndtest.app", "files", "B:" a tone while it writes and reads back 1 MB on the drive (A: writes turn the
                                      interrupts off for each block: expect gaps there)
     RUN "sndtest.app", "twice"       audio_start twice (the second -2), audio_stop, and start again
     RUN "sndtest.app", "end"         starts a tone and returns without audio_stop: the loader ends it
     RUN "sndtest.app", "ctrlc"       a tone until Ctrl-C, which ends the program from poll()

   fill calls micros, which only reads the timer: a program's fill must call no service, this one breaks the rule to
   time itself. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "cmm2.h"

#define TABLE 1024
static int16_t sine[TABLE];
static volatile uint32_t step_l, step_r;      /* the phase steps, 16.16 a sample into the table */
static uint32_t phase_l, phase_r;
static volatile uint32_t fills, gap_max;
static volatile uint64_t last_fill;
static int rate;

static void fill(int16_t *buf, uint32_t frames) {
    uint64_t now = cmm2->micros();
    if(last_fill && now - last_fill > gap_max) gap_max = (uint32_t)(now - last_fill);
    last_fill = now;
    fills++;
    while(frames--) {
        *buf++ = sine[(phase_l >> 16) & (TABLE - 1)];
        *buf++ = sine[(phase_r >> 16) & (TABLE - 1)];
        phase_l += step_l;
        phase_r += step_r;
    }
}

static void note(float left, float right) {    /* both steps changed together */
    uint32_t l = (uint32_t)(left * TABLE * 65536.0f / rate), r = (uint32_t)(right * TABLE * 65536.0f / rate);
    cmm2->audio_lock();
    step_l = l;
    step_r = r;
    cmm2->audio_unlock();
}

static int start(float f) {
    rate = 44100;                              /* (for the first two fills, made before audio_start returns) */
    note(f, f * 1.5f);
    fills = gap_max = 0;
    last_fill = 0;
    rate = cmm2->audio_start(fill);
    if(rate < 0) {
        printf("audio_start: %d\n", rate);
        return 0;
    }
    note(f, f * 1.5f);                         /* (again at the rate it gave) */
    printf("audio_start: %d Hz\n", rate);
    return 1;
}

static void report(uint64_t from) {
    uint64_t us = cmm2->micros() - from;
    uint32_t n = fills, gap = gap_max;
    printf("%lu fills in %lu ms: %lu.%lu a second (%lu.%lu expected), longest gap %lu.%lu ms\n", (unsigned long)n,
           (unsigned long)(us / 1000), (unsigned long)(n * 1000000ull / us), (unsigned long)(n * 10000000ull / us % 10),
           (unsigned long)(rate / 1024), (unsigned long)(rate * 10 / 1024 % 10), (unsigned long)(gap / 1000),
           (unsigned long)(gap / 100 % 10));
}

static int wait_ms(uint32_t ms) {              /* 1 if Ctrl-C */
    uint64_t t = cmm2->micros() + ms * 1000ull;
    while(cmm2->micros() < t) {
        if(cmm2->poll()) return 1;
        cmm2->delay_us(1000);
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *what = argc > 1 ? argv[1] : "";
    static const float scale[8] = {261.63f, 293.66f, 329.63f, 349.23f, 392.00f, 440.00f, 493.88f, 523.25f};
    uint64_t from;
    int r;

    for(int i = 0; i < TABLE; i++) sine[i] = (int16_t)(12000.0f * sinf(i * 6.2831853f / TABLE));

    if(strcmp(what, "twice") == 0) {
        if(!start(440)) return 1;
        r = cmm2->audio_start(fill);
        printf("second audio_start: %d (-2 expected)\n", r);
        if(wait_ms(1000)) return 0;
        cmm2->audio_stop();
        printf("stopped: 1 s of silence\n");
        if(wait_ms(1000)) return 0;
        if(!start(660)) return 1;
        from = cmm2->micros();
        if(wait_ms(1000)) return 0;
        report(from);
        cmm2->audio_stop();
        return 0;
    }
    if(!start(scale[0])) return 1;
    from = cmm2->micros();
    if(strcmp(what, "end") == 0) {
        wait_ms(1000);
        printf("returning with the sound on\n");
        return 0;
    }
    if(strcmp(what, "ctrlc") == 0) {
        printf("Ctrl-C to end\n");
        for(int i = 0;; i = (i + 1) & 7) {
            note(scale[i], scale[i] * 1.5f);
            if(wait_ms(500)) {
                report(from);
                printf("Ctrl-C: ending from poll()\n");
                exit(0);
            }
        }
    }
    if(strcmp(what, "load") == 0) {
        volatile uint32_t sum = 0;
        uint64_t end = from + 5000000, next = from;
        while(cmm2->micros() < end) {
            for(int i = 0; i < 20000; i++) sum += i * i;
            if(cmm2->micros() >= next) {
                if(cmm2->poll()) break;
                next += 10000;
            }
        }
        report(from);
        cmm2->audio_stop();
        return 0;
    }
    if(strcmp(what, "files") == 0) {
        const char *drive = argc > 2 ? argv[2] : "B:";
        char name[64];
        uint32_t *block = malloc(16384);
        FILE *f;
        int bad = 0;
        if(!block) return 2;
        snprintf(name, sizeof(name), "%s/sndtest.bin", drive);
        if(!(f = fopen(name, "wb"))) { printf("cannot write %s\n", name); cmm2->audio_stop(); return 1; }
        for(int b = 0; b < 64; b++) {
            for(int i = 0; i < 4096; i++) block[i] = b * 4096 + i;
            fwrite(block, 1, 16384, f);
            if(cmm2->poll()) break;
        }
        fclose(f);
        printf("written: ");
        report(from);
        fills = gap_max = 0;
        last_fill = 0;
        from = cmm2->micros();
        if(!(f = fopen(name, "rb"))) { printf("cannot read %s\n", name); cmm2->audio_stop(); return 1; }
        for(int b = 0; b < 64; b++) {
            if(fread(block, 1, 16384, f) != 16384) { bad++; break; }
            for(int i = 0; i < 4096; i++) if(block[i] != (uint32_t)(b * 4096 + i)) { bad++; break; }
            if(cmm2->poll()) break;
        }
        fclose(f);
        remove(name);
        printf("read back %s: ", bad ? "FAIL" : "PASS");
        report(from);
        cmm2->audio_stop();
        return 0;
    }
    for(int i = 1; i < 8; i++) {                   /* the scale */
        if(wait_ms(500)) break;
        note(scale[i], scale[i] * 1.5f);
    }
    wait_ms(500);
    report(from);
    cmm2->audio_stop();
    printf("stopped\n");
    return 0;
}
