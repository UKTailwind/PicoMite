/* PicoMite C services shared by the RP2040 ELF runner and RP2350 TinyCC.
 * There is no compiler dependency: error/abort handling belongs to
 * pm_core_call(), which dispatches through the active run context.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "core/pm_api.h"
#include "pico/time.h"
/* graphics/Draw.c provides these on VGA and non-VGA builds. */
extern int last_fcolour, last_bcolour;
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define PM_DEFAULT_ARG (-2)

static void pm_core_range(int v, int lo, int hi)
{
    if (v < lo || v > hi)
        error("~ is invalid (valid is ~ to ~)", (long long)v, (long long)lo, (long long)hi);
}

void pm_print(const char *s)
{
    if (s) MMPrintString((char *)s);
}

static void pm_core_yield_do(void *p)
{
    (void)p;
#ifdef PICOMITEWEB
    if (WIFIconnected || WebScanActive) ProcessWeb(1);
#endif
    ProcessBT();
    routinechecks();
}
void pm_yield(void) { pm_core_call(pm_core_yield_do, NULL); }

void pm_delay_us(unsigned int us)
{
    uint64_t end = time_us_64() + (uint64_t)us;
    while (time_us_64() < end)
        pm_core_call(NULL, NULL);
}
void pm_pause(unsigned int ms)
{
    uint64_t end = time_us_64() + (uint64_t)ms * 1000u;
    do pm_yield(); while (time_us_64() < end);
}

static void pm_core_keydown_do(void *p)
{
    int *a = p;
    pm_core_range(a[0], 0, 8);
    a[1] = KeyDownValue(a[0]);
}
int pm_keydown(int n)
{
    int a[2] = {n, 0};
    pm_core_call(pm_core_keydown_do, a);
    return a[1];
}

/* DEVICE(MOUSE channel, what): X/Y/L/R/M/W/B/D/T, matching BASIC. */
static void pm_core_mouse_do(void *p)
{
    int *a = p;
    volatile struct s_nunstruct *m;
    pm_core_range(a[0], 1, 4);
    m = &nunstruct[a[0]];
    switch (toupper((unsigned char)a[1])) {
    case 'X': a[2] = m->ax; break;
    case 'Y': a[2] = m->ay; break;
    case 'L': a[2] = m->L; break;
    case 'R': a[2] = m->R; break;
    case 'M': a[2] = m->C; break;
    case 'W': a[2] = m->az; break;
    case 'B': a[2] = m->x0; break;
    case 'D': a[2] = m->Z; m->Z = 0; break;
    case 'T': a[2] = m->classic[0]; break;
    default: SyntaxError();
    }
}
int pm_mouse(int channel, int what)
{
    int a[3] = {channel, what, 0};
    pm_core_call(pm_core_mouse_do, a);
    return a[2];
}

static void pm_core_usb_do(void *p)
{
    int *a = p;
#ifdef USBKEYBOARD
    pm_core_range(a[0], 0, 4);
    a[1] = a[0] ? HID[a[0] - 1].Device_type : Current_USB_devices;
#else
    (void)a;
    SyntaxError();
#endif
}
int pm_usb_device(int port)
{
    int a[2] = {port, 0};
    pm_core_call(pm_core_usb_do, a);
    return a[1];
}

int pm_errno(void) { return MMerrno; }
const char *pm_errmsg(void) { return MMErrMsg; }
void pm_error_clear(void) { MMerrno = 0; MMErrMsg[0] = 0; }

static int pm_core_fb_bpp(void)
{
#ifdef PICOMITEVGA
    if (DISPLAY_TYPE == SCREENMODE1) return 1;
#ifdef HDMI
    if (DISPLAY_TYPE == SCREENMODE4) return 16;
    if (DISPLAY_TYPE == SCREENMODE5) return 8;
#endif
#endif
    return 4;
}

static int pm_core_fill(void *dst, const void *src, unsigned have)
{
    unsigned want;
    if (!dst) return -1;
    want = *(const unsigned *)dst;
    if (want < sizeof(unsigned)) return -1;
    memcpy((char *)dst + sizeof(unsigned), (const char *)src + sizeof(unsigned),
           (want < have ? want : have) - sizeof(unsigned));
    return 0;
}

