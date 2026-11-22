/* doomgeneric_pico.c: Doom on the PicoMite (the PC3), a loadable program (RUN "doom.app") through doomgeneric, which
   make.py fetches. From the Colour Maximite 2's doomgeneric_cmm2.c. MIT License, as loader/pmimg.h; the program built
   with it is GPL-2.0, as Doom's source is.

     The picture: MODE 5 (320x240 at 640x480, 256 colours from MAP's palette), Doom's 320x200 frame in its middle (the
       bands above and below black: its messages go to the serial console only), its palette given to MAP when it
       changes. -vsync waits for the display's frame before a copy. A resolution whose
       MODE 5 has no palette (1280x720's RGB332) is refused.
     The keys: KEYDOWN's set, read each tic, its changes made Doom's key events. Ctrl fires, Space opens, Shift runs,
       Alt strafes with the arrows, and , and . strafe. A shifted digit or symbol is taken as its key (Shift-1 is 1).
     The files: the IWAD is the first of freedoom1.wad, doom1.wad, doom.wad, freedoom2.wad and doom2.wad in the
       program's directory, unless -iwad names one (B:/Doom: a WAD is too big for A:). The configuration and the saved
       games go in the current directory: CHDIR "B:/Doom" first keeps them with the game.
     The sound: the effects mixed here, in the firmware's sound stream (its fill, called in an interrupt), 16 voices
       of Doom's 8-bit lumps taken to the stream's rate as DMX took them (each sample repeated: no smoothing). No music:
       the PicoMite has no synth. PLAY VOLUME sets the level. -nosound for none.
     At the end it prints the frames it drew and their rate. Ctrl-C quits (and stops the BASIC program that ran it). */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "cmm2.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "doomtype.h"
#include "i_video.h"
#include "i_system.h"
#include "i_sound.h"
#include "m_argv.h"
#include "w_wad.h"
#include "z_zone.h"

static cmm2_video v;
static uint8_t *top;                                                // the frame's first line on the page
static uint64_t start;
static int vsync;
static uint32_t frames;

void DG_Init(void) {
    static const uint8_t black[3];
    if(cmm2->video_mode(5, 8, &v) < 0 || v.width < DOOMGENERIC_RESX || v.height < DOOMGENERIC_RESY ||
       cmm2->video_palette(0, 1, black) < 0) {
        fprintf(stderr, "Doom needs MODE 5 with a palette: OPTION RESOLUTION 640x480\n");
        exit(1);
    }
    cmm2->con_screen(0);                                            // (its messages to the serial console only: on the
    memset(v.fb, 0, v.height * v.pitch);                            // screen they showed above and below the picture)
    top = v.fb + (v.height - DOOMGENERIC_RESY) / 2 * v.pitch + (v.width - DOOMGENERIC_RESX) / 2;
    vsync = M_CheckParm("-vsync") > 0;
    start = cmm2->micros();
}

static void keys(void);

void DG_DrawFrame(void) {
    const uint8_t *s = DG_ScreenBuffer;
    uint8_t *d = top;
    int i;
    if(palette_changed) {                                           // (shown from the next frame)
        uint8_t rgb[256 * 3];
        for(i = 0; i < 256; i++) {
            rgb[i * 3] = colors[i].r;
            rgb[i * 3 + 1] = colors[i].g;
            rgb[i * 3 + 2] = colors[i].b;
        }
        cmm2->video_palette(0, 256, rgb);
        palette_changed = false;
    } else if(vsync) cmm2->video_wait();
    for(i = 0; i < DOOMGENERIC_RESY; i++, s += DOOMGENERIC_RESX, d += v.pitch)
        memcpy(d, s, DOOMGENERIC_RESX);
    frames++;
    keys();
}

void DG_SleepMs(uint32_t ms) {
    uint64_t end = cmm2->micros() + (uint64_t)ms * 1000;
    while(cmm2->micros() < end) {
        keys();                                                     // (the USB devices seen to while it waits)
        cmm2->delay_us(500);
    }
}