int pm_get_framebuffer(void *out)
{
    struct { unsigned size; void *pixels; int width, height, stride, format; } f;
    unsigned char *buf = WriteBuf;
#ifndef PICOMITEVGA
#if PICOMITERP2350
    if (buf == NULL && Option.DISPLAY_TYPE >= VIRTUAL && Option.DISPLAY_TYPE < VGA222)
        buf = FRAMEBUFFER;
#else
    if (buf == NULL && Option.DISPLAY_TYPE >= VIRTUAL)
        buf = FRAMEBUFFER;
#endif
#endif
    if (!buf || HRes <= 0 || VRes <= 0) return -1;
    memset(&f, 0, sizeof(f));
    f.pixels = buf;
    f.width = HRes;
    f.height = VRes;
    f.format = pm_core_fb_bpp();
    f.stride = f.format == 1 ? (HRes + 7) >> 3 : f.format == 4 ? (HRes + 1) >> 1 : HRes * (f.format >> 3);
    return pm_core_fill(out, &f, sizeof(f));
}
unsigned pm_fb_colour(int rgb)
{
    switch (pm_core_fb_bpp()) {
    case 1: return (rgb & 0xffffff) != 0;
    case 8: return RGB332((uint32_t)rgb);
    case 16: return RGB555((uint32_t)rgb);
    default: return RGB121((uint32_t)rgb);
    }
}

static int pm_core_font_code(int font, int scale)
{
    if (font == PM_DEFAULT_ARG) font = (gui_font >> 4) + 1;
    if (scale == PM_DEFAULT_ARG) scale = gui_font & 15;
    pm_core_range(font, 1, FONT_TABLE_SIZE);
    if (!FontTable[font - 1]) error("Invalid font #%", font);
    pm_core_range(scale, 1, 15);
    return ((font - 1) << 4) | scale;
}
static void pm_core_font_width_do(void *p)
{
    int *a = p; a[2] = GetFontWidth(pm_core_font_code(a[0], a[1]));
}
int pm_font_width(int font, int scale)
{
    int a[3] = {font, scale, 0};
    pm_core_call(pm_core_font_width_do, a);
    return a[2];
}
static void pm_core_font_height_do(void *p)
{
    int *a = p; a[2] = GetFontHeight(pm_core_font_code(a[0], a[1]));
}
int pm_font_height(int font, int scale)
{
    int a[3] = {font, scale, 0};
    pm_core_call(pm_core_font_height_do, a);
    return a[2];
}

static int pm_core_colour_arg(int c, int lo, int dflt)
{
    if (c == PM_DEFAULT_ARG) return dflt;
    pm_core_range(c, lo, WHITE);
    return c;
}

typedef struct {
    int x, y, font, scale, fg, bg;
    const char *s, *align;
} pm_text_args;
static void pm_core_text_do(void *p)
{
    const pm_text_args *a = p;
    int jh = 0, jv = 0, jo = 0;
    CheckDisplay();
    if (a->align && *a->align &&
        !GetJustification((char *)a->align, &jh, &jv, &jo)) error("Justification");
    GUIPrintString(a->x, a->y, pm_core_font_code(a->font, a->scale),
                   jh, jv, jo,
                   pm_core_colour_arg(a->fg, 0, gui_fcolour),
                   pm_core_colour_arg(a->bg, -1, gui_bcolour),
                   (char *)a->s);
    if (Option.Refresh) Display_Refresh();
}
void pm_text(int x, int y, const char *s, const char *align, int font, int scale, int fg, int bg)
{
    pm_text_args a = {x, y, font, scale, fg, bg, s, align};
    pm_core_call(pm_core_text_do, &a);
}

static void pm_core_draw_box_do(void *p)
{
    int *a = p;
    int x = a[0], y = a[1], w = a[2], h = a[3], lw = a[4], c = a[5], fill = a[6];
    CheckDisplay();
    if (lw == PM_DEFAULT_ARG) lw = 1;
    else pm_core_range(lw, 0, 100);
    c = pm_core_colour_arg(c, 0, gui_fcolour);
    fill = pm_core_colour_arg(fill, -1, -1);
    if (w && h) DrawBox(x, y, x + w + (w > 0 ? -1 : 1), y + h + (h > 0 ? -1 : 1), lw, c, fill);
    if (Option.Refresh) Display_Refresh();
}
void pm_draw_box(int x, int y, int w, int h, int lw, int c, int fill)
{
    int a[7] = {x, y, w, h, lw, c, fill};
    pm_core_call(pm_core_draw_box_do, a);
}

static void pm_core_draw_rbox_do(void *p)
{
    int *a = p;
    int x = a[0], y = a[1], w = a[2], h = a[3], r = a[4], c = a[5], fill = a[6];
    CheckDisplay();
    if (r == PM_DEFAULT_ARG) r = 10;
    else pm_core_range(r, 0, 100);
    c = pm_core_colour_arg(c, 0, gui_fcolour);
    fill = pm_core_colour_arg(fill, -1, -1);
    if (w && h) DrawRBox(x, y, x + w + (w > 0 ? -1 : 1), y + h + (h > 0 ? -1 : 1), r, c, fill);
    if (Option.Refresh) Display_Refresh();
}
void pm_draw_rbox(int x, int y, int w, int h, int r, int c, int fill)
{
    int a[7] = {x, y, w, h, r, c, fill};
    pm_core_call(pm_core_draw_rbox_do, a);
}