uint32_t DG_GetTicksMs(void) { return (uint32_t)((cmm2->micros() - start) / 1000); }

void DG_SetWindowTitle(const char *title) { (void)title; }

/* the keys: each KEYDOWN code held is turned into Doom's key when it is first seen, and the same key is released when
   it goes; the modifiers' bits (KEYDOWN(7)'s, as the CMM2's) are Doom's fire, run and strafe keys */
#define KQ 64
static uint16_t kq[KQ];                                             // (pressed << 8) | key
static int kqin, kqout, held[6], heldkey[6], mods;

static void kput(int pressed, int key) {
    if(key == 0 || (kqin + 1) % KQ == kqout) return;
    kq[kqin] = (uint16_t)(pressed << 8 | key);
    kqin = (kqin + 1) % KQ;
}

static int doomkey(int c, int m) {
    static const char shifted[] = "!1\"2@2#3$4%5^6&7*8(9)0_-+=<,>.?/:;{[}]|\\~`";
    const char *p;
    if(c >= 1 && c <= 26 && (m & 0x22)) return 'a' + c - 1;         // a letter with Ctrl held: its control code
    switch(c) {
        case 0x80: return KEY_UPARROW;
        case 0x81: return KEY_DOWNARROW;
        case 0x82: return KEY_LEFTARROW;
        case 0x83: return KEY_RIGHTARROW;
        case 0x84: return KEY_INS;
        case 0x7F: return KEY_DEL;
        case 0x86: return KEY_HOME;
        case 0x87: return KEY_END;
        case 0x88: return KEY_PGUP;
        case 0x89: return KEY_PGDN;
        case 0x08: return KEY_BACKSPACE;
        case 0x09: return KEY_TAB;
        case 0x0A: case 0x0D: return KEY_ENTER;
        case 0x1B: return KEY_ESCAPE;
        case ' ': return KEY_USE;
        case ',': return KEY_STRAFE_L;
        case '.': return KEY_STRAFE_R;
    }
    if(c >= 0x91 && c <= 0x9A) return KEY_F1 + c - 0x91;           // F1-F10 (Doom's are consecutive), F11, F12
    if(c == 0x9B) return KEY_F11;
    if(c == 0x9C) return KEY_F12;
    if(c >= 'A' && c <= 'Z') return c + 32;
    for(p = shifted; *p; p += 2) if(*p == c) return p[1];
    return c < 0x80 ? c : 0;
}

static void keys(void) {
    int k[7], i, j, m;
    if(cmm2->poll()) I_Quit();                                      // Ctrl-C (the settings saved, as Doom's quit does)
    cmm2->keys_held(k);
    m = k[6];
    for(i = 0; i < 6; i++) {                                        // released: no longer in the set
        if(!held[i]) continue;
        for(j = 0; j < 6 && k[j] != held[i]; j++) {}
        if(j == 6) { kput(0, heldkey[i]); held[i] = 0; }
    }
    if((m ^ mods) & 0x22) kput((m & 0x22) != 0, KEY_FIRE);          // Ctrl, either
    if((m ^ mods) & 0x88) kput((m & 0x88) != 0, KEY_RSHIFT);        // Shift
    if((m ^ mods) & 0x11) kput((m & 0x11) != 0, KEY_RALT);          // Alt
    mods = m;
    for(j = 0; j < 6; j++) {                                        // pressed: new in the set
        if(!k[j]) continue;
        for(i = 0; i < 6 && held[i] != k[j]; i++) {}
        if(i < 6) continue;
        for(i = 0; i < 6 && held[i]; i++) {}
        if(i == 6) break;
        held[i] = k[j];
        heldkey[i] = doomkey(k[j], m);
        kput(1, heldkey[i]);
    }
}

int DG_GetKey(int *pressed, unsigned char *key) {
    if(kqin == kqout) keys();
    if(kqin == kqout) return 0;
    *pressed = kq[kqout] >> 8;
    *key = (unsigned char)kq[kqout];
    kqout = (kqout + 1) % KQ;
    return 1;
}