static int pm_core_fb_letter(int b)
{
    b = toupper((unsigned char)b);
    if (b != 'N' && b != 'F' && b != 'L' && b != 'T' && b != '2')
        error("Invalid argument");
    return b;
}
static void pm_core_blit_fb_do(void *p)
{
    int *a = p;
    a[0] = pm_core_fb_letter(a[0]);
    a[1] = pm_core_fb_letter(a[1]);
    BlitFramebufferC(a[0], a[1], a[2], a[3], a[4], a[5],
                     a[6], a[7], a[8] == PM_DEFAULT_ARG ? -1 : a[8]);
}
void pm_blit_framebuffer(int from, int to, int x1, int y1, int x2, int y2,
                         int w, int h, int colour)
{
    int a[9] = {toupper((unsigned char)from), toupper((unsigned char)to),
                x1, y1, x2, y2, w, h, colour};
    pm_core_call(pm_core_blit_fb_do, a);
}

/* Direct services also used by the BASIC graphics/audio engine. */
static void pm_core_sprite_show_do(void *p)
{
    int *a = p;
    SpriteShowC(a[0], a[1], a[2], a[3], a[4] == PM_DEFAULT_ARG ? -1 : a[4]);
}
void pm_sprite_show(int n, int x, int y, int layer, int rotation)
{
    int a[5] = {n, x, y, layer, rotation};
    pm_core_call(pm_core_sprite_show_do, a);
}
static void pm_core_sprite_hide_do(void *p) { SpriteHideC(*(int *)p); }
void pm_sprite_hide(int n) { pm_core_call(pm_core_sprite_hide_do, &n); }
static void pm_core_sprite_write_do(void *p)
{
    int *a = p;
    SpriteWriteC(a[0], a[1], a[2], a[3] == PM_DEFAULT_ARG ? -1 : a[3]);
}
void pm_sprite_write(int n, int x, int y, int orientation)
{
    int a[4] = {n, x, y, orientation};
    pm_core_call(pm_core_sprite_write_do, a);
}
static void pm_core_sprite_close_all_do(void *p) { (void)p; SpriteCloseAllC(); }
void pm_sprite_close_all(void) { pm_core_call(pm_core_sprite_close_all_do, NULL); }

static void pm_core_fb_copy_do(void *p)
{
    int *a = p;
    FrameBufferCopyC(pm_core_fb_letter(a[0]), pm_core_fb_letter(a[1]), a[2] != 0);
}
void pm_framebuffer_copy(int from, int to, int at_blank)
{
    int a[3] = {from, to, at_blank};
    pm_core_call(pm_core_fb_copy_do, a);
}

typedef struct { const int16_t *samples; unsigned frames, rate, channels; } pm_pcm_args;
static void pm_core_pcm_play_do(void *p)
{
    pm_pcm_args *a = p;
    PCMPlay(a->samples, a->frames, a->rate, a->channels);
}
void pm_pcm_play(const int16_t *samples, unsigned frames, unsigned rate, unsigned channels)
{
    pm_pcm_args a;
    a.samples = samples; a.frames = frames; a.rate = rate; a.channels = channels;
    pm_core_call(pm_core_pcm_play_do, &a);
}
static void pm_core_pcm_stop_do(void *p) { (void)p; PCMStop(); }
void pm_pcm_stop(void) { pm_core_call(pm_core_pcm_stop_do, NULL); }
int pm_pcm_playing(void) { return PCMPlaying(); }
static void pm_core_pcm_poll_do(void *p) { (void)p; PCMPoll(); }
void pm_pcm_poll(void) { pm_core_call(pm_core_pcm_poll_do, NULL); }

static int pm_core_colour_value(int c)
{
    extern const int CMM1map[16];
    if (CMM1) {
        pm_core_range(c, 0, 15);
        return CMM1map[c];
    }
    pm_core_range(c, 0, 0xFFFFFFF);
    return c;
}
static void pm_core_colour_do(void *p)
{
    int *a = p;
    gui_fcolour = pm_core_colour_value(a[0]);
    if (a[1] != PM_DEFAULT_ARG) gui_bcolour = pm_core_colour_value(a[1]);
    last_fcolour = gui_fcolour;
    last_bcolour = gui_bcolour;
    if (!CurrentLinePtr) { PromptFC = gui_fcolour; PromptBC = gui_bcolour; }
}
void pm_colour(int fc, int bc)
{
    int a[2] = {fc, bc};
    pm_core_call(pm_core_colour_do, a);
}

static void pm_core_mode_do(void *p)
{
#ifdef PICOMITEVGA
    int mode = *(int *)p;
    pm_core_range(mode, 1, MAXMODES);
    setmode(mode, true);
#if defined(USBKEYBOARD) && defined(GUICONTROLS) && defined(PICOMITEVGA)
    OSK_Invalidate();
#endif
#else
    (void)p;
    error("MODE is not available on this firmware");
#endif
}
void pm_mode(int mode) { pm_core_call(pm_core_mode_do, &mode); }