/* the sound effects: voices of Doom's sound lumps (DMX: format 3, the rate, the length, then 8-bit unsigned samples, of
   which DMX played from the lump's 16th byte to 16 before the end), mixed in the stream's fill; a voice is set going
   with the fill held off, and stops by its data going */
#define NVOICE 16
typedef struct { const uint8_t *data; uint32_t pos, end, step; int left, right; } voice_t;   // pos, end: 16.16
typedef struct { const uint8_t *data; uint32_t len, rate; } sfx_t;
static voice_t voice[NVOICE];
static uint32_t rate;                                               // the stream's (0: not started)
static int sfxprefix;
int use_libsamplerate = 0;                                          // (i_sound.c's settings, for libsamplerate: unused)
float libsamplerate_scale = 0.65f;

static void fill(int16_t *buf, uint32_t frames) {
    uint32_t i;
    int v;
    for(i = 0; i < frames; i++) {
        int32_t l = 0, r = 0, s;
        for(v = 0; v < NVOICE; v++) {
            voice_t *x = &voice[v];
            if(x->data == NULL) continue;
            s = x->data[x->pos >> 16] - 128;
            l += s * x->left;
            r += s * x->right;
            if((x->pos += x->step) >= x->end) x->data = NULL;
        }
        l *= 2;                                                     // (a voice at full volume: +-32K)
        r *= 2;
        buf[2 * i] = l > 32767 ? 32767 : l < -32768 ? -32768 : l;
        buf[2 * i + 1] = r > 32767 ? 32767 : r < -32768 ? -32768 : r;
    }
}

static int audio(void) {                                            // the stream started, once
    int r;
    if(rate == 0 && (r = cmm2->audio_start(fill)) > 0) rate = r;
    return rate != 0;
}

static void gains(voice_t *x, int vol, int sep) {                   // vol 0-127; sep 0 (left) to 254 (right)
    int l = 254 - sep, r = sep;
    x->left = vol * (l > 127 ? 127 : l < 0 ? 0 : l) / 127;
    x->right = vol * (r > 127 ? 127 : r < 0 ? 0 : r) / 127;
}

static sfx_t *sfx(sfxinfo_t *s) {                                   // the lump, read when first played and kept
    const uint8_t *d;
    uint32_t n, len;
    sfx_t *x;
    if(s->driver_data) return s->driver_data;
    if(s->lumpnum < 0) return NULL;
    d = W_CacheLumpNum(s->lumpnum, PU_STATIC);
    n = W_LumpLength(s->lumpnum);
    if(n < 8 || d[0] != 3 || d[1] != 0) return NULL;
    len = d[4] | d[5] << 8 | d[6] << 16 | (uint32_t)d[7] << 24;
    if(len > n - 8 || len <= 48 || (x = malloc(sizeof(sfx_t))) == NULL) return NULL;
    x->data = d + 16;
    x->len = len - 32;
    x->rate = d[2] | d[3] << 8;
    s->driver_data = x;
    return x;
}

static boolean snd_init(boolean prefix) { sfxprefix = prefix; return audio(); }
static void snd_shutdown(void) { int v; for(v = 0; v < NVOICE; v++) voice[v].data = NULL; }
static int snd_lump(sfxinfo_t *s) {
    char name[9];
    if(s->link) s = s->link;
    snprintf(name, sizeof(name), sfxprefix ? "ds%s" : "%s", s->name);
    return W_CheckNumForName(name);
}
static void snd_update(void) {}
static void snd_params(int v, int vol, int sep) { if(v >= 0 && v < NVOICE) gains(&voice[v], vol, sep); }
static int snd_start(sfxinfo_t *s, int v, int vol, int sep) {
    sfx_t *x;
    if(!rate || v < 0 || v >= NVOICE || (x = sfx(s)) == NULL) return -1;
    cmm2->audio_lock();
    voice[v].pos = 0;
    voice[v].end = x->len << 16;
    voice[v].step = (x->rate << 16) / rate;
    gains(&voice[v], vol, sep);
    voice[v].data = x->data;
    cmm2->audio_unlock();
    return v;
}
static void snd_stop(int v) { if(v >= 0 && v < NVOICE) voice[v].data = NULL; }
static boolean snd_playing(int v) { return v >= 0 && v < NVOICE && voice[v].data != NULL; }
static void snd_cache(sfxinfo_t *s, int n) { (void)s; (void)n; }

static snddevice_t devices[] = { SNDDEVICE_SB, SNDDEVICE_PAS, SNDDEVICE_GUS, SNDDEVICE_WAVEBLASTER, SNDDEVICE_SOUNDCANVAS,
                                 SNDDEVICE_AWE32, SNDDEVICE_ADLIB, SNDDEVICE_GENMIDI };
sound_module_t DG_sound_module = {
    devices, sizeof(devices) / sizeof(devices[0]), snd_init, snd_shutdown, snd_lump, snd_update, snd_params, snd_start,
    snd_stop, snd_playing, snd_cache,
};

/* no music: the PicoMite has no synth (the CMM2's OPL3 was ruled out for it), so each song is taken and dropped */
static boolean mus_init(void) { return false; }
static void mus_none(void) {}
static void mus_volume(int v) { (void)v; }
static void *mus_register(void *data, int len) { (void)data; (void)len; return NULL; }
static void mus_unregister(void *h) { (void)h; }
static void mus_play(void *h, boolean loop) { (void)h; (void)loop; }
static boolean mus_playing(void) { return false; }

music_module_t DG_music_module = {
    devices, sizeof(devices) / sizeof(devices[0]), mus_init, mus_none, mus_volume, mus_none, mus_none,
    mus_register, mus_unregister, mus_play, mus_none, mus_playing, NULL,
};

static void report(void) {
    uint64_t t = cmm2->micros() - start;
    printf("%lu frames in %lu ms: %lu.%lu frames a second\n", (unsigned long)frames, (unsigned long)(t / 1000),
           (unsigned long)(frames * 1000000ull / t), (unsigned long)(frames * 10000000ull / t % 10));
}

int main(int argc, char **argv) {
    static const char *const wads[] = { "freedoom1.wad", "doom1.wad", "doom.wad", "freedoom2.wad", "doom2.wad" };
    char **args = malloc((argc + 5) * sizeof(char *)), dir[256], *p;
    static char mb[8];
    int i, n = 0;
    FILE *f;
    for(i = 0; i < argc; i++) args[n++] = argv[i];
    for(i = 1; i < argc && strcmp(argv[i], "-mb"); i++) {}
    if(i == argc) {                                                 // no -mb: the zone as large as the heap allows, up to
        int m = (int)((cmm2_start->heap_size - (512 << 10)) >> 20); // doomgeneric's 6 MB, with 512 KB left for the rest
        if(m > 6) m = 6;
        snprintf(mb, sizeof(mb), "%d", m);
        args[n++] = "-mb";
        args[n++] = mb;
    }
    for(i = 1; i < argc && strcmp(argv[i], "-iwad"); i++) {}
    if(i == argc) {                                                 // no -iwad: the first IWAD in the program's directory
        strncpy(dir, argv[0], sizeof(dir) - 32);
        dir[sizeof(dir) - 32] = 0;
        p = strrchr(dir, '/');
        if(p == NULL) p = strrchr(dir, '\\');
        if(p == NULL) p = (dir[0] && dir[1] == ':') ? dir + 1 : dir - 1;
        for(i = 0; i < (int)(sizeof(wads) / sizeof(wads[0])); i++) {
            strcpy(p + 1, wads[i]);
            if((f = fopen(dir, "rb")) != NULL) {
                fclose(f);
                args[n++] = "-iwad";
                args[n++] = strdup(dir);
                break;
            }
        }
    }
    args[n] = NULL;
    atexit(report);
    doomgeneric_Create(n, args);
    for(;;) doomgeneric_Tick();
}
